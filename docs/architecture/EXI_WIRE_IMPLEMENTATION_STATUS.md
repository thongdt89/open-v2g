# EXI and Linux wire implementation status

Revision: Draft 0.1 — 26/10/2026. Classification: `VERIFIED_HOST` only.
This record distinguishes the requested implementation steps from external
approval/qualification gates. No open point is declared formally frozen.

## Delivered scope

| Requested step | Implemented host result | Evidence / remaining limits |
|---|---|---|
| Connect SAP to Linux sockets | Actual SAP EXI in V2GTP; explicit wire-ID offer map; local pending generation/connection/request context; HLC owner selection | Both schema session tests, unoffered/malformed/timeout/disconnect/replayed-SAP negatives. No UDP SDP or TLS |
| Separate DIN and ISO-2 codecs | Separate generated types and mapper translation units bound to explicit schemas; checked physical-value conversion and bounded neutral DTOs | 12 DC message kinds, both request/response, per-vector truncation tests, malformed corpus. Closed DC EIM profile, not complete schema feature coverage |
| Full DC host sequence | Setup -> service/EIM auth -> parameters -> cable/precharge -> power start -> three demand loops -> power stop/welding/session stop | Two-process loopback tests for DIN and ISO-2. Simulated energy/hardware, not vehicle or EVSE conformance |
| Continuous supervision | Copying bounded async TX, nonblocking I/O pump, correlated completion; RX through HLC connection/generation guard; coordinator tick before renewal, provider refresh while waiting | Stale-energy test stops after reaching host transfer while waiting for peer; permission expiry remains coordinator-owned. Poll settings are not timing/WCET evidence |
| Stronger verification and external closure | Fixed vector regression, separate-codec differential verification, sanitizer and malformed tests, pinned hashes, CI configuration; explicit closure inventory below | Verification implemented locally. Standards/supplier/legal/safety approvals and target measurements cannot be manufactured from host tests |

The coordinator owns a bounded protocol-stop phase: revoke first, request host
hardware safe state, retain communication for PowerDeliveryStop/WeldingDetection/
SessionStop, then perform final coordinated HLC/PLC teardown. Repeat stop is
idempotent and permission cannot be renewed in STOPPING. This is a host design
extension, not an approved electrical stopping sequence.

## Reproducible local evidence

Environment: Linux, GCC 13.3, CMake 3.28.3. Project code builds with
`-Wall -Wextra -Wpedantic -Werror`; pinned generated code has warnings enabled
without promoting them to project errors. Exact external revisions:

- libcbv2g: `03350be048b35b179905129005a97144a4bdcf93`.
- Independent OpenV2G test oracle: `733d4f43cb6579c705d59c7386786ce12a0a0026`.

Build/runtime instructions: [Linux wire sessions](../../simulation/linux/wire/README.md).
Independent oracle instructions: [test-only oracle](../../tests/oracle/README.md).

Results recorded on 26/10/2026:

- Full Debug build with oracle: **24/24** host test executables/checks pass.
- Candidate Debug build without external oracle: **23/23** tests pass with
  AddressSanitizer, UndefinedBehaviorSanitizer and LeakSanitizer enabled on
  application, adapters and the candidate generated source.
- Baseline build with the candidate disabled: **19/19** host tests pass.
- Fifty payloads (SAP 2, DIN 24, ISO-2 24) decode/re-encode byte-identically in
  the separate pinned OpenV2G implementation. Fixed references are stored at
  `tests/oracle/independent_vectors.json`.
- Malformed tests: all byte truncations of the SAP fixed vectors and of 48 DC
  request/response examples; expected-kind mismatch, incorrect padding/header,
  unsupported bounds; 288 SAP single-bit mutations, 5,000 SAP random inputs and
  10,000 random inputs per DC schema. No partial decoded output on failure.
- Socket fault scenarios cover unoffered SAP selection, malformed framing,
  timeout, disconnect, wrong SessionID, negative response, wrong message type,
  stale mock energy, replayed SAP, wrong schedule/EVSE identity and wrong-schema EXI during protocol messaging.
