#!/bin/sh
# Build wipfel and assemble a deployable static site in dist/wipfel/.
# Everything the app loads (wasm, hill database, stylesheet) is fetched
# relative to the page URL, so the directory can be mounted under any
# path on any static server.
set -e
cd "$(dirname "$0")/../.."
# size-optimized build for the web (the workspace default profile
# targets native binaries); --remove-name-section drops the debug
# symbol names wasm-bindgen adds for devtools
CARGO_PROFILE_RELEASE_OPT_LEVEL=z cargo build -p wipfel --target wasm32-unknown-unknown --release
DIST=dist/wipfel
rm -rf "$DIST"
mkdir -p "$DIST"
wasm-bindgen --remove-name-section --target web --out-dir "$DIST" target/wasm32-unknown-unknown/release/wipfel.wasm
# the loader page (not the trunk entry point, which is dev-only)
rm -f "$DIST"/wipfel.d.ts "$DIST"/wipfel_bg.wasm.d.ts
cp crates/wipfel/www/index.html crates/wipfel/www/style.css "$DIST/"
gzip -9 -n -c gipfel.dat > "$DIST/gipfel.dat.gz"
echo "site assembled in $DIST ($(du -sh "$DIST" | cut -f1)):"
ls "$DIST"
