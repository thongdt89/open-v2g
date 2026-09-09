# Protocol-generated integration boundary

Status: `BLOCKED_FOR_TARGET` under the provisional baseline.

This directory is reserved for licensed/generated SAP, DIN 70121, ISO 15118-2 and later ISO 15118-20 artifacts. The repository currently contains no licensed schema package or generated protocol implementation.

Rules:

- select schema explicitly; never infer it from untrusted payload;
- keep SAP, DIN, ISO-2 and ISO-20 Common/AC/DC generated models separate;
- keep generated protocol objects behind the EXI/protocol adapter boundary;
- record schema edition, generator version and output checksum;
- validate bounds before allocation/decode;
- provide golden vectors, malformed-input tests and target workspace/WCET evidence.