- Dependency configuration rejects a wrong candidate commit or modified selected
  artifact. Both candidate and oracle use checked source hash lists.
- CI jobs are configured for the full differential suite and sanitizer suite;
  **no remote CI execution result is claimed**.

The 50 differential examples originate from project DTOs, not an independently
authored standards corpus. The separate codec decodes and re-encodes them; its
output becomes a fixed regression reference. This is stronger than same-codec
round-trip testing, but it does not establish normative correctness or full
interoperability. OpenV2G's `iso1` model has the 2013 namespace used here; its
separate `iso2` model is not substituted merely because of its name.

Local socket and LeakSanitizer runs needed approved elevated execution because
the review sandbox restricts socket send and tracing. No runtime downloads or
remote service access are involved in those tests.

## Explicit external closure inventory

Suggested roles below come from the existing decision log; they are not assigned
owners or approvals. No new external requirement identifiers are created.

| Existing open point | New evidence | Still required to close / freeze |
|---|---|---|
| OP-001 / OP-002 / OP-003 | Host project DTOs, explicit ports and private generated-code adapters | Selected AUTOSAR release, supplier ARXML/headers, imported operation/enum/type mapping and reviewed generation outputs |
| OP-004 | Cooperative owner loop, bounded I/O and queue settings | Actual runnable/task priorities, exclusive areas, queue/freshness budgets and target timing analysis |
| OP-005 | Fail-closed permission, host stop and stale-evidence tests | Approved HARA/topology, safe state, ASIL allocation, electrical sequencing, reaction deadlines and target/HIL evidence |
| OP-006 | Concrete inspected schema grammar and namespace, response mapping for the closed profile | Licensed exact editions/corrigenda, compliance matrix, complete response-code/timer/retry policies and conformance suite |
| OP-007 | Pinned generated snapshot/hashes, three host backends, independent-codec references and sanitizer corpus | Original schema source/hash/license chain; full generator revision/config/output provenance; legal acceptance; target compiler/MISRA/deviations and ROM/RAM/stack/WCET; independent normative vectors and longer fuzz campaigns |
| OP-008 | Linux TCP transport and simulated PLC/readiness | Supplier modem/PEV raw-MME capability, UDP/IPv6 SDP, actual SoAd/TcpIp/PduR/TLS and ECU network configuration |
| OP-009 | Security remains unavailable / fail-closed | OEM PKI, trusted time, Csm/KeyM/HSM/NvM lifecycle and credential evidence |
| OP-010 | BPT remains disabled | Vehicle powertrain/topology, direction/zero-crossing policy and approved BPT scope |
| OP-011 | Reproducible host build, external commit pins, local test commands and evidence locations | Target repository/toolchain ownership and approved configuration-management baseline |

These inputs were not supplied in this repository. Continue host development
within the explicit profile, but do not treat upstream availability, a source
license notice, a host-safe hardware mock or passing tests as their approval.

## Next work requiring additional scope or inputs

The requested host implementation is now executable. Production progression
requires the external artifacts above. Additional independent host work can
expand the currently bounded service/schedule model, add UDP/IPv6 discovery,
exercise an independently implemented EVSE, add coverage-guided fuzzing and
measure host resources. Such expansion needs explicit profile/evidence updates;
it is not silently represented as already implemented by this record.

## Feature increment — 26/10/2026

Selected audit gaps now have host implementations: coordinator physical-assessment
port/gate, ServiceDetail, pending CPD, explicit SAP minor-deviation policy, bounded
multi-tuple/interval schedules, minimum/dynamic limits, and preservation of selected
ISO-2 session fields. Normal wire sessions now exchange 15 protocol messages.
See [the feature work log](FEATURE_COMPLETION_WORKLOG.md) for exact restrictions,
verification and remaining implementation. This supplement supersedes the old
single-flat-schedule/no-ServiceDetail scope above; the overall completion request
is still in progress. Production closure gates remain unchanged.
