# Ngữ cảnh dự án V2G / EVCC AUTOSAR

Snapshot: 2026-09-08. Bản tổng hợp cho bàn giao, không thay thế specification nguồn.

## 1. Mục tiêu và tiến trình trao đổi

Phát triển charging stack phía EVCC trong môi trường AUTOSAR Classic, với boundary rõ giữa điều phối xe, communication, PLC và hardware. Các cuộc trao đổi đã chuyển từ khảo sát EVerest/SwitchEV sang kiến trúc, interface, behavior, state machines, coverage và kế hoạch phát triển.

| Mốc | Nội dung được ghi nhận |
|---|---|
| 05/09/2026 | Khảo sát layer architecture và dependency; làm rõ EVCC/EVSE; xác định safety SWCs và communication BSW/service modules; bổ sung DIN, interface, behavior, state machines và AUTOSAR interface requirements |
| 07/09/2026 | Đối chiếu scope theo `Composition::Module`; làm rõ EXI theo adapter; lập project overview và release/work-package plan |
| 08/09/2026 | Chi tiết hai composition chính; chuyển interface sang atomic và Markdown; bổ sung datatype catalogue; đặt vấn đề đóng/freeze toàn bộ open points |
| Đợt bàn giao này | Xuất tài liệu đã có và bổ sung hướng dẫn Codex; không thực hiện freeze hoặc sinh production code |

Yêu cầu làm việc đã thể hiện: giữ tài liệu ở Markdown; interface atomic tới từng data element/operation; sub-component có responsibility, interface, behavior, state machine, dependencies; bảng và hình state machine phải thống nhất; phải thấy rõ datatype đã định nghĩa và điểm còn mở.

## 2. Hai composition chính

| Composition | Sub-components | Ownership |
|---|---|---|
| `ChargingCoordinator` | `ChargeStateMachine`, `PermissionManager`, `BptCoordinator`, `FaultCoordinator` | Vehicle charging lifecycle, energy permission, BPT policy/direction và fault coordination |
| `ISO15118Chrg` | `HlcSessionAdapter`, `HlcSessionSupervisor`, `SdpClient`, `TcpTlsSession`, `V2gtpCodec`, `ExiCodec`, `SchemaRegistry`, `SapNegotiator`, `ProtocolAdapterFactory`, `Din70121Adapter`, `Iso15118_2Adapter`, `Iso15118_20Adapter`, `PkiSecurityAdapter`, `CertificateManager` | HLC facade, session, transport, EXI/SAP, protocol state và security evidence |

`HlcSessionAdapter` là project ASW facade quanh standardized AUTOSAR boundary; việc liệt kê trong composition map không tự xác định runnable/partition placement cuối cùng.

Các domain hỗ trợ nằm ngoài hai composition chính:

| Domain | Module/chức năng |
|---|---|
| `CpPpManager` | `CpStateEvaluator`, `PpCapabilityEvaluator`, `CpPpHwIf`: acquisition/debounce/qualification |
| `EvEnergyDataProvider` | `EnergySnapshotProvider`, `BptCapabilityProvider`, `EnergyDataValidator`: BMS/OBC/thermal/isolation snapshots |
| `ChargeHardwareAdapter` | Contactor, precharge, lock, discharge, bidirectional power adapters và measured feedback |
| `PlcLinkManager` | `LinkSupervisor`, `PevSlac`, `PlcModemIf`: modem, SLAC, association, link/IPv6 readiness |
| `PlatformServices` | TcpIp/SoAd, Csm/KeyM/HSM, credentials/NvM, diagnostics, OS/scheduler abstraction |

## 3. Hợp đồng quan trọng

- Communication chỉ tạo request/evidence; không tự cấp quyền energize.
- `PermissionManager` tạo permission có thời hạn và giới hạn; missing/stale/inconsistent evidence làm permission DENY.
- Mỗi state domain có authoritative status, không chỉ event queue. Mọi async path phải xử lý generation, time, sequence và correlation.
- Giữ public `ChargeControlPort` / `ChargeParametersPort` theo selected AUTOSAR/supplier ABI; không sửa normative data layout để nhét project metadata.
- SDP và PEV-SLAC là hai module khác nhau. SDP chỉ khởi động sau link/IPv6 readiness hợp lệ.
- DIN, ISO-2 và ISO-20 có protocol adapter và schema tương ứng; chỉ một protocol adapter hoạt động cho session.
- EXI engine interface có thể dùng chung; schemas/generated types/validation rules vẫn riêng. BPT dùng schema AC/DC ISO-20 tương ứng.
- Production target là bounded C/C++ tích hợp AUTOSAR; Python/asyncio/POSIX của reference không phải production dependency mặc định.

