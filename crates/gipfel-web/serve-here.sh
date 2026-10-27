#!/bin/sh
# Build gipfel-web for wasm and refresh the static dist dir served from
# /tmp/gipfel-web-dist (python http.server on :8090). Dev-only helper.
set -e
cd "$(dirname "$0")/../.."
cargo build -p gipfel-web --target wasm32-unknown-unknown --release
DIST=/tmp/gipfel-web-dist
mkdir -p "$DIST"
wasm-bindgen --target web --out-dir "$DIST" target/wasm32-unknown-unknown/release/gipfel-web.wasm
cp crates/gipfel-web/www/style.css "$DIST/"
gzip -9 -n -c gipfel.dat > "$DIST/gipfel.dat.gz"
echo "dist refreshed: $(ls -la "$DIST" | awk '{s+=$5} END {print s" bytes"}')"
