"""Fail closed if workflow edits hide failures or use mutable action references."""
from pathlib import Path
import re

suppression = re.compile(r"continue-on-error\s*:|\|\|\s*(?:(?:true|echo)\b|:(?:\s|$))|--if-present|exit\s+0\b")
for example in ("continue-on-error: true", "cmd || true", "cmd || :", "cmd || echo skipped", "exit 0"):
    if not suppression.search(example):
        raise SystemExit(f"policy self-check failed: {example}")

root = Path(__file__).resolve().parents[1]
for path in sorted((root / ".github" / "workflows").glob("*.y*ml")):
    text = path.read_text(encoding="utf-8")
    for number, line in enumerate(text.splitlines(), 1):
        if line.lstrip().startswith("#"):
            continue
        if suppression.search(line):
            raise SystemExit(f"{path.name}:{number}: failure suppression is forbidden")
        action = re.search(r"uses:\s*([^\s#]+)", line)
        if action and not re.fullmatch(r"[^@]+@[0-9a-f]{40}", action.group(1)):
            raise SystemExit(f"{path.name}:{number}: pin actions to a full commit SHA")
        if "ctest " in line and "--no-tests=error" not in line:
            raise SystemExit(f"{path.name}:{number}: CTest must fail on empty discovery")
print("CI policy passed")
