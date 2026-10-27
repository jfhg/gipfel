# AGENTS.md — gipfel-rs

Guidance for AI coding agents working in this repository.

## What this is

`gipfel-rs` is a Rust port of **gipfel** (a mountain-photo annotator: you load a
photo, mark known peaks, and it fits the camera viewpoint + lens distortion from
the hill database). The original is C++/FLTK (`https://git.flpsed.org/gipfel`,
cloned at `~/gipfel`). This port targets **Cosmic DE** (libcosmic) on Linux.

The port is organized as milestones (M1 core → M5 polish); see `~/gipfel/plan.md`
for the full plan and `~/gipfel-spikes/MEMO.md` for early UI-spike decisions.

## Workspace layout

```
crates/gipfel-core/   headless computational core (no GUI, no native C deps)
  hill.rs             hill DB: CSV load (Latin-1), duplicate marking
  panorama.rs         viewpoint, geodesy, refraction, occlusion, visibility
  projection/         rectilinear + cylindrical projections
    generated/        maxima-generated symbolic functions (DO NOT hand-edit)
  lsq.rs              pure-Rust Levenberg-Marquardt fit (was GSL; GSL removed)
  metadata.rs         `gipfel:` JPEG comment + EXIF read/write
  profile.rs          distortion profiles (FLTK prefs format, XDG data dir)
  stitch.rs           cylindrical panorama resampling (ScanImage/Stitch port) + TIFF/JPEG out
  track.rs            GPX track parser (hardened: no XXE, size cap)
crates/gipfel-cli/    CLI: export-position, export-hills, info, stitch, profiles
crates/gipfel-app/    libcosmic GUI app (binary name: `gipfel`)
  main.rs             app state + Elm-style update/view + sidebar
  viewport.rs         custom canvas Widget (image + markers + labels)
  compass.rs          compass-strip orientation widget
  dump.rs             off-screen image renderer (screen dump)
codegen/              maxima scripts + regen.sh (regenerates projection/generated/)
tests/                C++ reference harnesses (.cxx) + golden files
```

## Build & test

```sh
cargo build --release                 # app binary: target/release/gipfel
cargo test --lib --test golden        # unit + golden tests (the main gate)
cargo test --workspace                # everything incl. app-crate tests
cargo clippy --workspace --all-targets
```

- **Rust edition 2021.** No pinned toolchain file.
- **No native C dependencies** since GSL was replaced by a pure-Rust LM. The only
  system libs are what libcosmic/wgpu need (Vulkan/GL, fontconfig).
- The dev environment has **no reachable display server** — the GUI app compiles
  but cannot be run here. Runtime UI verification is done by the user on a real
  Wayland desktop. Do not assume you can launch `gipfel` to test it.

## Golden tests — the critical invariant

`crates/gipfel-core/tests/golden.rs` compares output against files in
`tests/golden/` produced by the **original C++ build**. This is the contract that
keeps the math faithful.

- Fit tests (`fit_rectilinear_matches_c`, `fit_cylindrical_matches_c`): fitted
  params within **1e-6 relative**, final projected coords within **1e-9 relative**.
- Function-grid tests: ~1e-15 agreement on the maxima-generated derivatives.
- Several golden tests **skip silently** if the hill DB is missing. They look for
  the data file via `$GIPFEL_DATA`, then `$HOME/gipfel/gipfel.dat`. Set
  `GIPFEL_DATA=/path/to/gipfel.dat` so they actually run (a skipped golden test
  is a green test that verified nothing).
- **Never edit `tests/golden/*` to make a failing test pass.** They are ground
  truth from C++. If a golden test fails, your change altered behavior — fix the
  code or understand the deviation.

Regenerating goldens (only when the C++ reference changes):
`tests/gen_golden.sh` — needs the built C++ tree (`$GIPFEL_SRC`, default
`~/gipfel/src`), `$GIPFEL_DATA`, g++, GSL, maxima. The `.cxx` harnesses still use
C++ GSL; that is intentional (they are the reference, not part of the Rust build).

## Code generation (maxima)

`crates/gipfel-core/src/projection/generated/*_funcs.rs` are **generated** from
`codegen/lsq_{rectilinear,cylindrical}.mac` via `codegen/regen.sh` (needs maxima;
optional rustfmt). The mathematical models (refraction, cone-visibility,
distortion) and their symbolic derivatives are **frozen** — do not change the
models. If you regenerate, re-run the golden tests immediately.

## Hard compatibility constraints

- **`gipfel.dat` CSV format** and the **`gipfel:` JPEG comment parameter string**
  must stay byte-compatible with the C++ version (keys `nick`/`tilt` are kept in
  the file even though the code renamed them to pitch/roll).
