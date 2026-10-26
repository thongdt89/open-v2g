# Feature completion work log

Started 26/10/2026 after coverage audit `0b79f7e`. Status: IN PROGRESS.
User scope: implement all missing features identified by the protocol audit.

Current inventory: [INCOMPLETE_ITEMS.md](INCOMPLETE_ITEMS.md). Historical rows
below retain the scope and evidence of each earlier increment.

## Implementation work streams

- [ ] Explicit correlated/fresh physical-readiness and welding assessments; no invented vehicle threshold.
- [ ] Pending/Ongoing/customer-waiting behavior, SAP minor-deviation policy and multi-offer negotiation.
- [ ] Bounded multiple services, parameters, schedules, minimum/dynamic limits and completion policy.
- [ ] ServiceDetail, metering/receipt wire mapping; signed evidence separate from mock tokens.
- [ ] Explicit pause/resume retention and renegotiation lifecycles.
- [x] Gated single-phase AC EIM host wire path and supporting permission/port model; broader AC remains open.
- [ ] Real host UDP/IPv6 SDP and optional TLS/crypto/credential services.
- [ ] Evaluate ISO-20/PnC/BPT code against available pinned models and capability/evidence gates.
- [ ] Permanent wire/negative/sanitizer/independent-codec regressions and coverage instrumentation.
- [ ] Update scope/evidence/architecture docs and commit all completed work.

## External inputs remain required

Exact licensed editions/schema/generator provenance, supplier ARXML/BSW/modem,
OEM credentials, HARA/topology/approved thresholds, target toolchain and physical
interoperability/conformance evidence are absent. Host work must not imply these
are supplied, approved or frozen. Feature gates remain fail-closed until their
required capability/evidence is present.

## Implemented increment — 26/10/2026

The overall request remains **IN PROGRESS**, not fully implemented. This increment
closes specific host-profile gaps; unchecked streams above are not external
blockers merely because they are unfinished.

| Feature | Implemented behavior | Verification scope |
|---|---|---|
| Physical assessment port | Coordinator stores generation/freshness/correlation/hardware-sequence checked assessment; opt-in start guard; new generations clear evidence; wire client requires assessment | Reject stale/wrong-generation/correlation/hardware-sequence evidence and explicit failed assessment; zero precharge and nonzero welding peer voltage socket negatives |
| Host physical criteria | Precharge requires nonzero target, equal peer voltage and READY hardware; welding requires zero peer and measured voltage/current, SAFE hardware and open contactors | Deterministic surrounding fixture only. Precharge does not establish independently measured inlet-to-battery voltage tolerance. Vehicle assessment provider/thresholds remain open |
| Pending CPD | Separate DIN empty abstract alternatives and ISO-2 omitted alternatives accepted during Ongoing; Finished requires concrete parameters/schedule | Both generated round trips and repeated CPD socket exchanges; finite session/request budgets are host settings, not normative timers |
| SAP minor deviation | Explicit host compatibility policy, still validates selected wire ID against recorded offer map | Both protocol socket sessions; multi-offer runner coverage remains open |
| ServiceDetail | New project message kind 15, optional semantic owner route, real request/response EXI mapping; runner requests detail after discovery and validates ServiceID | Both codecs/socket sessions; at most five parameter sets with one integer/string parameter each; other parameter types/multiple parameters per set rejected |
| Minimum/dynamic limits | Preserve nonzero minimum voltage/current; reject incompatible minima; retain and intersect dynamic maxima; clamp subsequent current against current/power limits; zero dynamic maxima stop | Both codec tests; station verifies subsequent DIN/ISO-2 current requests respect a lowered 10-kW/30-A limit |
| Schedules | At most three tuples/five contiguous intervals per tuple; reject duplicate IDs, zero duration, gaps and uint32 overflow; retain all tuples | Both codec round trips and invalid input tests; host selects first tuple and conservative minimum power across duration; tariffs/absolute intervals remain rejected |
| Completion and session fields | CurrentDemand ChargingComplete preserved in both schemas; ISO-2 Pause, OldSessionJoined and customer-interaction waiting preserved distinctly | Mapper round trips only; socket runner still starts new sessions, uses local three-loop stop and has no pause retention/resume or renegotiation lifecycle |
| Broader negative tests | Common DIN negatives added, plus physical mismatch negatives for both protocols | DIN excludes ISO-2-only schedule/EVSE checks and the existing DIN-encoded wrong-schema injection |
| Coverage | Opt-in GCC/gcov instrumentation plus JSON collection script | Executed host coverage is line/branch evidence, not feature, safety or normative coverage |

Existing 50 saved differential vectors remain historical and unchanged. Their
WeldingDetection fixture explicitly retains its original 400-V encoding for
byte-regression comparison; current station welding evidence is zero volts.
New feature examples are round-trip/socket tests, not independently authored
standards vectors or newly expanded differential fixtures.

## Remaining implementation, distinct from unavailable external evidence

