/-
  GipfelFind.lean — using Lean to LOOK FOR flaws in gipfel-core.

  Method: transcribe the suspect logic into Lean twice.
    * `Int`   model → prove the general fact (why it fails, where the boundary is)
    * `Float` model → exhaustive search for a concrete witness
  Every witness below was then re-checked against the real Rust code.
-/

def PI : Float := 3.141592653589793
def degf (d : Float) : Float := d * PI / 180

-- ════════════════════════════════════════════════════════════════════════
-- FINDING 1 — `Panorama::is_visible` silently accepts *everything* once
--              |α − a_center| > 2π
--   STATUS: FIXED on master — is_visible now wraps with `% 2π` before the
--           fold and rejects non-finite α / a_center. Regression test:
--           panorama::tests::out_of_range_center_angle_stays_safe.
-- ════════════════════════════════════════════════════════════════════════
-- panorama.rs:406
--   let mut center_dist = (alph - self.parms.a_center).abs();
--   if center_dist > PI { center_dist = 2*PI - center_dist; }
--   if center_dist >= self.proj.get_view_angle() { return false; }
--   self.max_view_angle <= 0.0 || center_dist < self.max_view_angle

/-- Faithful transcription (single fold). -/
def centerDist (α c : Float) : Float :=
  let d := (α - c).abs
  if d > PI then 2 * PI - d else d

def isVisible (α c viewAngle : Float) : Bool := !((centerDist α c) >= viewAngle)

/-- What the function *means*: angular separation folded into [0, π].
    `wrapTurns` is exact for the search domain (|α − c| < 16π). -/
def wrapTurns (d : Float) : Float :=
  (List.range 16).foldl (init := d) fun acc _ =>
    if acc > 2 * PI then acc - 2 * PI else acc

def angDist (α c : Float) : Float :=
  let d := wrapTurns (α - c).abs
  if d > PI then 2 * PI - d else d

-- ── 1a. Why it fails — exact, over Int (`p` stands for π) ──

def fold (d p : Int) : Int := if d > p then 2 * p - d else d

/-- The fold produces a *negative* "distance" exactly when d > 2π. -/
theorem fold_neg_iff (d p : Int) (hd : 0 ≤ d) : fold d p < 0 ↔ 2 * p < d := by
  unfold fold; split <;> omega

theorem fold_ge_zero_iff (d p : Int) (hd : 0 ≤ d) : 0 ≤ fold d p ↔ d ≤ 2 * p := by
  unfold fold; split <;> omega

/-- A negative distance passes every comparison in `is_visible`. -/
def visibleOf (cd viewAngle : Int) : Bool := !(cd >= viewAngle)

theorem all_visible_when_negative (cd viewAngle : Int) (h : cd < 0) (hv : 0 ≤ viewAngle) :
    visibleOf cd viewAngle = true := by
  unfold visibleOf
  have h2 : ¬(cd ≥ viewAngle) := by omega
  simp [h2]

/-- When the distance is honest, visibility implies "within the field of view". -/
theorem ok_when_honest (cd viewAngle : Int) (h : ¬(cd ≥ viewAngle)) :
    cd < viewAngle := by omega

-- ── 1b. Search: which center angles actually break it? ──

def viewAngle : Float := 1.0471976              -- rectilinear half-angle = 60°
def alphas : List Float := (List.range 73).map fun i => degf (i.toFloat * 5)

def violationAt (cdeg : Float) : Bool :=
  let c := degf cdeg
  alphas.any fun a => isVisible a c viewAngle && angDist a c > viewAngle

/-- Sampled center angles (degrees) for which some hill *behind* the camera is
    reported visible. -/
def badCenters : List Float :=
  (List.range 145).foldl (init := []) fun acc i =>
    let c := i.toFloat * 15 - 1080
    if violationAt c then acc ++ [c] else acc

#eval "── Finding 1: center angles (degrees) where a hill outside the field of view is 'visible'"
#eval badCenters

/-- An ordinary, realistic input: camera azimuth −75° (i.e. the user entered a
    bearing in [-180,180]), hill at azimuth 350°. -/
def witness1 : Float × Float × Float × Float × Bool :=
  (degf (-75), degf 350, centerDist (degf 350) (degf (-75)),
                 angDist (degf 350) (degf (-75)), isVisible (degf 350) (degf (-75)) viewAngle)

#eval "── Finding 1: (center°, α°, reported center_dist, true separation°, isVisible)"
#eval witness1

/-- Exact characterization of the safe window, derived from the search. -/
def safeWindow : List Float :=
  (List.range 97).foldl (init := []) fun acc i =>
    let c := i.toFloat - 48   -- -48 .. 48 degrees, 1° steps
    if violationAt c then acc ++ [c] else acc

