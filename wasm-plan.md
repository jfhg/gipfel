# wipfel — plan for a real browser (WASM) version

Status: **planning**. Feasibility spike done and verified on this branch
(`crates/gipfel-wasm`): the full computational core — hill DB, JPEG
comment + EXIF I/O, geodesy, projections, Levenberg–Marquardt fit — runs
in the browser at native speed (sub-pixel refit on a real 4080×3072
photo in 50 ms). See `crates/gipfel-wasm/README.md`.

This document defines scope, architecture, and milestones for turning
that into a usable web app. It assumes the desktop app (master) stays
the primary product; the web version is a second front end on the same
`gipfel-core`.

---

## 1. Goals and non-goals

### Goals
- View a photo, place/move/delete markers, identify peaks, fit the
  projection, save the annotated JPEG — the complete desktop workflow
  minus OS-integration extras.
- All processing local (client-side WASM); images never leave the
  machine. This is a feature, say it loudly.
- Works offline after first visit (PWA), including the hill database.
- Reasonable on a mid-range phone, good on a desktop browser.

### Non-goals (v1)
- No panorama stitching / TIFF export (heavy, desktop-only; revisit as
  W6 stretch — `stitch::resample` is already wasm-clean).
- No GPX track loading initially (cheap to add later: `track.rs` is
  pure Rust; deferred only to keep UI scope small).
- No distortion-profile management UI beyond load/save (localStorage).
- No i18n in v1 (same open question as desktop; gettext in wasm is its
  own rabbit hole).
- No multi-window, no screen dump (browser screenshots exist).

### Feature parity map
| Desktop feature | Web v1 | How |
|---|---|---|
| Load image (EXIF + gipfel comment) | ✅ | File input / drag-drop → `load_image_from_bytes` (done) |
| Markers: add / move / delete / hide | ✅ | canvas hit-testing (port of viewport.rs logic) |
| Fit (4/7-param, distortion lock) | ✅ | `comp_params[_with]` (proven in spike) |
| Compass strip | ✅ | port `compass.rs` math (already wasm-clean) |
| Sliders (lat/lon/height/az/pitch/roll/focal/visibility) | ✅ | HTML range inputs |
| Save annotated JPEG | ✅ | `save_image_to_bytes` (done) + Blob download |
| Screen dump with labels | ✅ | `dump.rs` is pixel-based — ports nearly unchanged |
| Profiles | ✅ (basic) | `profile.rs` format in localStorage |
| Hill DB | ✅ | bundled gzipped, user-overridable via file input |
| GPX tracks | ⏳ v1.1 | `track.rs` ports directly |
| Stitch export | ⏳ stretch | `stitch.rs` ports directly |
| Menu bar / nav bar | ➡️ | HTML sidebar + keyboard shortcuts |

---

## 2. Architecture decisions

### A1. UI framework: **Yew 0.21**
- Most mature Rust web framework, stable API, plain Rust, plays well
  with wasm-bindgen; no proc-macro magic beyond `#[function_component]`.
- Alternatives considered: Leptos (finer-grained reactivity, faster
  DOM updates — but newer API churn), Dioxus (nice, but its web backend
  is Leptos-derived; adds a second dependency family). The app is
  canvas-centric: 95 % of per-frame work is canvas drawing, not DOM, so
  framework reactivity differences are second-order. Yew's maturity
  wins.
- The heavy widgets (viewport, compass) are **canvas**, not DOM — the
  framework only manages the sidebar, dialogs, and file plumbing.

### A2. Rendering: **canvas 2D, hybrid image layer**
- Base image: decode once with `createImageBitmap` (browser-native,
  fast, GPU-backed); draw with `drawImage` + transform for pan/zoom —
  the browser does scaled blitting on the GPU. This is strictly better
  than the desktop path (per-frame `putImageImageData`).
