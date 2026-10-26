#!/usr/bin/env python3
"""Post-process maxima output for Rust:
 - convert bare integer literals to float literals (1 -> 1.0)
 - drop maxima noise lines (keep only 'pub fn' lines)
"""
import re
import sys

pat = re.compile(r"(?<![\w.])-?\d+(?![\w.])")

for line in sys.stdin:
    if not line.startswith("pub fn"):
        continue
    sys.stdout.write(pat.sub(lambda m: m.group(0) + ".0", line))
