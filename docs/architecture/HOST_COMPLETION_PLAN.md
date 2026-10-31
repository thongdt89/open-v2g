# Remaining host implementation plan

Draft 0.1 — 31/10/2026. User scope: complete remaining features except production
integration. This is an execution inventory, not a frozen release/conformance
claim. Use pinned generated models and explicit provisional host policies;
missing supplier/OEM/vehicle artifacts do not block independent host work.

- [x] Portable certificate transactions, fake NvM and optional real host PKI.
- [x] Managed certificate proof in signed-receipt consumers; in-flight expiry and invalidation tests.
- [x] Scoped multicast/link-local IPv6 discovery/TCP, bounded retry and runner configuration (real physical-interface multicast/link-local execution remains unverified).
- [ ] PEV-SLAC bounded host peer simulation and owner integration, distinct from SDP.
- [x] All bounded DIN/ISO-2 parameter forms and ISO-2 multi-parameter sets.
- [ ] Broader ISO-2 payment/service/mode offers and selection.
- [x] ISO-2 three-phase AC, balanced phase limits and explicit invalid-phase rejection (asymmetric dispatch remains open).
- [ ] ISO-2 charging profiles, richer schedule/tariff and completion/notification handling.
- [ ] DIN metering branches where represented by the pinned model.
- [ ] Host contract roles/eMAID and PaymentDetails/challenge signature path.
- [ ] Host certificate installation/update wire and encrypted key import transaction.
- [ ] ISO-20 Common/AC/DC source hashes and separately selected bounded EXI backends.
- [ ] ISO-20 scheduled/dynamic EIM socket sessions and negative coverage.
- [ ] ISO-20 AC/DC BPT wire loops with explicit simulated capability, measured zero and new permission on reversal.
- [ ] Expand malformed/fuzz/coverage and independent cross-codec evidence to new schemas/features.
- [ ] Refresh status/architecture/traceability docs and commit completed increments.

Excluded production work: supplier ARXML/BSW/HSM/NvM/modem deployment, actual
vehicle OBC/contactor/measurement acquisition, vehicle safety sequencing/WCET,
selected OEM PKI provisioning and production approvals/certification/physical
interoperability. Host policies and simulator capability must remain explicit;
none may silently become a vehicle safety allocation or normative standard value.
