# libcbv2g SAP EXI host candidate

Revision: Draft 0.1 — 26/10/2026. Status: `VERIFIED_HOST_SAP_CANDIDATE`.
Implements the first generated-backend slice of the EXI proposal: SAP request
and response encode/decode through the existing `OpenV2g_ExiCodecCsPort`.
This backend executes generated EXI code; it does not use the fake test codec.
DIN and ISO-2 message backends, session mapping and target acceptance remain open.

## Reproduce

The candidate is opt-in and uses a separate, pinned upstream checkout. No
third-party generated sources or schema files are copied into this repository.
CMake never downloads dependencies or runs the generator.

```sh
git clone https://github.com/EVerest/libcbv2g.git out/deps/libcbv2g
git -C out/deps/libcbv2g checkout --detach 03350be048b35b179905129005a97144a4bdcf93
cmake -S . -B build-exi -DCMAKE_BUILD_TYPE=Debug \
  -DOPENV2G_ENABLE_CBV2G_SAP_POC=ON \
  -DOPENV2G_CBV2G_SOURCE_DIR="$PWD/out/deps/libcbv2g"
cmake --build build-exi --parallel
ctest --test-dir build-exi --output-on-failure
```

The checkout path must be absolute. CMake verifies both Git HEAD and every file
in the selected artifact `SHA256SUMS`. Changing an inspected file causes
reconfiguration and a checksum failure. This guards the compiled SAP/common
subset, not all upstream files. Generated sources build as a separate static
library; no upstream CMake, FetchContent, C++ test dependency, DIN/ISO-2/ISO-20
source or target is selected. Project adapter/test code uses strict warnings;
upstream warning diagnostics are enabled without promoting them to errors.

See [artifact manifest](../../generated/libcbv2g/03350be048b35b179905129005a97144a4bdcf93/manifest.md)
for provenance, hashes, unknown inputs and evidence classification.

## Bind and use

Initialize a project `OpenV2g_SchemaRegistry` and `OpenV2g_ExiCodec`, then call
`OpenV2g_Cbv2gSap_Bind(&codec)` **before** `OpenV2g_ExiCodec_Start`.
Create/use the existing C/S port for encode/decode. The test source demonstrates
all requests. Typed objects are `OpenV2g_ExiSapDocument`; encode and decode must
supply exactly that object size. Decode supplies separate candidate/output
objects and at least `OPENV2G_EXI_SAP_WORKSPACE_SIZE` bytes of workspace.

- Select `OPENV2G_SCHEMA_SAP` explicitly.
- Message IDs `OPENV2G_EXI_SAP_REQUEST` / `RESPONSE` are project DTO IDs.
- `wire_schema_id` is the one-byte peer offer/selection identifier. It is **not**
  the project's `OpenV2g_SchemaId`; no cast or automatic mapping is provided.
- Namespace strings and major/minor versions are explicit caller inputs.
  No protocol edition, version, namespace or offered schema is inferred.
- Generated structs/headers remain private to `exi_cbv2g_sap.c`.
- SAP decode does not activate a protocol adapter or grant energy permission.

The current `ISO15118Chrg` facade retains its lifecycle placeholder and semantic
SAP take/response API. The Linux station smoke flow also remains on its custom
semantic frame. Neither silently switches to the new backend. A later mapper
must preserve generation/request correlation, associate peer wire IDs with the
recorded offered schema, and feed the existing SAP owner; stale or unoffered
selections must fail closed. Full protocol/generated-wire integration is a
separate proposal step after SAP acceptance.

## Bounded host profile and upstream findings

The pinned generated model has capacity for **five** offers. Its comments record
an original schema maximum of 20; the existing project semantic config can hold
eight. This backend rejects more than five; it never silently truncates them.
Generated namespace storage is 100 bytes but the decoder appends a terminator,
so this adapter permits **99 text bytes**. No project-wide catalogue capacity is
changed. The profile accepts printable ASCII namespace bytes without spaces,
priorities 1–20 and unique wire schema IDs. It requires a selected schema on a
success/minor-deviation response and none on a no-negotiation response. These
are explicit host profile checks, not a selected normative edition baseline.

Maximum EXI payload is 1024 bytes. Decode copies input into 1025 bytes of
caller-owned guarded storage; it does not cast away the caller's input constness.
The generated document has compile-time bounded automatic storage. Encode uses
a bounded automatic 1025-byte staging buffer. There is no heap allocation in
the adapter or compiled upstream subset. Stack/WCET suitability for an ECU is
not measured or claimed.

At this exact upstream revision, `exi_bitstream_has_overflow` can advance to
`byte_pos == data_size` and read/write one extra byte before reporting overflow.
The adapter does not change upstream files. It contains that access in owned
guard storage, checks actual consumed/produced length, rejects truncated input,
requires complete payload consumption and zero final padding, and publishes
only on success. The guard byte is never delivered as input evidence or output.
A future upstream update needs review and new hashes; this containment is not a
claim that all upstream parser defects have been found or resolved.

## Verification — 26/10/2026

- Linux GCC 13.3 Debug: all **20/20** host test executables pass with the option ON.
- The SAP test checks both fixed upstream vectors byte-for-byte on encode and
  decoded fields on decode, all byte truncations of those vectors, trailing
  data/padding, bad header, expected-message mismatch, tiny output buffers,
  maximum collections/string/version bounds, duplicate IDs, response variants,
  stale generation and lifecycle rejection.
- Mutation corpus: 288 single-bit mutations plus 5,000 deterministic random
  inputs up to 1024 bytes, checking no partial output publication on failure.
- SAP test passes with AddressSanitizer, UndefinedBehaviorSanitizer and
  LeakSanitizer, instrumenting both upstream generated code and the adapter.
- Configure negative checks reject a wrong Git HEAD and a modified generated header.

The vectors originate from the **same pinned upstream codec's** tests. They are
useful fixed regression references, not independent golden vectors or proof of
protocol conformance. The request fixture contains an ISO-20 namespace as test
data; it does not enable ISO-20 support. Schema package/license approval,
independent differential oracle, long-running fuzzing, exact generator/schema
provenance, target compiler/MISRA, resources, interoperability and conformance
remain OPEN. Sanitizer execution needed an approved elevated command because
the review sandbox's tracing prevented LeakSanitizer startup.
