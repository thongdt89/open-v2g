# Chỉ mục tài liệu và thứ tự đọc

Các file có tên `EVCC_AUTOSAR_*` bên dưới là bản nguồn hoặc bản chuyển nội dung nguồn. Bản bàn giao không tự cập nhật thiết kế bên trong.

| Ưu tiên sử dụng | File | Document revision | Nội dung / nơi tra cứu |
|---|---|---|---|
| Working proposal — tiếp tục khi open points chưa chốt | [Provisional Baseline Proposal](PROVISIONAL_BASELINE_PROPOSAL_VI.md) | Draft 0.1, 09/09/2026 | Phạm vi G0–G1 được phép, hard invariants, xử lý tạm `OP-001`…`OP-011`, milestone, risk và điều kiện thay thế giả định; không phải freeze approval |
| EXI implementation proposal | [EXI Implementation Proposal](architecture/EXI_IMPLEMENTATION_PROPOSAL_VI.md) | Draft 0.2, 11/09/2026 | Candidate selection, typed EXI/schema boundary, generated artifact layout, license/resource/conformance gates; không phải supplier selection hoặc target evidence |
| Provisional ASR interface style | [Provisional ASR Interface Style](architecture/PROVISIONAL_ASR_INTERFACE_STYLE.md) | Draft 0.4, 15/09/2026 | Quy ước và mapping C/S, S/R, queued event cho coordinator, support, HLC, transport, EXI và protocol request/context; chuyển sang RTE/ARXML sau này |
| DIN/ISO-2 protocol data flows | [Protocol Data Flows](architecture/PROTOCOL_DATA_FLOWS_DIN_ISO2_VI.md) | Draft 0.1, 15/09/2026 | Main session/DC/AC/metering/stop flows, per-message data projection và mapping vào C/S–S/R hiện có; wire conformance còn ở G2 |
| G1 implementation — host/SIL skeleton | [Host/SIL Skeleton Architecture](architecture/HOST_SIL_SKELETON.md) | Draft 0.10, 20/09/2026 | C11 repository layout, implemented host behavior, ASR-style ports, correlated TCP/TLS lifecycle, SAP và request-generating DIN/ISO-2 semantic FSMs/data flows, AUTOSAR/protocol replacement seams và test mapping; không phải target evidence |
| G1 traceability — interface/code/test | [Interface Implementation Traceability](architecture/INTERFACE_IMPLEMENTATION_TRACEABILITY.md) | Draft 0.8, 20/09/2026 | Bao phủ 218 atomic interface rows theo subcomponent; phân biệt implemented/partial/stub/disabled/blocked G2 |
| 1 — hai composition và atomic interfaces | [Atomic Interface Specification](EVCC_AUTOSAR_Two_Main_Compositions_Atomic_Interface_Spec_VI.md) | Rev.3, 08/09/2026 | §§1–2 scope/rules/map; §§3–7 coordinator; §§8–23 HLC và sub-components; §§24–28 cross-composition/freeze; §§29–35 datatypes; §36 applicability |
| 2 — behavior chi tiết | [Detailed Compositions](EVCC_AUTOSAR_Two_Main_Compositions_Detailed_Description_VI.md) | Rev.1, 08/09/2026 | §3 ChargingCoordinator, §4 ISO15118Chrg, §5 interactions, §8 open points |
| 3 — kiến trúc toàn stack và supporting modules | [Charging Architecture](EVCC_AUTOSAR_Charging_Architecture.md) | Rev.4, 07/09/2026 | §9 safety SWCs; §10 HLC layers; §11 protocol adapters; §12 PLC/link/modem; phần requirements/AUTOSAR boundary phía sau |
| Theo chủ đề — scope, resources, roadmap | [Project Overview](EVCC_AUTOSAR_Project_Overview_VI.md) | Rev.1, 07/09/2026 | §5 scope mapping, §6 EXI, §§7–10 release/WP, §§11–15 verification/resources/risks |
| Historical input | [Scope đầu vào nguyên văn](Scope_Input_Original.md) | Không ghi revision | Input ban đầu có tên SDP/SLAC bị lẫn; xem cách tách trong Overview, không dùng input làm normative spec |

## Quy tắc xử lý khác biệt

Atomic Rev.3 nói rõ việc thay interface dạng nhóm bằng atomic catalogue. Khi cùng chủ đề của hai composition chính được mô tả khác nhau, ưu tiên nội dung cập nhật đó và ghi lại khác biệt. Không coi toàn bộ kiến trúc/supporting domains hoặc project plan cũ đã bị hủy.

Nếu khác biệt ảnh hưởng ABI, safety, state-transition guard/action hoặc normative protocol behavior, tạo issue và đối chiếu nguồn đúng scope; không suy ra formal approval chỉ từ ngày mới hơn. `DECISIONS.md` ở thư mục gốc ghi các vấn đề đã nhận thấy khi bàn giao.

## Nơi tìm nhanh

| Cần tìm | Nguồn chính |
|---|---|
| ChargeStateMachine / Permission / BPT / Fault | Atomic §§4–7; Detailed §3 |
| Standardized AUTOSAR operations | Atomic §8 và §33; Architecture phần detailed AUTOSAR requirements |
| HlcSessionAdapter và facade metadata | Atomic §9 |
| SDP / TCP-TLS / V2GTP / EXI / SAP | Atomic §§11–16 |
| DIN / ISO-2 / ISO-20 và sub-FSMs | Atomic §§18–21 |
| Certificate/security lifecycle | Atomic §§22–23 |
| PLC supervisor / PEV-SLAC / modem state machines | Architecture §12 |
| Atomic payload datatypes / units / enums / capacities | Atomic §§29–34 |
| Những điều chưa đủ để freeze | Atomic §§28, 35; root DECISIONS.md |
| Development plan R0–R6 / WP0–WP10 | Overview §§7–10 |
| Cách làm việc tạm trong khi chờ open points | Provisional Baseline Proposal §§4–14 |

Các hình kiến trúc được giữ tại `assets/EVCC_AUTOSAR_Charging_Architecture/media/`. Markdown chuyển từ Word không bảo toàn page layout; bản Word gốc ở `../references/originals/` dùng để đối chiếu hình thức khi cần.
