# gipfel-web

Browser front end for gipfel (WASM, Yew + canvas 2D). See
[`/wasm-plan.md`](../../wasm-plan.md) for the overall plan and milestones.

**Status: W1 (scaffold + image display) done** — open/drag-drop a JPEG,
pan (drag) and zoom (wheel, cursor-anchored). Peak identification,
fitting and saving arrive in W2/W3.

## Development

Prerequisites: [rustup](https://rustup.rs) and trunk:

```sh
cargo install --locked trunk
```

Serve with hot reload:

```sh
cd crates/gipfel-web
trunk serve --port 8090     # then open http://localhost:8090
```

Release build (static output in `dist/`, serveable by any web server):

```sh
trunk build --release
```

## Layout

| File | Purpose |
|---|---|
| `src/main.rs` | entry point |
| `src/app.rs` | root component: sidebar, file loading, state |
| `src/canvas_view.rs` | viewport canvas: bitmap blit, pan/zoom |
| `src/bridge.rs` | JS glue: image decode, blob read, canvas ctx |
| `index.html` | trunk entry point (canonical trunk layout) |
| `www/style.css` | stylesheet (trunk asset) |

## Notes

- The base image is decoded by the browser (`createImageBitmap`), not
  by the `image` crate — faster decode and GPU-backed blitting.
- The crate compiles natively (`cargo check -p gipfel-web`) so it can
  be checked in CI without a wasm toolchain; runtime is wasm-only.
