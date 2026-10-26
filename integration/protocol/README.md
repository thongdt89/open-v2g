# Protocol-generated integration boundary

Status: `BLOCKED_FOR_TARGET` under the provisional baseline.

This directory is reserved for licensed/generated SAP, DIN 70121, ISO 15118-2 and later ISO 15118-20 artifacts. The repository contains no licensed schema package or production generated protocol implementation. An opt-in external SAP generated-code host candidate is now available; see below.

The project-side typed `SchemaRegistry` and `ExiCodec` callback boundary is now
implemented and host-tested with a fake backend. See
`docs/architecture/EXI_IMPLEMENTATION_PROPOSAL_VI.md`. This does not change the
licensed/generated artifact status in this directory.

The project-side `SapNegotiator`, static `ProtocolAdapterFactory`,
`Din70121Adapter` and `Iso15118_2Adapter` now implement bounded lifecycle,
selection correlation, request-generating semantic host state machines, exact
pending response matching and energy/permission gates. They deliberately use
project semantic IDs rather than claiming generated DIN/ISO wire types. This
directory must provide the later schema-specific EXI and typed-message mapping
layer between `OpenV2g_ProtocolTxRequest`/`OpenV2g_ProtocolMessage` and the
licensed generated objects.

`OpenV2g_ProtocolDataFlow_GetRule` and
`OpenV2g_ProtocolDataFlow_PrepareRequest` define the project-side projection:
flow case, relevant/required energy fields, permission/action, target-limit
checks and metering evidence. A generated mapper shall consume these fields;
it shall not duplicate a second, divergent application-flow table.

Runtime consumers shall use the provisional C/S and S/R port handles described
in `docs/architecture/PROVISIONAL_ASR_INTERFACE_STYLE.md`; direct access to a
codec/registry implementation is reserved for local initialization and binding.

Rules:

- select schema explicitly; never infer it from untrusted payload;
- keep SAP, DIN, ISO-2 and ISO-20 Common/AC/DC generated models separate;
- keep generated protocol objects behind the EXI/protocol adapter boundary;
- record schema edition, generator version and output checksum;
- validate bounds before allocation/decode;
- provide golden vectors, malformed-input tests and target workspace/WCET evidence.

The host/SIL `v2gtp_codec` now provides bounded framing and incremental stream
handling with caller-owned storage. `ISO15118Chrg` exposes a provisional bounded
transport-byte intake connected to that parser. A typed host-only `TcpTlsSession`
models immutable SDP endpoint selection, connection generation, TLS handshake and
no-downgrade failure behavior. Binding these seams to actual
SoAd/TcpIp/PduR callbacks and proving protocol conformance remain G2 work.

## SAP host candidate — 26/10/2026

[libcbv2g SAP backend](adapters/cbv2g/README.md) implements real generated encode/decode behind the existing codec C/S interface, with project-neutral objects and pinned external source/hash checks. This is host-only SAP evidence. DIN/ISO-2 backends, independent golden vectors, schema provenance/legal acceptance and target integration remain open.
