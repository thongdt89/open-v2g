# Host TLS, crypto and processing supervision

Revision: Draft 0.1 — 28/10/2026. Implemented host development provider;
not an AUTOSAR supplier/PKI selection or protocol-conformance result.

## Build

The portable/default build still has no OpenSSL dependency. Enable the separate
Linux provider explicitly, with OpenSSL 3 development headers/libraries available:

```sh
cmake -S . -B build-security -DCMAKE_BUILD_TYPE=Debug \
  -DOPENV2G_ENABLE_OPENSSL_HOST=ON \
  -DOPENV2G_ENABLE_CBV2G_SAP_POC=ON \
  -DOPENV2G_CBV2G_SOURCE_DIR=/absolute/path/to/pinned/libcbv2g
cmake --build build-security --parallel
ctest --test-dir build-security --output-on-failure
```

Tests use Python `cryptography` to generate ephemeral credentials in a protected
temporary directory. The runtime is C/OpenSSL and does not depend on Python.
Private test keys are neither committed nor printed, and are removed after tests.
No dependency download occurs during configure/build.

This environment has OpenSSL runtime 3.0.13, Ubuntu package
`libssl3t64 3.0.13-0ubuntu3.16`. Matching `libssl-dev` was downloaded and extracted
under `/tmp/openv2g-openssl-dev/root` without installing system packages. Its deb
SHA-256 is `2b4734df0475e305ac03abb0a2cc5659651ed37ef02d967a1c4033a154cf714b`.
The tested configuration adds:

```sh
-DOPENSSL_INCLUDE_DIR=/tmp/openv2g-openssl-dev/root/usr/include \
-DOPENSSL_SSL_LIBRARY=/lib/x86_64-linux-gnu/libssl.so.3 \
-DOPENSSL_CRYPTO_LIBRARY=/lib/x86_64-linux-gnu/libcrypto.so.3
```

The extracted distro multiarch include directory is detected by CMake. These
paths/package versions describe local evidence, not a selected production stack.

## TLS provider and owner contract

| Operation / datum | Producer / caller | Consumer / owner | Contract |
|---|---|---|---|
| `Ov2g_TlsInit` | Context owner | Host TLS context | Initializes a fresh, unused object; not a replacement for cancel on a live context |
| `Ov2g_TlsStart` | Transport owner | Host TLS provider | Borrows current nonblocking socket; copies correlation/deadline; consumes supplied config files during call; rejects active-context replacement |
| `Ov2g_TlsPump` | Cooperative owner runnable | TLS context | Generation + connection must match; one nonblocking handshake step; own handshake deadline; stale calls do not mutate current context |
| `Ov2g_TlsRead` | Transport receive runnable | TLS context | Bounded output, exact current context; BUSY means retry; never returns plaintext from the underlying socket |
| `Ov2g_TlsWrite` | Transport TX owner | TLS context | Copies bounded input; retained retry buffer/length stay immutable until completion; mismatched retry rejected |
| `Ov2g_TlsCancel` | Current transport owner | TLS context | Idempotent for current context; releases transient library/key references and retained TX; socket lifetime stays with caller |
| `Ov2g_TlsHasData` | Transport receive runnable | TLS context | Read-only buffered-plaintext query; no socket operation or state mutation |
| `state` | TLS context writer | HLC completion facade | READY only after configured checks; CLOSED/FAILED cannot carry application data |
| `peer_verified` | TLS context writer | HLC completion facade | True only for a validated configured peer; cleared on release |
| `negotiated_version` | TLS context writer | Diagnostics/policy | Actual negotiated protocol version, not SDP evidence |
| `reason` | TLS context writer | Diagnostics | Bounded project reason enum |
| `certificate_result` | TLS context writer | Diagnostics | Numeric verification result; no certificate/key buffer pointer |

`station`/`evcc` and `station6`/`evcc6` can use TLS. IPv6 discovery advertises the
TLS-required bit and is followed by actual TCP/TLS before SAP/EXI. That bit does
not negotiate a TLS version. The backend enforces the configured exact TLS1.2 or
TLS1.3 version. DIN with TLS is rejected before session opening. HLC receives
TLS completion only from the real provider in these secured runner paths.
Failure/cancellation stops the session; there is no retry over plaintext.

The host policy uses ECDSA certificates/P-256, TLS1.2
`ECDHE-ECDSA-AES128-GCM-SHA256` and TLS1.3 `TLS_AES_128_GCM_SHA256`, with early data,
renegotiation and session resumption disabled. This is a deliberately explicit
AEAD **host profile**, not an assertion of the cipher/version requirements of a
particular DIN/ISO edition. Exact-edition compatibility remains to be selected.

The EVCC requires an explicit CA file and DNS/IP SAN identity; it never relies
on the default system trust store or skips verification. Host wall-clock time
checks certificate validity. An optional explicit CRL file enables offline
revocation checking. Online OCSP, OEM trust/time policies and HSM/provider
credentials remain open. The station CLI is server-authenticated EIM; mutual
certificate verification is available through the typed config and tested, but
does not establish PnC authorization.

OpenSSL WANT_READ/WANT_WRITE directions drive polling; buffered decrypted bytes
are drained without waiting for another kernel read event. A custom socket BIO
uses MSG_NOSIGNAL and does not change process-global signal policy. No callback
retains a caller TX/RX buffer. TLS retries use owned read/write storage; a read
that needs write-side maintenance must be completed before starting another write. Input certificate-chain size/depth are bounded by the
host configuration; system OpenSSL heap use and target WCET are not qualified.
Shutdown sends close_notify on a best-effort nonblocking basis, then releases
context; coordinated permission revoke/safe-state still precedes teardown.

