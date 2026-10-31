# Certificate transactions and host validation

Draft 0.1 — 31/10/2026. Implemented host behavior; no supplier ABI, selected OEM
PKI profile, PnC authorization or production flash guarantees are established.

## Portable owner

`ISO15118Chrg::certificate_manager` is now a concrete bounded C owner rather than
a named subsystem placeholder. `BindCertificateManager` accepts PROJECT storage,
security and monotonic-clock ports while HLC is idle, once per initialized object.
HLC start begins its generation; closing/error/idle invalidate pending work,
validation proof and key leases. HLC supervision expires pending transactions.
The owner APIs are serialized runnable calls; this synchronous host seam does
not establish an asynchronous supplier Csm/KeyM/NvM binding. Call Close before
reinitializing a bound manager or destroying its borrowed port contexts. Release
manager leases before closing its crypto provider. Initialize HLC before binding.

A record contains at most three public DER certificates of 2048 bytes each,
a bounded identity, a monotonic storage version and opaque key handle/version.
Private key bytes never enter this record, the protocol DTO or the fake NvM.

- Stage copies and validates a candidate, checks generation/version/transaction
  ID/deadline and acquires its exact key-version lease. It does not replace the
  active reference. Transaction IDs increase within a generation.
- Commit checks the lease, proof and deadline again and uses compare-and-commit.
  The previous active lease is released only on successful replacement.
- Rollback releases only the matching candidate. Repeated rollback is idempotent;
  an old generation or transaction cannot cancel a newer transaction.
- Failed candidate validation preserves the existing active credential. Failed
  revalidation of that active credential invalidates its authority immediately.
- Any uncertain write, including a lost commit acknowledgement or late return,
  reloads the selected public record and invalidates authority. Only explicit
  successful revalidation can return a usable reference again.
- GetReference checks current generation, observed monotonic time, proof expiry
  and key version. Outputs are copied only on success. No permission is granted
  by the presence of a public record.
- Delete requires explicit configured policy and matching version, writes a
  versioned empty tombstone and releases the active lease. A new session does
  not reset the persisted record version or retain old validation authority.

The deterministic memory NvM model uses two copied slots, a selected-slot marker
and its inverse, and CRC32 over explicitly serialized fields. A staged inactive
slot is never automatically selected. Fault injection covers failure before
writing, failure after staging and lost acknowledgement after selecting the new
slot. Corrupt selected storage fails closed. CRC detects corruption; it provides
no authenticity, durable atomic-write or adversarial rollback guarantee.

## Optional OpenSSL host profile

`Ov2g_PkiBind` supplies actual chain-validation and opaque key-lease ports when
`OPENV2G_ENABLE_OPENSSL_HOST=ON`. Trust anchors and optional CRL are explicitly
provided files, with no system trust-store fallback. The configured DNS SAN must
match; CN fallback is disabled. The leaf must be a non-CA P-256 client certificate
with clientAuth EKU and digitalSignature key usage, and match the exact loaded
private-key handle/version. Chain verification uses OpenSSL authentication level
2 and depth 4. DER parsing consumes the entire bounded input.

Validation requires a fresh VALID generation-matched trusted wall-clock sample.
The caller owns its trust/acquisition policy; a station message is not implicitly
trusted time. The provider advances that sample by monotonic age. Proof lifetime
is bounded by configured TTL, time freshness, request deadline and leaf lifetime.
The entire path and configured CRLs are also checked at the proof horizon rounded
conservatively upward, so an intermediate or CRL cannot expire during that proof.
Absent/invalid/stale time, revoked/untrusted/expired/future certificates, wrong
SAN/EKU/key or malformed DER are rejected. CRL enforcement is selected explicitly
by providing a CRL; absence is not evidence of an OEM-approved revocation policy.

The fixed DNS client-signing profile is a host fixture, not the ISO-2 contract
certificate/eMAID role model. It does not enable PnC in the feature configuration
or implement PaymentDetails, challenge authorization, certificate-install/update
wire messages, encrypted contract-key import, OCSP, secure time acquisition,
HSM persistence or an approved root/revocation rotation policy. Signed receipts
still use explicitly provisioned fixture key pinning; they do not automatically
consume this manager's trust proof. Those bindings remain explicit next work.

## Verification

`openv2g_test_certificate_manager` covers copied staging, old-active retention,
rollback/cancel generation and transaction matching, expired pending work, late
validation, uncertain writes, recovery/revalidation, deletion policy, corrupt
storage and HLC lifecycle invalidation. It uses explicitly fake security ports.
`openv2g_test_linux_pki` runs real OpenSSL against ephemeral test CA/CRLs/keys,
including revoked, unrelated, expired, future, wrong-SAN and wrong-EKU cases;
trailing DER, mismatched key/version, old generation and invalid time quality
fail. A successful active lease blocks key reload until invalidation.

See the source hashes and executed build configurations in
[verification evidence](../../docs/architecture/evidence/certificate_transaction_verification.json).
No physical EVSE, target, supplier integration or conformance result is claimed.
