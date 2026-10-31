# ISO-20 generated-model qualification candidate

Draft 0.1 — 31 October 2026. This separate optional target compiles the pinned
CommonMessages, AC and DC generated models. It does **not** enable ISO-20 protocol
adapters, SchemaRegistry bindings, charging sessions, BPT or production support.

Candidate commit: `03350be048b35b179905129005a97144a4bdcf93` from the same external
libcbv2g checkout as the earlier host codecs. `SHA256SUMS` checks all 18 selected
headers and implementation files at configure time. The outer build also checks
the exact checkout revision and common-runtime source manifest. ACDP and WPT
are deliberately not part of this candidate. No claim about exact licensed XSD
edition, selected supplier provenance or upstream conformance follows from these
filenames or source hashes.

Build the isolated candidate:

```sh
cmake -S . -B /tmp/openv2g-iso20-build \
  -DOPENV2G_ENABLE_CBV2G_SAP_POC=ON \
  -DOPENV2G_ENABLE_CBV2G_ISO20_POC=ON \
  -DOPENV2G_CBV2G_SOURCE_DIR=/tmp/openv2g-libcbv2g
cmake --build /tmp/openv2g-iso20-build --target openv2g_test_iso20_models -j4
ctest --test-dir /tmp/openv2g-iso20-build -R iso20_models --output-on-failure
```

The internal probe constructs Common SessionSetup, AC ChargeParameterDiscovery
and DC PreCharge documents, checks selected decoded values and every byte-prefix
truncation, and rejects the tested AC document with the DC decoder. Successful
raw decode is additionally checked for exact consumed length and zero padding.
The common runtime can access a guard byte at its end boundary, so probe buffers
include a guard byte. Future public wrappers must keep guarded owned workspace,
strict size/choice/bound checks, schema/message correlation and no partial output;
these qualification objects are not a safe public generic serialization API.

The probe is candidate encode/decode self-consistency evidence only. The older
separate DIN/ISO-2 oracle has no ISO-20 model and is not used as an ISO-20 oracle.
No independent ISO-20 interoperability evidence exists here yet. Next work is
bounded PROJECT DTO mapping with separate Common/AC/DC bindings, then session
ownership and scheduled/dynamic EIM/BPT runtime paths. Feature defaults stay off.