## 4. Luồng tóm tắt theo atomic specification

Luồng thường: qualified connect → tạo `sessionGeneration` → yêu cầu PLC/PEV-SLAC → link và IPv6 ready → start HLC → SDP/TCP-TLS/SAP → protocol negotiation → coordinator đánh giá evidence → cấp permission có giới hạn → cho phép tiến triển năng lượng.

Luồng dừng: phát hiện stop/fault/stale/link loss → revoke permission → yêu cầu hardware stop/safe state → HLC/PLC teardown idempotent → loại late callbacks. Đây là thứ tự responsibility; không phải thay thế electrical safe-stop sequence theo HARA/topology.

ChargeStateMachine theo Rev.3: `UNPLUGGED`, `CONNECTED_IDLE`, `LINK_SETUP`, `HLC_NEGOTIATION`, `READY_FOR_ENERGY`, `ENERGY_TRANSFER`, `STOPPING`, `FAULT_LATCHED`. Guards/actions và mô hình module khác xem tài liệu nguồn, không suy ra transition hợp lệ chỉ từ danh sách state.

## 5. Phạm vi release theo planning baseline

| Release | Phạm vi |
|---|---|
| R0 | Requirements, architecture, interfaces, state ownership và safety boundary baseline |
| R1 | PLC/network foundation, IPv6, SDP/TCP |
| R2 | DIN SPEC 70121 / ISO 15118-2 EIM, DC MVP và safe stop |
| R3 | ISO 15118-2 PnC |
| R4 | ISO 15118-20 charging, Common/AC/DC, scheduled/dynamic |
| R5 | AC-BPT/DC-BPT và vehicle bidirectional control |
| R6 | Production qualification: conformance, interoperability, safety/cybersecurity evidence |

Nguồn là Project Overview Rev.1, §§7–10. Dự kiến trong tài liệu: 15–18 tháng với 10–12 người, hoặc 20–24 tháng với 5–6 người, có chồng lấn work packages. Đây là estimate, không phải lịch đã cam kết. Bảng FTE chi tiết trong nguồn cộng thành 12–14 người, cần làm rõ peak/average hoặc cập nhật estimate trước khi cam kết nguồn lực.

## 6. Đã có gì, chưa có gì

Đã lấy được: atomic specification Rev.3 (gồm §§29–35 datatype), detailed compositions Rev.1, architecture Rev.4, project overview Rev.1, scope đầu vào và hình tham chiếu. File Word đã chuyển nội dung sang Markdown và giữ bản gốc để đối chiếu.

Chưa có trong bộ nguồn: target implementation repository/commit, frozen supplier ARXML/generated headers, licensed standards/schema package, actual HARA/safety concept, target toolchain/setup, concrete per-interface timing/queue configuration, test execution artifacts và biên bản freeze. Không suy diễn rằng các tài liệu này không tồn tại ở nơi khác; chỉ là chưa được cung cấp trong gói này.

Ưu tiên tiếp tục: đối chiếu consistency, đóng các quyết định phụ thuộc supplier/platform/safety, lập traceability tới implementation và verification. Xem [NEXT_STEPS.md](NEXT_STEPS.md).


## 7. Host implementation supplement — 26/10/2026

The original handoff snapshot above remains historical. The repository now has
C11 host logic, Linux surrounding stubs and opt-in pinned SAP/DIN/ISO-2 generated
EXI adapters. Actual TCP/V2GTP/EXI closed DC EIM sessions complete for DIN and
ISO-2, including supervised stop; external standards/supplier/safety gates are
still open. Current implementation/evidence is documented in
`docs/architecture/EXI_WIRE_IMPLEMENTATION_STATUS.md`. No production AUTOSAR,
physical EVSE interoperability or formal approval is implied by these additions.
