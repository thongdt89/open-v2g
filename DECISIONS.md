# Decision log và trạng thái freeze

Ngày tổng hợp: 2026-09-08. IDs `HD-*`, `OP-*`, `HF-*` trong file này chỉ dùng cho bàn giao; không thay thế requirement/interface IDs của nguồn.

## Quy ước

- `DOCUMENTED`: được phát biểu trong baseline thiết kế đã lấy được; không đồng nghĩa đã có formal approval hoặc verified implementation.
- `OPEN`: nguồn vẫn yêu cầu quyết định/evidence để freeze.
- `HANDOFF_FINDING`: chỗ cần đối chiếu được nhận thấy khi xuất; chưa sửa tài liệu nguồn.
- `PROPOSED`: cách tiếp tục do bộ bàn giao đề xuất.

## Quyết định thiết kế được ghi nhận

| ID | Trạng thái | Nội dung | Nguồn |
|---|---|---|---|
| HD-001 | DOCUMENTED | Hai composition chính là ChargingCoordinator và ISO15118Chrg | Atomic Rev.3 §2 |
| HD-002 | DOCUMENTED | Coordinator sở hữu progression và ChargePermission; HLC không gọi actuator trực tiếp | Atomic §§3–5, 24–26 |
| HD-003 | DOCUMENTED | ChargeStateMachine, PermissionManager, BptCoordinator và FaultCoordinator có state ownership tách biệt | Atomic §§4–7, 26 |
| HD-004 | DOCUMENTED | PEV-SLAC thuộc PlcLinkManager; SDP thuộc ISO15118Chrg | Atomic §1.3; Overview §5 |
| HD-005 | DOCUMENTED | Metadata của project nằm ở HlcSessionAdapter/project interfaces; imported AUTOSAR ABI được giữ nguyên | Atomic §§8–9, 33 |
| HD-006 | DOCUMENTED | Generation/freshness/correlation, bounded resources, idempotent cancel và authoritative status | Atomic §§1, 26, 34 |
| HD-007 | DOCUMENTED | Một protocol adapter active; EXI chọn explicit schema cho SAP/DIN/ISO-2/ISO-20 Common/AC/DC | Atomic §§14–21 |
| HD-008 | DOCUMENTED | BPT reversal cần measured zero và permission mới | Atomic §§6, 27 |
| HD-009 | DOCUMENTED | Rollout DIN/ISO-2 EIM trước, rồi PnC, ISO-20, BPT, qualification | Overview §§7–10 |
| HD-010 | DOCUMENTED | Reference Python dùng cho nghiên cứu/test oracle; production implementation native AUTOSAR | Overview §§1–2, 16 |

## Open points trước khi freeze

Nguồn: Atomic Rev.3 §§28, 33, 35; Overview §§9, 14, 17. Vai trò owner dưới đây là gợi ý phân công, chưa có người nhận hoặc deadline được duyệt.

| ID | Cần chốt | Vai trò owner gợi ý | Evidence đóng điểm |
|---|---|---|---|
| OP-001 | AUTOSAR Classic release, supplier ISO15118Chrg và generated artifacts | Platform / supplier integration | Versioned ARXML + generated headers + checksum |
| OP-002 | Từng C/S parameter direction, signature, server/client ownership, imported enum mapping | RTE / interface architect | Operation-by-operation ICD/ARXML/header diff và compile mapping check |
| OP-003 | Project naming, application/implementation types, data constraints, CompuMethod, init values | Interface architect | Project datatype/port ARXML, nested-type resolution và compatibility review |
| OP-004 | Runnable/event/queue/exclusive area/priority; per-interface maxAge; E2E khi áp dụng | OS/RTE / timing / safety | Timing/concurrency budget và configuration đã review |
| OP-005 | HARA, ASIL allocation, safe state, reaction deadlines, latching/recovery theo topology | Functional safety / vehicle integration | Approved safety concept và trace tới fault reactions/verification |
| OP-006 | Protocol editions/corrigenda, timer/retry/response-code mapping | Protocol lead | Compliance matrix theo exact edition, timer ownership/config và negative-case evidence |
| OP-007 | EXI product, licensed schemas, code-generation flow và resource limits | Codec / tools | Generator/schema version, golden vectors, target workspace/WCET measurements |
| OP-008 | PLC modem/raw MME/driver, PEV support, IPv6/TcpIp/SoAd/TLS capability | PLC / platform | Supplier capability matrix và target integration PoC |
| OP-009 | PnC PKI, provisioning/update/revocation, trusted time, HSM/KeyM/NvM transaction policy | Security / OEM PKI | Credential lifecycle design và integration/negative tests |
| OP-010 | BPT AC/DC, V2G/V2H/V2L, channel/control/grid modes và powertrain capability | Vehicle / BPT / safety | Feature/topology matrix, sign/zero-crossing constraints và new-permission tests |
| OP-011 | Repository/toolchain ownership, build commands, reference commit pins và evidence location | SW lead / configuration management | Reproducible build definition, dependency versions và evidence index |

## Điểm cần đối chiếu khi tiếp tục

| ID | Trạng thái | Quan sát cụ thể | Hành động |
|---|---|---|---|
| HF-001 | HANDOFF_FINDING | Atomic §1.1 định nghĩa P=provided, R=required; nhưng CC-EXT-009…017 dùng P/Pp cho các C/S calls có semantic caller là ChargingCoordinator. HSA-IF-001 đồng thời xác định StartHlc được provided tại HlcSessionAdapter | Đối chiếu caller/server và perspective; sửa catalogue có kiểm soát trước khi generate RTE, không đảo semantic producer theo nhãn P/R |
| HF-002 | HANDOFF_FINDING | Scope_Input_Original mục 3 gọi SDP nhưng mô tả SLAC. Overview đã tách 3a SDP và 3b PEV-SLAC | Giữ input gốc làm provenance; dùng phân tách đã ghi nhận cho thiết kế |
| HF-003 | HANDOFF_FINDING | Source scope có các phát biểu chi tiết về PnC/TLS/OCSP/certificate messages; đây là scope đầu vào, không phải trích chuẩn đã xác minh | Đối chiếu licensed edition và project feature matrix trước khi triển khai normative behavior |
| HF-004 | HANDOFF_FINDING | Atomic §35 đánh dấu project datatype/nested types hoàn tất nhưng để mở imported signatures và per-interface timing/queue | Giữ nguyên checkbox nguồn; chạy independent closure/generation checks trước khi tuyên bố datatype/ABI freeze |
| HF-005 | HANDOFF_FINDING | Overview §8 nêu 10–12 người; các dòng FTE §13 cộng thành 12–14 | Làm rõ peak/average/overlap hoặc hiệu chỉnh kế hoạch; không tự sửa estimate trong bản nguồn |

## Điều kiện gọi một điểm là frozen — PROPOSED

Một điểm chỉ chuyển sang `FROZEN` khi có quyết định cụ thể, owner/reviewer thực tế, artifact/version áp dụng, evidence kiểm tra, phạm vi release và ngày hiệu lực. Ghi cả các phụ thuộc còn mở và tác động thay đổi.

Không có dòng nào trong bản bàn giao này được tự chuyển thành `FROZEN`. Yêu cầu trước đây về “freeze hết open point” được giữ như mục tiêu công việc, không phải bằng chứng rằng việc freeze đã hoàn tất.
