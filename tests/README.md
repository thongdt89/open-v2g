# Host/SIL tests

The tests in this directory exercise provisional logical behavior and hard architecture invariants. They use no supplier, AUTOSAR, network, crypto, PLC or actuator implementation.

Evidence classification: `VERIFIED_HOST` only.

The `.github/workflows/host-sil.yml` workflow performs a clean CMake build and
runs the full suite on Windows and Linux with compiler warnings treated as
errors. A passing workflow remains host/SIL evidence only; it does not establish
target compiler, generated RTE, timing, safety or protocol-conformance evidence.

Covered high-risk host cases now include stale generations, idempotent stop,
SDP cancel/late response, TCP/TLS cancel/reconnect with connection-correlated
callbacks and RX, TLS no-downgrade, bounded HLC
event overflow/status resynchronization, V2GTP fragmentation/coalescing/overflow,
EXI/schema adapter bounds and no-partial-delivery, permission expiry and BPT
reversal guards, correlated SAP selection, DIN-over-TLS filtering, one-active
adapter ownership, outbound DIN/ISO-2 DC semantic sequences, exact pending
correlation, energy/permission gates, current-demand/metering loops and retained
protocol diagnostics. The EXI test backend is deterministic test code, not a protocol
codec or conformance oracle.

`openv2g_test_protocol_data_flow` verifies the shared DIN/ISO-2 flow rules,
required energy-field availability, target/permission limit intersection,
target-power projection, correlated metering evidence and controlled-stop action.

`openv2g_test_asr_style_ports` verifies provisional C/S and S/R bindings across
coordinator internals, CP/PP, energy, PLC, hardware, SDP, TCP/TLS, V2GTP and the
HLC façade. Passing it proves host connection semantics only, not generated RTE.

Remaining extensions requiring more implementation or G2/G3 inputs:

- bounded TCP send/TX-result/backpressure and SoAd/TcpIp/PduR adapter mapping for the tested V2GTP stream input;
- malformed EXI and unoffered schema using licensed/generated backends and golden vectors;
- edition-specific DIN/ISO-2 response-code, timer and generated-message mapping;
- permission expiry is covered at the first dependent transition; a state-by-state expiry matrix remains;
- hardware command/result mismatch;
- BPT capability/limit integration while the feature remains disabled.
