#!/bin/sh
# Build the gipfel WASM spike and serve the demo page.
# Requires: rustup (for the wasm target) and wasm-pack.
set -e
cd "$(dirname "$0")"

if ! rustup target list --installed 2>/dev/null | grep -q wasm32-unknown-unknown; then
	echo "adding wasm32-unknown-unknown target ..."
	rustup target add wasm32-unknown-unknown
fi
command -v wasm-pack >/dev/null 2>&1 || {
	echo "wasm-pack not found. Install it once with:"
	echo "  cargo install wasm-pack    (or: cargo binstall wasm-pack)"
	exit 1
}

wasm-pack build --target web

echo
echo "built. serve the demo with:"
echo "  (cd crates/gipfel-wasm && python3 -m http.server 8080)"
echo "then open  http://localhost:8080/demo/"
