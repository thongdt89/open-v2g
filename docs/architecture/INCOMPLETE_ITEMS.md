# Current feature completion status

Revision: Draft 0.13 — 31/10/2026. **Overall implementation remains in progress.**
This is the current inventory; older audits/work-log entries describe their own
snapshots. Implemented means host code, not selected supplier, frozen design,
vehicle safety allocation or protocol conformance.

## Secondary service detail increment — 31/10/2026

The portable ISO-2 adapter owns up to three additional ServiceDetail transactions.
Each request binds the target service ID into SessionContext alongside generation,
schema, session and correlation. A secondary response cannot replace the primary
charging-service identity. The runner separately copies each service's parameter
sets, validates the selected set against that service and clears evidence on stop.
Wrong-service responses and unavailable secondary sets stop before energy.
See [service contracts](../../simulation/linux/SERVICE_OFFERS.md).

## PnC host authorization increment — 31/10/2026

Explicit PnC runners now select Contract over TLS, validate a persisted contract
role/identity and public chain, exchange PaymentDetails with a fresh station
nonce, sign/verify Authorization and complete existing AC/DC charging with signed
receipts. Ongoing preserves the original challenge and deadline. Certificate
version/fingerprint, key version/lease, generation, session, job and correlation
remain bound through the portable PncContext. Ordinary EIM defaults remain off
for PnC. Exact-CN whitelist and 15-character identity are a restricted host policy,
not full normative/OEM eMAID policy. See [contracts and limits](PNC_HOST.md).
Certificate installation/update and encrypted-key import remain unimplemented;
ISO-20 and broader schedules/profile/notification handling remain on the [execution list](HOST_COMPLETION_PLAN.md).

## PEV-SLAC typed host increment — 31/10/2026

PlcLinkManager now owns bounded PEV-SLAC matching with copied ingress, RunID/MAC/
generation/freshness checks, bounded sounds/groups, attenuation policy, retries,
stage/aggregate budgets and key-reference liveness. The deterministic typed peer
is enabled before SDP in EXI runners, including resume. Established-link proof
loss revokes evidence immediately and defers provider teardown to coordinated
Stop; key loss during energy start reaches DENY and simulated safe hardware.
See [host contracts](../../simulation/linux/SLAC_HOST.md).
Raw HomePlug MME bytes/PHY pacing, multiple physical candidates and real modem/
association/IP acquisition are not claimed by this typed fixture. SDP ownership
and the no-HLC-to-actuator boundary remain unchanged.

## Multiple-offer/selection increment — 31/10/2026

ISO-2 retains both payments, six advertised transfer modes and bounded named
services/selections. The EIM runner selects its explicit mode/external payment,
checks requested ancillary IDs and primary parameter sets against correlated,
generation-bound, expiring copied evidence, and clears it on stop. AC3/DC mixed
offer sessions and before-energy negative selections are exercised. A pinned
build-copy overlay fixes the independently exposed six-mode terminal grammar;
old emitted bytes are now a rejection regression. See [contracts](../../simulation/linux/SERVICE_OFFERS.md).
Contract serialization does not enable PnC. Secondary ServiceDetail selection,
charging profiles/tariffs/notifications and ancillary application behavior remain
separate work; the primary charging-service flow stays bounded and explicit.

## Typed service-parameter increment — 31/10/2026

Both DIN and ISO-2 now map all six bounded parameter value choices with explicit
physical units, signed-width checks and truncation/capacity rejection. ISO-2
groups multiple parameters per set; DIN preserves one parameter per generated
set. Real socket ServiceDetail responses exercise typed parameter data. Two
independent bundles extend the feature corpus to 29, preserving earlier cases.
See [host parameter contracts](../../simulation/linux/SERVICE_PARAMETERS.md).
Multiple offers, selection, charging profiles and tariffs remain open; the
parameter mapper does not silently treat arbitrary names as charging authority.

## Balanced three-phase AC increment — 31/10/2026

Explicit `iso2-ac3` now supports balanced AC over IPv4/IPv6 with optional TLS.
Phase-to-neutral voltage/per-phase current and total-power conventions are
explicit project metadata, checked in PermissionManager and HLC projection.
Unequal host phase caps select the lowest cap; invalid/mismatched phase offers
fail before energy. Two additional independent EXI comparisons preserve the
previous 25 feature and 50 baseline cases. See [host contracts](../../simulation/linux/AC_THREE_PHASE.md).
Asymmetric dispatch, richer charging profiles and production OBC acquisition
remain distinct; this increment does not close them.