- Overlays (markers, labels, leaders, compass): drawn per frame on the
  same canvas with `ctx` primitives + `fillText`.
- Export/dump: keep the pixel-based renderer (`dump.rs` port) — it
  renders at full image resolution independent of the viewport and is
  already proven.
- **Not** wgpu/WebGPU: uneven support (Safari), and the desktop wgpu
  path exists only because putImageData was too slow there; canvas 2D
  removes the need.

### A3. State: one reducer, ported 1:1 from `GipfelApp`
- The desktop `Message` enum maps almost directly to a Yew `Msg` enum;
  keep field names identical so bugs can be cross-checked between
  front ends.
- `Panorama` lives in a ` Rc<RefCell<>>` (or Yew state handle) inside
  the root component; markers/params in reducer state.

### A4. Storage
- Settings + distortion profiles: `localStorage` (JSON), reusing the
  `profile.rs` parameter semantics (k0/k1/x0 triples).
- Hill DB: bundled `gipfel.dat.gz` (~300 KB gzipped) cached by the
  service worker; decompressed with `flate2` (pure Rust, wasm-safe) or
  `DecompressionStream` in JS. User may load a different DB file.
- No IndexedDB needed for v1.

### A5. Files in / out
- In: `<input type="file">` + drag-drop → `Vec<u8>` → byte-oriented
  core APIs (already exist).
- Out: `Blob` + object URL + synthetic `<a download>` click.
  `save_image_to_bytes` already produces the annotated JPEG bytes.

### A6. Concurrency
- Fit is 50 ms including DB parse — main thread is fine for v1.
- Escape hatch if needed: move `Panorama` into a Web Worker
  (`wasm-bindgen-worker`), messages map 1:1 to core calls. Do not
  build this until a measurement demands it.

### A7. Text / fonts
- Canvas `fillText` with the CSS font stack; label layout code
  (`layout_label_y`, collision push-up) ports unchanged — it is pure
  math over widths, which come from `ctx.measureText`.

---

## 3. Crate / module layout

New crate `crates/wipfel` (on this branch; `gipfel-wasm` folds into
it or becomes its `demo/`):

```
crates/wipfel/
  src/lib.rs            Yew root component, routing between views
  src/state.rs          app state + reducer (port of GipfelApp fields/Message)
  src/canvas_view.rs    viewport canvas: bitmap layer + overlay layer,
                        zoom/pan, hit-testing, marker drag
  src/compass.rs        compass strip on canvas (port of app compass.rs)
  src/sidebar.rs        settings controls (sliders, checkboxes, file pickers)
  src/dump.rs           full-res annotated export (port of app dump.rs)
  src/bridge.rs         JS glue: downloads, localStorage, image bitmaps
  www/                  index.html, manifest.webmanifest, sw.js, gipfel.dat.gz
  tests/                wasm-bindgen-test (headless browser)
```

Port map from the desktop app (`crates/gipfel-app/src/`):

| Desktop | Web | Change required |
|---|---|---|
| `main.rs` state + `Message` | `state.rs` | cosmic → Yew plumbing; fields identical |
| `viewport.rs` (iced Widget) | `canvas_view.rs` | draw calls → `ctx`; zoom/pan/hit-test/label layout port directly |
| `compass.rs` | `compass.rs` | draw → `ctx`; `newton_inverse` unchanged |
| `dump.rs` | `dump.rs` | nearly unchanged (already pixel-based) |
| settings sidebar | `sidebar.rs` | cosmic sliders → `<input type=range>` |
| config/profiles | `bridge.rs` | XDG file → localStorage |
| file dialogs | `bridge.rs` | cosmic dialog → file input + Blob |

`gipfel-core` is consumed **unchanged** — it is already wasm-clean and
the byte-oriented APIs exist. If anything in core needs a change, it
goes through the same golden-test gate as desktop work.

---

## 4. Milestones