#eval "── Finding 1: bad center angles in [-48°, 48°] (empty = safe near zero)"
#eval safeWindow

/-- NaN and infinite azimuths are reported visible as well. -/
def isVisibleFull (α c viewAngle maxAngle : Float) : Bool :=
  let cd0 := (α - c).abs
  let cd := if cd0 > PI then 2 * PI - cd0 else cd0
  if cd >= viewAngle then false else (maxAngle <= 0 || cd < maxAngle)

#eval "── Finding 1b: is_visible(NaN) , is_visible(+∞)   (both true = bug)"
#eval (isVisibleFull (0/0) 0 viewAngle 0, isVisibleFull (1/0) 0 viewAngle 0)

-- ════════════════════════════════════════════════════════════════════════
-- FINDING 2 — can the projection denominator be exactly zero?
--   _11 = 1/(cos pitch · cos elev · cos Δα + sin pitch · sin elev)
--   and `project_point` filters NaN but not ±∞.
--   (First-quadrant grid; see Finding 2b for the full picture + Rust referee.)
-- ════════════════════════════════════════════════════════════════════════

def denom (pitch elev dAlpha : Float) : Float :=
  Float.cos pitch * Float.cos elev * Float.cos dAlpha + Float.sin pitch * Float.sin elev

/-- Grid search: exact zeros, and the smallest |denominator| seen. -/
def denomSearch : (List (Float × Float × Float)) × (Option Float) :=
  (List.range 46).foldl (init := (([] : List (Float × Float × Float)), (none : Option Float))) fun acc i =>
    (List.range 46).foldl (init := acc) fun acc j =>
      (List.range 91).foldl (init := acc) fun acc k =>
        let p := degf (i.toFloat * 2)
        let e := degf (j.toFloat * 2)
        let d := degf (k.toFloat * 4 - 180)
        let v := Float.abs (denom p e d)
        if v == 0 then (acc.1 ++ [(p, e, d)], some 0)
        else (acc.1, match acc.2 with
                      | none => v
                      | some b => if v < b then v else b)

#eval "── Finding 2: (exact zeros found, smallest |denominator|) on a 46×46×91 grid"
#eval denomSearch

#eval "── Finding 2: if it were zero → 1/0 = inf, and isNaN does NOT catch it"
#eval (1 / (0 : Float), (1 / (0 : Float)).isNaN)

-- ════════════════════════════════════════════════════════════════════════
-- FINDING 2b — full-domain grid + exact model of ALL zero families,
--              cross-checked against the real Rust code (rust-check/).
-- ════════════════════════════════════════════════════════════════════════

/-
  Exact model (pen and paper, via the addition theorems). With
  denom p e dα = cos p · cos e · cos dα + sin p · sin e:

    dα = ±180°:  denom = sin p sin e − cos p cos e = −cos(p + e)
                 → zero  ⟺  p + e = ±90°            (family A)
    dα = 0    :  denom = cos p cos e + sin p sin e =  cos(p − e)
                 → zero  ⟺  p − e = ±90°            (family B)
    otherwise :  |cos dα| < 1 forces |denom| ≥ (1−|cos dα|)/2 · something
                 strictly positive on the domain — the two families are
                 the complete list of exact real zeros.

  The ORIGINAL grid above only sampled p, e ∈ [0°, 90°] (justified by the
  p↔e swap and (p,e)→(−p,−e) symmetries for SAME-sign pairs), which silently
  skips the opposite-sign quadrant — and family B lives exactly there
  (e.g. pitch +45°, elev −45°: camera tilted up, peak dead ahead below the
  horizon). The full grid below removes the reduction.

  Whether the *Float* implementation actually reaches 0.0 at a mathematical
  zero is libm-dependent — the audit below answers that empirically, and
  rust-check/ re-checks every witness against the real generated Rust.

  Rust referee (rust-check/, 2024-11, x86_64 glibc):
    * 84 exact zeros on the full grid — 56 family A, 28 family B, 0 outside
      the two predicted families (the exact model is confirmed complete).
    * All four original Lean witnesses ((6,84,±180) etc.) are exact zeros
      in Rust too — the two libms agree here.
    * Pushing every witness through the real mac_x/mac_y yields NaN, never
      ±inf: the zero denominator makes some factor inf but another factor
      exactly 0 (sin 0 or sin ±π_float), and 0·inf = NaN poisons the sum.
      So `project_point`'s NaN guard DOES filter all of them — the feared
      "inf passes the guard" does NOT reproduce on this path.
    * The residual hazard is NEAR-zero denominators instead: smallest
      nonzero |denom| on the grid = 3.7e-33 (p=−90°, e=0°, dα=−90°), giving
      finite coordinates ~1e32 px that pass every guard.