The implementation follows the primary documentation for
[nonblocking retry/error handling](https://docs.openssl.org/3.0/man3/SSL_get_error/),
[peer identity checking](https://docs.openssl.org/3.0/man3/SSL_set1_host/), and
[EVP signing](https://docs.openssl.org/3.0/man3/EVP_DigestSignInit/).

Example with your development certificate/CA files, station first:

```sh
./build-security/simulation/linux/wire/openv2g_linux_exi station6 15118 iso2-ac tls12 server-chain.pem server.key
./build-security/simulation/linux/wire/openv2g_linux_exi evcc6 15118 iso2-ac tls12 test-ca.pem localhost
```

The EVCC can append a CRL file; `tls13` selects the alternate host TLS version.
Use `iso2` for DC, and `station`/`evcc` for the legacy IPv4 transport.

## Typed software crypto jobs

| Operation | Ownership and bounded behavior |
|---|---|
| `CryptoInit` | Initialize a fresh context; no active resources may be overwritten |
| `CryptoLoadPrivateKey` | Import/validate a software P-256 key under a versioned handle; pending users prevent replacement; private bytes never returned |
| `CryptoLoadPublicCertificate` | Extract/validate a P-256 public key under a versioned handle; this is not a certificate trust decision |
| `CryptoBeginGeneration` | Cancels/clears old queued/results on generation replacement, retaining credential handles; repeated same-generation begin does not reset jobs |
| `CryptoSubmit` | Four slots, <=4096 input bytes, copied buffers, unique current job ID, deadline; explicit SHA256/sign/verify operation |
| `CryptoPump` | One software job per owner turn; elapsed jobs fail; no callback/pointer into freed input |
| `CryptoTakeResult` | Correlated generation/job/key-version result, consumed once; SHA256 digest or P-256 raw `r || s` signature; no private bytes |
| `CryptoCancel` | Idempotent current-generation job cancel; stale-generation cancel rejected; canceled result remains until consumed |
| `CryptoClose` | Clear all jobs and key references; idempotent context teardown |

Function signatures/types are in the project headers `openv2g/linux/tls.h` and
`openv2g/linux/crypto.h`. Context arguments are INOUT; generation, connection,
job/key IDs, config, immutable input and timestamps are IN; read buffers,
written/received lengths and consumed results are OUT. Query contexts are
read-only IN. Config paths are consumed during TLS start/key import; caller file
buffers are not retained. The wire config itself is copied and its path/name
strings must remain valid through start. These are local C/project contracts,
not an imported supplier ABI or generated RTE port catalogue.

ECDSA uses EVP with SHA256; DER/raw conversion is internal and bounded. The
legacy underspecified `CryptoPort` remains a mock seam; the new typed provider
is PROJECT code, not a silently changed supplier Csm ABI. It supplies primitives,
**not** EXI canonicalization, XMLDSIG transforms, validated challenge lifecycle,
signed MeteringReceipt, PnC certificate/payment authorization or transactional
KeyM/NvM provisioning. Those features remain disabled/unimplemented.

## Cumulative processing budgets

| Operation | Caller / owner | Direction and failure semantics |
|---|---|---|
| `ConfigureProcessing` | Configuration caller / HLC supervisor | Config IN, HLC INOUT, result OUT; only idle; rejects invalid/duplicate kinds or zero configured bounds without partial update |
| `TickProcessing` | Scheduler / HLC supervisor | Generation/time IN, HLC INOUT, result OUT; inline timer ownership; expiry fails the active adapter and clears pending TX; stale generation does not mutate the current session |

The HLC supervisor owns one active cumulative timer configured before session
start through `OpenV2g_Iso15118Chrg_ConfigureProcessing`. Up to three explicit
limits cover Authorization, ChargeParameterDiscovery and CableCheck. Each limit
has elapsed-time and Ongoing-response-count bounds. Repeated requests retain the
original deadline; entering a different state starts that state's budget.

`TickProcessing` is called by the wire scheduler during polling, and the facade
checks the same deadline before accepting a response. Late Finished, excessive
Ongoing, stale generation and an early/stale processing timer event cannot revive
or abort another current stage. Timeout fails the adapter, clears queued and
dispatched requests and enters HLC error; the coordinator then owns safe teardown.
Start/stop clear transient timer state while retaining config. Legacy configs
may leave limits disabled; the wire profile explicitly uses a 2-second/8-response
budget for each of these stages. These are provisional **host** settings, not
normative timer values. Other setup/stop/physical watchdog budgets remain open.

## Verification scope

TLS tests exercise both versions, DNS/IP identity, mutual certificates, trust,
expiry/not-yet-valid, CRLs, protocol mismatch, cancel/timeout/stale contexts,
backpressure/immutable retry and peer disconnect without SIGPIPE. Eight additional
wire sessions cover encrypted AC/DC and security failures before any energy
transfer; CLI tests execute two complete encrypted sessions and reject DIN/TLS.
Crypto tests cover SHA256 KAT, two-way raw-signature interoperability with Python
cryptography, modified data/wrong key, bounds, queue overflow, cancellation,
key replacement exclusion, deadlines and stale generation/result consumption.
Processing-budget tests cover DIN/ISO-2 retries, count caps, late Finished,
per-state reset and old-generation/old-stage timer events.

ASan/UBSan instrument project and generated C code, not the installed OpenSSL
shared objects. Host success is not physical EVSE interoperability, a supplier
integration result, security certification, or standards conformance.
