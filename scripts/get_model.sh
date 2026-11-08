#!/usr/bin/env bash
# Download akkhara-mini (ONNX int8 + tokenizer, ~18.5 MB, open on Hugging Face) into models/akkhara-mini.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p models/akkhara-mini
base="https://huggingface.co/INGMUYLEANG/akkhara-mini/resolve/main/onnx"
for f in encoder.int8.onnx decoder.int8.onnx tokenizer.json config.json abbreviations.tsv; do
  curl -fL --progress-bar -o "models/akkhara-mini/$f" "$base/$f"
done
echo "model ready in models/akkhara-mini"
