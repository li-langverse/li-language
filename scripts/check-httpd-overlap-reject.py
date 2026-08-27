#!/usr/bin/env python3
"""M1: overlapping routes at the same priority must fail validate_routes."""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

from httpd_config import CanonicalRoute, ConfigError, validate_routes


def main() -> int:
    routes = [
        CanonicalRoute("a", "GET", "/api", "exact", "static:a", {}, 0),
        CanonicalRoute("b", "GET", "/api", "prefix", "static:b", {}, 0),
    ]
    try:
        validate_routes(routes)
    except ConfigError:
        print("check-httpd-overlap-reject: ok")
        return 0
    print("check-httpd-overlap-reject: expected ConfigError", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
