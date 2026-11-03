#!/usr/bin/env python3
"""Render the five editable DOT diagrams with Graphviz; no network required."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parent
for source in sorted(ROOT.glob('[0-9][0-9]-*.dot')):
    for kind in ('svg', 'png'):
        subprocess.run(['dot', f'-T{kind}', '-Gdpi=120', str(source),
                        '-o', str(source.with_suffix('.' + kind))], check=True)
    print(source.stem)
