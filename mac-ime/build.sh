#!/bin/bash
# Build the Akkhara Mac keyboard → build/Akkhara.app (Apple M-series, macOS 13.3+).
# Needs: Xcode Command Line Tools, ../third_party/onnxruntime-osx-arm64-1.22.0, ../models/akkhara-mini.
#   bash mac-ime/build.sh [version]
set -euo pipefail
cd "$(dirname "$0")"
VERSION="${1:-0.4.0}"
ROOT=..
ORT="${ORT_DIR:-$ROOT/third_party/onnxruntime-osx-arm64-1.22.0}"
MODEL="${MODEL_DIR:-$ROOT/models/akkhara-mini}"
B=build; APP=$B/Akkhara.app
rm -rf $B && mkdir -p $B/obj $APP/Contents/{MacOS,Frameworks,Resources/model}
TARGET=arm64-apple-macos13.3

echo "1/4 C++ engine"
for f in $ROOT/core/akkhara_engine.cpp src/akkhara_c.cpp; do
  clang++ -std=c++17 -O2 -target $TARGET -I"$ORT/include" -I$ROOT/core -Isrc -c "$f" -o $B/obj/$(basename "${f%.cpp}").o
done

echo "2/4 Swift input method"
swiftc -O -swift-version 5 -target $TARGET -module-name Akkhara \
  -import-objc-header src/akkhara_c.h src/*.swift $B/obj/*.o \
  -L"$ORT/lib" -lonnxruntime.1.22.0 -lc++ -framework InputMethodKit -framework Cocoa -framework Carbon \
  -Xlinker -rpath -Xlinker @executable_path/../Frameworks -o $APP/Contents/MacOS/Akkhara

# key test: the same input method code + a fake text box (test/KeyTest.swift), no app needed
swiftc -O -swift-version 5 -target $TARGET -module-name AkkharaKeyTest \
  -import-objc-header src/akkhara_c.h src/Engine.swift src/CandidateWindow.swift src/InputController.swift \
  test/main.swift $B/obj/*.o -L"$ORT/lib" -lonnxruntime.1.22.0 -lc++ -framework InputMethodKit -framework Cocoa \
  -Xlinker -rpath -Xlinker "$(cd "$ORT/lib" && pwd)" -o $B/akkhara_keytest

echo "3/4 bundle"
sed "s/__VERSION__/$VERSION/g" resources/Info.plist > $APP/Contents/Info.plist
cp "$ORT/lib/libonnxruntime.1.22.0.dylib" $APP/Contents/Frameworks/
cp "$MODEL"/{encoder.int8.onnx,decoder.int8.onnx,tokenizer.json,abbreviations.tsv} $APP/Contents/Resources/model/
swift make_icons.swift $B
cp $B/menu-icon.tiff $APP/Contents/Resources/
iconutil -c icns $B/AppIcon.iconset -o $APP/Contents/Resources/AppIcon.icns
cp $ROOT/LICENSE $APP/Contents/Resources/

echo "4/4 sign (ad-hoc)"
codesign --force -s - $APP/Contents/Frameworks/libonnxruntime.1.22.0.dylib
codesign --force -s - $APP
du -sh $APP
