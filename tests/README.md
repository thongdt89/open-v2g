# Host/SIL tests

The tests in this directory exercise provisional logical behavior and hard architecture invariants. They use no supplier, AUTOSAR, network, crypto, PLC or actuator implementation.

Evidence classification: `VERIFIED_HOST` only.

Covered high-risk host cases now include stale generations, idempotent stop,
SDP cancel/late response, TCP/TLS callback state, TLS no-downgrade, bounded HLC
event overflow/status resynchronization, V2GTP fragmentation/coalescing/overflow,
EXI/schema adapter bounds and no-partial-delivery, permission expiry and BPT
reversal guards, correlated SAP selection, DIN-over-TLS filtering, one-active
adapter ownership, DIN/ISO-2 DC semantic sequences and protocol timeout/sequence
failures. The EXI test backend is deterministic test code, not a protocol
codec or conformance oracle.

`openv2g_test_asr_style_ports` verifies provisional C/S and S/R bindings across
coordinator internals, CP/PP, energy, PLC, hardware, SDP, TCP/TLS, V2GTP and the
HLC façade. Passing it proves host connection semantics only, not generated RTE.

Remaining extensions requiring more implementation or G2/G3 inputs:

- cancel during active TCP/TLS I/O and reordered callbacks after reconnect;
- SoAd/TcpIp/PduR adapter mapping for the tested V2GTP stream input;
- malformed EXI and unoffered schema using licensed/generated backends and golden vectors;
- edition-specific DIN/ISO-2 response-code, timer and generated-message mapping;
- permission expiry during every energy-related state;
- hardware command/result mismatch;
- BPT capability/limit integration while the feature remains disabled.
