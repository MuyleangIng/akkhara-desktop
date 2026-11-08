"""The desktop engine must give the SAME answers as the reference (akkhara/onnx_model.py, phone)."""
from pathlib import Path

import pytest

from akkhara_desktop.engine import Engine, join

MODEL = Path(__file__).resolve().parents[1] / "models" / "akkhara-mini"
needs_model = pytest.mark.skipif(not (MODEL / "encoder.int8.onnx").exists(), reason="run scripts/get_model.sh")


def test_join_spacing():
    assert join("ខ្ញុំ", "ទៅ") == "ខ្ញុំទៅ"
    assert join("សុខសប្បាយអត់", "bro") == "សុខសប្បាយអត់ bro"
    assert join("", "ទៅ") == "ទៅ"


@needs_model
def test_examples():
    e = Engine(MODEL)
    assert e.quick("nh jg tv sala rean") == "ខ្ញុំចង់ទៅសាលារៀន"
    assert e.candidates("tos jol CG")[0] == "តោះចូល CG"
    assert len(e.candidates("sok sbay ot bro")) == 5


@needs_model
def test_long_text_uses_chunks_and_cache():
    e = Engine(MODEL)
    text = "nh jg tv sala rean nv thngai saek"
    assert len(e.chunks(text)) == 2
    e.candidates(text)
    assert "nh jg tv sala rean nv" in e.cache  # finished chunk is cached


@needs_model
def test_cancel_stops_and_is_not_cached():
    e = Engine(MODEL)
    assert e.candidates("sl oun nah", cancelled=lambda: True) == []
    assert "sl oun nah" not in e.cache
