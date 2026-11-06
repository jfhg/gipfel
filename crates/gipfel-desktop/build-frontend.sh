#!/bin/sh
# Build the gipfel WASM front end into dist/ for tauri to bundle.
# Runs from crates/gipfel-desktop (tauri beforeBuildCommand).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
DIST="$HERE/dist"
mkdir -p "$DIST"
cd "$HERE/../.."
cargo build -p gipfel --target wasm32-unknown-unknown --release
wasm-bindgen --target web --out-dir "$DIST" \
    target/wasm32-unknown-unknown/release/gipfel.wasm
cp crates/gipfel/www/index.html crates/gipfel/www/style.css \
   crates/gipfel/www/loader.js "$DIST/"
gzip -9 -n -c gipfel.dat > "$DIST/gipfel.dat.gz"
echo "frontend dist ready: $DIST"
