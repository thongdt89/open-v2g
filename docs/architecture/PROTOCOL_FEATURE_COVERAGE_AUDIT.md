# Protocol-feature coverage audit

Revision: Draft 0.1 — 26/10/2026. Inspected project commit: `8bc49b2`.
Result: **the nominal closed DC EIM host sequence is implemented; complete
DIN SPEC 70121 / ISO 15118-2 feature coverage is not established**.
This is a source/test/profile audit, not a standards compliance certification.
No licensed normative edition/corrigenda package is available for a complete
requirement denominator, so no percentage of standards coverage is reported.

## 1. Coverage classification

- **HOST WIRE**: mapped to generated EXI and exercised over the Linux socket path.
- **PARTIAL**: semantic behavior, codec support or only a restricted wire profile;
  some relevant variants/fields/integration remain absent.
- **DISABLED / ABSENT**: no active project implementation for that feature.
- **UNVERIFIED**: the required independent/target/standards evidence is absent.

A generated library containing a datatype is not evidence that the project maps,
validates, selects, exercises or supports that feature at runtime.

## 2. Feature matrix

| Protocol feature | Semantic / codec status | Active socket behavior and evidence |
|---|---|---|
| DIN DC EIM nominal sequence | HOST WIRE | Both generated directions; setup through protocol stop, three demand loops; `openv2g_test_linux_wire` |
| ISO-2 DC EIM nominal sequence | HOST WIRE | Same closed sequence, with ISO-2 service/auth names and identity/schedule checks |
| SAP request/response EXI | HOST WIRE, bounded profile | Explicit wire-ID map and exact offered selection; 5 offers / 99 namespace text bytes maximum |
| SAP multi-offer negotiation | PARTIAL | Semantic negotiator and bounded codec can hold multiple offers; full wire runner enables one protocol per run and the station requires exactly one offer |
| SAP minor-version-deviation result | PARTIAL | SAP codec supports it; wire client accepts only `OPENV2G_EXI_SAP_SUCCESS`, rejecting minor-deviation success |
| SessionSetup new session | HOST WIRE | Nonzero assigned SessionID, response-kind and subsequent session matching |
| Existing-session join / resume | ABSENT in wire profile | Mapper does not mark `OK_OldSessionJoined` as accepted; client requires `new_session` and stores no resumable session lifecycle |
| ServiceDiscovery | PARTIAL | One DC-core charging service, EIM/external payment; additional services and multiple payment/mode choices rejected by profile |
| ServiceDetail | ABSENT | No project semantic message kind/FSM step or generated DTO mapping |
| PaymentServiceSelection / DIN ServicePaymentSelection | PARTIAL | One selected service; no ParameterSetID; external payment only |
| EIM Authorization / DIN ContractAuthentication | PARTIAL | Basic success plus selected Ongoing enum handling; no challenge/signature credentials; ISO-2 waiting-for-customer-interaction variant rejected |
| Plug-and-Charge / PaymentDetails | DISABLED / ABSENT | PnC gate is 0; no PaymentDetails, certificate or cryptographic workflow in active project path |
| ChargeParameterDiscovery | PARTIAL | DC core only; one schedule tuple with one flat relative interval; zero minimum voltage/current required by profile |
| CPD Ongoing without a schedule | NOT COVERED by project decoder | Raw generated encoder can produce the inspected example; project decoder rejects it; confirmed by an isolated audit probe |
| Schedule selection / limits | PARTIAL | One tuple ID, flat power and duration, maxima intersection, duration expiry; no multiple tuples/intervals, tariffs or richer schedule selection |
| CableCheck | PARTIAL | Mapped status/isolation and Finished/Ongoing flags; station nominal case always finishes; real isolation/vehicle procedure not integrated |
| PreCharge | PARTIAL, behavioral gap | Target and present values mapped, but runner does not use present voltage to qualify readiness before PowerDelivery/enable; zero-voltage probe completed the nominal host session |
| PowerDelivery START / STOP | HOST WIRE, restricted profile | Permission-gated host enable, coordinator-owned revoke/safe-state phase, bounded protocol tail; no charging profile or renegotiation |
| CurrentDemand loop | PARTIAL | Three-loop local application budget; values/identity/schedule checked; dynamic limits and limit-achieved flags are rejected instead of handled |
| ChargingComplete / richer loop policy | PARTIAL / missing projection | No neutral ChargingComplete field and no battery-completion/long-duration policy in the wire runner; loop count supplies local stop policy |
| MeterInfo / MeteringReceipt | PARTIAL semantic only | ISO-2 semantic token/unsigned receipt loop is unit-tested; wire mapper rejects meter info/receipt requests and has no MeteringReceipt DTO mapping |
| Signed metering | DISABLED / ABSENT | Semantic signature-required path rejects; no actual signing/verification backend |
| WeldingDetection | PARTIAL, behavioral gap | Present voltage is decoded but not consumed by a vehicle welding assessment; nominal station uses default 400000 mV without an assessment failure |
| SessionStop terminate | HOST WIRE | Nominal stopping sequence and final teardown tested |
| SessionStop pause | NOT IMPLEMENTED; discriminator lost | ISO-2 decoder accepts a Pause request as the same neutral SessionStop kind, without projecting ChargingSession; isolated probe confirms distinction is discarded |
| Renegotiation | ABSENT in active path | `PAD-IF-003`, `HSA-IF-003` / `CC-EXT-017` intent exists; no active port operation or wire ChargeProgress Renegotiate support |
| ISO-2 AC EIM / ChargingStatus | DISABLED, partial semantic branches | AC feature gate is 0; semantic branches/data-flow rules exist, but no active AC EXI/socket DTO path or full AC session evidence |
| ISO 15118-20 Common/AC/DC | DISABLED / ABSENT | Gate is 0, lifecycle placeholders only; factory/backend build selects DIN/ISO-2, not ISO-20 |
| AC/DC BPT | DISABLED | Guard tests do not establish protocol, converter or vehicle BPT functionality |
| Real TLS / PKI / certificates | ABSENT in socket runner | Callback/lifecycle/no-downgrade host tests exist; sockets are plaintext and crypto returns unavailable |
| PLC/SLAC and UDP/IPv6 SDP | SIMULATED / injected | Readiness/endpoint injections only in runner; actual sockets use IPv4 loopback |
| Edition-specific timers / retries / processing budgets | PARTIAL / UNVERIFIED | Semantic timeout hooks and a fixed 500-ms host exchange deadline; no edition-derived per-state policy, full retry/backoff or processing budget matrix |
| Detailed response-code policy | PARTIAL | Mainly OK/new-session versus rejected boolean; no complete edition-specific mapping/recovery/diagnostic policy |
| Actual AUTOSAR RTE/BSW / target resources | UNVERIFIED / blocked | Host ports and stubs do not replace supplier interfaces, target scheduling, WCET or physical integration |
| Physical EVSE interoperability / full conformance | UNVERIFIED | Same-profile station and separate-codec byte comparisons do not establish these |

