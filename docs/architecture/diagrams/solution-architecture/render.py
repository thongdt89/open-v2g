"""Render the editable architecture sources with local Graphviz."""
from pathlib import Path
import subprocess
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent
for source in sorted(ROOT.glob("*.dot")):
    for fmt in ("svg", "png"):
        output = source.with_suffix("." + fmt)
        subprocess.run(["dot", "-T" + fmt, str(source), "-o", str(output)], check=True)
        if fmt == "svg":
            ET.parse(output)
        print(output.name)
