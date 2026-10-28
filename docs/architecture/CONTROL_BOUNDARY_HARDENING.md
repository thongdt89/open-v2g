# Coordinator control-boundary changes

Revision: Draft 0.1 — 28/10/2026. PROJECT host implementation, following
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
| `OnHardwareStatus(status)` | Qualify measured SAFE, open contactors, inactive precharge and zero current using a newer hardware sequence after the command and before its deadline |
| `OnHlcStatus(status)` | An accepted asynchronous stop is completed by current IDLE evidence |
| `OnLinkStatus(status)` | An accepted asynchronous stop is completed by current DOWN evidence |
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
