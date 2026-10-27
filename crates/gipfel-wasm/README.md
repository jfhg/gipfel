# gipfel-wasm (EXPERIMENT)

Browser spike proving the gipfel core runs as WebAssembly. Enabled by the
GSL removal — the core is pure Rust end to end.

## Status: spike verified (2026-10-26)

`fit_demo()` ran the full pipeline in a real browser on a 4080×3072
gipfel-annotated photo (21324 hills, gipfel JPEG comment found):

```json
{"ok":true,"hills_in_db":21324,"image":"4080x3072","had_gipfel_info":true,
 "observations":5,"viewpoint":{"lat":47.65526,"lon":11.61856,"height_m":1319},
 "perturbed":{"center_deg":245.8905,"scale":2558.0},
 "refitted":{"center_deg":245.390505,"pitch_deg":-7.763227,"roll_deg":1.245000,"scale":2692.61},
 "max_reproj_err_px":0.0000}
```

Wall time 50 ms. The refit exactly undid the synthetic perturbation
(center to 5e-6 deg, scale to 7e-6 rel) — identical convergence to the
native golden tests.

## Build & run (requires rustup)

```sh
cd crates/gipfel-wasm
./build.sh                                # wasm-pack build --target web
python3 -m http.server 8080               # then open /demo/
```

## What is proven / what is not
- Proven: hill DB parsing, JPEG comment + EXIF I/O, geodesy, projections,
  Levenberg-Marquardt fit — all in wasm at native speed.
- Not built: the UI shell (libcosmic has no browser story). Next step if
  pursued: Yew (or Leptos) + canvas 2D reusing the viewport/compass logic
  and the pixel-based dump renderer.