1. Multiple service/payment/mode offers, broader ServiceDetail parameter forms,
   charging profiles/tariffs, EVSE notifications and richer completion policy.
2. Wire MeterInfo/MeteringReceipt, signed receipt validation and complete PnC
   certificate/payment/authorization lifecycle.
3. Pause retention/resume, fresh evidence/permission on renegotiation, and per-state
   configurable cumulative processing budgets; persistent sessions need surrounding
   link/hardware measurement providers rather than restamping cached data.
4. AC wire charging, ISO-20 Common/AC/DC and BPT protocol adapters with explicit
   capability gates, separate generated schemas and coordinated vehicle permission.
5. Actual IPv6 UDP SDP, TLS and cryptographic/credential service providers, PLC/modem
   integration and selected AUTOSAR supplier ABI.
6. Broader independent vectors, fuzz/coverage-driven cases, target resources,
   conformance and physical interoperability.

Items 1–5 contain substantial implementable host work; they are **not delivered**
by this increment. Production completion additionally requires the external
artifacts listed above. No disabled feature has been represented as implemented.

## Executed checks for this increment

- GCC strict normal EXI build: **24/24 CTest checks passed**, including the separate
  pinned oracle and unchanged 50-reference regression.
- ASan/UBSan build: **23/23 passed**, including the generated code and socket cases.
- Default generated-code-OFF build: **19/19 passed**.
- The wire test now exercises **33 two-process sessions**: successful nominal,
  SAP minor-deviation, pending-CPD and dynamic-limit sessions for both protocols;
  11 DIN and 14 ISO-2 negative scenarios. Success ends with DENY and drained queues.
- New feature vectors are project round trips; independent reference corpus
  expansion remains open. No standards/target conformance result is asserted.
- Clean coverage build: **23/23 passed**. [Coverage counts and source hashes](evidence/feature_increment_coverage.json)
  retain the measured source snapshot. Portable core: 2904/3357 executable lines,
  1504/2592 branches; project EXI adapters: 969/979 lines, 805/1244 branches;
  generated codec: 10343/32715 lines, 4602/19824 branches. These are exercised
  code counts, not percentages of supported protocol features.

## IPv6 platform increment — 26/10/2026

Added a real, nonblocking IPv6 UDP SDP provider and server fixture under
`simulation/linux/src/sdp6.c`, plus IPv6 loopback TCP helpers. Jobs retain owner
generation/request/deadline, validate exact packet shape/security/source and
publish only current responses. Tests discover a real TCP listener and exchange
data, plus reject malformed packets, downgrade/mismatch, expired and stale jobs.
See [provider details and source reference](../../simulation/linux/SDP_IPV6.md).

The provider is integrated into `station6`/`evcc6`: real discovery precedes
IPv6 TCP and SAP/EXI for both protocols. Multicast/link-local discovery, retry
policy and TLS remain open. SDP's TLS bit is not proof
of cryptography or a negotiated TLS version. The original coverage JSON above
records the earlier increment and excludes this new network provider.

IPv6 increment verification: normal suite **25/25**, ASan/UBSan suite **24/24**,
default-codec-OFF suite **20/20** passed. Final focused discovery/wire checks were
also rerun after timeout/completion cleanup changes. Wire regression now includes
**35** two-process sessions (two additional full IPv6 discovery sessions).
Actual `station6`/`evcc6` CLI smoke runs for DIN and ISO-2 each exited 0 at both
ends and completed 15 protocol exchanges. This is host evidence only.


## AC EIM and metering increment — 26/10/2026

Implemented a gated single-phase ISO-2 AC path, both legacy IPv4 and discovered
IPv6: AC parameter discovery, PowerDelivery, ChargingStatus, controlled stop and
SessionStop. Permission and simulated hardware each require explicit AC
capability; prepared/permission modes must match. AC has a separate host
readiness assessment; DC precharge evidence cannot satisfy its assessment kind.
RCD and mode-mismatch tests stop with DENY and simulated hardware safe.

Implemented bounded ISO-2 MeterInfo and MeteringReceipt wire mapping for AC/DC.
The retained meter record is tied to generation/source correlation and consumed
once. `allow_unsigned_receipts` is false by default; positive unsigned fixture
tests opt in explicitly. Receipt policy rejection and unverified meter signature
negatives stop after transfer began. No signed-receipt/PnC completion is claimed.

Added integer boundary/truncation tests and 19 additional independently
round-tripped AC/metering differential vectors in `tests/oracle/feature_vectors.json`.
The original 50 references remain unchanged. New profile names in the test
corpus map explicitly to the same ISO-2 oracle/schema, not new wire schema IDs.
See the current inventory for all remaining implementation and external gates.

Verification for the AC/metering increment: **28/28** normal, **26/26** sanitizer,
**20/20** default-codec-OFF and **26/26** clean coverage checks passed. The wire
regression now has **43** sessions. Real IPv4 and IPv6 `iso2-ac` CLI pairs each
completed 12 exchanges and exited 0. See [coverage/source hashes](evidence/ac_metering_coverage.json).
