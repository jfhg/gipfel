# Lean proofs and flaw-finding for gipfel-core

Machine-checked transcriptions of gipfel-core's numerically sensitive logic,
plus a Rust "referee" that re-checks every witness against the real code.

## Files

| file | what it is |
|---|---|
| `Gipfel.lean` | Verified structural invariants (pure Lean 4 core, no Mathlib): `mark_duplicates` preserves length and order — the stable-hill-index invariant everything downstream relies on. |
| `GipfelFind.lean` | Flaw hunting. Each suspect function is transcribed twice (exact `Int` model for the general fact, `Float` model + grid search for concrete witnesses). Running it (`lean GipfelFind.lean`) prints the witnesses. |
| `GipfelMathlib.lean.txt` | NOT CHECKED design sketch of the Mathlib half (trig over ℝ, stitch cubic kernel). Needs `lake new` + `lake exe cache get` before any of it elaborates. Kept as `.txt` so it cannot be mistaken for a verified artifact. |
| `Fib.lean`, `fib.c` | Warm-up comparison (Lean vs C, exhaustive fib overflow search). |
| `rust-check/` | The referee: a small cargo project that re-runs each finding's witnesses against the real `gipfel-core` (path dependency on `../../crates/gipfel-core`). |

## Running

```sh
# proofs + witness searches (needs elan/lean, https://lean-lang.org)
lean Gipfel.lean && lean GipfelFind.lean

# referee against the current Rust code
cd rust-check && cargo run --bin rust-check   # projection denominator (Finding 2/2b)
cd rust-check && cargo run --bin f1          # is_visible azimuth wrap (Finding 1)
```

## Findings status (last audit: master, 2024-11)

* **Finding 1** — `is_visible` accepted hills behind the camera once
  `|α − a_center| > 2π` (e.g. azimuth entered as 400°), and accepted
  NaN/±∞ azimuths. **FIXED** on master (`panorama::tests::out_of_range_center_angle_stays_safe`).
* **Finding 2** — the projection denominator `cos p·cos e·cos Δα + sin p·sin e`
  can be *exactly* 0.0 in IEEE-754. The full-domain grid finds 84 witnesses,
  all predicted by the exact model (families `p+e = ±90° at Δα = ±180°` and
  `p−e = ±90° at Δα = 0`). Referee: every witness yields NaN (0·inf), which
  `project_point` filters — benign on this path. Residual: near-zero
  denominators (3.7e-33 at pitch −90°) give finite ~1e32 px coordinates.
* **Finding 3** — `mark_hidden`: two hills at identical azimuth *and* elevation
  give `h = 0/0 = NaN`, which passes both guards, so neither hides the other.
  Cosmetic (overlapping labels), rare. Open.
* **Finding 4** — Keys cubic convolution overshoots [0,1] on single-spike
  kernels. Already handled: the stitch caller clamps (as in the original C).
