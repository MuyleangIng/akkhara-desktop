"""The "brain": akkhara-mini (ONNX int8) → Khmer suggestions. Same on Mac and Windows.

Only needs onnxruntime + numpy (no PyTorch, no transformers), so the app stays small.
It is a line-by-line copy of the phone keyboard (android/.../MiniModel.kt + LiveEngine.kt):

    1. tokenizer: every character → an id (tokenizer.json), then </s>
    2. encoder: ids → hidden states                    (once per chunk)
    3. decoder: hidden states + Khmer letters so far → scores of the next letter
         greedy = take the best letter each step (fast, #1 only)
         beam   = keep the 5 best partial sentences (top-5)
    4. live typing: cut long text into 6-word chunks; finished chunks come from a cache,
       so every key press only recomputes the LAST chunk.

    from akkhara_desktop.engine import Engine
    e = Engine("models/akkhara-mini")
    e.candidates("nh jg tv sala rean")   # ['ខ្ញុំចង់ទៅសាលារៀន', ...]
"""

from __future__ import annotations

import json
import re
import threading
from collections import OrderedDict
from pathlib import Path
from typing import Callable

import numpy as np
import onnxruntime as ort

PAD, EOS, UNK = 0, 1, 2  # T5: the decoder starts from <pad>
MAX_IN = MAX_OUT = 160

Cancelled = Callable[[], bool]  # returns True when a newer key press made this work useless


def _never() -> bool:
    return False


def _log_softmax(x: np.ndarray) -> np.ndarray:
    x = x - x.max(axis=-1, keepdims=True)
    return x - np.log(np.exp(x).sum(axis=-1, keepdims=True))


