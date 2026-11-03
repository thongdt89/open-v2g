# EVCC architecture re-review — 31 October 2026

Revision: 0.1. Implementation baseline: `4f4df51184d2c279c86e791681201f601fc3c759`.
Status: review findings and proposed corrections; no approval or formal freeze.
Comparison: [28 October review](ARCHITECTURE_REVIEW_2026-10-28.md), baseline `1bf5613`.

## Assessment

Keep `ChargingCoordinator` and `ISO15118Chrg` as the two main compositions. The
portable implementation now owns independent permission limits, protocol
readiness, session correlation and bounded stop transactions. These changes
address substantial parts of the previous review. Two reproduced control-path
defects remain: queue-overflow revocation does not start safe stopping, and
producer timestamps can displace the coordinator's deadline clock.

The review covers selected ownership, permission, asynchronous ingress, timing,
stop and portability paths against Atomic Specification Rev.3. It does not
establish protocol conformance, target timing, electrical safety or supplier ABI
compatibility. Prior diagrams and source snapshots retain their recorded
baselines. No implementation code was changed during this review.

## Findings

### 1. P1 — HLC event overflow denies permission without requesting safe stopping

**Reproduced in the portable host composition.** During energy transfer,
`MarkHlcEventOverflow` sets the resynchronization flag and revokes permission,
but does not request the hardware safe state or start a stop transaction.
`EvaluatePermission` returns early while resynchronization is pending, bypassing
its normal denial-to-stop handling. `Tick` checks permission expiry only while
the decision is ALLOW, so revocation also removes that fallback.

The probe energizes simulated hardware, marks overflow at time 170, evaluates
permission at 171, then calls `Tick(10000000)`. Permission is DENY, hardware is
still ENERGIZED, and neither `safe_requested` nor `teardown_started` is set.
There is no stop deadline to supervise. This is simulated state, not a physical
vehicle measurement.

Evidence: [coordinator](../../../src/charging_coordinator/charging_coordinator.c),
lines 412–430, 503–509 and 812–818;
[probe](evidence/architecture_probe_20261031.c).
Related contract: `CC-EXT-005` requires status resynchronization after overflow;
the permission/safe-state contract requires coordinated action after revocation.

**Scope qualification:** the Linux wire runner separately detects dropped HLC
events in `sync_hlc` and propagates failure to its stop path
([client](../../../simulation/linux/wire/client.c), lines 34–48 and 626 onward).
That caller-level mitigation does not make the public coordinator operation
safe for another integration. The existing bridge test exercises overflow
before energy transfer.

**Proposed correction:** make active-transfer overflow enter the coordinator's
idempotent stop transaction after revocation. Retain the resynchronization flag
for evidence recovery; fresh status must not erase a pending stop. If any
alternative recovery is intended, define and supervise its bounded physical
response explicitly rather than leaving DENY with energized hardware.

**Closure checks:** overflow during READY_FOR_ENERGY and ENERGY_TRANSFER;
missing/delayed resynchronization; repeated overflow; rejected safe-state
request; measured-safe completion; late status that must not restart transfer.

### 2. P1 — A future-dated fault status can postpone stop-timeout supervision

**Reproduced in the portable host composition.** The HLC error, PLC-down and
hardware-fault handlers pass the producer's envelope timestamp to `Stop`.
Those ingress paths do not compare that timestamp with a trusted receiving
clock before using it for control. `Stop` assigns it to `owner_time` and uses it
to arm the physical stop deadline. Subsequent `Tick` calls with a smaller time
return immediately.

The probe starts transfer at 160, installs a rejecting hardware safe-state port,
and injects a current-generation, higher-sequence HLC_ERROR stamped 1000000.
With a test stop budget of 100, the coordinator records `owner_time=1000000`
and `stop_deadline=1000100`. `Tick(270)` leaves it STOPPING with simulated
hardware ENERGIZED. The timeout latches only at 1000100. Permission is correctly
DENY, and the failed safe-state result is retained; the defect is the delayed
timeout escalation. These numbers are synthetic test values, not normative
protocol or vehicle safety deadlines.

Evidence: [coordinator](../../../src/charging_coordinator/charging_coordinator.c),
lines 329–341, 380–390, 474–484, 721–722 and 772–788;
[probe](evidence/architecture_probe_20261031.c).
Related contracts: monotonic envelope time, freshness validation, one timer
owner, and bounded stopping in Atomic §§4–5 and 25–26.

**Proposed correction:** distinguish producer evidence time from receiving owner
time. Supply trusted monotonic receipt/dispatch time to owner operations, reject
or invalidate future evidence according to an explicit clock contract, and arm
reactions using the owner clock. Invalid fault evidence should fail closed
without allowing its timestamp to move or rewind supervision. Preserve an
existing stop transaction's deadline on repeated faults.

**Closure checks:** future and delayed HLC/PLC/hardware fault timestamps;
invalid quality; repeated fault callbacks; ordinary owner ticks after rejected
safe-state requests; no deadline extension or backward owner-time movement.

## Disposition of the earlier findings

These are host-scope dispositions, not production approvals. Original finding
numbers are retained for traceability.

