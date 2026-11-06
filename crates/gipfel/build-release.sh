#!/bin/sh
# Build gipfel and assemble a deployable static site in dist/gipfel/.
# Everything the app loads (wasm, hill database, stylesheet) is fetched
# relative to the page URL, so the directory can be mounted under any
# path on any static server.
set -e
cd "$(dirname "$0")/../.."
ROOT="$(pwd -P)"
# sanity check: refuse to run anywhere that is not the repo root
[ -f "$ROOT/Cargo.toml" ] && [ -d "$ROOT/crates/gipfel" ] || {
    echo "error: $ROOT does not look like the gipfel repo root" >&2
    exit 1
}
DIST="$ROOT/dist/gipfel"
# belt and braces: the only path this script may ever delete
case "$DIST" in
    "$ROOT"/dist/gipfel) ;;
    *) echo "error: refusing to touch unexpected path $DIST" >&2; exit 1 ;;
esac

# size-optimized build for the web (the workspace default profile
# targets native binaries); --remove-name-section drops the debug
# symbol names wasm-bindgen adds for devtools
CARGO_PROFILE_RELEASE_OPT_LEVEL=z cargo build -p gipfel --target wasm32-unknown-unknown --release

# stage into a fresh temp dir and swap it in only on success, so a
# failing build never destroys the previous site and the old site is
# removed at the last possible moment
mkdir -p "$ROOT/dist"
TMP="$(mktemp -d "$ROOT/dist/.gipfel-stage.XXXXXX")"
trap 'rm -rf "$TMP"' EXIT INT TERM

wasm-bindgen --remove-name-section --target web --out-dir "$TMP" \
    target/wasm32-unknown-unknown/release/gipfel.wasm
# the loader page (not the trunk entry point, which is dev-only)
rm -f "$TMP"/gipfel.d.ts "$TMP"/gipfel_bg.wasm.d.ts
cp crates/gipfel/www/index.html crates/gipfel/www/style.css crates/gipfel/www/loader.js "$TMP/"
gzip -9 -n -c gipfel.dat > "$TMP/gipfel.dat.gz"

rm -rf "$DIST"
chmod 755 "$TMP"
mv "$TMP" "$DIST"
trap - EXIT INT TERM

echo "site assembled in $DIST ($(du -sh "$DIST" | cut -f1)):"
ls "$DIST"
