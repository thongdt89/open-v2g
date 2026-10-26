# Host/SIL tests

The tests in this directory exercise provisional logical behavior and hard architecture invariants. They use no supplier, AUTOSAR, network, crypto, PLC or actuator implementation.

Evidence classification: `VERIFIED_HOST` only.

The `.github/workflows/host-sil.yml` workflow performs a clean CMake build and
runs the full suite on Windows and Linux with compiler warnings treated as
errors. A passing workflow remains host/SIL evidence only; it does not establish
target compiler, generated RTE, timing, safety or protocol-conformance evidence.

Covered high-risk host cases now include stale generations, idempotent stop,
SDP cancel/late response, TCP/TLS cancel/reconnect with connection-correlated
callbacks and RX, bounded TX queue/backpressure and request/result/deadline
correlation, TLS no-downgrade, bounded HLC
event overflow/status resynchronization, V2GTP fragmentation/coalescing/overflow,
EXI/schema adapter bounds and no-partial-delivery, state-by-state DIN/ISO-2 DC
permission expiry and BPT
reversal guards, hardware command/feedback correlation and measured contactor
mismatch, correlated SAP selection, DIN-over-TLS filtering, one-active
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

`openv2g_test_host_simulation` verifies the reusable host-only surrounding
component simulator. It injects CP/PP and energy samples through S/R providers,
drives PLC/SDP/TCP/SAP callbacks, accepts a correlated protocol request, reaches
energy transfer through the permission/hardware path, rejects a stale-generation
energy sample and checks revoke/safe-state/HLC/link stop ordering.

Remaining extensions requiring more implementation or G2/G3 inputs:

- generated SoAd/TcpIp/PduR adapter mapping for the host-tested bounded transport TX/RX seams;
- malformed EXI and unoffered schema using licensed/generated backends and golden vectors;
- edition-specific DIN/ISO-2 response-code, timer and generated-message mapping;
- BPT capability/limit integration while the feature remains disabled.

Linux simulation builds also register `openv2g_test_linux_bsw`, covering bounded OS/service stubs and local socket nominal/reject/stale/disconnect flows. See `simulation/linux/README.md` for the exact smoke scope. Socket tests require local network permissions.

With `OPENV2G_ENABLE_CBV2G_SAP_POC=ON` and a verified external checkout, `openv2g_test_cbv2g_sap` executes generated SAP EXI request/response encoding and decoding against fixed upstream vectors plus malformed/mutation corpora. These are host regression references from the same codec, not independent conformance vectors.