### W1 — Scaffold + image display (2–3 days)
- Yew app, Trunk build, `www/` shell, wasm target wired in CI.
- File input + drag-drop → `createImageBitmap` → pan (drag) + zoom
  (wheel, cursor-anchored) on canvas.
- **Accept:** photo loads, pans, zooms at 60 fps on a 4000×3000 image.

### W2 — Core wiring (2–3 days)
- Load bundled hill DB (gzip decompress + `load_data_from_bytes`).
- `load_image_from_bytes` → EXIF/comment → initial viewpoint.
- Marker add/move/delete with hit-testing; `comp_params` on ≥4
  markers; distortion-lock checkbox.
- **Accept:** on a gipfel-saved photo, markers reproduce the desktop
  fit to the same parameters (compare against desktop on same image).

### W3 — Overlays + controls (3–4 days)
- Labels with collision layout, leader lines, known/hidden flags.
- Compass strip (drag to rotate azimuth).
- Sidebar sliders (lat/lon/height, azimuth, pitch, roll, focal,
  visibility) with live update; visibility inversion parity.
- **Accept:** visual parity review against desktop screenshots.

### W4 — Save + profiles + settings (2 days)
- Save annotated JPEG (dump renderer port + Blob download).
- Profiles: list/save/load in localStorage; profile-name suggestion
  from EXIF (same rule as desktop).
- Persist app settings (last viewpoint params) across reloads.
- **Accept:** round trip: save in browser → open in desktop app →
  parameters identical.

### W5 — PWA + polish (2–3 days)
- manifest + service worker (offline incl. DB), loading states,
  error surfaces (bad file, <2 visible hills, …), keyboard shortcuts
  (fit, save, undo if trivial), mobile layout (sidebar → bottom sheet).
- **Accept:** Lighthouse PWA pass; works in airplane mode after one
  online visit.

### W6 — Hardening + deploy (2 days, + stretch)
- `wasm-pack test --headless` smoke tests (DB parse, fit on fixture
  bytes); Playwright e2e against preview deploy.
- Static hosting (any static host; `git.flpsed.org` pages if available),
  CSP headers, size budget: initial download ≤ 1.5 MB gzipped
  (wasm ~400 KB + DB ~300 KB + app shell).
- Stretch: GPX tracks; TIFF stitch export via download.

**Total: ~2.5–3 weeks focused**, matching the earlier estimate. The
spike already retired the two scariest risks (core-in-wasm, fit speed).

---

## 5. Risks & mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| Canvas label metrics differ from iced → label layout drift | cosmetic | layout code is width-driven; tune line-height constant on real browser |
| Memory on mobile (4080×3072 RGBA ≈ 50 MB + wasm heap) | OOM on low-end phones | downscale preview bitmap for display (keep full-res only in dump path); measure iOS Safari early (W1) |
| `image`/`exif`/`flate2` wasm compile surprises | days | all pure Rust; verify in W1 scaffold (`cargo check --target wasm32-unknown-unknown -p wipfel`) |
| Service worker caching stale DB/app | confusing updates | versioned cache keys, skipWaiting flow with "update available" toast |
| Scope creep toward desktop parity | never ships | non-goals table is binding for v1; stitch/GPX explicitly deferred |
| Two front ends drift apart (desktop fixes not applied to web) | long-term maintenance | shared core + shared golden tests; web-specific tests only for glue |

---

## 6. Open questions (decide before W1)
1. **Where does it live?** subdomain (web.gipfel.flpsed.org?) + which static host.
2. **Bundle the full 21k hill DB** (~1 MB gz ≈ 300 KB) or offer regional subsets? Default: full, it fits the budget.
3. Name: same "gipfel" brand for web, or distinct (e.g. "wipfel")?
4. Does the web version need its own About/privacy page text (all-local processing statement)?
5. Framework confirmation: Yew per A1, or do you want to evaluate Leptos first? (One-day spike would settle it; Yew default stands.)