class MiniModel:
    """akkhara-mini: one chunk of Sing Khmer → Khmer (greedy #1 or beam top-k)."""

    def __init__(self, model_dir: str | Path, threads: int = 4):
        d = Path(model_dir)
        opts = ort.SessionOptions()
        opts.intra_op_num_threads = threads
        opts.graph_optimization_level = ort.GraphOptimizationLevel.ORT_ENABLE_ALL
        cpu = ["CPUExecutionProvider"]
        self.encoder = ort.InferenceSession(str(d / "encoder.int8.onnx"), opts, providers=cpu)
        self.decoder = ort.InferenceSession(str(d / "decoder.int8.onnx"), opts, providers=cpu)
        # abbreviation → full Khmer name (#2 suggestion: mptc → ក្រសួងប្រៃសណីយ៍…), optional file
        self.abbreviations: dict[str, str] = {}
        if (d / "abbreviations.tsv").exists():
            for line in (d / "abbreviations.tsv").read_text(encoding="utf-8").splitlines():
                parts = line.split("\t")
                if len(parts) >= 2 and parts[0] and not line.startswith("#") and parts[0] != "abbreviation":
                    self.abbreviations[parts[0].strip().lower()] = parts[1].strip()
        vocab = json.loads((d / "tokenizer.json").read_text(encoding="utf-8"))["model"]["vocab"]
        self.vocab: dict[str, int] = vocab
        self.id_to_char = {i: c for c, i in vocab.items()}

    # ---- tokenizer -------------------------------------------------------------------------
    def tokenize(self, text: str) -> np.ndarray:
        """One id per character (unknown → <unk>), then </s>."""
        ids = [self.vocab.get(ch, UNK) for ch in text[: MAX_IN - 1]]
        return np.array([ids + [EOS]], dtype=np.int64)

    def detokenize(self, ids) -> str:
        return "".join(self.id_to_char.get(int(i), "") for i in ids if int(i) > UNK)

    # ---- model calls -----------------------------------------------------------------------
    def _encode(self, text: str):
        ids = self.tokenize(text)
        mask = np.ones_like(ids)
        hidden = self.encoder.run(None, {"input_ids": ids, "attention_mask": mask})[0]
        return hidden, mask

    def _next_logprobs(self, hidden, mask, prefixes: list[list[int]]) -> np.ndarray:
        """Log-probabilities of the next letter for each prefix (all for the same input)."""
        b = len(prefixes)
        logits = self.decoder.run(None, {
            "decoder_input_ids": np.array(prefixes, dtype=np.int64),
            "encoder_hidden_states": np.repeat(hidden, b, 0),
            "attention_mask": np.repeat(mask, b, 0),
        })[0]
        return _log_softmax(logits[:, -1, :])  # only the LAST position matters

    # ---- greedy: #1 only (while typing) ------------------------------------------------------
    def greedy(self, text: str, cancelled: Cancelled = _never) -> str:
        if not text.strip():
            return ""
        hidden, mask = self._encode(text)
        seq = [PAD]
        for _ in range(MAX_OUT):
            if cancelled():
                break
            best = int(self._next_logprobs(hidden, mask, [seq])[0].argmax())
            if best == EOS:
                break
            seq.append(best)
        return self.detokenize(seq)

    # ---- beam search: top-k ------------------------------------------------------------------
    def top_k(self, text: str, k: int = 5, beams: int = 5,
              cancelled: Cancelled = _never) -> list[tuple[str, float]]:
        """Top-k (khmer, score). Score = sum of log-probs / length (like transformers)."""
        if not text.strip():
            return []
        hidden, mask = self._encode(text)
        seqs, scores = [[PAD]], np.zeros(1)
        finished: list[tuple[float, list[int]]] = []
        for step in range(MAX_OUT):
            if cancelled():
                return []  # a newer key press arrived: stop, and do not cache this
            total = scores[:, None] + self._next_logprobs(hidden, mask, seqs)  # (beams, vocab)
            new_seqs, new_scores = [], []
            for idx in np.argsort(total, axis=None)[::-1][: 2 * beams]:  # best (beam, letter) pairs
                b, tok = divmod(int(idx), total.shape[1])
                seq = seqs[b] + [tok]
                if tok == EOS:
                    finished.append((float(total[b, tok]) / (len(seq) - 1), seq))
                else:
                    new_seqs.append(seq)
                    new_scores.append(float(total[b, tok]))
                if len(new_seqs) == beams:
                    break
            # stop when k are finished and no open beam can beat the worst of them
            if len(finished) >= k and new_scores:
                if max(new_scores) / (step + 1) < sorted(s for s, _ in finished)[-k]:
                    break
            if not new_seqs:
                break
            seqs, scores = new_seqs, np.array(new_scores)
        if len(finished) < k:  # max length reached: open beams count too
            finished += [(float(s) / max(1, len(q) - 1), q) for s, q in zip(scores, seqs)]
        out, seen = [], set()
        for s, q in sorted(finished, key=lambda t: -t[0]):
            text_out = self.detokenize(q)
            if text_out not in seen:
                seen.add(text_out)
                out.append((text_out, s))
            if len(out) == k:
                break
        return out


def _is_latin(ch: str) -> bool:
    return ch.isascii() and ch.isalpha()


def join(left: str, right: str) -> str:
    """No space between Khmer pieces; one space next to a Latin word (training target rule)."""
    if not left:
        return right
    if not right:
        return left
    if _is_latin(left[-1]) or _is_latin(right[0]):
        return left + " " + right
    return left + right


