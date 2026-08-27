#!/usr/bin/env python3
"""Contract tests: verify.py tier-0 smoke vs bench.py timing/verify separation."""

from __future__ import annotations

import ast
import re
import subprocess
import sys
import unittest
from pathlib import Path

HARNESS = Path(__file__).resolve().parent
REPO = HARNESS.parents[1]
BENCH = HARNESS / "bench.py"
VERIFY = HARNESS / "verify.py"
LIC = REPO / "build" / "compiler" / "lic" / "lic"
TIER0_DIR = REPO / "li-tests" / "benchmarks" / "tier0_correctness"
MANIFEST = REPO / "li-tests" / "manifest.toml"


def _bench_main_tier0_body() -> str:
    tree = ast.parse(BENCH.read_text())
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name == "main":
            return ast.get_source_segment(BENCH.read_text(), node) or ""
    raise AssertionError("bench.py: main() not found")


class HarnessContractTest(unittest.TestCase):
    def test_tier0_sources_nonempty(self) -> None:
        sys.path.insert(0, str(HARNESS))
        try:
            import verify as verify_mod  # noqa: PLC0415
        finally:
            sys.path.pop(0)
        sources = verify_mod.tier0_sources()
        self.assertGreaterEqual(len(sources), 3)
        for path in sources:
            self.assertTrue(path.is_file(), path)
            self.assertEqual(path.parent, TIER0_DIR)

    def test_tier0_sources_align_with_manifest(self) -> None:
        sys.path.insert(0, str(HARNESS))
        try:
            import verify as verify_mod  # noqa: PLC0415
        finally:
            sys.path.pop(0)
        stems = {p.name for p in verify_mod.tier0_sources()}
        manifest_paths = set(
            re.findall(
                r'file = "benchmarks/tier0_correctness/([^"]+\.li)"',
                MANIFEST.read_text(),
            )
        )
        self.assertEqual(stems, manifest_paths)

    def test_bench_tier0_pipeline_order(self) -> None:
        body = _bench_main_tier0_body()
        tier0_block = re.search(
            r"if args\.tier == 0:.*?(?=\n    if args\.tier == 1:)",
            body,
            re.DOTALL,
        )
        self.assertIsNotNone(tier0_block, "tier 0 branch missing in bench.py main()")
        block = tier0_block.group(0)
        self.assertIn("run_tier0()", block)
        self.assertIn("run_verify()", block)
        self.assertIn("stability.py", block)
        self.assertLess(block.index("run_tier0()"), block.index("run_verify()"))
        self.assertLess(block.index("run_verify()"), block.index("stability.py"))

    def test_skip_verify_gates_checksum_before_timing(self) -> None:
        tree = ast.parse(BENCH.read_text())
        fn = next(n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == "run_tier_benches")
        source = ast.get_source_segment(BENCH.read_text(), fn) or ""
        self.assertIn("if verify:", source)
        self.assertIn("verify_checksum", source)
        self.assertIn("run_benchmark", source)
        self.assertLess(source.index("if verify:"), source.index("run_benchmark"))

    def test_verify_lic_build_uses_manifest_honesty_flags(self) -> None:
        text = VERIFY.read_text()
        self.assertIn("--allow-open-vc", text)
        self.assertIn("--no-lean-verify", text)

    @unittest.skipUnless(LIC.is_file(), "lic not built (run ./scripts/build.sh)")
    def test_verify_py_tier0_smoke_green(self) -> None:
        proc = subprocess.run(
            [sys.executable, str(VERIFY)],
            cwd=REPO,
            capture_output=True,
            text=True,
        )
        self.assertEqual(proc.returncode, 0, msg=proc.stderr or proc.stdout)
        self.assertIn("PASS verify", proc.stdout)


if __name__ == "__main__":
    raise SystemExit(unittest.main(verbosity=2))