-/

/-- Full-domain grid: pitch, elev over [−90°, 90°] step 1°, Δα over
    [−180°, 180°] step 2° — no symmetry reduction. sin/cos are precomputed
    per axis so the inner loop is pure multiplication. -/
def denomZerosFull : List (Float × Float × Float) :=
  let ps := (List.range 181).map fun i =>
    let p := degf (i.toFloat - 90); (p, Float.sin p, Float.cos p)
  let es := (List.range 181).map fun j =>
    let e := degf (j.toFloat - 90); (e, Float.sin e, Float.cos e)
  let ds := (List.range 181).map fun k =>
    let d := degf (k.toFloat * 2 - 180); (d, Float.cos d)
  ps.foldl (init := ([] : List (Float × Float × Float))) fun acc (p, sp, cp) =>
    es.foldl (init := acc) fun acc (e, se, ce) =>
      ds.foldl (init := acc) fun acc (d, cd) =>
        if cp * ce * cd + sp * se == 0 then acc ++ [(p, e, d)] else acc

/-- Integer-degree pairs (p, e) in [−90, 90]² that the exact model says are
    zeros of family A (p + e = ±90°) / family B (p − e = ±90°). -/
def famAPairs : List (Int × Int) := Id.run do
  let mut acc : List (Int × Int) := []
  for i in List.range 181 do
    for j in List.range 181 do
      let p : Int := (i : Int) - 90
      let e : Int := (j : Int) - 90
      if Int.natAbs (p + e) == 90 then acc := acc ++ [(p, e)]
  return acc

def famBPairs : List (Int × Int) := Id.run do
  let mut acc : List (Int × Int) := []
  for i in List.range 181 do
    for j in List.range 181 do
      let p : Int := (i : Int) - 90
      let e : Int := (j : Int) - 90
      if Int.natAbs (p - e) == 90 then acc := acc ++ [(p, e)]
  return acc

/-- Of those mathematical zeros, which ones does THIS libm actually round to
    exactly 0.0? (Compare with the Rust referee output.) -/
def famAExact : List (Int × Int) :=
  famAPairs.filter fun (p, e) => denom (degf p.toFloat) (degf e.toFloat) (degf 180) == 0

def famBExact : List (Int × Int) :=
  famBPairs.filter fun (p, e) => denom (degf p.toFloat) (degf e.toFloat) 0 == 0

#eval "── Finding 2b: exact zeros on the full 181×181×181 grid (Lean's libm)"
#eval denomZerosFull.length

#eval "── Finding 2b: family A (math pairs, exact-Float pairs)"
#eval (famAPairs.length, famAExact.length)

#eval "── Finding 2b: family B (math pairs, exact-Float pairs)"
#eval (famBPairs.length, famBExact.length)

#eval "── Finding 2b: family B exact witnesses in Lean's libm (pitch, elev)"
#eval famBExact

-- ════════════════════════════════════════════════════════════════════════
-- FINDING 3 — `mark_hidden`: 0/0 gives NaN, which passes both guards
--   h = (elev_n − elev_m)/|α_m − α_n| ; guarded by is_infinite and `>`
-- ════════════════════════════════════════════════════════════════════════

def hideQuotient (elevM elevN alphM alphN : Float) : Float :=
  (elevN - elevM) / (alphM - alphN).abs

def finding3 : Float × Bool × String :=
  let h := hideQuotient 0.5 0.5 0.3 0.3
  (h, h.isNaN, if h > 1.2 then "hidden" else "not hidden")

#eval "── Finding 3: (h, isNaN, guard verdict) — NaN ⇒ neither guard fires"
#eval finding3

-- ════════════════════════════════════════════════════════════════════════
-- FINDING 4 — how far does `interp_cubic` overshoot? (caller must clamp)
-- ════════════════════════════════════════════════════════════════════════

def interpCubic (x x2 x3 v0 v1 v2 v3 : Float) : Float :=
  let a0 := v3 - v2 - v0 + v1
  let a1 := v0 - v1 - a0
  let a2 := v2 - v0
  let a3 := v1
  a0 * x3 + a1 * x2 + a2 * x + a3

def spikeOvershoot : List (Float × Float) :=
  (List.range 101).foldl (init := []) fun acc i =>
    let x := i.toFloat / 100
    let v := interpCubic x (x * x) (x * x * x) 0 0 1 0
    if v < 0 || v > 1 then acc ++ [(x, v)] else acc

#eval "── Finding 4: bicubic single-spike samples outside [0,1] (caller must clamp)"
#eval spikeOvershoot
