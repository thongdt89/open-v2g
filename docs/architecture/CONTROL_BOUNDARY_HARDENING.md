# Coordinator control-boundary changes

Revision: Draft 0.4 — 02/11/2026. PROJECT host implementation, following
Atomic Rev.3 §§4, 5, 25 and 26. This revision changes C11 interfaces; it does not
change imported AUTOSAR datatypes or freeze supplier/vehicle contracts.

## Evidence ownership and ordering

Coordinator CP/PP, energy, PLC, HLC and hardware inputs reject unrelated
session generations and older timestamps/sequences. Repeated valid envelopes
are idempotent: their payload cannot replace the retained sample or extend its
freshness. Sequence zero supports legacy timestamp-only producers. A nonzero
sequence must increase for new samples; wrap/reset requires a new generation.
Same-tick samples therefore need distinct sequences. The wire energy provider
now supplies them for periodic and response-driven derating samples.

A current-source invalid-quality/zero-timestamp update removes usable evidence
and immediately revokes permission. During ready/transfer states it also begins
coordinated stopping. Old-generation invalidation cannot stop a new session.
Energy interlock loss or missing INTERLOCK/LIMITS availability also revokes.
A mismatched current PLC link generation invalidates its evidence. CP/energy
publishers propagate invalid quality rather than hiding it behind an old valid
S/R sample; publication success describes delivery, not evidence qualification.

The no-`now` ingress signatures remain serialized owner operations. The last
owner time is maintained by CP/PP, evaluation, stop and Tick calls. Invalidation
uses that clock rather than trusting an incoming timestamp for reaction timing.
Envelope freshness and future timestamps are checked by permission evaluation;
this is not a concurrent BSW/ISR ingress implementation or a target clock binding.
Callbacks must still copy/notify a serialized owner. Actual receiver-time APIs,
queue deployment and target watchdog behavior remain integration work.

## Permission actions

INTERLOCK and LIMITS availability are required in addition to the existing
freshness, generation, schema/mode, link, interlock and fault checks. With
`require_physical_assessment`, PermissionManager checks the assessment kind,
quality, generation, correlation, age and hardware sequence before publishing
ENERGIZE. Missing readiness during negotiation permits PREPARE only. The wire
runner reevaluates after applying an assessment, before advancing to START.

A start assessment records readiness before energization. During transfer it is
historical start evidence; current fresh ENERGIZED hardware evidence supervises
continuation. It is not required to keep the pre-start sequence/age after enable
changes hardware state. A newly failed assessment revokes permission. The final
hardware permission gate remains in place.

Legacy semantic fixtures can still disable the assessment requirement explicitly;
the real EXI wire fixture enables it. Explicit protocol authorization/selected
service evidence and independent vehicle/cable/charger/schedule limit ownership
remain open. This increment does not claim the full readiness finding closed.

`EvaluatePermission` enforces expiry through coordinator Tick before renewal.
An already expired ALLOW initiates stopping and cannot be renewed by changing
scheduler call order. Direct internal PermissionManager calls are not the public
coordinator lifecycle entry point and cannot perform hardware teardown.

## Bounded stop transaction

`CoordinatorConfig.stop_timeout_us` is now required and nonzero. Host defaults
and the test rig use 2,000,000 microseconds; boundary tests use 100. These are
provisional host budgets, not protocol timer values or vehicle safety deadlines.

| Owner operation | Resulting contract |
|---|---|
| `BeginProtocolStop(now)` | DENY first, request safe hardware once, retain HLC/PLC for the protocol tail, arm the owner deadline |
| `Stop(now, reason)` | DENY first, retain the hardware result, then request HLC/PLC teardown; repeated calls preserve the result and do not repeat physical commands |
| `OnHardwareStatus(status, now)` | Qualify measured SAFE, open contactors, inactive precharge and zero current using a newer hardware sequence after the command and before its deadline |
| `OnHlcStatus(status, now)` | An accepted asynchronous stop is completed by current IDLE evidence |
| `OnLinkStatus(status, now)` | An accepted asynchronous stop is completed by current DOWN evidence |
| `Tick(now)` | Force protocol-tail teardown at the deadline; missing physical confirmation latches a timeout fault, as does incomplete asynchronous teardown |
| `RestartConnected(now)` | Explicitly restart from connected-safe completion with fresh CP/hardware and no latched fault; create a new generation and clear old usable session evidence |

An OK HLC/link stop result means synchronous completion in the PROJECT port
contract. ACCEPTED is pending until authoritative status confirms completion.
Hardware command OK/ACCEPTED never substitutes for measured safe feedback.
BUSY/rejection is retained; there is no invented actuator retry/emergency-action
policy. Failure remains denied and escalates diagnostically at the deadline.