class Engine:
    """Live typing on top of MiniModel: 6-word chunks + a cache (same as the phone)."""

    def __init__(self, model_dir: str | Path, chunk_words: int = 6, cache_size: int = 500):
        self.model = MiniModel(model_dir)
        self.chunk_words = chunk_words
        self.cache: OrderedDict[str, list[tuple[str, float]]] = OrderedDict()
        self.cache_size = cache_size
        self.lock = threading.Lock()

    def chunks(self, text: str) -> list[str]:
        words = re.split(r"\s+", text.strip())
        words = [w for w in words if w]
        n = self.chunk_words
        return [" ".join(words[i:i + n]) for i in range(0, len(words), n)]

    def _top(self, chunk: str, cancelled: Cancelled = _never) -> list[tuple[str, float]]:
        with self.lock:
            if chunk in self.cache:
                self.cache.move_to_end(chunk)
                return self.cache[chunk]
        r = self.model.top_k(chunk, cancelled=cancelled)
        if r:  # a cancelled (empty) result is NOT cached
            with self.lock:
                self.cache[chunk] = r
                while len(self.cache) > self.cache_size:
                    self.cache.popitem(last=False)
        return r

    def _prefix(self, cs: list[str]) -> str:
        out = ""
        for c in cs[:-1]:
            top = self._top(c)
            out = join(out, top[0][0] if top else "")
        return out

    def quick(self, text: str, cancelled: Cancelled = _never) -> str:
        """#1 only, greedy for the last chunk (fast, shown first while typing)."""
        special = with_abbreviations(text, self.model.abbreviations, lambda s: [self._quick(s, cancelled)], 1)
        return special[0] if special is not None else self._quick(text, cancelled)

    def _quick(self, text: str, cancelled: Cancelled = _never) -> str:
        cs = self.chunks(text)
        if not cs:
            return ""
        with self.lock:
            known = self.cache.get(cs[-1])
        last = known[0][0] if known else self.model.greedy(cs[-1], cancelled)
        return join(self._prefix(cs), last)

    def candidates(self, text: str, cancelled: Cancelled = _never) -> list[str]:
        """Top-5 for the whole text: best finished chunks + each candidate of the last chunk.
        Known abbreviations never reach the model: as typed #1, full Khmer name #2."""
        special = with_abbreviations(text, self.model.abbreviations, lambda s: self._candidates(s, cancelled), 5)
        return special if special is not None else self._candidates(text, cancelled)

    def _candidates(self, text: str, cancelled: Cancelled = _never) -> list[str]:
        cs = self.chunks(text)
        if not cs:
            return []
        prefix = self._prefix(cs)
        return [join(prefix, t) for t, _ in self._top(cs[-1], cancelled)]


TRAIL = ".,!?។៕"


def with_abbreviations(text: str, table: dict[str, str], top_fn, k: int = 5) -> list[str] | None:
    """Top-k for text with known abbreviations, which are NOT sent to the model (alone, the model turned
    "mptc" into a wrong ministry): #1 other words + abbreviation as typed, #2 the same with the full Khmer
    name, then other candidates of the last plain piece. None when there is no known abbreviation.
    Same logic as akkhara/abbrev.py (Akkhara repo), LiveEngine.kt and core/akkhara_engine.cpp."""
    if not table:
        return None
    segs: list[tuple[bool, str]] = []
    cur: list[str] = []
    for w in text.split():
        core = w.rstrip(TRAIL)
        if core and core.lower() in table:
            if cur:
                segs.append((False, " ".join(cur)))
                cur = []
            segs.append((True, w))
        else:
            cur.append(w)
    if cur:
        segs.append((False, " ".join(cur)))
    if not any(a for a, _ in segs):
        return None
    results = [[] if a else top_fn(v) for a, v in segs]
    plain = [i for i, (a, _) in enumerate(segs) if not a]
    last = plain[-1] if plain else None

    def build(alt: int = 0, full: bool = False) -> str:
        out = ""
        for i, (a, v) in enumerate(segs):
            if a:
                core = v.rstrip(TRAIL)
                piece = table[core.lower()] + v[len(core):] if full else v
                if full:  # 'theanakear aba': do not repeat ធនាគារ already written
                    piece = next((piece[n:] for n in range(len(piece) - 1, 2, -1) if out.endswith(piece[:n])), piece)
            else:
                r = results[i]
                j = alt if i == last else 0
                piece = r[j] if j < len(r) else (r[0] if r else v)
            out = join(out, piece)
        return out

    cands = [build(), build(full=True)]
    for j in range(1, len(results[last]) if last is not None else 0):
        c = build(alt=j)
        if c not in cands:
            cands.append(c)
        if len(cands) >= k:
            break
    return cands[:k]
