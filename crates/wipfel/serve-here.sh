#!/bin/sh
# Build wipfel for wasm and refresh the static dist dir served from
# /tmp/wipfel-dist (python http.server on :8090). Dev-only helper.
set -e
cd "$(dirname "$0")/../.."
cargo build -p wipfel --target wasm32-unknown-unknown --release
DIST=/tmp/wipfel-dist
mkdir -p "$DIST"
wasm-bindgen --target web --out-dir "$DIST" target/wasm32-unknown-unknown/release/wipfel.wasm
cp crates/wipfel/www/index.html crates/wipfel/www/style.css "$DIST/"
cp crates/wipfel/www/sw.js crates/wipfel/www/manifest.webmanifest "$DIST/"
mkdir -p "$DIST/icons"
cp crates/wipfel/www/icons/*.png "$DIST/icons/"
gzip -9 -n -c gipfel.dat > "$DIST/gipfel.dat.gz"
echo "dist refreshed: $(ls -la "$DIST" | awk '{s+=$5} END {print s" bytes"}')"
