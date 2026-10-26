# gipfel-rs

Rust port of [gipfel](https://git.flpsed.org/gipfel)'s computational core.

- `crates/gipfel-core` — hills, panorama geometry, projections (maxima-generated
  symbolic derivatives), pure-Rust Levenberg-Marquardt fit, `gipfel:` JPEG comment + EXIF I/O
- `crates/gipfel-cli` — `export-position`, `export-hills`, `info`
- `codegen/` — maxima scripts (`expr2rust.mac`) and `regen.sh`
- `tests/` — C reference harnesses + golden files from the original C++ build

```sh
cargo test --lib --test golden     # unit + golden tests
cargo run -p gipfel-cli -- info <img.jpg>
```

The Rust build has no native C dependencies. Regenerate goldens with
`tests/gen_golden.sh` (needs the built C++ tree, which uses GSL; see
`GIPFEL_SRC`/`GIPFEL_DATA` in the script).

## gipfel-app (M2/M3)

COSMIC desktop application: `cargo build --release -p gipfel-app` →
`target/release/gipfel [image.jpg] [--data gipfel.dat]`.

- Viewport: ctrl+wheel zoom (cursor-anchored), wheel/drag pan, fit/zoom buttons
- Left-drag a flag to reposition (refits on release); middle-click or
  double-click toggles a hill "known"; right-click → Find Peak / Hide-Unhide
- Viewpoint / Find Peak pickers are searchable dialogs
- Sidebar: compass dial, focal/pitch/roll/visibility/track sliders, viewpoint
  lat/lon/height, distortion k0/k1/x0, projection, show-hidden
- File menu: Open/Save (writes the `gipfel:` comment), Screen Dump (PNG/JPG
  with markers burned in), Load Track (CSV: lat,lon,height rows)
- Install `crates/gipfel-app/data/org.gipfel.App.desktop` per freedesktop
