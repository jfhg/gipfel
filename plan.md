# Plan: Porting gipfel to a Cosmic DE Application in Rust

Status: **planning only — no implementation started**
Reference: C++/FLTK sources in `src/` (gipfel 0.4.1, commit `45425dd`)

---

## 1. Analysis of the Existing Application

### 1.1 What gipfel does

gipfel is "georeferencing for arbitrary images": given a photo, the GPS position of
the camera, and two or more manually identified peaks in the image, it fits the
camera view parameters and then labels all other peaks/POIs from a GPS database
directly on the image. It also stitches referenced images into panoramas.

### 1.2 Module map (current C++/FLTK)

| Component | Files | Responsibility |
|---|---|---|
| Main / control window | `gipfel.cxx` | CLI parsing, two FLTK windows (control + scrolled image), menu bar, sliders/dials, stitching & export CLI modes |
| Image canvas | `GipfelWidget.cxx/.H` | Draws image + labels + markers + GPS track; mouse interaction (see 1.4) |
| Domain model | `Panorama.cxx/.H` | Viewpoint (lat/lon/height), hill DB, azimuth/nick/distance with earth curvature (`EARTH_RADIUS 6371000.785`), refraction model (Tom Chester approximation), hidden-object detection (cone model + `hide_value` / "Visibility" slider), visible-hill computation |
| Projection + fit | `ProjectionLSQ.*`, `ProjectionRectilinear.*`, `ProjectionCylindrical.*` | Non-linear least squares (GSL `gsl_multifit_fdfsolver_lmsder`, max 100 iterations) over `ViewParams {a_center, a_nick, a_tilt, scale, k0, k1, x0}`. Fit level depends on # marked hills: 1 hill → 2 params, 2–3 → 4 params, ≥4 → 7 params (lens distortion calibration) |
| **Symbolic codegen** | `lsq_rectilinear.mac`, `lsq_cylindrical.mac`, `expr2c.mac`, `Makefile.lsq_funcs` | **maxima** computes the projection equations and all analytic partial derivatives (Jacobian) symbolically and emits C functions `mac_x`, `mac_y`, `mac_x_d*` into `Projection*_funcs.cxx` |
| Data model | `Hill.cxx/.H` | `Hill {phi, lam, height, alph, a_nick, dist, x, y, flags}`, `Hills` collection, CSV loader, duplicate marking, sorting |
| Metadata | `ImageMetaData.cxx/.H` | EXIF read via exiv2 (GPS lat/lon/alt, focal length, 35mm-equiv, Make/Model) + own serialization of fit results into the **JPEG comment** (`gipfel: longitude …, latitude …, height …, direction …, nick …, tilt …, focal_length_35mm …, projection type N, k0 …, k1 …, x0 …`) |
| Stitching | `Stitch.*`, `ScanImage.*`, `OutputImage.*`, `JPEG/TIFF/PreviewOutputImage.*` | Resample a view-angle range across referenced images (nearest/bicubic), output JPEG/TIFF (8/16-bit) or on-screen preview |
| Custom widgets | `Fl_Value_Dial.*`, `Fl_Search_Chooser.*`, `choose_hill.*` | Compass dial (view direction), searchable hill picker dialog |
| Preferences | `GipfelWidget.cxx` (Fl_Preferences) | Distortion profiles in `$HOME/.fltk/Johannes.HofmannATgmx.de/gipfel` |
| Data file | `gipfel.dat` (~21k rows) | `SHORTNAME,NAME,DESCRIPTION,LATITUDE,LONGITUDE,HEIGHT` per line |

### 1.3 Data flow

```
gipfel.dat ─┐
EXIF/JPEG ──┼─► Panorama (viewpoint, angles, distances, refraction, visibility)
user marks ─┘        │
                     ▼
          ProjectionLSQ.comp_params()          ◄── maxima-generated mac_* + Jacobian
                     │  ViewParams
                     ▼
          get_coordinates(alph, nick) → (x, y) per hill ─► GipfelWidget redraw
```

### 1.4 Interaction model (to preserve / improve)

