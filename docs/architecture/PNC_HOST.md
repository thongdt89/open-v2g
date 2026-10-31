# ISO-2 PnC host authorization

Draft 0.1 — 31 October 2026. Implemented host behavior; no production PKI,
AUTOSAR deployment, normative conformance or physical interoperability claim.

The explicit `OvWireClient_InitPncMode` constructor enables a host-only PnC
capability. `SetPncSigner` binds the portable CertificateManager, persisted public
record, opaque private-key handle/version and trusted-time port. TLS must be
configured before this binding. Ordinary EIM constructors and default feature
flags remain unchanged. PnC-required clients refuse to run without the binding
and TLS; failures never fall back to EIM.

## Current path

SAP → SessionSetup → ServiceDiscovery → ServiceDetail → Contract selection →
PaymentDetails → signed Authorization → charge parameters → existing AC/DC
charging and stop path. Optional Authorization Ongoing keeps the original nonce
and absolute authentication deadline. Signed metering receipts reuse the
validated contract identity through fresh CertificateManager proof and key
leases. One- and balanced three-phase AC use the existing electrical limits.

The station validates the public chain/identity, imports only the public P-256
key into its crypto service, leases its exact version and generates a fresh
16-byte OpenSSL nonce. It verifies the signed Authorization fragment and exact
session/nonce before accepting charging requests. Finished authorization consumes
the nonce; replay and another PaymentDetails in the same station session fail.
All paths release leases and clear transient nonce/signature data on cleanup.

The portable PncContext correlates session generation, PaymentDetails transaction,
nonce, registered signing job, certificate storage version/fingerprint, key
handle/version, digest and each Authorization transaction. Invalid or stale
callbacks cannot authorize a session. HLC remains an evidence/request producer;
ChargingCoordinator retains charging progression and permission ownership.

## Explicit provisional host security profile

- Contract usage is distinct from the existing DNS client-signing role. Imported
  AUTOSAR datatypes are untouched; this metadata belongs to PROJECT records.
- Host identity is exactly 15 uppercase/alphanumeric characters, first two letters;
  an exact unique subject CN must match the configured whitelist and wire identity.
  This is a restricted host fixture policy, **not complete normative eMAID or OEM
  contract policy**. TLS server identity remains SAN-based.
- Chain/trust/time validation requires non-CA P-256 leaf, digitalSignature and
  clientAuth EKU. CRLs are enforced when configured; tests configure a CRL. This
  implementation does not fetch online revocation or select production roots.
- XMLDSIG accepts one matching body-ID reference, Canonical EXI, SHA-256 and raw
  P-256 signature only. Header session correlation is checked independently;
  the header is not included in the signed Authorization body fragment.
- Authentication has a provisional 500 ms host exchange budget capped by the
  credential proof expiry. Ongoing never extends it. Active authorized sessions
  refresh proof through a fresh trusted-time validation before expiry; an already
  expired or invalid reference fails closed. Peer timestamps are telemetry only.
- Public certificate DTOs permit at most three chain entries; PaymentDetails wire
  entries are limited to 800 bytes each and the complete payload to 4096 bytes.
  Local opaque key references and storage version are never serialized.

CertificateInstallation/Update, encrypted private-key import and their multi-reference
signatures are still separate remaining host work. This path starts from a locally
provisioned contract record and does not claim those features.

## Verification

Portable context tests cover stale generation, wrong session/correlation/job/key,
zero/repeated challenge, absolute expiry, clock rewind, cancel and Ongoing.
Host PKI tests cover wrong role/CN/key, revocation, invalid trusted time, DER bounds
and lease-protected reload. Socket tests cover IPv4/TLS1.2 and IPv6 SDP/TLS1.3,
DC/AC, Ongoing, three-phase AC, signed receipts and rejection before energy.
Station probes cover signature tampering, wrong session, consumed nonce replay,
nonce freshness across sessions and deadline expiry. An independent Python
implementation verifies the digest/P-256 signature; a separately built pinned
codec round-trips the Authorization document and both signed fragments.
