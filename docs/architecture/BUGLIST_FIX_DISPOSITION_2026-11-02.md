# Additional coordinator buglist fix disposition

Revision 0.1 — 2 November 2026. PROJECT host implementation only.

Input: TASK-5 through TASK-7 in `buglist.txt`, from the additional review of baseline
`5b084401bd47da8b769a58efd94757323d433bae`. The source report and reproduction
under `docs/architecture/reviews/` remain unchanged. Their assertions describe
that historical baseline; they are not acceptance tests for this implementation.

Implementation commit: `47fb1b17ee4138289546c320b91b2780081dbcfe`.

| Task | Disposition | Desired-behavior evidence |
|---|---|---|
| TASK-5 / P1 | Fixed: ENERGY_TRANSFER requires ENERGIZED hardware before any action selection, independently of start-assessment enablement. Denial follows the existing revoke/stop path. | `transfer_hardware`: assessed and legacy start, SAFE/PREPARING/READY/STOPPING/FAULT, invalid/stale feedback, fresh continuation, no prepare commands in transfer and no revival after denial. Existing renegotiation and wire sessions also pass. |
| TASK-6 / P2 | Fixed: invalid source updates never overwrite accepted limit/energy ordering. Explicit receipt time validates energy, and the provider separately retains accepted ordering and visible invalid publication. | `limit_energy_recovery`: all three limit sources, future/invalid/high-sequence inputs, metadata/limits, energy public publication path, negotiation recovery, duplicates/reordering/generation isolation, and stopped-session denial. |
| TASK-7 / P2 | Fixed: the first blocking cause/id/time stays latched while severity can strictly increase. One bounded stronger escalation is exposed by S/R; duplicate/lower reports cannot overwrite it. | `fault_escalation`: authoritative S/R read, root identity, stronger timeout and hardware fault, duplicate/lower reports, monotonic time, generation isolation and coordinator deadline/state/permission behavior. |

These functions run in `openv2g_test_coordinator_fault_ingress`. All previous
coordinator acceptance regressions remain in the same portable suite.

## PROJECT interface and status changes

- `OnEnergySnapshot(coordinator, energy, now)` and
  `HostSimulation_InjectEnergy(simulation, value, now)` take trusted serialized
  owner receipt time independently of producer timestamps.
- `EnergyDataProvider_Publish(provider, snapshot, now, max_age)` takes the source
  owner's receipt clock and configured age. Delivery of invalid evidence remains
  visible with INVALID quality; delivery OK is not qualification. An accepted
  envelope is stored separately and only valid ordered evidence advances it.
- `OpenV2g_BlockingFaultStatus.cause` and `fault_id` retain the first latched root.
  Added PROJECT fields are `root_timestamp`, `has_escalation`, `escalation_cause`
  and `escalation_timestamp`. Existing severity and envelope reason/time/sequence
  expose the strictly stronger accepted reaction. Equal/lower reports preserve
  the current trace. New-session/clear remains a trusted lifecycle operation.

Portable, example and Linux callers were updated. Imported AUTOSAR/BSW ABI is
unchanged. No physical retry, new stop deadline or vehicle emergency policy was
introduced. See [contracts](CONTROL_BOUNDARY_HARDENING.md), Draft 0.4.

## Executed verification

- Fresh out-of-tree portable configuration/build/full suite: **27/27 passed**.
- Separate portable ASAN/UBSAN rebuild/full suite: **27/27 passed**.
- Generated EXI/OpenSSL/Linux/TLS/PnC rebuild/full integration suite: **51/51 passed**.

All runs used the final implementation. Only documentation/closure metadata
changed afterward. The fresh directory was created immediately before configure.

```sh
cmake -S . -B /tmp/openv2g-bugfix567-ckx4pil6 -DOPENV2G_BUILD_LINUX_STUBS=OFF -DOPENV2G_ENABLE_CBV2G_SAP_POC=OFF
cmake --build /tmp/openv2g-bugfix567-ckx4pil6 -j 4
ctest --test-dir /tmp/openv2g-bugfix567-ckx4pil6 --output-on-failure
cmake --build /tmp/openv2g-bugfix-sanitize -j 4
ctest --test-dir /tmp/openv2g-bugfix-sanitize --output-on-failure
cmake --build /tmp/openv2g-tls-build -j 4
ctest --test-dir /tmp/openv2g-tls-build --output-on-failure
```

The portable sanitizer configuration uses C flags
`-fsanitize=address,undefined -fno-omit-frame-pointer` and executable linker flags
`-fsanitize=address,undefined`. The integration configuration retains the pinned
libcbv2g candidate/oracle and OpenSSL host service used in earlier evidence.
System OpenSSL libraries are not sanitizer-instrumented. Host test results are
not production qualification or physical vehicle interoperability evidence.

[Machine-readable verification](evidence/coordinator_bugfix567_verification.json)
records source hashes and logs. No target architecture freeze or full-project
absence of defects is implied by these three finding closures.
