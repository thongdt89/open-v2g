# Current feature completion status

Revision: Draft 0.4 — 30/10/2026. **Overall implementation remains in progress.**
This is the current inventory; older audits/work-log entries describe their own
snapshots. Implemented means host code, not selected supplier, frozen design,
vehicle safety allocation or protocol conformance.

## Portable session/lifecycle increment — 30/10/2026

- PermissionManager now intersects independent vehicle/cable/peer/schedule
  evidence, caps permission by source lifetime, and requires explicit correlated
  service selection/authorization in the real wire profile. Vehicle snapshots
  remain immutable; outgoing targets are projected separately.
- ISO15118Chrg owns portable bounded session/EVSE/service/schedule and response
  validation. Aggregate link/HLC setup and per-response budgets supplement the
  earlier cumulative processing and measured-stop supervision.
- Discovered IPv6 ISO-2 EIM pause/resume and in-session renegotiation complete
  AC/DC flows in plaintext and with fresh TLS. Resume requires measured connected
  safety, a new generation, repeated authorization, fresh limits/assessment and
  new permission. Renegotiation revokes first and regenerates physical readiness.

[Implementation contracts, host profiles and remaining gates](SESSION_LIFECYCLE_IMPLEMENTATION.md).
The historical review and prior increments below retain their snapshot scope.
Concrete generated mapper/codec placement, ingress deployment, real acquisition,
nonvolatile/PnC recovery, physical fallback policy and production qualification
remain open. New host behavior does not close those external gates.

## Control-boundary increment — 28/10/2026

Implemented current-source evidence invalidation, stale/duplicate protection,
action-specific physical assessment checks, expiry enforcement before renewal,
a bounded coordinator stop transaction and measured connected/unplugged completion.
An explicit connected-safe restart creates a new generation. Host CP/energy
publishers propagate invalid quality; the wire provider sequences same-tick
samples. See [contracts and remaining gates](CONTROL_BOUNDARY_HARDENING.md).

Still open in the control review: explicit protocol authorization/service evidence,
independent cable/peer/schedule limit intersection in PermissionManager, portable
HLC session/wire mapping, target ingress/runnable/watchdog binding and vehicle
physical retry/fallback/fault-reset policy. The earlier review files are historical
evidence and are preserved without editing their reproduction assertions.

## Latest increment — 28/10/2026

| Item | Current host result | Remaining limits |
|---|---|---|
| TLS | Real optional OpenSSL TLS1.2/1.3 over IPv4 and discovered IPv6 before SAP/EXI; CA/SAN/time verification, optional CRLs and mutual-certificate API | Explicit host AEAD profile; supplier/OEM PKI/trusted-time/revocation policy, target resource evidence and exact-edition cipher selection remain open |
| Software crypto | Typed bounded SHA256/P-256 sign/verify jobs, versioned key handles, copied input, deadlines/cancel/generation checks and two-way raw-signature fixture verification | Not XMLDSIG/EXI canonicalization, signed receipts, PnC authorization, HSM or transactional credential provisioning |
| Processing budgets | HLC-owned cumulative Authorization/CPD/CableCheck deadlines and Ongoing count limits; no reset on retry; late Finished/old timer events fail closed | Wire settings are provisional; broader setup/response watchdogs and target physical-stop integration and normative timing selection remain open |

See [host security and processing implementation](../../simulation/linux/TLS_CRYPTO.md)
for API contracts, build/run examples, security policy and evidence scope.

## Earlier AC/metering increment

| Item | Current result | Limits |
|---|---|---|
| ISO-2 AC EIM | Implemented single-phase host path: service/mode selection, AC charge parameters, PowerDelivery, ChargingStatus, controlled stop and SessionStop | Explicit `iso2-ac` profile. Global defaults remain DC. Three-phase, charging profiles and full AC compliance remain open |
| AC permission/hardware | Explicit `allow_ac_eim` permission and `host_ac_enabled` simulated-hardware capability; permission retains selected mode; enable must match prepared mode | Coordinator remains sole permission owner. AC readiness is a deterministic local hardware/OBC fixture, not a reviewed vehicle assessment |
| AC fault/limit response | Reject AC/DC mode mismatch and asserted RCD before continuing; stop revokes and opens simulated contactors; apply lower current limits to energy context and publish updated permission immediately | Real RCD/OBC acquisition, nominal-voltage tolerance, phase/power-factor policy and electrical sequencing need vehicle integration |
| ISO-2 metering wire | Bounded MeterInfo in CurrentDemand/ChargingStatus; MeteringReceipt request/response; retained meter record copied once with generation/source correlation and consumed on acknowledgement | Unsigned development fixture only, behind `allow_unsigned_receipts` (default false). Unverified meter signatures and signature-required receipts stop; no XMLDSIG/PKI verification |
| Independent regression | 19 additional AC/metering vectors decoded/re-encoded byte-identically by the separate pinned oracle; original 50 unchanged | Candidate-constructed differential examples, not standards-authored vectors or conformance evidence |