After measured safety and service teardown, Tick dispatches connected-safe or
unplugged completion. Unplug while stopping cannot erase the pending transaction
before measured confirmation. A connected restart is explicit; it does not reset
latched faults, retain a protocol session, or implement ISO-2 Pause/resume.

Vehicle-specific safety thresholds, physical fallback/retry sequencing, actual
hardware command/result correlation, fault clear/recovery policy and an
independent watchdog if the owner runnable stops executing remain external gates.
These host observations do not establish electrical safety.

## Verification

`tests/integration/test_control_boundaries.c` contains acceptance regressions for
old-generation invalidation, reordered lower limits, duplicate samples, each
current-source invalidation, missing availability, readiness before START,
historical start evidence during transfer, failed assessment, expiry without a
prior Tick, stalled protocol stop, measured completion, connected restart,
unplug during stop and retained safe-state rejection/timeout.

The earlier review probes under `reviews/` deliberately assert historical gaps.
They remain unchanged as review evidence; this new regression checks desired
behavior. Broader independent limit ownership, portable session mapping,
protocol readiness and deployment findings remain open.

Observed final checks: **34/34** normal generated-code/OpenSSL/oracle tests,
**32/32** ASan/UBSan generated-code/OpenSSL tests and **22/22** default-build
tests passed. [Verification snapshot and source hashes](evidence/control_boundaries_verification.json).
These counts supersede earlier counts only for this host increment. The prior
security coverage snapshot remains tied to its own recorded source hashes.


## Review fixes: overflow and owner time — 31/10/2026

`MarkHlcEventOverflow(generation, now)` revokes permission first and routes READY
or ENERGY_TRANSFER (including renegotiation) through the existing idempotent Stop.
It keeps the resynchronization requirement and first overflow watermark until a
fresh authoritative HLC status arrives. Resynchronization cannot cancel a stop.
Repeated overflow preserves the stop result, original deadline and command count.
Existing hardware acceptance/rejection and measured-confirmation rules apply.
No actuator retry or new vehicle deadline is introduced.

The three PROJECT ingress operations now take an explicit `OpenV2g_TimeUs now`
from the serialized owner dispatch/receipt clock. It is separate from the producer
`env.timestamp`; imported AUTOSAR types and envelope layouts are unchanged.

| Operation | Trusted input and ownership | Evidence and failure contract |
|---|---|---|
| `OnHlcStatus(coordinator, hlc, now)` | Owner supplies monotonic receipt time; HLC supplies copied source evidence | Source future/zero/stale/invalid quality fails closed. ERROR reactions use `now`; no source time sets a deadline. |
| `OnLinkStatus(coordinator, link, now)` | Owner supplies receipt time; PLC owns source state/time | Same time validation; link-generation mismatch invalidates evidence. HLC start and DOWN reactions use `now`. |
| `OnHardwareStatus(coordinator, hardware, now)` | Owner supplies receipt time; hardware owns measured evidence | Same validation; FAULT reactions use `now`. Safe/renegotiation proof must be sourced and received before the existing deadline. |
| `HostSimulation_InjectHardwareFeedback(simulation, feedback, now)` | Simulator caller supplies its explicit owner clock | Measured producer timestamp does not become coordinator receipt time. |

All four are serialized nonqueued PROJECT calls with copied bounded payloads;
`now` is an input, not a producer-supplied envelope field. Zero/rewinding receipt
time is rejected without state/clock change. Unrelated generation is rejected
before adopting receipt time. Current-generation invalid evidence removes its
usable flag and revokes permission; in active charging it starts supervised stop.
Invalid producer timestamps/sequences are not retained as ordering watermarks.
Older/reordered normal samples are rejected; fault indications in reordered or
duplicate envelopes fail closed. A valid duplicate cannot refresh source age.
There is no invented future-clock tolerance: source time must be at most `now`
and within the existing configured source age.

Stop rejects zero/rewinding owner time and never rearms an existing transaction.
Late measured-safe or asynchronous teardown callbacks cannot backdate completion
past a deadline by carrying an earlier source timestamp. Tick uses the owner clock
and the existing configured timeout escalation. Fresh later status may restore a
source's evidence, but STOPPING/teardown guards prevent renewed energy transfer.

Desired-behavior regression: `openv2g_test_coordinator_fault_ingress`, including
both active states, safe-request OK/ACCEPTED/BUSY/REJECTED, revoke-before-command,
missing resync/measurement, future/zero/invalid/stale/reordered/duplicate faults on
all three paths, generation isolation, repeated-fault deadlines, clock rewind,
and late measured/teardown evidence. Dated historical review probes are preserved.


## Review follow-ups: current stop proof and CP/PP recovery — 31/10/2026

