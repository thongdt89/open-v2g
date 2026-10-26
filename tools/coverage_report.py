"""Collect GCC host line/branch coverage; this is not protocol conformance."""
import argparse
import gzip
import json
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("build", type=Path)
parser.add_argument("--output", required=True, type=Path)
args = parser.parse_args()
files = {}
objects = sorted(args.build.resolve().rglob("*.gcno"))
if not objects:
    parser.error("No .gcno files: configure with OPENV2G_ENABLE_COVERAGE=ON")
with tempfile.TemporaryDirectory(prefix="openv2g-gcov-") as folder:
    for obj in objects:
        subprocess.run(["gcov", "--json-format", "--branch-probabilities", str(obj)],
                       cwd=folder, check=True, capture_output=True, text=True)
        for report in Path(folder).glob("*.gcov.json.gz"):
            with gzip.open(report, "rt", encoding="utf-8") as source:
                data = json.load(source)
            for entry in data["files"]:
                name = entry["file"]
                lines = files.setdefault(name, {})
                for line in entry["lines"]:
                    previous = lines.setdefault(line["line_number"], {"count": 0, "branches": []})
                    previous["count"] += line["count"]
                    branches = line.get("branches", [])
                    if not previous["branches"]:
                        previous["branches"] = [0] * len(branches)
                    if len(previous["branches"]) != len(branches):
                        raise ValueError(f"Incompatible instrumentation for {name}")
                    for index, branch in enumerate(branches):
                        previous["branches"][index] += branch["count"]
            report.unlink()
summary = {}
for name, lines in sorted(files.items()):
    summary[name] = {
        "lines": len(lines), "lines_hit": sum(row["count"] > 0 for row in lines.values()),
        "branches": sum(len(row["branches"]) for row in lines.values()),
        "branches_hit": sum(count > 0 for row in lines.values() for count in row["branches"]),
    }
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(json.dumps({"scope": "instrumented host tests, not conformance", "files": summary}, indent=2) + "\n")
print(f"Coverage collected for {len(summary)} source files: {args.output}")
