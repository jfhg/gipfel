/-
  Feasibility probe for gipfel-core, part 1: **pure Lean 4 core** (no Mathlib).

  This is what you get for free. It covers the *structural* invariants — the
  ones the repo documents as load-bearing (stable hill indices).
  See GipfelMathlib.lean.txt for the parts that need Mathlib (trig, `ring`,
  `Real.pi`, analysis).
-/

-- ─────────────────────────────────────────────────────────────
-- hill.rs: `mark_duplicates` must never reorder or drop entries, because
-- track lists / known-hill sets / visible lists hold indices into `hills`.
-- AGENTS.md: "mark_duplicates sorts an index permutation, never the vec".
-- ─────────────────────────────────────────────────────────────

structure Hill where
  name   : String
  phi    : Float   -- latitude,  radians
  lam    : Float   -- longitude, radians
  height : Float   -- meters
  dup    : Bool    -- FLAG_DUPLIC

/-- `near d x y` = within tolerance `d` in φ and λ, and within 50 m in height. -/
def near (d : Float) (x y : Hill) : Bool :=
  (x.phi - y.phi).abs ≤ d && (x.lam - y.lam).abs ≤ d && (x.height - y.height).abs ≤ 50.0

/-- Set FLAG_DUPLIC (monotonically: never clears). -/
def Hill.setDup (h : Hill) (b : Bool) : Hill := { h with dup := h.dup || b }

/-- Model of `mark_duplicates`: an entry is marked iff an earlier unmarked entry
    is near it. Written as a `map`, which is exactly the invariant we want:
    the function is *pointwise*, so order and length cannot change. -/
def markDup : Float → List Hill → List Hill
  | _, []      => []
  | d, x :: xs => x.setDup (xs.any fun y => !y.dup && near d x y) :: markDup d xs

/-- **Invariant: length is preserved.** -/
theorem markDup_length (d : Float) (l : List Hill) :
    (markDup d l).length = l.length := by
  induction l with
  | nil => rfl
  | cons x xs ih => simp [markDup, Hill.setDup, ih]

/-- **Invariant: the name sequence is unchanged** (no reordering, no drops). -/
theorem markDup_names (d : Float) (l : List Hill) :
    (markDup d l).map Hill.name = l.map Hill.name := by
  induction l with
  | nil => rfl
  | cons x xs ih => simp [markDup, Hill.setDup, ih]

/-- **Invariant: only the flag can change** — geometry is untouched. -/
theorem markDup_geom (d : Float) (l : List Hill) :
    (markDup d l).map (fun h => (h.name, h.phi, h.lam, h.height))
      = l.map (fun h => (h.name, h.phi, h.lam, h.height)) := by
  induction l with
  | nil => rfl
  | cons x xs ih => simp [markDup, Hill.setDup, ih]

-- ─────────────────────────────────────────────────────────────
-- panorama.rs: azimuth normalization. Modelled over `Int` (exact, core-only)
-- to check the *logic* of the folds, independent of float rounding.
-- ─────────────────────────────────────────────────────────────

/-- `|a - b|` without needing an absolute-value API. -/
def adiff (a b : Int) : Int := if a ≥ b then a - b else b - a

theorem adiff_nonneg (a b : Int) : 0 ≤ adiff a b := by
  unfold adiff; by_cases h : a ≥ b <;> simp [h] <;> omega

/-- `is_visible`'s fold of `|α − c|` into `[0, π]`, on integers with `p` for π. -/
def centerDist (α c p : Int) : Int :=
  if adiff α c > p then 2 * p - adiff α c else adiff α c

theorem centerDist_le (α c p : Int) : centerDist α c p ≤ 2 * p := by
  have h0 : 0 ≤ adiff α c := adiff_nonneg α c
  unfold centerDist
  by_cases h : adiff α c > p <;> simp [h] <;> omega

/-- NOTE the extra hypothesis: `2π − d ≥ 0` needs `d ≤ 2π`. In the program that
    holds because `alpha` comes from `atan2` (so `|α| ≤ π`) — exactly the kind of
    hidden range assumption a formalization forces you to state explicitly. -/
theorem centerDist_ge (α c p : Int) (_hp : 0 ≤ p) (hbound : adiff α c ≤ 2 * p) :
    0 ≤ centerDist α c p := by
  have h0 : 0 ≤ adiff α c := adiff_nonneg α c
  unfold centerDist
  by_cases h : adiff α c > p <;> simp [h] <;> omega

/-- The fold is invariant under rotating camera and hill together
    (`set_center_angle` + a rotated database must not change visibility). -/
theorem centerDist_rot (α c t p : Int) :
    centerDist (α - t) (c - t) p = centerDist α c p := by
  have h : adiff (α - t) (c - t) = adiff α c := by
    unfold adiff
    by_cases h1 : α - t ≥ c - t <;> by_cases h2 : α ≥ c <;> simp [h1, h2] <;> omega
  unfold centerDist
  rw [h]

/-- `alpha`'s `(x + 2π) % 2π` normalization lands in `[0, 2π)`. -/
theorem alpha_range (x p : Int) (hp : 0 < p) :
    0 ≤ (x + 2 * p) % (2 * p) ∧ (x + 2 * p) % (2 * p) < 2 * p := by
  have h1 : 0 ≤ (x + 2 * p) % (2 * p) := Int.emod_nonneg _ (by omega)
  have h2 : (x + 2 * p) % (2 * p) < 2 * p := Int.emod_lt_of_pos _ (by omega)
  exact ⟨h1, h2⟩

-- ─────────────────────────────────────────────────────────────
-- lsq.rs: `params_from_x` must not touch parameters above the fitted level.
-- Pure data manipulation → core Lean.
-- ─────────────────────────────────────────────────────────────

structure ViewParams where
  aCenter : Float
  scale   : Float
  aPitch  : Float
  aRoll   : Float
  k0      : Float
  k1      : Float
  x0      : Float

/-- `np_for_level` -/
def npForLevel : Nat → Nat
  | 0 => 2
  | 1 => 4
  | _ => 7

/-- `params_from_x`: overwrite the first `npForLevel level` entries of the
    parameter record, leaving the rest at their initial values. -/
def paramsFromX (p0 : ViewParams) (level : Nat) (x : List Float) : ViewParams :=
  let p := { p0 with aCenter := x[0]!, aPitch := x[1]! }
  if level > 0 then
    let p := { p with aRoll := x[2]!, scale := x[3]! }
    if level > 1 then { p with k0 := x[4]!, k1 := x[5]!, x0 := x[6]! } else p
  else p

/-- Parameters above the fitted level are untouched. -/
theorem paramsFromX_locked (p0 : ViewParams) (x : List Float) :
    (paramsFromX p0 0 x).k0 = p0.k0 ∧ (paramsFromX p0 0 x).scale = p0.scale ∧
    (paramsFromX p0 1 x).k1 = p0.k1 ∧ (paramsFromX p0 1 x).x0 = p0.x0 := by
  unfold paramsFromX
  simp

/-- The distortion fit really does move the distortion parameters. -/
theorem paramsFromX_fits (p0 : ViewParams) (x : List Float) :
    (paramsFromX p0 2 x).k0 = x[4]! ∧ (paramsFromX p0 2 x).x0 = x[6]! := by
  unfold paramsFromX
  simp