The shared project DTO retains its legacy `OpenV2g_ExiDcDocument` name. Explicit
schema remains ISO-2 for AC/DC; explicit `ac_mode` selects the bounded message
branch. MeteringReceipt requests carry common fields and obtain mode from the
active session; their wire payload does not invent an AC/DC discriminator.
Meter timestamps/readings/signature bytes are retained as data, not trusted-time,
settlement, identity or signature-verification evidence.

## Still incomplete

| Work item | Already present | Remaining implementation |
|---|---|---|
| TLS / credential integration | Actual verified host TLS and typed software crypto jobs, with explicit development trust/identity and optional CRL configuration | Selected supplier/HSM integration, OEM provisioning/trust/time/OCSP policy, exact-edition TLS profile and qualified resources; no automatic plaintext fallback |
| PnC / signed receipts | Disabled feature gates; bounded opaque meter signatures; signed headers rejected | PaymentDetails, certificate install/update, challenge binding, EXI/XMLDSIG signing/verification and transactional credential lifecycle |
| Broader timing supervision | Configurable cumulative Authorization/CPD/CableCheck budgets plus exchange/session guards | Target watchdog/physical-stop integration and exact-edition configuration; select normative budgets from exact editions and vehicle timing work products |
| Services and schedules | ServiceDetail integer/string sets; bounded three tuples/five intervals with conservative selection | Multiple payment/service/mode offers, more parameter forms, tariffs/profiles, notifications and richer completion/schedule policy |
| Broader AC | Single-phase EIM through real IPv4/IPv6 sockets | Three-phase, phase-specific limits, actual OBC control/measurement and complete AC protocol feature coverage |
| Broader metering | ISO-2 unsigned host AC/DC receipt loop | DIN metering branches, authenticated meter data, signed receipt policy and compliant signature generation/verification |
| ISO-20 | Disabled registry/factory seams; separate generated models exist in the external candidate checkout | Pinned/hash-controlled Common/AC/DC bindings, message mappings, scheduled/dynamic adapters, independent vectors and sessions |
| BPT | Coordinator semantic model and measured-zero/new-permission invariants; wire feature disabled | ISO-20 BPT services/loops, vehicle capability contract, reversal/cancel/expiry integration; protocol support alone cannot authorize powertrain reversal |
| Network/PLC | Real unicast loopback IPv6 SDP and TCP before SAP/EXI | Multicast/link-local scopes, retries, real modem/SLAC and selected TcpIp/SoAd integration |
| Vehicle readiness qualification | Correlated/fresh DC and AC assessment guard at coordinator energy-enable operation | Integrate vehicle-approved readiness into the full permission/evidence contract, real OBC/precharge measurements and topology-specific safe sequence |
| Persistent supervision | Cooperative tick, fresh simulated CP/energy, fail-closed stale hardware/link | Real surrounding hardware/link acquisition for long sessions; do not restamp cached measurements |
| Qualification | Host unit/socket/malformed/sanitizer/differential tests and coverage tooling | Broader fuzz/coverage cases, exact-edition matrix, physical interoperability, target resources/WCET and conformance |

These rows contain implementable host work. They are not all blocked by missing
external decisions. Production closure additionally needs the licensed edition/
schema/generator provenance, supplier ARXML/BSW, OEM PKI/trusted-time policy,
vehicle HARA/topology/approved thresholds and target toolchain/bench evidence.
No such artifact or approval is manufactured by these host changes.

## Run the new AC path

With the existing opt-in generated-code build, use separate terminals:

```sh
./build-exi/simulation/linux/wire/openv2g_linux_exi station6 15118 iso2-ac
./build-exi/simulation/linux/wire/openv2g_linux_exi evcc6 15118 iso2-ac
```

`station`/`evcc` use legacy IPv4; `station6`/`evcc6` run real unicast IPv6 UDP SDP
then TCP. AC nominal sessions exchange 12 protocol messages with three status
loops. Metering fixture tests add three receipts. Their unsigned opt-in is a
programmatic test policy, not an automatically enabled CLI security downgrade.

For the wire layout, pinned generated headers and separate pinned oracle are
the build inputs. Supporting research also checked
[the reference AC datatype model](https://raw.githubusercontent.com/EcoG-io/iso15118/master/iso15118/shared/messages/iso15118_2/datatypes.py)
and [reference simulator](https://raw.githubusercontent.com/EcoG-io/iso15118/master/iso15118/secc/controller/simulator.py)
(the latter uses a clear RCD bit for its healthy AC example). These unpinned
research links are not selected production dependencies or normative authorities.

## Verification for this revision

- Normal EXI build: **28/28 CTest checks passed**, including both independent
  differential corpora (50 original + 19 feature vectors).
- ASan/UBSan build: **26/26 passed**, with generated code instrumented.
- Default generated-code-OFF build: **20/20 passed**; default AC capability remains off.
- Clean GCC coverage build: **26/26 passed**; [counts and source hashes](evidence/ac_metering_coverage.json).
- Wire test: **43 two-process sessions**, including AC, AC metering, IPv6 AC,
  RCD/mode mismatch, receipt policy/signature rejection and existing DC faults.
- Actual IPv4 and IPv6 `iso2-ac` CLI pairs each exited 0 and completed 12 exchanges.

No target, physical-EVSE, formal safety or standards conformance claim follows
from these host checks.