- **Middle-click** a label cross → mark hill as known (turns red)
- **Left-drag** a marked cross → position it on the peak; on release, refit
- **Right-click** → context menu: "Find Peak", "Hide/Unhide <name>"
- **Hover** → tooltip "name (height m), distance x.xkm"
- Control window: compass dial (view direction), sliders (nick, tilt, focal length
  in 35mm, visibility, track width), numeric inputs (viewpoint lat/lon/height,
  distortion k0/k1/x0)
- CLI modes: `-s` stitch, `-e/-E` export hills, `-p` export position

### 1.5 Constraints carried into the port

- **The algorithm stays as-is**: same projection math, same LM fit strategy, same
  refraction/visibility models, **same maxima-based symbolic derivative generation**.
- Wire formats stay compatible: `gipfel.dat` CSV, the JPEG-comment parameter string
  (so images referenced with the old gipfel still open correctly), distortion
  profile semantics.

---

## 2. Target Application

**gipfel-rs**: a native [Cosmic](https://system76.com/cosmic) DE application built
with `cosmic-toolkit` (`cosmic::app`, iced under the hood), single window, with
optional image zooming.

### 2.1 UI concept — single window

The FLTK version uses two windows (control + image) plus modal dialogs. The Cosmic
port consolidates everything into one window:

```
┌──────────────────────────────────────────────────────────────────────┐
│  ⌘ Menu (File / Projection / Distortion / Options / Help)            │
├──────────────────────────────────────────────────────────────────────┤
│  Viewpoint: <Innsbruck, …> [✎]   Lat [____] Lon [____] Hgt [____]   │
├───────────────────────────────┬──────────────────────────────────────┤
│                               │  Controls (collapsible sidebar)     │
│                               │  ┌───────────┐  ┌────────────────┐  │
│      Image canvas             │  │ compass   │  │ Focal length ⌬ │  │
│   (custom iced Canvas)        │  │ dial      │  │ Nick        ⌬  │  │
│                               │  │ (view dir)│  │ Tilt        ⌬  │  │
│  - pan: drag / space+drag     │  └───────────┘  │ Visibility  ⌬  │  │
│  - zoom: ctrl+scroll, +/-,    │                 │ Track width ⌬  │  │
│    pinch; fit / 1:1 buttons   │  Distortion k0/k1/x0  [advanced]   │
│  - labels, markers, track     │  Projection: ○ normal ● panoramic  │
│                               │  [Compute] [Reset] [Show hidden ☐] │
├───────────────────────────────┴──────────────────────────────────────┤
│  Status: hover → "Simetsberg (1836 m), distance 12.4 km" · zoom 150% │
└──────────────────────────────────────────────────────────────────────┘
```

Design principles:
- **One window, no modal dialogs for the core loop.** Viewpoint selection becomes an
  inline searchable overlay (iced `text_input` + filtered list in a popover/panel),
  replacing the `choose_hill` dialog.
- **Zoom is optional and non-destructive**: a viewport transform (scale + offset)
  applied at render time; all hit-testing and label placement in image coordinates.
  Labels stay screen-space (constant font size) so they remain readable when zoomed.
- Context menus (Find Peak, Hide/Unhide) via `cosmic::widget::popover` / context-menu.
- Overlays (viewpoint picker, find-peak, about) as Cosmic **dialogs** (they render
  inside/over the app window) — no secondary top-level windows.

### 2.2 UI options to explore first (spikes, before committing)

| Option | Pros | Cons |
|---|---|---|
| **A. cosmic-toolkit (`cosmic::app`)** — recommended | Native look & feel, app menu, dialogs, popover, theming, Cosmic conventions, active development | API still moving; pin a release/tag |
| B. Plain iced 0.13 | More stable API, more examples | Not "Cosmic-native"; lose cosmic widgets/menus/theming |
| C. Slint / GTK4 / Qt | Mature canvas options | Not idiomatic for Cosmic DE |

Exploration tasks (throwaway prototypes, no product code):
1. **Canvas perf spike**: draw a 6000×4000 JPEG in an iced `Canvas`, pan/zoom at
   60 fps (texture upload cost, `wgpu` backend, downscale-on-zoom-out strategy).
2. **Text spike**: render ~300 hill labels + leader lines via glyphon/iced canvas
   text; measure frame times; decide label decluttering strategy.
3. **Input spike**: verify middle-mouse (or map "toggle known" to click + `K` key /
   double-click if middle-click is unreliable under Wayland), drag-and-drop of image
   files into a Cosmic window, ctrl+scroll zoom gesture.
4. **Widget inventory**: sliders, radio groups, menu bar, dialog, popover,
   searchable list — confirm cosmic-widget coverage; identify what needs a custom
   canvas widget (compass dial, canvas).
5. **Packaging spike**: minimal `cosmic-app-library` hello-world, `.desktop` file,
   `cargo vendor` build without network.

---

## 3. Rust Architecture

Cargo workspace:

```
gipfel-rs/
├── Cargo.toml            # workspace
├── crates/
│   ├── gipfel-core/      # pure domain logic, no UI, fully testable
│   │   ├── geo.rs        # distance/azimuth/nick, earth radius, refraction
│   │   ├── hill.rs       # Hill, Hills, flags, CSV loader, duplicate marking
│   │   ├── panorama.rs   # Panorama: viewpoint, visible/hidden computation
│   │   ├── projection/
│   │   │   ├── mod.rs    # Projection trait, ViewParams, get_coordinates
│   │   │   ├── rectilinear.rs
│   │   │   ├── cylindrical.rs
│   │   │   └── generated/{rectilinear,cylindrical}_funcs.rs   # ← maxima output
│   │   ├── lsq.rs        # Levenberg–Marquardt driver (lmsder-equivalent)
│   │   ├── metadata.rs   # EXIF read + JPEG-comment read/write (gipfel: string)
│   │   ├── stitch.rs     # scan/resample pipeline (nearest + bicubic)
│   │   └── prefs.rs      # distortion profiles (~/.config/gipfel/)
│   ├── gipfel-codegen/   # maxima scripts + generator (see 3.2)
│   │   ├── lsq_rectilinear.mac
│   │   ├── lsq_cylindrical.mac
│   │   └── expr2rust.mac
│   └── gipfel-app/       # Cosmic application
│       ├── src/app.rs    # cosmic::Application: Model/Message/view/update
│       ├── src/canvas.rs # image viewport widget (pan/zoom/labels/hit-test)
│       ├── src/panels.rs # controls sidebar, viewpoint picker, dialogs
│       └── i18n/
├── data/gipfel.dat       # unchanged format
└── examples/             # CLI: stitch, export (clap subcommands)
```

### 3.1 Keeping the algorithm identical

- Port `Panorama`/`ProjectionLSQ` logic **line-for-line in spirit**: same formulas,
  same initial-value heuristics (`comp_scale`, alpha-monotonicity fix in the
  cylindrical case), same fit levels (2/4/7 params), same 100-iteration LM loop.
- **Derivatives stay symbolic via maxima** (explicit requirement):
  - Port `expr2c.mac` → `expr2rust.mac` (emit `pub fn mac_x(c_view: f64, …) -> f64`
    with the same `_1.._n` temporary-variable scheme maxima already produces).
  - Reuse `lsq_rectilinear.mac` / `lsq_cylindrical.mac` unchanged except for the
    `expr2c(...)` calls.
  - Generated `*_funcs.rs` files are **committed to the repo** (like today's
    generated `.cxx`), regenerated manually by the maintainer via a `just`/`make`
    target; optionally a `build.rs` that regenerates if `maxima` is on `$PATH`.
  - Validation: differential test comparing generated Rust vs. original C output
    bit-for-bit on a parameter grid, and vs. numerical finite differences.
- **Solver**: two candidates, decide during M2:
  1. `gsl` crate (bind to GSL `multifit_fdfsolver_lmsder`) — bit-identical behavior,
     keeps the C dependency (acceptable: Cosmic is Linux-first).
  2. Pure-Rust LM (small problem: n≤2·#hills, p≤7) with QR-based inner steps
     (e.g. `levmar`-style, `ndarray` + householder QR) — no C deps, needs
     equivalence testing against the GSL reference.
  Default preference: **(1) gsl crate** to honor "algorithm stays as it is".

### 3.2 Replacements for C libraries

| C dependency | Rust replacement |
|---|---|
| FLTK | `cosmic-toolkit` / iced |
| libjpeg/libtiff (via FLTK/exiv2) | `image` crate (jpeg, tiff features) |
| exiv2 (read) | `kamadak-exif` (EXIF + GPS tags) |
| exiv2 (write JPEG comment) | custom JPEG segment writer: parse markers, replace/insert `COM` (0xFFFE) segment — small, well-defined; keeps the exact `gipfel: …` string format |
| GSL | `gsl` crate (see 3.1) |
| Fl_Preferences | `directories` + simple key-file (or `confy`) for distortion profiles |
| getopt | `clap` (subcommands: `gui`, `stitch`, `export-hills`, `export-position`) |

### 3.3 State & concurrency model

- `cosmic::Application` Elm architecture: `App` holds `AppState` (loaded image,
  `Panorama` model, viewport transform, UI state).
- Heavy operations (image decode of large JPEGs, LM fit, stitching) run via
  `cosmic::task::future` / `tokio` background tasks; messages (`Msg::FitFinished(ViewParams)`,
  `Msg::ImageLoaded(..)`) return results to the UI — keeps the window responsive
  (the C version blocks with a wait cursor today; this is an improvement we keep).
- The 21k-row hill DB is loaded once and indexed (grid by lat/lon) for the
  "close mountains" pre-filter (the C code rescans linearly).

---

## 4. Milestones

### M0 — UI exploration (spikes from §2.2) — ✅ DONE
Deliverables (see `../gipfel-spikes/`):
- `spike-render/` — headless wgpu+glyphon benchmark, **executed** on lavapipe:
  24 MP JPEG decode 206 ms, texture upload 60 ms (once), image frame 5.3 ms,
  300 glyphon labels 0.8 ms → rendering approach viable
- `spike-canvas/` — iced 0.14 custom viewport widget (zoom/pan/middle-click
  markers/300 screen-space labels/sidebar), compile-verified
- `spike-cosmic/` — libcosmic app with menu bar + key binds, dialog, popover,
  settings section, `.desktop` file, compile-verified
- `MEMO.md` — decision memo (toolkit = libcosmic pinned to tag; custom `Widget`
  viewport; `GSL` crate for `lmsder`; Ctrl tracked via event subscription;
  middle-click exists in iced 0.14; cosmic's iced fork = 0.14.2 API parity)

Deferred (no display server in spike env): interactive runtime feel, middle-click
verification under a real Cosmic session, file-chooser choice.

### M1 — Core port (`gipfel-core`, headless) — ✅ DONE

Repo: `~/gipfel-rs` (workspace: `crates/gipfel-core`, `crates/gipfel-cli`,
`codegen/`, `tests/`). All behaviour is verified against the original C++
through committed golden files (`tests/gen_golden.sh` regenerates them).

- [x] `hill.rs`: `Hill`/`Hills` + CSV loader (Latin-1, `atof` semantics, both
      6-field and 3-field record layouts), `mark_duplicates`, all five sort orders
- [x] `panorama.rs`: geodesy — `distance`/`alpha`/`nick`/`refraction`/
      `get_earth_radius`/`get_real_distance`, `mark_hidden` (incl. the original's
      `fabs(alph_diff > pi/2)` quirk, ported as-is), close/visible bookkeeping
- [x] `projection/`: `Projection` trait (16 macro entry points, defaults `NAN`
      like the C++ base class) + rectilinear/cylindrical impls
- [x] `codegen/expr2rust.mac` + `lsq_{rectilinear,cylindrical}.mac` + `fixup.py`
      + `regen.sh`; generated `*_funcs.rs` committed (16 + 10 functions)
- [x] `lsq.rs`: `gsl_multifit_fdfsolver_lmsder` via a minimal hand-written FFI,
      identical 2/4/7-parameter levels and identical two-pass schedule
- [x] `metadata.rs`: `gipfel:` JPEG-comment read/write (byte-identical format),
      EXIF fallback (GPS/focal/make/model), atomic save
- [x] Golden tests: 200-point function grid, full fit for both projections
      (agreement to ~1e-15), export output, comment round-trip, real `gipfel.dat`
- [x] `gipfel-cli`: `export-position`, `export-hills`, `info` — `export-position`
      is byte-identical to the C binary, `export-hills` matches the C pipeline

Notes / deviations from the C original (all deliberate):
- `gsl_multifit_fdfsolver_lmsder` is a **pointer** in GSL ≥ 2.7; the FFI models
  it as `*const` (the C++ code dereferences implicitly). Using the struct form
  yields `failed to allocate space for multifit solver state`.
- Upstream C bug fixed in the reference tree: `ImageMetaData::load_image_jpgcom`
  took `c_str()` of a temporary `std::string`, so `-p` read garbage. The Rust
  port is correct; the C tree was patched only to produce valid goldens.
- Upstream C bug not reproduced: `Hill`'s copy constructor does not copy
  `a_nick`, so fitting a *copy* of a hill fed the solver uninitialised memory.
  The harness sets `a_nick` explicitly to model real-app behaviour.
- Rust `Hill` is plain data (no manual `strdup`/`free`); `Panorama` keeps one
  `Vec<Hill>` plus index lists instead of three aliasing pointer arrays.
- `rint` → `round_ties_even` (matches C's round-half-to-even); `atof` prefix
  parsing reproduced exactly.

### M2 — Cosmic app skeleton ✅ DONE (2026-10-25)
- [x] `cosmic::app` project (`crates/gipfel-app`, libcosmic pinned to rev
      `430f3d4`), app menu (File/View/Projection/Help), Ctrl+O/S/D/T/V/F/Q
      (menu `KeyBind` map is display-only → real dispatch via
      `event::listen_with` subscription)
- [x] Open image (xdg-portal file dialog via `cosmic::dialog::file_chooser`,
      feature `xdg-portal`), drag&drop (`Event::Window(FileDropped)`), hill DB
      load (`--data` / `$GIPFEL_DATA` / XDG + system paths), EXIF/gipfel-comment
      driven initial state (`apply_image_metadata`)
- [x] Controls sidebar: custom compass dial widget (center angle), sliders for
      focal (1–300), nick (−40…20), tilt (−10…10), visibility (0.01–0.1),
      track width (1–500); lat/lon/height + k0/k1/x0 inputs (on submit);
      projection radios; show-hidden toggle; zoom/fit buttons
- [x] Status-bar hover info (name/height/distance, like C's FL_MOVE label)
- Notes: cosmic fork text API differs from crates.io iced 0.14 — text is
  measured via `paragraph::Plain::<R::Paragraph>` + `fill_paragraph`, and
  `cosmic::Renderer` is the *fallback* (wgpu+tiny-skia) renderer. `Point`/`Rectangle`
  are f32; hill coords stay f64 in core, cast at the draw boundary.

### M3 — Canvas & interaction ✅ DONE (2026-10-25)
- [x] `ViewportWidget` production widget (`src/viewport.rs`): ctrl+scroll zoom
      anchored at cursor (image-space anchor math), wheel/drag pan, fit/zoom
      buttons; single draw pass image→geometry→text
- [x] Crosses + leader lines (yellow), known-hill flags (red ≤3, green >3),
      decluttered labels (port of C `set_labels` collision push-up, cached in
      widget Tree state keyed by a version counter), GPS track polyline with
      distance-scaled width (C `get_rel_track_width`), hidden hills blue and
      only drawn with show-hidden on
- [x] Interactions: left-drag known marker → live move + refit on release
      (`comp_params`), middle-click **and double-click** toggle known,
      right-click context dialog (Find Peak / Hide-Unhide), hover highlight
- [x] Viewpoint & Find-Peak pickers: searchable dialog (name filter, sorted,
      300-row cap), Find Peak places the marker at the right-click position
      exactly like C's `find_peak_cb`
- [x] Save Image (file dialog → `md_from_state` → `gipfel:` comment write),
      Screen Dump (re-decode + render markers/labels/track via fontdue → PNG/JPG),
      Load Track (3-field CSV, `TRACK_POINT` flags, indices remapped by name
      across reloads)
- Core change for M3: `Panorama` no longer re-sorts `hills` (keeps insertion
  order + `alpha_order` index list) so hill indices are stable handles — the
  C port used stable pointers; golden tests unaffected.
- Not yet verified interactively (no display in dev env) — needs a real
  Wayland/X11 session run-through.
- Field fixes after live testing on real Wayland:
  - text labels + overlay z-order: overlay recorded in its own graphics
    layer (`start_layer`/`end_layer`); strong `Plain` paragraph refs kept
    alive in widget Tree state (renderer only holds `Weak`)
  - **segfault on marker-drag refit**: the hand-written GSL FFI struct
    `gsl_multifit_function_fdf` was missing the trailing `nevalf`/`nevaldf`
    counters (gsl_multifit_nlin.h) — GSL's `fdfsolver_set` wrote 16 bytes
    past the struct, corrupting `lsq()`'s stack. Fixed; regression test
    runs the fit on a 128 KB stack (guard-page canary).

### M4 — Stitching & export ✅ DONE
- [x] Port `ScanImage` (nearest + bicubic) and `Stitch` resampling
      (`gipfel-core/src/stitch.rs`; bit-for-bit golden tests vs C harness
      `stitchdump.cxx` for nearest/bicubic, rect/cyl, with and without
      distortion — sampling on the C 0..=65025 scale, Keys cubic, `rint`)
- [x] `gipfel-cli stitch -j/-t/-4/-w/-h/-r/-b` parity (C defaults kept:
      2000x500, range 0..380 deg); TIFF writer matches C `TIFFOutputImage`
      (RGBA, deflate, 8/16-bit, alpha 65025), JPEG q90
- [x] App: File → “Stitch Cylindrical TIFF…” — exports the current image
      as a cylindrical 16-bit TIFF (bicubic) spanning the lens FOV derived
      from the fitted projection (edge-angle inverse + vertical Newton),
      for external stitching tools (Hugin etc.)
- [ ] Preview of stitched result in-app (optional, not done)

### M5 — Polish & packaging
- [x] Distortion profiles: `gipfel-core/src/profile.rs` reads/writes the
      C version's FLTK prefs file (`~/.fltk/Johannes.HofmannATgmx.de/gipfel/
      DistortionProfiles.prefs`) so profiles are shared with the C binary
      (verified both directions with a Fl_Preferences probe). Auto-match by
      camera+focal length on load (comment > profile > zero, C order). App:
      Distortion section has a profile name field, Save/Load buttons and a
      stored-profiles dropdown; CLI gains `profiles`/`profile-save`.
- [ ] Config/data dir handling (`XDG_DATA_HOME`), ship `gipfel.dat`
- [ ] i18n (de/en), theming (light/dark), HiDPI scaling
- [ ] `.desktop` file, icon, Cargo vendor for offline builds, CI (fmt, clippy, tests)
- [ ] Docs: README port, migration notes from the C version

---

## 5. Risks & Mitigations

| Risk | Mitigation |
|---|---|
| iced canvas performance with huge images + many labels | M0 spike; tile/downscale pyramid; label decluttering; only draw visible viewport |
| cosmic-toolkit API churn | pin to a tagged release; isolate UI in `gipfel-app` only |
| LM solver divergence from GSL reference | use `gsl` crate first; differential golden tests vs C binary |
| JPEG comment writing corner cases (progressive, large EXIF) | dedicated marker-walker with tests on real-world photos; atomic write via temp file (as C version does) |
| maxima output differs from C expectations | generated-code diff tests; keep maxima version pinned/documented |
| Middle-mouse availability on Wayland/touchpads | alternative binding (double-click or key modifier) decided in M0 |

## 6. Explicit non-goals (for now)

- No changes to the mathematical models (refraction, cone-visibility, distortion).
- No new projection types, no southern-hemisphere bugfix sweep, no GUI stitching mode.
- No Windows/macOS support; Linux/Cosmic first.
- No database rewrite (keep flat CSV; in-memory index is internal only).
