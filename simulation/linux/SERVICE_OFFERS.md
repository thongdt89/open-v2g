# Multiple ISO-2 service and payment offers

Draft 0.2 — 31/10/2026. Bounded host behavior, not PnC authorization or standards
conformance. Existing DIN wire remains its explicitly closed DC EIM profile.

The ISO-2 codec now retains both payment options, all six energy-transfer-mode
enumerators, primary charging-service metadata and up to four additional named/
scoped services. It maps up to four selected services and optional signed int16
ParameterSetIDs. IDs, enum values, duplicate entries, capacities, string bounds
and value-choice validity are checked. Imported/generated types are unchanged.
Zero offer/selection counts retain the earlier DTO construction defaults; decoded
wire objects always retain their actual explicit counts and payment choices.

The real runner explicitly selects external payment and its configured DC-core,
single-phase AC or three-phase AC mode from the offered list. The first item in
a mixed-mode list does not silently choose the vehicle's mode. DC extended/combo/
unique modes are retained as advertisements; those additional DC operating
profiles are not automatically enabled by their presence. A contract-only offer
fails for this EIM runner; it does not execute a PnC attempt or downgrade policy.
Mapping a Contract selection is serialization, never authorization evidence.

`OvWireClient_SetSelectedServices` copies an optional ancillary-service whitelist
before the first generation, with an optional primary ParameterSetID. The runner
retains the correlated offer and ServiceDetail parameter entries in owned
storage, binds them to the generation and a provisional two-second host validity
budget, and checks them immediately before transmitting its selection. Requested
ancillary IDs must have been offered, and a primary parameter set must have been
returned in its correlated ServiceDetail response. Up to three selected ancillary
services with a requested ParameterSetID each receive a separate ServiceDetail
transaction. The portable adapter owns this bounded loop; each request carries
its service ID and fresh correlation. SessionContext validates the exact pending
service ID without overwriting the primary charging-service identity. The runner
copies each service's parameters into separate owned storage and checks a selected
set against that service only. The generation and offer expiry also apply to all
secondary details. Ancillary service selection does not itself implement an Internet
service, certificate provisioning or billing application.

Stop clears primary and secondary offer/parameter evidence; resume obtains new offers/details.
The station fixture validates selected external payment, service IDs and its
known primary parameter set. Socket tests cover mixed offers for DC and AC3,
valid ancillary/parameter selection, and rejection before energy of contract-only,
unoffered mode, unoffered service and unknown parameter set.

## Pinned generated-source correction

The independent decoder exposed a maximum-cardinality disagreement: the pinned
libcbv2g encoder/decoder continued the two-bit repeat/end grammar after the sixth
SupportedEnergyTransferMode item. The separate pinned OpenV2G model switches to
a one-bit end-only event at that maximum. A project overlay corrects precisely
that encoder/decoder terminal case and now independently round-trips identically.
A captured pre-correction candidate payload is a permanent rejection regression.

`patches/iso2_transfer_mode_cardinality.py` checks exact upstream source hashes
and replacement counts and writes **build copies only**. The external pinned
checkout and its source manifest are unchanged. CMake validates the original
manifest first, includes the overlay script digest in the compiled package digest,
and records original/compiled source hashes in `iso2_overlay/patch_manifest.json`.
The ISO-2 implementation ID explicitly identifies the cardinality overlay. The
optional generated-code build now requires Python during configuration to apply
this deterministic overlay. Default generated-code-OFF builds remain independent.
This is a tested project correction, not an assertion that the full upstream
generator or every schema cardinality has been qualified.

Verification includes the dedicated offer codec tests and truncations, all prior
50 baseline/29 feature references, two new independent offer/selection vectors,
and 51 existing-plus-expanded socket scenarios. See
[executed evidence](../../docs/architecture/evidence/service_offer_verification.json).

## Secondary detail increment

The socket fixture now exercises one and three ancillary detail transactions,
then selects the verified ParameterSetIDs. Negative cases include an unknown
secondary set and a response for a different service; both stop before energy.
Context tests verify that a correct secondary response preserves the primary ID
and that a wrong-service response leaves the context unchanged. No parameter
name or value is treated as permission or as an implemented ancillary application.
