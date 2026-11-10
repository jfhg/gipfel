/-
  Fibonacci numbers in plain Lean 4 (no Mathlib needed).
  Compile / check with:  lean Fib.lean
-/

-- ─────────────────────────────────────────────────────────────
-- 1. The definition (structural recursion on `Nat`)
-- ─────────────────────────────────────────────────────────────

/-- The `n`-th Fibonacci number: 0, 1, 1, 2, 3, 5, 8, 13, ... -/
def fib : Nat → Nat
  | 0     => 0
  | 1     => 1
  | n + 2 => fib (n + 1) + fib n

-- A quick sanity check. `#eval` runs the compiler; `#print` shows the
-- equation lemmas Lean derived from the definition.
#eval (List.range 11 |>.map fib)          -- [0, 1, 1, 2, 3, 5, 8, 13, 21, 34, 55]

-- ─────────────────────────────────────────────────────────────
-- 2. The property: every Fibonacci number is the sum of the two
--    that precede it.
-- ─────────────────────────────────────────────────────────────

-- Stated with `n + 2`, this is *literally* the definition: `rfl` proves it,
-- because both sides reduce to the same thing.
theorem fib_succ_two (n : Nat) : fib (n + 2) = fib (n + 1) + fib n := rfl

-- Stated the way we normally write it on paper (with subtraction) it needs a
-- real proof, because Lean must first know `n - 1` and `n - 2` are not
-- truncated at zero.
/-- For every `n ≥ 2`, `fib n` is the sum of its two predecessors. -/
theorem fib_two_predecessors (n : Nat) (h : 2 ≤ n) :
    fib n = fib (n - 1) + fib (n - 2) := by
  match n with
  | 0 | 1 => omega                       -- excluded by `h : 2 ≤ n`
  | k + 2 =>
    -- goal: fib (k + 2) = fib (k + 2 - 1) + fib (k + 2 - 2)
    show fib (k + 2) = fib (k + 1) + fib k
    rfl                                   -- exactly `fib_succ_two k`

-- The same fact phrased as "the next one is the sum of the two before it".
theorem fib_next (n : Nat) : fib (n + 2) = fib (n + 1) + fib n := rfl

-- ─────────────────────────────────────────────────────────────
-- 3. Bonus: a fast (tail-recursive) version, proved correct.
--    The naive `fib` above is exponential; `fibLoop` is linear.
-- ─────────────────────────────────────────────────────────────

/-- Iterative Fibonacci: `a` holds `fib (b+1)`, `b` holds `fib b`. -/
def fibLoop : Nat → Nat → Nat → Nat
  | 0,     a, _ => a
  | n + 1, a, b => fibLoop n (a + b) a

/-- Correctness of the loop, proved for *any* starting pair `(fib (b+1), fib b)`.
    Generalizing over `b` is what makes the induction go through: in the step we
    need the hypothesis one position further along the sequence. -/
theorem fibLoop_correct (n : Nat) : ∀ b, fibLoop n (fib (b + 1)) (fib b) = fib (n + b + 1) := by
  induction n with
  | zero => intro b; simp [fibLoop]
  | succ n ih =>
    intro b
    -- one step of the recurrence, in the direction we need
    have step : fib b + fib (b + 1) = fib (b + 2) := by
      show fib b + fib (b + 1) = fib (b + 1) + fib b
      rw [Nat.add_comm]
    show fibLoop (n + 1) (fib (b + 1)) (fib b) = fib ((n + 1) + b + 1)
    -- unfold one loop step, reorder the sum, fold it into `fib (b + 2)`,
    -- then use the induction hypothesis at `b + 1`
    rw [show fibLoop (n + 1) (fib (b + 1)) (fib b) = fibLoop n (fib (b + 1) + fib b) (fib (b + 1)) from rfl,
        Nat.add_comm (fib (b + 1)) (fib b), step, ih (b + 1)]
    apply congrArg fib   -- remaining goal is pure arithmetic
    omega

/-- `fibLoop` computes the same numbers as `fib`. -/
theorem fib_succ_eq_fibLoop (n : Nat) : fib (n + 1) = fibLoop n 1 0 := by
  have h := fibLoop_correct n 0   -- fibLoop n (fib 1) (fib 0) = fib (n + 0 + 1)
  simp [fib] at h                 -- reduces `fib 1` to `1`, `fib 0` to `0`, `n + 0 + 1` to `n + 1`
  exact h.symm

#eval fib 30                         -- 832040     (slow: exponential)
#eval fibLoop 29 1 0                 -- 832040     (fast: linear; note fibLoop n 1 0 = fib (n+1))
#eval fibLoop 40 1 0                 -- 165580141
#eval List.range 21 |>.map fun n => fib n

-- Using the proved property as a rewrite rule:
example : fib 10 = fib 9 + fib 8 := fib_two_predecessors 10 (by omega)
example : fib 100 = fib 99 + fib 98 := fib_two_predecessors 100 (by omega)