## 3. Message inventory: what reaches the project mapper

For each pinned DIN and ISO-2 generated `BodyType`, inspection found **35 body
alternatives**, including generic `BodyElement`. The project decoder maps **22
named request/response alternatives** per schema, representing **11 message
families**. The application describes **12 logical kinds** because PowerDelivery
START and STOP share the same standardized request/response family.

Mapped families, with both request and response:

1. SessionSetup.
2. ServiceDiscovery.
3. DIN ServicePaymentSelection / ISO-2 PaymentServiceSelection.
4. DIN ContractAuthentication / ISO-2 Authorization.
5. ChargeParameterDiscovery.
6. CableCheck.
7. PreCharge.
8. PowerDelivery (START/STOP share the generated family).
9. CurrentDemand.
10. WeldingDetection.
11. SessionStop.

Not mapped: ServiceDetail, PaymentDetails, CertificateInstallation,
CertificateUpdate, ChargingStatus and MeteringReceipt (request/response for each),
plus generic BodyElement. Some are intentionally outside the active release
profile. Counting these declarations is a structural inventory, **not** a
standards feature-completeness or test-coverage percentage.

## 4. Concrete audit observations

### A. Physical-readiness evidence is not evaluated by the runner

`client.c` checks `ready` / `isolation_valid` for cable/precharge/power/current
responses, then progresses or enables host hardware. It does not compare the
received precharge voltage to the requested/measured vehicle condition.
Welding response voltage is likewise decoded without a welding assessment.

Reproduction: an isolated temporary station variant changed only the PreCharge
response's `voltage_mv` to 0, preserving nominal status fields. The unmodified
client completed all 14 protocol exchanges, all three CurrentDemand loops and
host energy enable. The original station already returns 400000 mV in its
WeldingDetection default response; the normal session succeeds.

This is a confirmed **missing host/vehicle assessment**, not proof of an actual
vehicle safety failure: hardware/plant and safety criteria are explicitly mocks
or unavailable. Do not invent a voltage tolerance, welding threshold or deadline
to close it. Before expanding beyond the simulation, add explicit qualified
readiness/assessment evidence and tests using reviewed vehicle criteria.

Source: `simulation/linux/wire/client.c:174–208`,
`integration/protocol/adapters/cbv2g/exi_cbv2g_dc_defaults.c`,
`integration/protocol/adapters/cbv2g/exi_cbv2g_iso2.c` PreCharge/Welding mappings.

### B. Pending-response support is narrower than the semantic state machines

Both semantic adapters can repeat a request on `ongoing`. The CPD decoder,
however, requires DC parameters, one schedule tuple and one schedule interval
regardless of the processing flag. An example with processing Ongoing and no
schedule was successfully emitted by the pinned raw ISO-2 generated encoder,
then rejected by the project decoder. Also, the runner requires ready/valid
isolation before handling a cable-check Ongoing response.

