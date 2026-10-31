"""Check fixed references and optionally repeat external-codec differential runs."""
import argparse
import json
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("--candidate", required=True)
parser.add_argument("--vectors", required=True)
parser.add_argument("--oracle")
args = parser.parse_args()
with open(args.vectors, encoding="utf-8") as source:
    saved = json.load(source)
expected = {(row["schema"], row["message_type_id"]): bytes.fromhex(row["hex"])
            for row in saved["vectors"]}
actual = {}
for line in subprocess.check_output([args.candidate], text=True, timeout=10).splitlines():
    schema, message, payload = line.split()
    key = (schema, int(message))
    if key in actual:
        raise AssertionError(f"Duplicate vector: {key}")
    actual[key] = bytes.fromhex(payload)
assert actual == expected, "Candidate differs from saved differential references"
if args.oracle:
    for (schema, message), payload in actual.items():
        oracle_schema = {"iso2-offers": "iso2", "din-parameters": "din", "iso2-parameters": "iso2", "iso2-ac3": "iso2", "iso2-ac": "iso2", "iso2-meter": "iso2", "iso2-lifecycle": "iso2", "iso2-lifecycle-ac": "iso2"}.get(schema, schema)
        result = subprocess.run([args.oracle, oracle_schema], input=payload, capture_output=True,
                                timeout=5, check=True)
        assert result.stdout == payload, f"Differential mismatch: {schema}/{message}"
print(f"PASS: {len(actual)} fixed reference vectors" +
      (" and separate-codec differential round-trips" if args.oracle else ""))
