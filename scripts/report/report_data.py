"""Recorded validation and benchmark data consumed by the report."""
import csv
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
EVIDENCE = ROOT / "docs" / "evidence"

def validation():
    return json.loads((EVIDENCE / "validation.json").read_text(encoding="utf-8"))

def tests():
    return json.loads((EVIDENCE / "test-summary.json").read_text(encoding="utf-8"))

def benchmarks():
    with (ROOT / "docs" / "bench" / "results.csv").open(newline="", encoding="utf-8") as source:
        return list(csv.DictReader(source))