TASK-3: Tick now requalifies retained measured-safe proof using hardware freshness,
generation, post-command sequence/time, SAFE/open contactors, inactive precharge
and zero measured current. Expiry clears the retained proof flag. A new qualified
sample can restore proof before the original deadline; no new command or deadline
is issued. Completion and timeout use the same current prerequisites. If CP/PP,
measurement or service completion prevents the stop from completing at the existing
deadline, the existing timeout/fault path runs. Hardware proof loss uses the
existing missing-measurement escalation; missing other completion evidence uses
the existing incomplete-stop escalation. No actuator retry policy is introduced.
Late evidence cannot complete a fault-latched stop or authorize transfer.

TASK-4: Invalid current-generation CP/PP data clears usability and revokes before
coordinated stopping, while retaining the last accepted snapshot as the ordering
watermark. Future, zero, stale, invalid-quality and invalid-semantic samples cannot
poison the accepted timestamp/sequence. Valid newer connected or unplug samples
can restore source evidence, while STOPPING still denies transfer. Valid reordered
samples and same-sequence mutations remain rejected, including before the unplug
branch. Unrelated active generations are rejected before advancing owner time.
Initial connection retains the existing owner-assigned session-generation behavior.

`openv2g_test_coordinator_fault_ingress` now includes `stop_evidence_expiry` and
`cp_recovery`. They cover the two additional dated review reproductions with
desired behavior, short synthetic evidence ages, missing/invalid CP/PP and hardware,
fresh normal completion, pre-deadline proof recovery, late evidence after fault,
connected/unplug recovery after six invalid CP/PP forms, accepted-sample ordering
and generation isolation. Historical probes and observations remain unchanged.


## Additional findings: TASK-5 through TASK-7 — 2 November 2026

TASK-5: Permission evaluation requires ENERGIZED hardware throughout
ENERGY_TRANSFER, independent of whether start assessment is enabled. SAFE,
PREPARING, READY, STOPPING, FAULT or invalid/stale evidence cannot select PREPARE
in transfer. The coordinator's existing denial path revokes before supervised
stop. Initial preparation and explicit renegotiation use their existing lifecycle
states and guards; no new hardware retry or reaction sequence is introduced.

TASK-6: Limit and energy ingress retain only accepted source ordering metadata.
Current-generation invalid/future/zero/stale evidence clears usability and revokes;
negative limits, invalid mode/correlation/validity, missing interlock/required
energy availability and zero snapshot ID are rejected before caching. Reordering
and duplicate handling apply after validation. Source recovery during negotiation
is allowed; recovery while stopping never grants renewed transfer permission.

Changed PROJECT contracts:

| Operation | Inputs and ownership | Bounded delivery/failure semantics |
|---|---|---|
| `OnEnergySnapshot(coordinator, energy, now)` | Owner receipt time is distinct from energy producer time | Copies accepted evidence only; invalid current evidence clears usability; unrelated generation/receipt rewind rejects before adoption. |
| `HostSimulation_InjectEnergy(simulation, value, now)` | Explicit synthetic/OS owner clock | Rejects unrelated owner generations before publication; forwards qualified/invalid publications to the coordinator. |
| `EnergyDataProvider_Publish(provider, snapshot, now, max_age)` | Source owner supplies receipt time and configured freshness age | Owns one visible publication plus separate accepted envelope. Invalid data is delivered as INVALID without advancing accepted ordering; valid reordered/duplicate-mutation data is rejected. Delivery OK does not mean qualification. |

The energy provider change closes the upstream route as well as the coordinator
cache; otherwise the simulator could still reject recovery before ingress.
Linux, examples and portable callers now supply receipt time explicitly. These
are PROJECT interfaces; imported AUTOSAR/BSW ABI and envelopes are unchanged.

TASK-7: `BlockingFaultStatus` is a PROJECT status extended with `root_timestamp`,
`has_escalation`, `escalation_cause` and `escalation_timestamp`. Its existing `cause`
and `fault_id` retain the first latched blocking root. A current-generation report
with strictly stronger severity updates `severity`, envelope reason/time/sequence
and the bounded escalation fields, leaving root identity and latch intact. Equal
or lower reports do not overwrite that trace or refresh timestamps. There is at
most one strictly stronger level after CONTROLLED_STOP in the current enum.
The S/R read exposes the same authoritative status. Wrong-generation, invalid
severity and rewinding reports are ignored. Explicit trusted BeginSession/clear
operations retain lifecycle ownership; this is not a new physical emergency policy.

Desired-behavior regressions in `openv2g_test_coordinator_fault_ingress`:
`transfer_hardware`, `limit_energy_recovery`, and `fault_escalation`. They cover
assessment-enabled/disabled transfer, all three limit sources, the public energy
publication path, negotiation recovery, stopped-session denial, timeout/hardware
escalation with root trace and publication, duplicate/lower severity and generation
isolation. Dated additional-review probes remain unchanged at their old baseline.
