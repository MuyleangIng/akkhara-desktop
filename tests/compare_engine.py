"""C++ engine output (akkhara_cli) must equal the Python engine's top-5 on the same inputs.
Run with the SAME onnxruntime version as the C++ build (int8 maths differ a little between versions).

    python tests/compare_engine.py models/akkhara-mini tests/data/inputs.txt cpp.tsv
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import onnxruntime  # noqa: E402

from akkhara_desktop.engine import Engine  # noqa: E402

model, inputs, cpp = sys.argv[1:4]
e = Engine(model)
ins = Path(inputs).read_text(encoding="utf-8").splitlines()
want = ["\t".join(e.candidates(s)) for s in ins]
got = Path(cpp).read_text(encoding="utf-8").splitlines()
same = sum(a == b for a, b in zip(want, got))
print(f"onnxruntime {onnxruntime.__version__}: same top-5 {same} / {len(want)}")
for s, a, b in zip(ins, want, got):
    if a != b:
        print(f"  DIFF {s!r}\n    python: {a[:150]}\n    c++:    {b[:150]}")
sys.exit(0 if same == len(want) and len(got) == len(want) else 1)
