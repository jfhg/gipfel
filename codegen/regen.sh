#!/bin/sh
# Regenerate the symbolic projection functions with maxima.
# Run from anywhere; requires maxima (and rustfmt if available).
set -e
cd "$(dirname "$0")"
OUT=../crates/gipfel-core/src/projection/generated

for p in rectilinear cylindrical; do
	maxima -b lsq_${p}.mac | python3 fixup.py > ${OUT}/${p}_funcs.rs
done

if command -v rustfmt >/dev/null; then
	rustfmt ${OUT}/rectilinear_funcs.rs ${OUT}/cylindrical_funcs.rs 2>/dev/null || true
fi
echo "generated:"
wc -l ${OUT}/*_funcs.rs
