# Separate-codec EXI differential verification

Revision: Draft 0.1 — 26/10/2026. Host verification only.

The optional oracle is the independently implemented Siemens/OpenV2G codec at
[commit 733d4f43cb6579c705d59c7386786ce12a0a0026](https://github.com/Martin-P/OpenV2G/tree/733d4f43cb6579c705d59c7386786ce12a0a0026).
Its README identifies version 0.9.6 and LGPL-3.0-or-later terms. It is compiled
as a **separate test executable**, never linked into the application or
libcbv2g candidate. No external source is vendored here or installed by the
application build. Preserve the external project's license/notice obligations
when using/distributing it; this record is not legal approval.

With the candidate source already checked out:

```sh
git clone https://github.com/Martin-P/OpenV2G.git out/deps/openv2g-oracle
git -C out/deps/openv2g-oracle checkout --detach 733d4f43cb6579c705d59c7386786ce12a0a0026
cmake -S . -B build-exi -DCMAKE_BUILD_TYPE=Debug \
  -DOPENV2G_ENABLE_CBV2G_SAP_POC=ON \
  -DOPENV2G_CBV2G_SOURCE_DIR="$PWD/out/deps/libcbv2g" \
  -DOPENV2G_ORACLE_SOURCE_DIR="$PWD/out/deps/openv2g-oracle"
cmake --build build-exi --parallel
ctest --test-dir build-exi --output-on-failure
```

CMake checks the oracle commit and all relevant original source/header hashes
in `SHA256SUMS`. It copies external sources into the build directory and changes
only `DEPLOY_DIN_CODEC` from `SUPPORT_NO` to `SUPPORT_YES` in the copied DIN
header; the checkout remains unchanged. This is test configuration, not a
schema generator. `iso1` in that source implements the 2013 namespace matching
the candidate ISO-2 grammar; the distinct external `iso2` model is not used.

`openv2g_exi_vector_dump` constructs 50 bounded project DTO examples and emits
candidate EXI. `openv2g_independent_oracle` receives each as binary stdin and
independently decodes/re-encodes it. The test requires a byte-identical result.
`independent_vectors.json` records those observed independent-codec outputs and
provenance; the fixed-reference test also runs without an oracle checkout.

This is differential encode/decode evidence. The inputs are candidate-constructed,
not independently authored normative golden messages. The comparison does not
prove untested branches, selected licensed schema provenance, full protocol
conformance or interoperability with a physical EVSE. The external oracle is
used only on well-formed fixture inputs; candidate malformed-input tests do not
feed arbitrary hostile bytes into an unaudited reference process.

## AC and metering extension — 26/10/2026

`feature_vectors.json` adds 19 candidate-constructed AC/metering examples. They
were decoded and re-encoded byte-identically by the same separate pinned oracle.
Test profile labels `iso2-ac` and `iso2-meter` map explicitly to oracle `iso2`
(the external iso1 model); they do not introduce new production schema IDs.
The original 50-vector corpus is unchanged. These are differential regression
vectors, not independently authored standards vectors or signature evidence.
