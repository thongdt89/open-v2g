# libcbv2g SAP / DIN / ISO-2 EXI host candidate

Revision: Draft 0.2 — 26/10/2026. Status: `VERIFIED_HOST_DC_EIM_CANDIDATE`.
Separate schema-specific backends execute actual generated EXI code through the
existing project codec C/S interface. They do not use the fake boundary codec.
Generated structs remain private; no supplier ABI is guessed or replaced.

## Reproduce

The candidate is opt-in and uses a pinned external checkout. No third-party
generated C or XSD files are vendored. CMake never downloads dependencies or runs
cbExiGen. The legacy option name is retained for compatibility.

```sh
git clone https://github.com/EVerest/libcbv2g.git out/deps/libcbv2g
git -C out/deps/libcbv2g checkout --detach 03350be048b35b179905129005a97144a4bdcf93
cmake -S . -B build-exi -DCMAKE_BUILD_TYPE=Debug \
  -DOPENV2G_ENABLE_CBV2G_SAP_POC=ON \
  -DOPENV2G_CBV2G_SOURCE_DIR="$PWD/out/deps/libcbv2g"
cmake --build build-exi --parallel
ctest --test-dir build-exi --output-on-failure
```

The source path must be absolute. Configure verifies both Git HEAD and every
file in the selected artifact `SHA256SUMS`. Changing an inspected file causes
reconfiguration and checksum failure. This verifies the compiled subset, not
all upstream output. No upstream CMake, FetchContent, C++ test framework or
ISO-20 source is selected. Project code uses strict warnings; generated-source
warnings are visible without promoting them to project errors.

[Artifact manifest](../../generated/libcbv2g/03350be048b35b179905129005a97144a4bdcf93/manifest.md)
records hashes, provenance gaps and evidence. Python 3 is used by optional
reference/differential tests, not by the C application runtime.

## API and runtime integration

Initialize project `SchemaRegistry` and `ExiCodec`, then bind the required
schemas **before** `ExiCodec_Start`:

- `OpenV2g_Cbv2gSap_Bind`: `OPENV2G_SCHEMA_SAP` and `OpenV2g_ExiSapDocument`.
- `OpenV2g_Cbv2gDin_Bind`: `OPENV2G_SCHEMA_DIN_70121` and `OpenV2g_ExiDcDocument`.
- `OpenV2g_Cbv2gIso2_Bind`: `OPENV2G_SCHEMA_ISO_15118_2` and the neutral DC DTO.

Use the codec's existing C/S encode/decode port. Decode needs separate
candidate/output objects and bounded caller-owned workspace. The backend
requires an exact neutral object size. Neither decode nor backend selection
activates an adapter or grants permission.

DC kinds cover SessionSetup, ServiceDiscovery, external-payment service
selection, EIM authorization, ChargeParameterDiscovery, CableCheck, PreCharge,
PowerDelivery START/STOP, CurrentDemand, WeldingDetection and SessionStop.
Request DTO IDs use the project semantic kind. Response DTO IDs add 100;
PowerDeliveryRes decodes to the shared START+100 ID because the standardized
response has no start/stop discriminator. The pending local request supplies
that context. These are project IDs, not standardized message tags.

The [new Linux wire runner](../../../../simulation/linux/wire/README.md) connects
these backends to real TCP/V2GTP and existing HLC SAP/protocol owners. RX passes
through the HLC generation/connection guard. The core's `exi_codec` lifecycle
placeholder is still not a supplier-generated service; the host runner binds a
separate concrete project codec. The original custom-frame smoke example remains
available. Do not confuse the two executables or infer target RTE integration.

## Bounded SAP profile

The generated SAP model holds five offers despite comments recording an original
schema maximum of 20; the project semantic config can hold eight. The backend
rejects more than five, never truncates. Namespace storage is 100 bytes but the
decoder appends a terminator, so text is limited to 99 bytes. The profile accepts
printable ASCII URI text, priorities 1–20, unique wire IDs, a selected ID on
success/minor-deviation and no selected ID on no-negotiation.
`wire_schema_id` is a peer identifier, **not** `OpenV2g_SchemaId`. Namespace and
version are explicit caller configuration, not inferred from untrusted payload.
SAP payload/workspace bounds are 1024/1025 bytes.

## Bounded DC profile

DIN and ISO-2 compile as separate mapper translation units against separate
generated schema structures. The shared neutral DTO is project-owned and exports
no generated types. Supported host profile: DC core, EIM, one charge service and
one flat schedule tuple/interval, no signatures/tariffs/metering receipts/AC/BPT.
The decoder rejects unsupported choices and profile constraints rather than
silently selecting a different schema. Supplemental informational fields not
projected into the neutral DTO do not become safety evidence.

Physical values use checked value/multiplier/unit conversion into millivolts,
milliamps and watts. Nonrepresentable values and unsupported units fail closed.
Profile maxima, schedule power/duration, service ID, tuple ID, status/isolation
and SessionID are exposed to the runner. Incoming limit-achieved indications or
unsupported dynamic maximum changes fail closed in this fixed test profile.
Payload/workspace bounds are 4096/4097 bytes. Generated documents and encode
staging buffers have bounded automatic storage; no heap is used by the adapter
or compiled candidate subset. Target stack/WCET suitability is not established.

## Bitstream containment

At this exact upstream revision, `exi_bitstream_has_overflow` can advance to
`byte_pos == data_size` and access one extra byte before reporting overflow.
The project leaves upstream sources unchanged. Owned guard storage absorbs that
access; consumed/produced length checks prevent treating it as input evidence
or publishing it as output. Full payload consumption and zero padding are
required by this host profile. Truncated input, oversized output and decode
failure never publish a partial object. This contains the observed issue; it
does not establish that all parser defects have been found.

## Verification — 26/10/2026

- Linux GCC 13.3 Debug: 24/24 checks with optional independent oracle, or 23/23
  without it. All 23 candidate checks pass with ASan/UBSan/LSan.
- SAP fixed upstream request/response examples verify decoded fields and exact
  encoded bytes; bounds, truncation, mismatch, response variants and stale
  lifecycle are tested.
- DC tests cover both directions for all 12 kinds and every byte truncation.
  Mutation/random corpus includes 5,000 SAP and 20,000 DC inputs plus SAP
  single-bit mutations; failures must preserve published output.
- Fifty project-generated payloads decode/re-encode byte-identically in a
  separately compiled pinned OpenV2G oracle. Fixed observations remain testable
  without checking out the oracle. See [oracle guide](../../../../tests/oracle/README.md).
- Full loopback sessions and twelve fault scenarios verify coordinator/HLC flow
  and continued supervision during network waits.
- Candidate/oracle revision and selected-source hashes are checked. CI is
  configured; no remote CI execution is claimed here.

These are host regression and differential references, not independently authored
normative golden vectors. Original schema/generator provenance, exact licensed
editions, legal acceptance, target compiler/MISRA/resources, long-running fuzzing,
physical interoperability and conformance remain separate gates. See the
[current evidence and closure inventory](../../../../docs/architecture/EXI_WIRE_IMPLEMENTATION_STATUS.md).