- **Hill indices are stable handles.** `Panorama::hills` keeps insertion order;
  `mark_duplicates` sorts an index permutation, never the vec. Track lists and
  known-hill sets index into `hills`. Do not reorder `hills` in place.
- **Terminology:** UI says "Pitch"/"Roll"; internal fields are `a_pitch`/`a_roll`;
  hill elevation is `a_elev` (distinct from camera pitch). JPEG keys stay
  `nick`/`tilt`.
- Distortion profiles live in `$XDG_DATA_HOME/gipfel/distortion-profiles.prefs`
  (FLTK prefs format, legacy auto-import). Keep the `%g`-style float formatting
  (`profile::fmt_g`) matching C.

## Code conventions

- **Elm architecture** in the app: `update(&mut self, Message) -> Task` is one
  flat, **exhaustive** `match` (no catch-all) — keep it exhaustive so new
  `Message` variants are forced to be handled. Prefer extracting small helper
  methods over a catch-all arm.
- Shared helpers to reuse (don't duplicate): `slider_item()`, `pick_open`/
  `pick_save` (file dialogs), `compass::newton_inverse` (scalar inverse
  projection), `viewport::layout_label_y` (label layout shared by screen + dump).
- Sidebar rows use `settings::item_row` with fixed-width labels when fields must
  align to equal widths (see the Distortion section).
- libcosmic is **pinned to git rev `430f3d4`** (see `crates/gipfel-app/Cargo.toml`)
  and vendors iced 0.14.2. The correct nav-bar return type is
  `cosmic::Action<Message>` (crate root), not `cosmic::app::Action`.
- Rendering gotchas: the wgpu renderer draws in fixed layer order (use
  `start_layer`/`end_layer` to force overlays above the image); keep strong
  `Plain<P>` paragraph refs alive across the frame in widget `Tree` state.

## Platform notes

- **Linux / Cosmic first.** No Windows/macOS support yet.
- A standalone Windows build is an open experiment. The main blocker (GSL) is
  already removed. Remaining work: swap libcosmic's `xdg-portal` feature for
  `rfd` (file dialogs), port the hill-DB search path (`/usr/share`, `$HOME` →
  `dirs` + exe-relative) and font fallback, then get libcosmic itself to compile
  for `x86_64-pc-windows-gnu` (upstream does not test Windows).

## Releasing a new version

The version has a **single source of truth**: `[workspace.package] version` in
the root `Cargo.toml`. All three crates inherit it (`version.workspace = true`),
and the app's **About dialog** reads it at compile time via
`env!("CARGO_PKG_VERSION")`. Do **not** hardcode a version anywhere else.

To cut a release `X.Y.Z`:

1. Bump `version` in `[workspace.package]` (root `Cargo.toml`). That updates all
   three crates and the About dialog at once.
2. The **ChangeLog entry is human-authored** (see the Do / Don't rule above) —
   only touch it when explicitly asked. Its top `gipfel-X.Y.Z` header must equal
   the crate version; the test `gipfel-app: changelog_top_matches_version` fails
   otherwise, so the manifest and the ChangeLog cannot silently drift apart.
3. `cargo test --lib --test golden` (with `GIPFEL_DATA` set) — must be green.
4. Commit, then tag: `git tag vX.Y.Z`.
5. Build the **source** tarball (tracked files only, no build artifacts):
   ```sh
   git archive --format=tar.gz --prefix=gipfel-X.Y.Z/ \
       -o ~/gipfel-X.Y.Z.tar.gz vX.Y.Z
   ```
   Recipients build with `cargo build --release`. For a fully offline tarball,
   run `cargo vendor --versioned-dirs vendor/` and commit `vendor/` first.

## Do / Don't

- ❌ **Never write to `ChangeLog` or `README.md` unless explicitly asked.** These
  files are human-authored. Do not draft release notes, add ChangeLog entries,
  or edit README prose on your own initiative. The only exception is a purely
  mechanical version bump (e.g. renaming the top `gipfel-X.Y.Z` header during a
  release) when the user has asked you to cut that release.
- ✅ Run `cargo test --lib --test golden` (with `GIPFEL_DATA` set) after any
  change to the core math, projection, fit, metadata, or stitch code.
- ✅ Keep the build warning-free (rustc `dead_code`/`unused_*` lints are the
  signal; clippy may not be installed in every environment).
- ❌ Don't hand-edit `projection/generated/` or `tests/golden/`.
- ❌ Don't change the mathematical models or file formats.
- ❌ Don't assume the GUI can be launched in this environment.
