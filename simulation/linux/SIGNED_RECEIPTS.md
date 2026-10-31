# Restricted ISO-2 signed receipt host profile

Revision: Draft 0.1 — 31/10/2026. Extends the host implementation after `1dcea23`.
This is an optional OpenSSL/libcbv2g Linux implementation, not a selected OEM
PKI profile, supplier Csm/KeyM ABI or formal ISO 15118 conformance result.

## Implemented behavior

The ISO-2 mapper supports a bounded XMLDSIG header only for MeteringReceiptReq:
one local URI referencing that body's ASCII NCName Id, SHA-256 digest,
schema-generated EXI fragments and a raw 64-byte P-256 ECDSA signature. Digest,
transform and signature algorithms are fixed. External references, multiple
references, XPath/ANY extensions, KeyInfo/Object, extra signature/SignedInfo IDs,
wrong body/reference identity and wrong digest/signature lengths are rejected.
Other signed message kinds remain unsupported. DIN rejects this profile.

The body fragment includes session, schedule and copied MeterInfo. SignedInfo
binds the body Id, transform and digest. Verification recomputes the body digest
before checking the signature against the configured public-key handle. The
ordinary EXI decoder reports signature presence/data; it does not report trust.
Unverified data cannot become `signature_ready` protocol evidence by decoding it.

The selected method URIs/raw-signature convention were cross-checked against
[the EcoG reference implementation](https://github.com/EcoG-io/iso15118/blob/main/iso15118/shared/security.py).
That unpinned research link is not a supplier or normative baseline. The compiled
generated model remains pinned at `03350be048b35b179905129005a97144a4bdcf93`.
The separate oracle at `733d4f43cb6579c705d59c7386786ce12a0a0026` independently
round-trips the signed document, receipt fragment and SignedInfo fragment.
Exact-edition canonical EXI/signature-profile qualification remains external.

## Policy and lifecycle

`OvWireClient_SetReceiptSigner` explicitly configures a private-key handle and a
borrowed crypto service before a session. TLS must already be configured; actual
signing also requires a completed verified TLS peer. The default unsigned-receipt
policy remains false. Signature failure never invokes an unsigned fallback.
Opaque meter-vendor SigMeterReading values remain unverified and fail closed;
signing an EVCC receipt is distinct from authenticating the meter's measurement.

A prepared receipt has copied bounded data and generation/source correlation,
body digest and key version. A bounded key-version lease prevents replacement
between signature preparation, socket transmission and receipt acknowledgement.
The token is consumed once. Acknowledgement and stop/cancel retire the prepared
record and lease. New generations invalidate old crypto jobs/leases. Late old-
generation release cannot affect a current lease, and counters fail on overflow.

The station verifies signatures using an explicitly provisioned expected public
key. The test fixture pins that key out of band; no key from message KeyInfo is
accepted. `CryptoLoadPublicCertificate` imports a key and does not establish chain
trust, validity, revocation or contract identity. OEM chain/provisioning/time/
revocation policy, eMAID binding, credential transactions, PnC authorization,
settlement policy and target HSM integration remain separate unfinished work.

## PROJECT operations

| Operation | Parameters / ownership | Result and failure semantics |
|---|---|---|
| `Cbv2gIso2_ReceiptFragment` | IN receipt/body Id, OUT bytes/length; bounded caller storage | Encodes the supported body only; no partial byte publication on failure |
| `Cbv2gIso2_SignedInfoFragment` | IN Id/digest, OUT bytes/length | Fixed one-reference algorithm profile; invalid/big Id rejected |
| `OvWireSignReceipt` | IN service/generation/key/job/deadline; INOUT owned receipt; OUT key version | Two bounded copied-input crypto jobs; publishes signature only on success |
| `OvWireVerifyReceipt` | IN public-key handle and copied receipt | Recompute digest then verify; no certificate trust decision |
| `CryptoAcquireKeyLease` | IN generation/handle/version, OUT lease Id | Four bounded leases, exact current version; no private bytes returned |
| `CryptoKeyLeaseIsCurrent` | IN generation/lease/handle/version | Reject missing, stale or version-mismatched lease |
| `CryptoReleaseKeyLease` | IN generation/lease Id | One-use release; stale generation cannot release current resources |
| `OvWireClient_SetReceiptSigner` | IN borrowed crypto service and key handle | Explicit TLS-only configuration before the first generation |
| `OvWireStation_RunSigned` | IN TLS configuration and pinned verification service/key | IPv4 station; signed receipts required; unsigned/wrong key rejected |
| `OvWireStation_RunDiscoveredSigned` | Same plus IPv6 SDP sockets/port | Real unicast IPv6 discovery followed by TLS and EXI |

The crypto service object must outlive the client run/resume and be serialized
by its owner. Keys remain versioned opaque service handles. Host software EVP
work is not a target WCET/partitioning result; production callback/runnable
binding still requires the selected platform artifacts.

## Reproduce host checks

Use the OpenSSL/generated-code build described in [TLS_CRYPTO.md](TLS_CRYPTO.md):

```sh
ctest --test-dir build-exi --output-on-failure -R 'signature|wire_signed|linux_crypto'
```

Ephemeral test keys live only in a private temporary folder and are deleted after
the run. Independent Python cryptography checks the signature and body digest.
With the separate oracle configured, it also checks all three EXI representations.
Malformed profile tests cover algorithm/reference/Id/cardinality/length changes
and unchanged caller output on failure. Crypto tests cover queue/lease exhaustion,
cancel/generation/key-version boundaries and overflow. Six station sessions cover
AC/DC, IPv4 TLS1.2, discovered IPv6 TLS1.3, wrong signing keys and key-reload attempts
during transport. Wrong-key rejection stops the simulated energy path with DENY.

Observed final checks: **40/40** normal, **38/38** ASan/UBSan, **34/34**
generated-code/OpenSSL-OFF and **25/25** default-build tests passed.
[Verification snapshot and source hashes](../../docs/architecture/evidence/signed_receipt_verification.json).
