#!/usr/bin/env python3
"""Check screen/ledger coverage; this does not accept UI or platform behavior."""
import json
from pathlib import Path
import re

root = Path(__file__).resolve().parent.parent
expected = {
    f"{family}{number:02}"
    for family, count in {"C": 3, "M": 25, "E": 11, "A": 11, "H": 19, "P": 5}.items()
    for number in range(1, count + 1)
}
headings = re.findall(r"^#### ([CME AHP]\d{2})\b", (root / "docs/ui.md").read_text(), re.M)
ledger = json.loads((root / "docs/evidence/ui-acceptance-2026-10-19.json").read_text())
rows = ledger["screens"]
ids = [row["id"] for row in rows]
errors = []
if len(headings) != 74 or set(headings) != expected:
    errors.append("Specification must contain each of the 74 screen IDs exactly once")
if len(ids) != 74 or set(ids) != expected:
    errors.append("Ledger must contain each of the 74 screen IDs exactly once")
for row in rows:
    role = "computer" if row["id"].startswith("E") else "manager"
    platforms = row["platforms"]
    if len(platforms) != 2 or {p["name"] for p in platforms} != {f"Mac {role}", f"Windows {role}"}:
        errors.append(f'{row["id"]}: both native platforms must be represented')
    for platform in platforms:
        for kind in ("build", "runtime"):
            if platform[f"{kind}_status"] not in {"Verified", "Failed", "Not Verified"}:
                errors.append(f'{row["id"]}: invalid {kind} status')
        if platform["runtime_status"] == "Verified":
            evidence = platform.get("runtime_evidence")
            if not isinstance(evidence, dict) or not all(evidence.get(k) for k in ("source_revision", "artifact_sha256", "scenarios")):
                errors.append(f'{row["id"]}: runtime acceptance requires artifact-specific scenario evidence')
if errors:
    raise SystemExit("\n".join(errors))
print("74 screen families and 148 platform rows match; coverage is not runtime acceptance")
