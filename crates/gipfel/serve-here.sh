#!/bin/sh
# Build gipfel for wasm and refresh the static dist dir served from
# /tmp/gipfel-dist (python http.server on :8090). Dev-only helper.
set -e
cd "$(dirname "$0")/../.."
touch crates/gipfel/src/app.rs  # re-stamp every build (option_env! does not track STAMP)
STAMP=$(date -u +%Y-%m-%d_%H:%M) cargo build -p gipfel --target wasm32-unknown-unknown --release
DIST=/tmp/gipfel-dist
mkdir -p "$DIST"
wasm-bindgen --target web --out-dir "$DIST" target/wasm32-unknown-unknown/release/gipfel.wasm
cp crates/gipfel/www/index.html crates/gipfel/www/style.css crates/gipfel/www/loader.js "$DIST/"
gzip -9 -n -c gipfel.dat > "$DIST/gipfel.dat.gz"
echo "dist refreshed: $(ls -la "$DIST" | awk '{s+=$5} END {print s" bytes"}')"
