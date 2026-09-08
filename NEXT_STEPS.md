# Công việc tiếp tục trong Codex

Trạng thái: PROPOSED — kế hoạch tiếp tục từ bộ tài liệu hiện có. Không thay thế release/WP plan gốc.

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