| Earlier finding | Re-review disposition | Evidence / remaining boundary |
|---|---|---|
| 1. Cached invalid/reordered evidence | Addressed for reviewed paths | Current-source invalidation revokes/stops; obsolete and duplicate samples cannot replace usable evidence. `test_control_boundaries` passed. |
| 2. Incomplete permission readiness | Addressed in configured host profile | Physical assessment and HLC service/authorization readiness gates exist. Legacy semantic fixtures explicitly disable some gates; target profile selection remains external. |
| 3. Runner-owned limit intersection | Addressed for implemented sources | `PermissionManager` intersects independent vehicle, cable, peer and schedule evidence with expiry/provenance. `test_permission_limits` passed. Supplier acquisition remains open. |
| 4. Hidden stop failure / missing measured completion | Original cases addressed; related defect remains | Retained results, measured-safe confirmation, teardown completion and connected restart are implemented. New finding 1 exposes a path that never starts the transaction. |
| 5. Renewal ordering / unbounded protocol stop | Original cases addressed; clock defect remains | Evaluation checks expiry before renewal; core link/stop and HLC communication/processing deadlines exist. New finding 2 can displace the stop deadline. |
| 6. HLC session logic in runner | Partially addressed | Portable `SessionContext` owns identity, correlation and response validation. Concrete schema DTO mapping, codec instances and some security supervision still require the host runner. |
| 7. Execution/deployment binding | Open external integration work | Serialized owner APIs exist; runnable/task/queue allocation, supplier signatures, target budgets and safety/watchdog allocation are not established by this review. |
| 8. Inconsistent current architecture views | Partially addressed | New structure/behavior diagrams are explicitly pinned to `346c11f`; they predate the current session, SLAC and PnC increments. Historical diagrams must not be treated as HEAD views. |

Supporting implementation contracts:
[control boundaries](../CONTROL_BOUNDARY_HARDENING.md),
[portable limits and session lifecycle](../SESSION_LIFECYCLE_IMPLEMENTATION.md).

## Structural and behavioral assessment of newer work

The core allocation remains consistent with the source design: vehicle charging
progression and permission belong to `ChargingCoordinator`; HLC owns protocol
progression and publishes requests/evidence. `PlcLinkManager` owns typed PEV-SLAC
matching and link proof, while `ISO15118Chrg::SdpClient` owns SDP. No reviewed
path adds an HLC-to-actuator command boundary.

There is now a portable session context and PnC context, plus host implementations
for bounded service selection, balanced three-phase AC, managed credentials,
signed receipts and pre-provisioned ISO-2 PnC. Their presence must not be confused
with unrestricted feature coverage or supplier implementation. In particular:

- Typed PEV-SLAC does not establish raw HomePlug MME/modem/PHY integration.
- PnC is a restricted host profile. Active credential/key-reference checks and
  revalidation still occur in `OvWirePncClient_Supervise` / `reference_matches`
  in [pnc_client.c](../../../simulation/linux/wire/pnc_client.c). A future
  transport replacement must preserve that supervision explicitly; extracting
  `PncContext` alone is insufficient. This is an allocation observation, not a
  reproduced failure of the current TLS runner.
- ISO-20 Common/AC/DC generated models are isolated qualification candidates.
  Their probes do not bind active SchemaRegistry slots or enable ISO-20/BPT
  sessions. Vehicle powertrain capability remains a separate decision.
- Concrete schema mapping and codec ownership need an explicit production
  allocation inside or behind `ISO15118Chrg`; replacing socket calls alone is
  insufficient for AUTOSAR integration.

Scope references: [current inventory](../INCOMPLETE_ITEMS.md),
[PnC host](../PNC_HOST.md), [SLAC host](../../../simulation/linux/SLAC_HOST.md),
[ISO-20 candidates](../../../integration/protocol/adapters/cbv2g/iso20/README.md).

## Verification performed for this review

Fresh out-of-tree portable build using GCC 13.3.0:

```sh
cmake -S . -B /tmp/openv2g-architecture-review-20261031 \
  -DOPENV2G_BUILD_LINUX_STUBS=OFF -DOPENV2G_ENABLE_CBV2G_SAP_POC=OFF
cmake --build /tmp/openv2g-architecture-review-20261031 -j 4
ctest --test-dir /tmp/openv2g-architecture-review-20261031 --output-on-failure
```

**Observed: 26/26 tests passed.** This includes control boundaries, permission
limits, session context, communication/processing budgets, certificate manager,
typed PEV-SLAC and PnC context. Passing those tests does not cover the two new
scenarios above. Optional Linux socket/OpenSSL/generated-code/oracle paths were
inspected selectively but were not rebuilt or rerun for this review. Historical
counts in implementation documents remain historical evidence.

The additional standalone review probe compiled with `-std=c11 -Wall -Wextra
-Werror`, linked the fresh host-simulation and core libraries, and exited 0:

```text
Overflow: DENY, no safe request or teardown, hardware still ENERGIZED after Tick(10000000).
Future error: owner_time=1000000, stop_deadline=1000100; Tick(270) leaves STOPPING and hardware ENERGIZED.
Future error: timeout only latches at injected future deadline 1000100.
```

Reproduce from the repository root after the build above:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude -Isimulation/include \
  docs/architecture/reviews/evidence/architecture_probe_20261031.c \
  /tmp/openv2g-architecture-review-20261031/simulation/libopenv2g_host_sim.a \
  /tmp/openv2g-architecture-review-20261031/libopenv2g_core.a \
  -o /tmp/openv2g-architecture-review-20261031/architecture_probe
/tmp/openv2g-architecture-review-20261031/architecture_probe
```

This probe asserts observed defects and is deliberately outside the acceptance
suite. After fixes, add regressions asserting the desired behavior; retain the
dated probe as historical evidence. The earlier probe likewise remains tied to
its original baseline.

## Recommended next increment

Close the two P1 paths and add the closure checks above before expanding further
protocol behavior. Then update the implementation/diagram entry point to a
single new baseline and record the remaining codec/security allocation. Keep
supplier ARXML, runnable deployment, hardware fallback and target evidence as
explicit external decisions rather than inventing values to freeze the design.
