# Additional coordinator findings — 2 November 2026

Revision: 0.1. Baseline: `5b084401bd47da8b769a58efd94757323d433bae`.
Scope: additional review of adjacent permission, evidence and fault paths after
the [four-task closure review](BUGLIST_CLOSURE_REVIEW_2026-11-02.md).
The original four fixes remain closed within their stated scope. The following
are newly reproduced findings, tracked as local TASK-5 through TASK-7 in
[buglist.txt](../../../buglist.txt). No implementation change was made.

## 1. P1 / TASK-5 — Transfer can fall back to PREPARE on unexpected hardware state

**Observed:** start a host session with `require_physical_assessment=true`,
apply a valid precharge assessment and enter ENERGY_TRANSFER. Supply a fresh,
current-generation, higher-sequence hardware status of SAFE or PREPARING, then
evaluate permission. In both cases the coordinator publishes ALLOW/PREPARE,
calls the hardware prepare port, stays in ENERGY_TRANSFER and does not begin
stopping.

**Cause:** `evidence_is_complete` accepts SAFE/PREPARING/READY/ENERGIZED without
restricting them by the coordinator's progression state. The physical-assessment
condition checks continued ENERGIZED evidence only inside a branch entered for
READY or ENERGIZED hardware. SAFE and PREPARING bypass that branch. The action
selection then chooses PREPARE and the coordinator calls the port before reaching
its denial-to-stop path.

Evidence: [permission manager](../../../src/charging_coordinator/permission_manager.c),
lines 140–143, 187–201 and 214–219;
[coordinator](../../../src/charging_coordinator/charging_coordinator.c), lines
549–558. The [control-boundary contract](../CONTROL_BOUNDARY_HARDENING.md),
"Permission actions", explicitly requires current fresh ENERGIZED evidence
during transfer. Atomic §§4–5 require progression/evidence consistency.

**Impact and scope:** the portable owner can issue preparation in the wrong
lifecycle state instead of coordinating the unexpected loss of transfer
readiness. The probe supplies explicit hardware evidence and a counting prepare
port; it proves the permission and command decision, not actual energization or
a failure of the current socket fixture. A downstream hardware adapter can still
reject the command, which does not repair the owner's ALLOW/state decision.

**Proposed correction:** enforce the transfer hardware invariant independently
of start-assessment validation and preparation action selection. Unexpected
hardware state during ENERGY_TRANSFER should deny permission and enter the
existing coordinated stop path. Keep legitimate initial preparation and explicit
renegotiation separate. Verify SAFE/PREPARING/READY/FAULT/invalid/stale transfer
evidence and fresh ENERGIZED continuation; do not introduce physical retry rules.

## 2. P2 / TASK-6 — Rejected limit and energy samples still poison ordering

**Observed:** while HLC_NEGOTIATION is active, a cable-limit sample with future
timestamp 1000000 and sequence 100 is rejected at receipt time 170 but stored as
the source's ordering watermark. A valid sample at timestamp/receipt 180 with
sequence 101 is then rejected by `obsolete()`. The source remains unusable.

The same happens for an energy snapshot with INVALID quality, timestamp 1000000
and sequence 100: it is rejected but cached, and a valid timestamp 180/sequence
101 update cannot restore the source. No stop has begun in these negotiation
reproductions, so legitimate source recovery within the session is blocked.

Evidence: [coordinator](../../../src/charging_coordinator/charging_coordinator.c),
lines 221–231 and 250–258. These branches still copy rejected evidence into the
accepted-data cache. TASK-4 corrected the CP/PP branch, not these two producers.

**Impact and scope:** permission remains fail-closed; this is a recovery and
availability defect. A malformed source can prevent normal updates until their
time/sequence overtakes the invalid watermark or the session is reset. The probe
does not claim these bad samples grant permission.

**Proposed correction:** retain each producer's last accepted ordering watermark
when invalidating usability. Store rejected evidence separately if diagnostics
need it. Apply explicit source-time validation using trusted receipt time where
needed, including the energy ingress contract. Cover all three limit sources,
energy, future/invalid/high-sequence inputs, normal recovery during negotiation,
generation isolation, and rejection of genuinely reordered accepted data.

## 3. P2 / TASK-7 — A latched controlled fault suppresses later emergency severity

**Observed:** a valid HLC_ERROR at 170 latches CONTROLLED_STOP/INTERNAL. The
hardware safe-state request is rejected. At the existing deadline 270, Tick
sets the state machine to FAULT_LATCHED and the stop result to NOT_OK, then calls
`FaultCoordinator_Raise` with EMERGENCY_STOP/TIMEOUT. The authoritative fault
status nevertheless remains CONTROLLED_STOP/INTERNAL.

**Cause:** `FaultCoordinator_Raise` returns immediately whenever any fault is
already latched, regardless of the incoming severity. It records neither the
stronger severity nor secondary timeout evidence.

Evidence: [fault coordinator](../../../src/charging_coordinator/fault_coordinator.c),
lines 43–45; [timeout caller](../../../src/charging_coordinator/charging_coordinator.c),
lines 809–815. Atomic `FC-IF-008` publishes authoritative severity and `FC-IF-010`
calls for root-cause plus secondary evidence. Preserving a root cause does not
require silently dropping the higher reaction severity explicitly requested by
the caller.

**Impact and scope:** this is a fault-status/escalation visibility gap. Permission
is still DENY and the vehicle state machine does reach FAULT_LATCHED; the original
bounded-stop fixes are not undone. A consumer of BlockingFaultStatus cannot see
the requested escalation. No additional physical emergency action is claimed or
prescribed without the vehicle safety concept.

**Proposed correction:** preserve root-cause traceability while allowing
monotonic severity escalation and recording secondary causes. Lower-severity or
duplicate reports must not downgrade the reaction or clear the latch. Test a
controlled fault followed by timeout/hardware fault, duplicate/lower-severity
reports, and generation handling.

## Reproduction and verification

The [standalone probe](evidence/additional_probe_20261102.c) compiled with strict
warnings and ran successfully against the portable libraries built at this
unchanged commit during the preceding closure review:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude -Isimulation/include \
  docs/architecture/reviews/evidence/additional_probe_20261102.c \
  /tmp/openv2g-closure-review-20261102/simulation/libopenv2g_host_sim.a \
  /tmp/openv2g-closure-review-20261102/libopenv2g_core.a \
  -o /tmp/openv2g-closure-review-20261102/additional_probe
/tmp/openv2g-closure-review-20261102/additional_probe
```

Observed output:

```text
Hardware SAFE during transfer: ALLOW/PREPARE, prepare_calls=1, no stop.
Hardware PREPARING during transfer: ALLOW/PREPARE, prepare_calls=1, no stop.
Limit recovery: rejected future sample retained; valid timestamp=180 sequence=101 rejected.
Energy recovery: rejected invalid sample retained; valid timestamp=180 sequence=101 rejected.
Timeout escalation: state FAULT_LATCHED, but authoritative fault remains CONTROLLED_STOP/INTERNAL.
```

The probe asserts observed defects and remains outside the acceptance suite.
Use new desired-behavior regressions to close these tasks. No full suite was
rerun for this additional probe-only review; the preceding 27-test portable and
51-test integration results apply to the same commit but did not exercise these
cases. This review is selective and does not establish target safety/conformance.