## Managed consumer and scoped network increment — 31/10/2026

Managed CertificateManager proof now guards signed-receipt creation and pending
transport. Certificate version/fingerprint/key-version changes, invalidation and
expiry fail closed. Thirteen signed TLS session cases cover pinned and managed
AC/DC profiles, revocation, invalid time and invalidation before transmission.
Scoped IPv6 SDP/TCP, ingress-interface validation, all-nodes membership and
bounded retries are implemented with runner configuration. Loopback ingress and
retry execution are verified; actual physical-interface multicast delivery and
link-local TCP execution remain unverified in this environment.

The user excluded production integration from the remaining request. Continue
all independent host work in [HOST_COMPLETION_PLAN.md](HOST_COMPLETION_PLAN.md).
Missing production artifacts do not block the remaining host protocol features.
Older rows below preserve their historical scope; managed receipt binding and
host scoped network/retry implementation are superseded by this increment.

## Certificate-transaction increment — 31/10/2026

Implemented a portable bounded CertificateManager with HLC generation/stop
binding, copied candidate staging, key-version leases, compare-and-commit,
idempotent rollback, policy-gated deletion, recovery and required revalidation
after uncertain writes. A memory-backed two-slot NvM fault model and optional
real OpenSSL host chain/SAN/EKU/key-match/trusted-time/CRL validator now exist.
See [certificate contracts](../../simulation/linux/CERTIFICATE_TRANSACTIONS.md).

These supersede the absence of host certificate transactions and validation in
older rows below. Still open: signed-receipt consumer binding to managed proof,
actual PnC PaymentDetails/challenge and certificate installation/update wire,
contract/eMAID roles, encrypted key import and selected OEM root/time/revocation
policy. The fixed host DNS client-signing profile does not select those policies.
Supplier NvM/KeyM/Csm, durable storage/anti-rollback and target evidence remain
external gates. Broader AC/services/profiles, scoped multicast/SLAC, ISO-20/BPT
and physical interoperability/conformance remain incomplete.

## Signed-receipt increment — 31/10/2026

Implemented a restricted one-reference ISO-2 signed MeteringReceiptReq profile,
actual SHA256/P-256 body/SignedInfo signing and verification over TLS AC/DC,
and key-version leases through transmission/acknowledgement. Default unsigned
policy remains off. Malformed/unknown signature profiles and wrong keys fail
closed. The signed document and both fragments independently round-trip through
the pinned separate oracle; Python independently verifies digest/signature.
See [signed-receipt host contracts](../../simulation/linux/SIGNED_RECEIPTS.md).

This is an explicit fixture key-pinning profile. Chain provisioning, trusted-time,
revocation, contract/eMAID authorization, certificate transactions and general PnC
remain incomplete. Opaque meter-vendor signatures remain unverified/rejected.
Production profile selection is an external decision; no supplier ABI is invented.

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
| PnC / credentials | Restricted signed receipt EXI/TLS path with explicit pinned fixture keys; PnC feature gate remains disabled | PaymentDetails, certificate install/update, challenge binding, other signed message profiles, trusted contract identity and transactional credential lifecycle |
| Broader timing supervision | Configurable cumulative Authorization/CPD/CableCheck budgets plus exchange/session guards | Target watchdog/physical-stop integration and exact-edition configuration; select normative budgets from exact editions and vehicle timing work products |
| Services and schedules | ServiceDetail integer/string sets; bounded three tuples/five intervals with conservative selection | Multiple payment/service/mode offers, more parameter forms, tariffs/profiles, notifications and richer completion/schedule policy |
| Broader AC | Single-phase EIM through real IPv4/IPv6 sockets | Three-phase, phase-specific limits, actual OBC control/measurement and complete AC protocol feature coverage |
| Broader metering | ISO-2 unsigned opt-in and signed TLS host AC/DC receipt loops | DIN metering branches, authenticated meter-vendor data, OEM receipt/settlement policy and exact-edition qualification |
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
