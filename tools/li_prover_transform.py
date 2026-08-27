#!/usr/bin/env python3
"""Rewrite bootstrap/prover/main.li into the accessor pattern.

The li index policy only accepts constant / refinement-param / loop-var /
bounds-helper indices. A recursive tree walker cannot index arrays at
computed positions, so every array read/write goes through per-size
accessor helpers (peek/set) whose position params are refinement types:

    type Pos2048 = {p: int | 0 <= p and p < 2048}
    def peek2048(buf: var array[2048, int], p: Pos2048) -> int ...

Rules applied:
  1. The input buffer is bumped from 1024 to 2048 so every data buffer
     shares one accessor pair (node_size/right_child/... are size-uniform).
  2. All array params become `var array[N, int]` (borrow, not move).
  3. `ARR[i]` reads  -> `peekN(ARR, i)`;  `ARR[i] = v` writes -> `setN(ARR, i, v)`.
  4. Bare scalar idents as call args become `x + 0` (ident args move the
     binding under the borrow checker; expressions do not).
"""

import re
import sys

ARRAY2048 = {"buf", "work", "cb", "td", "dst"}
ARRAY64 = {
    "ftype", "fval", "stack", "tsign", "tstart", "tsize",
    "kept", "kept_sign", "seen", "sst", "q", "qst", "ssign",
    "efrom", "eto", "estrict", "ea", "eb",
}
CELL1 = {"ne_cnt", "neq_cnt"}  # array[1] cells: constant index 0, left as-is
PTR = {"s"}

ALL = ARRAY2048 | ARRAY64


def size_of(name: str) -> int:
    if name in ARRAY2048:
        return 2048
    if name in ARRAY64:
        return 64
    return 0


def find_matching(s: str, i: int) -> int:
    """s[i] is '(' or '['; return index of matching closer."""
    open_c, close_c = (s[i], ")]"["([".index(s[i])])
    depth = 0
    for k in range(i, len(s)):
        if s[k] == open_c:
            depth += 1
        elif s[k] == close_c:
            depth -= 1
            if depth == 0:
                return k
    return len(s) - 1


def split_top(s: str):
    parts, depth, cur = [], 0, ""
    for ch in s:
        if ch in "([":
            depth += 1
        elif ch in ")]":
            depth -= 1
        if ch == "," and depth == 0:
            parts.append(cur.strip())
            cur = ""
        else:
            cur += ch
    if cur.strip():
        parts.append(cur.strip())
    return parts


def transform_expr(s: str) -> str:
    """Replace ARR[I] reads with peekN(ARR, I), innermost first."""
    out = s
    while True:
        m = re.search(r"\b([a-z_][a-z_0-9]*)\[", out)
        if not m or m.group(1) not in ALL:
            break
        name = m.group(1)
        j = m.end() - 1  # index of '['
        k = find_matching(out, j)
        inner = transform_expr(out[j + 1:k])
        out = out[:m.start()] + f"peek{size_of(name)}({name}, {inner})" + out[k + 1:]
    return out


def wrap_args(s: str) -> str:
    """Wrap bare scalar idents in call args with `+ 0` (recursively)."""
    out = []
    i = 0
    while i < len(s):
        m = re.match(r"[a-z_][a-z_0-9]*", s[i:])
        if not m:
            out.append(s[i])
            i += 1
            continue
        name = m.group(0)
        j = i + len(name)
        if j < len(s) and s[j] == "(":
            k = find_matching(s, j)
            args = [wrap_args(a) for a in split_top(s[j + 1:k])]
            wrapped = []
            for a in args:
                if (re.fullmatch(r"[a-z_][a-z_0-9]*", a) and a not in ALL
                        and a not in CELL1 and a not in PTR):
                    wrapped.append(f"{a} + 0")
                else:
                    wrapped.append(a)
            out.append(name + "(" + ", ".join(wrapped) + ")")
            i = k + 1
        else:
            out.append(name)
            i = j
    return "".join(out)


def rewrite_line(line: str) -> str:
    """Statement-level rewrite: array-write assignments -> setN(...)."""
    m = re.match(r"^(\s*)([a-z_][a-z_0-9]*)\[([^\]]+)\] = (.+)$", line)
    if m and m.group(2) in ALL:
        indent, name, idx, rhs = m.group(1), m.group(2), m.group(3), m.group(4)
        n = size_of(name)
        return f"{indent}set{n}({name}, {transform_expr(idx)}, {transform_expr(rhs)})"
    return line


def main() -> None:
    src = open("bootstrap/prover/main.li").read()

    # 1. size bump + var params inside def signatures (paren-depth tracked)
    lines = src.split("\n")
    out = []
    in_sig = False
    for ln in lines:
        if in_sig or ln.strip().startswith("def "):
            ln = ln.replace("array[1024, int]", "array[2048, int]")
            ln = re.sub(r"\b([a-z_][a-z_0-9]*): array\[(\d+), int\]",
                        r"\1: var array[\2, int]", ln)
            depth = ln.count("(") - ln.count(")")
            in_sig = depth > 0
        else:
            ln = ln.replace("array[1024, int]", "array[2048, int]")
        out.append(ln)
    src = "\n".join(out)

    # 2. rewrite bodies (statements of the procs)
    out = []
    for ln in src.split("\n"):
        stripped = ln.strip()
        if stripped and not stripped.startswith(("def ", "extern ", "type ", "#", "requires",
                                                "ensures", "decreases", "=")):
            ln = rewrite_line(ln)
            ln = transform_expr(ln)
            ln = wrap_args(ln)
        out.append(ln)
    src = "\n".join(out)

    # 3. accessor helpers + position types (bodies hold the only direct
    #    array indices, so they are inserted after the rewrite)
    accessors = """type Pos2048 = {p: int | 0 <= p and p < 2048}
type Pos64 = {p: int | 0 <= p and p < 64}

def peek2048(buf: var array[2048, int], p: Pos2048) -> int
  requires true
  ensures result == buf[p]
  decreases 0
=
  return buf[p]

def set2048(buf: var array[2048, int], p: Pos2048, v: int) -> int
  requires true
  ensures result == v
  decreases 0
=
  buf[p] = v
  return v

def peek64(buf: var array[64, int], p: Pos64) -> int
  requires true
  ensures result == buf[p]
  decreases 0
=
  return buf[p]

def set64(buf: var array[64, int], p: Pos64, v: int) -> int
  requires true
  ensures result == v
  decreases 0
=
  buf[p] = v
  return v

"""
    if "type Pos2048" not in src:
        src = src.replace(
            "# ---------------------------------------------------------------- parsing --\n",
            accessors + "# ---------------------------------------------------------------- parsing --\n",
            1)
    else:
        # idempotent: strip any previously inserted accessor block
        start = src.index("type Pos2048")
        end = src.index("# ---------------------------------------------------------------- parsing --")
        src = src[:start] + src[end:]

    open("bootstrap/prover/main.li", "w").write(src)
    print("transformed OK")


if __name__ == "__main__":
    main()
