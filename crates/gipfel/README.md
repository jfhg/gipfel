# gipfel — web front end

Browser front end for gipfel (WASM, Yew + canvas 2D). This is the main
front end: peak identification, view fitting, saving annotated JPEGs,
distortion profiles, rectilinear and cylindrical projection.

## Development

Prerequisites: rustup with the `wasm32-unknown-unknown` target and
`wasm-bindgen` (`cargo install --locked wasm-bindgen-cli`).

Dev build + hot reload (trunk) and quick local serve:

```sh
./serve-here.sh        # builds wasm, serves ./dist on :8090
```

Release build (static site in the repo-root `dist/gipfel/`, serveable
by any static web server):

```sh
./build-release.sh
```

## URL parameters

The loader page (`www/loader.js`) accepts query parameters, handy for
testing and for embedding:

| param | meaning |
|---|---|
| `?file=<url>` | load an image at startup (same-origin path or CORS-enabled URL) |
| `?az= --pitch= --roll= --focal=` | override the view parameters after EXIF |
| `?vis=<ratio>` | visibility height/distance ratio |
| `?debug=1` | log projection diagnostics to the console |

## Layout

| File | Purpose |
|---|---|
| `src/main.rs` | entry point |
| `src/app.rs` | root component: sidebar, file loading, state |
| `src/canvas_view.rs` | viewport canvas: bitmap blit, pan/zoom, markers |
| `src/markers.rs` | peak marker placement and label layout |
| `src/compass.rs` | compass/sun helpers |
| `src/widgets.rs` | sliders, dropdowns |
| `src/bridge.rs` | JS glue: image decode, blob read, canvas ctx |
| `www/loader.js` | production entry page (loads the wasm module) |
| `index.html` | trunk entry point (dev only) |
| `www/style.css` | stylesheet |

## Notes

- The base image is decoded by the browser (`createImageBitmap`), not
  by the `image` crate — faster decode and GPU-backed blitting.
- The crate compiles natively (`cargo check -p gipfel`) so it can be
  checked in CI without a wasm toolchain; runtime is wasm-only.
- The hill database is shipped gzip-compressed (`gipfel.dat.gz`).
