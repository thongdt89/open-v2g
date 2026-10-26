# SAP candidate artifact manifest

Recorded: 26/10/2026. Classification: `HOST_CANDIDATE_ONLY`, not supplier selection,
schema legal approval, a formal freeze or target PoC acceptance.

| Item | Recorded evidence |
|---|---|
| Provider | EVerest / chargebyte `libcbv2g` |
| Source | https://github.com/EVerest/libcbv2g/tree/03350be048b35b179905129005a97144a4bdcf93 |
| Exact inspected commit | `03350be048b35b179905129005a97144a4bdcf93` |
| Version declaration | Upstream root CMake declares 0.3.1; no release tag is asserted |
| Generator record | Root CMake reports cbExiGen `871fcb3`; full generator commit/config has not been recovered or approved |
| Generated grammar | `appHand_*`, generated comments reference `urn:iso:15118:2:2010:AppProtocol` |
| Exact standard/XSD edition/corrigenda | OPEN: generated comments are not independently verified schema provenance |
| Schema source/checksums/license record | OPEN: no licensed/original XSD package supplied or copied |
| Upstream source license | Inspected root `LICENSE` is Apache-2.0; common/appHand files carry Apache-2.0 SPDX and chargebyte/EVerest notices |
| Legal/production distribution approval | OPEN; no organizational legal approval or standard/schema redistribution right is inferred |
| Features compiled | Six common runtime C files and three generated appHand C files; SAP only |
| Generation step | None executed. Existing upstream-generated snapshot used externally for host evaluation |
| Source modification | None; adapter contains the one-extra-byte bitstream access in guarded storage |
| Hash scope | `SHA256SUMS` covers compiled C files, all relevant common/appHand headers, upstream LICENSE, generator/version declaration and vector source |
| Aggregate digest | SHA-256 of the adjacent `SHA256SUMS`, embedded by CMake in the schema descriptor |
| Profile | Five offers, 99 ASCII namespace bytes, 1024-byte payload; see adapter README |
| Compiler | Linux GCC 13.3, Debug, project adapter/tests `-Wall -Wextra -Wpedantic -Werror`; generated C `-Wall -Wextra -Wpedantic` |
| Host evidence | 20/20 CTest executables with candidate enabled; dedicated SAP ASan/UBSan/LSan corpus passes; checksum/revision negative gates pass |
| Independent golden/oracle evidence | OPEN; fixed vectors come from the same upstream codec's tests |
| Target compiler/deviations | OPEN |
| ROM/RAM/maximum stack/workspace/WCET | Target measurement OPEN; caller workspace bounded at 1025 bytes, generated document and encode scratch on bounded automatic storage |

The generated directory contains **metadata only**, not vendored generated C or
XSDs. A separate upstream checkout is required to build. The source license is
recorded as observed evidence; it does not close OP-006/OP-007 or select a
production standard/schema/supplier baseline. Preserve upstream license and
notices whenever distributing the external source or resulting binaries.

Fixed test vectors in the project SAP test were transcribed from
`tests/app_handshake/app_handshake.cpp` at the pinned commit. That source is
included in the hash list. No independent encoder or licensed standard test
suite was used.
