# Coordinator buglist disposition

Revision 0.1 — 31 October 2026. Host implementation and verification only; no
vehicle safety allocation, supplier ABI change or target architecture freeze.

Input: `buglist.txt`, local TASK-1 / TASK-2, review baseline
`4f4df51184d2c279c86e791681201f601fc3c759`. Dated review report/probes under
`docs/architecture/reviews/` are preserved as historical evidence. Their old
assertions are not rewritten into acceptance tests.

Implementation commit: `91cd37030d2af3c1c0a28616561ad34415e24704`.

| Finding / task | Disposition | Desired-behavior evidence |
|---|---|---|
| 1 / TASK-1 | Fixed: overflow revokes before the coordinator's idempotent stop, including hardware request, original deadline/result and HLC/PLC teardown. Resynchronization cannot cancel stopping. | `openv2g_test_coordinator_fault_ingress::overflow`: READY and ENERGY_TRANSFER, OK/ACCEPTED/BUSY/REJECTED, command-order probe, missing/delayed resync, repeated overflow, measured completion and timeout. |
| 2 / TASK-2 | Fixed: HLC/link/hardware ingress receives explicit trusted owner receipt time. Invalid source evidence fails closed without adopting its timestamp/sequence. Reactions and deadlines use owner time; repeated faults cannot rearm stop. | `openv2g_test_coordinator_fault_ingress::fault_times`: all three sources, future/zero/invalid/stale/reordered/duplicate faults, generation isolation, rewind rejection, original deadline and timeout; late measured-safe and asynchronous teardown cannot backdate completion. |

Changed PROJECT interfaces: `OnHlcStatus`, `OnLinkStatus`, `OnHardwareStatus`, and
simulator `InjectHardwareFeedback` gain `OpenV2g_TimeUs now`. Portable simulator,
Linux RTE/wire runner and existing fixtures supply owner/synthetic dispatch time.
Imported AUTOSAR ABI and producer envelopes remain unchanged. The host receipt
clock is trusted by contract; external integration must supply that clock rather
than copying the producer timestamp. No arbitrary future tolerance, retry or new
physical deadline is introduced.

Detailed contracts: [control-boundary hardening](CONTROL_BOUNDARY_HARDENING.md).
Executed commands/results and source/log hashes are in
[evidence](evidence/coordinator_bugfix_verification.json).

Verification:

- Fresh out-of-tree portable configuration, build and complete CTest suite: **27/27 passed**.
- Separate portable ASAN/UBSAN build and complete suite: **27/27 passed**.
- Affected generated EXI/OpenSSL/TLS/PnC/Linux integration build: **51/51 passed**; the final resynchronization-watermark tightening was subsequently checked by a **3/3 passing focused rerun** in that build, plus both complete 27-test portable suites..

The configured portable test budget is synthetic (100 microseconds in the new
regression); it is not a normative protocol or vehicle safety deadline. The
regressions use explicit measured-status fixtures, not target hardware evidence.

Portable commands (fresh build directory created immediately before configure):

```sh
cmake -S . -B /tmp/openv2g-bugfix-final-8fun3jdb -DOPENV2G_BUILD_LINUX_STUBS=OFF -DOPENV2G_ENABLE_CBV2G_SAP_POC=OFF
cmake --build /tmp/openv2g-bugfix-final-8fun3jdb -j 4
ctest --test-dir /tmp/openv2g-bugfix-final-8fun3jdb --output-on-failure
```

Sanitizer configuration adds `-fsanitize=address,undefined -fno-omit-frame-pointer`
to C flags and `-fsanitize=address,undefined` to executable linker flags in
`/tmp/openv2g-bugfix-sanitize`. Both complete suites pass on the final source.

```sh
cmake --build /tmp/openv2g-bugfix-sanitize -j 4
ctest --test-dir /tmp/openv2g-bugfix-sanitize --output-on-failure
cmake --build /tmp/openv2g-tls-build -j 4
ctest --test-dir /tmp/openv2g-tls-build --output-on-failure
ctest --test-dir /tmp/openv2g-tls-build -R 'coordinator_fault_ingress|hlc_event_bridge|control_boundaries' --output-on-failure
```

The TLS configuration retains the previously pinned generated codecs/oracle and
optional OpenSSL host service. The old custom-frame smoke fixture now supplies
its explicit synthetic scheduler dispatch time; the EXI runner uses the OS
monotonic clock. No timing tolerance was added to accept future source evidence.
