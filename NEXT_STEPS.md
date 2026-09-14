# Công việc tiếp tục trong Codex

Trạng thái: PROPOSED — kế hoạch tiếp tục từ bộ tài liệu hiện có. Không thay thế release/WP plan gốc.

## Cập nhật thực thi G1 — 10/09/2026

Phần có thể hoàn thành độc lập với target/supplier trong baseline tạm đã được triển khai và host-verified:

- logical wiring cho hai composition và supporting providers;
- fail-closed permission với fault/BPT evidence;
- bounded V2GTP stream codec và HLC event queue/status resync;
- immutable SDP endpoint và typed TCP/TLS lifecycle, không implicit downgrade;
- typed EXI/schema registry boundary với schema-specific backend, bounded caller-owned workspace và no-partial decode publication;
- provisional ASR-style C/S operation ports và S/R status/diagnostic ports cho EXI/schema, giữ khả năng thay binding bằng generated RTE sau này;
- typed `SapNegotiator` với ordered offer snapshot, generation/request correlation, DIN-over-TLS filtering, timeout/cancel và fail-closed unoffered selection;
- static one-active `ProtocolAdapterFactory` cùng DIN 70121 và ISO 15118-2 EIM semantic host FSMs; generated EXI/wire mapping và normative timers vẫn ở G2;
- cùng C/S/S/R pattern cho coordinator, permission/fault/BPT/state, CP/PP, energy, PLC, hardware, HLC facade, SDP, TCP/TLS và V2GTP; nominal host flow đã đọc snapshot qua ports;
- stale-generation, cancel/late callback, overflow, expiry, reversal và teardown tests;
- ma trận bao phủ 218 atomic interface rows tại `docs/architecture/INTERFACE_IMPLEMENTATION_TRACEABILITY.md`.

Điểm dừng hiện tại là ranh giới G2/G3, không phải lỗi host còn che giấu: supplier ARXML/generated headers, licensed EXI schemas/generator, SoAd/TcpIp/PduR/Csm/KeyM APIs, modem capability và HARA/timing artifacts chưa có trong repository. Không tự sinh thay các đầu vào này.

## 1. Kiểm tra baseline và consistency

- Đọc Atomic Rev.3 và đối chiếu Detailed Compositions Rev.1 / Architecture Rev.4 theo docs/INDEX.md.
- Lập bảng interface ID → caller/server hoặc producer/consumer → type → owning runnable → consumer → verification case.
- Làm rõ các P/R ambiguities ghi trong DECISIONS.md trước khi sinh RTE wiring.
- Đối chiếu mọi state-transition table với diagram và enum; giữ phân biệt coordinator lifecycle, HLC supervisor, protocol FSM và PLC link FSM.
- Kiểm tra catalogue project types tới nested base/enum/array/structure; tách imported types và exact signatures còn thiếu.

Kết quả mong đợi: consistency report, issue list có source section/ID và đề xuất sửa cụ thể; chưa tự tuyên bố freeze.

## 2. Làm rõ các phụ thuộc triển khai

- Xác định repository, compiler/toolchain, AUTOSAR release/vendor và generated interface artifacts thực tế.
- Chốt protocol edition/feature matrix theo từng R0–R6.
- Lập decision record cho OP-001…011; tiếp tục phần độc lập trong lúc chờ evidence bên ngoài.
- Chốt timing/maxAge/queue/priority/exclusive-area và safe-state semantics từ safety/timing work products.
- Chọn EXI/modem/network/security integration theo capability evidence, không suy từ reference Python.

## 3. Sinh artifacts khi đầu vào tương ứng đã đủ

- Project type definitions và ARXML mapping theo frozen naming/range/init/ownership.
- Interface wrappers cho đúng supplier-generated AUTOSAR ABI; không tự dựng normative signatures.
- Requirement–module–interface–state–test traceability.
- Host/SIL skeletons cho coordinator/permission/link/session boundaries; provisional mocks/configs phải có nhãn rõ.

## 4. Ưu tiên verification theo rủi ro

| Nhóm | Các tình huống đầu tiên |
|---|---|
| Permission | Expiry, stale BMS/CP/PP/HW evidence, conflicting limits, generation mismatch |
| Async lifecycle | Cancel trong SDP/TCP/TLS/SAP, repeated stop, late callback từ session cũ, event overflow/resync |
| Codec/transport | Fragmented/coalesced TCP frames, oversize length, malformed EXI, wrong/unoffered schema, bounded workspace |
| Safety boundary | HLC không có actuator path, revoke-before-teardown, measured feedback khác command echo |
| Protocol | Sequence/negative response/timer/retry theo exact edition và release scope |
| Security/BPT | No implicit downgrade, key-handle boundary, certificate transaction failure; zero confirmation và new permission khi reversal |

Đây là danh sách đề xuất, chưa phải kết quả test. HIL, WCET, safe-stop latency và conformance cần target/bench và artifact liên quan.

## 5. Đóng WP0/R0

Theo Overview: không có interface không có owner; không có direct HLC→actuator path; ICD/ARXML draft sẵn sàng. Bổ sung evidence index và decision status rõ để biết phần nào chỉ thiết kế, phần nào đã generated, compiled, tested hoặc approved.
