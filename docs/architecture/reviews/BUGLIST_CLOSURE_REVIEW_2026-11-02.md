# Coordinator buglist closure review — 2 November 2026

Revision: 0.1. Reviewed commit: `5b084401bd47da8b769a58efd94757323d433bae`.
Scope: independent follow-up review of TASK-1 through TASK-4 in
[buglist.txt](../../../buglist.txt), their implementation, regressions and changed
host integration call sites. No implementation changes were made in this review.

## Conclusion

**All four listed bug-fix tasks are fixed within their documented host scope.**
No unresolved finding was identified in the reviewed fixes. This is not a
whole-project defect-free assertion or closure of the earlier architecture's
external supplier, deployment, safety and conformance decisions.

## Findings disposition

| Task | Confirmed behavior | Source and executed regression |
|---|---|---|
| TASK-1 — HLC overflow | Revokes before coordinated stopping in READY_FOR_ENERGY and ENERGY_TRANSFER; repeated overflow preserves the original transaction. Resynchronization cannot authorize transfer during stopping. | `charging_coordinator.c:416`; `test_coordinator_fault_ingress.c:42` (`overflow`) |
| TASK-2 — producer time controls stop | HLC/link/hardware ingress takes separate trusted receipt time, checks source freshness and rejects future evidence without adopting its watermark. Stop rejects clock rewind and retains the original deadline. | `charging_coordinator.c:282`, `:348`, `:442`, `:693`; `test_coordinator_fault_ingress.c:83` (`fault_times`) |
| TASK-3 — expired measured-safe proof | Tick rechecks current measured proof and shares current completion prerequisites with timeout supervision. Missing/stale hardware or CP/PP cannot leave an incomplete stop outside deadline escalation. | `charging_coordinator.c:775`, `:819`; `test_coordinator_fault_ingress.c:148` (`stop_evidence_expiry`) |
| TASK-4 — CP/PP watermark poisoning | Invalid CP/PP removes usability without replacing the accepted sample. Later valid connected/unplug evidence is accepted, while stopping continues to deny power and reordered accepted samples remain rejected. | `charging_coordinator.c:85`; `test_coordinator_fault_ingress.c:195` (`cp_recovery`) |

Files: [coordinator implementation](../../../src/charging_coordinator/charging_coordinator.c),
[regression suite](../../../tests/integration/test_coordinator_fault_ingress.c).
Line references are pinned to the reviewed commit.

Regression review included revoke-before-command ordering; OK/ACCEPTED/BUSY/
REJECTED hardware results; missing and delayed resynchronization; repeated faults;
all three fault ingress sources; generation isolation; future/zero/stale/invalid/
reordered/duplicate evidence; late hardware/service completion; evidence expiry;
fresh replacement proof; no revival after fault; and CP/PP recovery after six
invalid input forms. Synthetic test times are not vehicle safety allocations.

The changed simulator, Linux RTE fixture and EXI runner call sites supply explicit
owner/dispatch time. The wire runner uses the OS clock; deterministic fixtures
use their documented synthetic clock. Integrators must retain this distinction
instead of copying the producer timestamp into the receipt-time argument.

The current coordinator source, regression source and control-boundary document
hashes match the recorded TASK-3/4 implementation evidence. The final commit
after implementation `5d76f65` changes closure documentation only.

## Verification executed on 2 November

| Configuration | Observed result | Scope |
|---|---|---|
| Fresh portable build | **27/27 passed** | Includes all four buglist regression groups in `openv2g_test_coordinator_fault_ingress` |
| Rebuilt existing generated EXI/Linux/OpenSSL/TLS/PnC/oracle configuration | **All 51 tests passed across the initial run and permitted retry** | Initially 42 passed and 9 socket-dependent tests failed under sandbox restrictions; all 9 passed when rerun outside the sandbox with approval |

The integration configuration was rebuilt against the current source; its build
directory was reused, not freshly configured. It has ISO-20 qualification OFF.
The first integration failures included `Operation not permitted` for local
sockets and listener/handshake failures. No source changes were needed for the
successful retry. Sanitizers were not rerun in this review; previously recorded
ASan/UBSan results remain historical evidence.

Commands executed from the repository root:

```sh
cmake -S . -B /tmp/openv2g-closure-review-20261102 \
  -DOPENV2G_BUILD_LINUX_STUBS=OFF -DOPENV2G_ENABLE_CBV2G_SAP_POC=OFF
cmake --build /tmp/openv2g-closure-review-20261102 -j 4
ctest --test-dir /tmp/openv2g-closure-review-20261102 --output-on-failure
cmake --build /tmp/openv2g-tls-build -j 4
ctest --test-dir /tmp/openv2g-tls-build --output-on-failure
# Approved retry with local socket access:
ctest --test-dir /tmp/openv2g-tls-build --rerun-failed --output-on-failure
```

[Verification metadata](evidence/buglist_closure_20261102.json) records the source
and log hashes, build settings and results. Dated defect probes remain unchanged;
they describe old behavior and were not treated as acceptance tests.

## Remaining architecture scope

The four buglist fixes do not close the earlier residual work on concrete
codec/security allocation, current architecture diagrams, runnable/task/queue
deployment, exact supplier-generated interfaces, or vehicle safety and timing
evidence. Those remain separate from the confirmed coordinator defects. See
[earlier review dispositions](ARCHITECTURE_REVIEW_2026-10-31.md) and
[open decisions](../../../DECISIONS.md).