This establishes a project/generated-model coverage mismatch, not a normative
claim that every such response is valid in the selected standard edition. Add
edition-reviewed pending variants and wire-level waiting/timeout tests.

Source: ISO-2 mapper CPD branch, `client.c` readiness guard, both adapters'
`ongoing` branches. Default station does not exercise these peer variants.

### C. Pause/resume is neither implemented nor explicitly represented

The ISO-2 request decoder recognizes SessionStopReq but does not inspect or
project its ChargingSession value. A generated Pause request decoded
successfully to neutral SessionStop, indistinguishable from Terminate. New-session
join is also restricted: `OK_OldSessionJoined` decodes to `accepted=false`.

A terminate-only profile should explicitly reject Pause until retention/resume
semantics are implemented, rather than silently losing its distinction.

Source: ISO-2 mapper SessionStopReq branch and SessionSetupRes response mapping.

### D. Dynamic limits, richer schedules and receipts stop at the boundary

The CurrentDemand response mapper rejects optional new maximum limits,
limit-achieved indicators, MeterInfo and required receipts. The CPD profile
requires one flat schedule and zero minimum voltage/current. A CurrentDemand
response with an updated current maximum encoded in the raw generated model
was rejected by the project decoder. This is fail-closed, but it is a clear
interoperability limitation, not support for adaptive charger behavior.

Unsigned metering semantic tests do not close the wire/signature gap.

## 5. Executed checks and evidence limits

Reran the candidate build and current full suite on 26/10/2026: **24/24 pass**,
including the separately pinned OpenV2G differential oracle. The 50 differential
examples and the two normal socket sessions remain valuable host evidence.

Five isolated raw-generated-code probes produced the following observations
(`0` = project codec OK, `3` = rejected):

| Probe | Raw generated encoder | Project result |
|---|---|---|
| ISO-2 CPD Ongoing with schedule omitted | OK | Decode rejected (`3`) |
| ISO-2 SessionStop Pause | OK | Decode OK (`0`), SessionStop kind, no pause field |
| ISO-2 CurrentDemand with an updated maximum current | OK | Decode rejected (`3`) |
| ISO-2 PreCharge with zero present voltage | OK | Decode OK (`0`), accepted, voltage preserved as 0 |
| ISO-2 SessionSetup OldSessionJoined | OK | Decode OK (`0`), `accepted=false` |

The additional temporary socket probe confirmed zero-precharge-voltage progression
as described above. These isolated probes were audit drivers, **not new permanent
regression cases**; application behavior has not been changed by this audit.
No new sanitizer or coverage-instrumentation run was needed for this read-only
implementation audit. Earlier sanitizer evidence is recorded separately.

Important test gaps:

- Socket tests execute 12 fault scenarios for ISO-2, but only the normal DIN
  socket session. Core DIN unit negatives do not equal DIN wire fault coverage.
- The nominal station finishes authorization, CPD and cable check immediately;
  it does not exercise the full Ongoing/customer-waiting/processing matrix.
- Generic DC round trips assert kind/direction and truncation behavior; the
  differential references also originate from candidate-constructed DTOs.
  Neither verifies every supported field/constraint or independent vehicle policy.
- Random malformed tests use expected SessionSetup kind for DC. The corpus
  cannot be presented as exhaustive body-choice/field/branch coverage.
- There is no coverage instrumentation report, independent normative message
  corpus or selected-edition conformance denominator.

## 6. Recommended completion order

1. Make physical readiness/welding and pause handling explicit: assessment
   evidence must be consumed or unsupported behavior rejected; add focused
   wire regressions. Vehicle thresholds remain OP-005/OP-006 inputs.
2. Cover EIM pending variants, SAP minor-deviation policy and exact per-state
   response/timer/retry behavior; run negative wire scenarios for both schemas.
3. Add peer minimum/dynamic limit handling, multiple schedules and completion
   policy; maintain bounded capacities and explicit limit intersections.
4. Add ServiceDetail and MeterInfo/MeteringReceipt wire mapping when needed by
   the selected release. Keep unsigned mock tokens distinct from signed evidence.
5. Implement session pause/resume and renegotiation as separate reviewed
   lifecycles, not just decoder enum acceptance.
6. Integrate actual UDP/IPv6 SDP, PLC and TLS/PKI when supplier/platform inputs
   exist. AC, PnC, ISO-20 and BPT remain separate release work.
7. Expand independent vectors/interoperability and instrument coverage after an
   exact-edition compliance matrix is selected; complete target qualification.

Sources: Atomic Rev.3 §§18–23 and acceptance/open items; feature gates; project
FSMs, generated-code mappers, runner and tests. No imported ABI, safety allocation,
standard timer or external approval has been invented or changed.
