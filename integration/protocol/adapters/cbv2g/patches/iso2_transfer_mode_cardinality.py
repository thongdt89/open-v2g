"""Build-copy overlay for the pinned six-mode EXI terminal grammar.

Never changes the external checkout. The separate pinned oracle exercises the
maximum-cardinality form. Source hashes and exact replacement counts fail closed.
"""
from pathlib import Path
import hashlib
import json
import sys

source, output = map(Path, sys.argv[1:])
output.mkdir(parents=True, exist_ok=True)
manifest = {"patch": "iso2-transfer-mode-cardinality-v1", "files": {}}
for kind, expected in [("Encoder", "cda605929689af3a5e1625f3b0ea563c15825b69f4753aef771dd8ccdebe61ed"),
                       ("Decoder", "b35e0e6dd14804c7e7483d27af87db9b1a43e74fbdc6ec4ab9de1c67c7d7201e")]:
    path = source / f"iso2_msgDef{kind}.c"
    data = path.read_bytes()
    if hashlib.sha256(data).hexdigest() != expected:
        raise RuntimeError(f"Pinned source hash mismatch: {path}")
    text = data.decode("utf-8")
    operation = "encode" if kind == "Encoder" else "decode"
    marker = f"static int {operation}_iso2_SupportedEnergyTransferModeType(exi_bitstream_t* stream, "
    start = text.index(marker, text.index(marker)+1)
    end = text.index("\nstatic int ", start+1)
    fragment = text[start:end]
    if kind == "Encoder":
        old = "error = exi_basetypes_encoder_nbit_uint(stream, 2, 1);"
        new = """/* PROJECT overlay: after the sixth item only EE remains. */
                error = exi_basetypes_encoder_nbit_uint(stream,
                    EnergyTransferMode_currentIndex == iso2_EnergyTransferModeType_6_ARRAY_SIZE ? 1 : 2,
                    EnergyTransferMode_currentIndex == iso2_EnergyTransferModeType_6_ARRAY_SIZE ? 0 : 1);"""
    else:
        old = "error = exi_basetypes_decoder_nbit_uint(stream, 2, &eventCode);"
        new = """/* PROJECT overlay: after the sixth item only EE remains. */
            error = exi_basetypes_decoder_nbit_uint(stream,
                SupportedEnergyTransferModeType->EnergyTransferMode.arrayLen == iso2_EnergyTransferModeType_6_ARRAY_SIZE ? 1 : 2, &eventCode);
            if (error == 0 && SupportedEnergyTransferModeType->EnergyTransferMode.arrayLen == iso2_EnergyTransferModeType_6_ARRAY_SIZE)
            {
                if (eventCode != 0) { error = EXI_ERROR__UNKNOWN_EVENT_CODE; }
                else { eventCode = 1; } /* Existing switch's EE branch. */
            }"""
    if fragment.count(old) != 1:
        raise RuntimeError(f"Unexpected overlay match count: {kind}")
    changed = (text[:start] + fragment.replace(old,new) + text[end:]).encode("utf-8")
    (output/path.name).write_bytes(changed)
    manifest["files"][path.name] = {"upstream_sha256": expected, "compiled_copy_sha256": hashlib.sha256(changed).hexdigest()}
(output/"patch_manifest.json").write_text(json.dumps(manifest,indent=2)+"\n")
