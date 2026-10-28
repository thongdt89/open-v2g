## Current continuation — control boundaries, 28/10/2026

Evidence invalidation/order, expiry-before-renewal, physical assessment in the
permission action, bounded stopping and connected-safe fresh-generation restart
are implemented in the host core. See
[control-boundary status](docs/architecture/CONTROL_BOUNDARY_HARDENING.md).
Next close explicit protocol authorization/service evidence and move independent
vehicle/cable/peer/schedule limit intersection into PermissionManager. Then move
portable session/wire validation into ISO15118Chrg before wider signed EXI/PnC,
retained-session recovery and ISO-20/BPT work. Target safety/PKI/supplier gates
remain open. Earlier entries below describe their own historical snapshots.

# Công việc tiếp tục trong Codex

## Current implementation priorities — 28/10/2026

See [current incomplete items](docs/architecture/INCOMPLETE_ITEMS.md). Real optional
host TLS, typed software crypto and cumulative processing budgets now extend the
DC/AC EIM runner. Next close reusable control/evidence/stop-boundary findings,
then signed EXI/receipt and PnC credential lifecycle, retained pause/resume and
renegotiation, richer offers/schedules and ISO-20/BPT. Supplier, vehicle safety,
PKI and target qualification remain external. Historical entries follow.

## Bổ sung host simulation — 15/10/2026

- Đã thêm `openv2g_host_sim` để ghép các project-side components qua C/S/S/R ports và inject CP/PP, energy, PLC, EVSE protocol response, link loss và measured hardware feedback.
- Đã thêm flow simulator có ordered trace và integration test cho nominal transfer, stale generation, hardware mismatch và coordinated stop; tổng cộng 18 host tests pass bằng MSVC `/W4 /WX`.
- Đã thêm helper build MinGW-w64 GCC trên Windows. Môi trường review ngày 15/10 không có GCC cài sẵn, nên chưa tuyên bố bằng chứng chạy Windows-GCC; helper sẽ fail-fast và chỉ build khi tìm thấy GCC cùng Ninja hoặc `mingw32-make`.

## Cập nhật thực thi G1 — 15/09/2026

Phần có thể hoàn thành độc lập với target/supplier trong baseline tạm đã được triển khai và host-verified:

- logical wiring cho hai composition và supporting providers;
- fail-closed permission với fault/BPT evidence;
- bounded V2GTP stream codec và HLC event queue/status resync;
- immutable SDP endpoint và typed TCP/TLS lifecycle, không implicit downgrade;
- TCP/TLS generation + connection-ID correlation cho connect/TLS/RX, cancel khi handshake, reconnect cùng generation và loại late callback của connection cũ;
- bounded transport TX queue metadata, lower send seam, request/result/deadline correlation, backpressure và cancel cleanup; binding SoAd/TcpIp/PduR thật vẫn ở G2;
- typed EXI/schema registry boundary với schema-specific backend, bounded caller-owned workspace và no-partial decode publication;
- provisional ASR-style C/S operation ports và S/R status/diagnostic ports cho EXI/schema, giữ khả năng thay binding bằng generated RTE sau này;
- typed `SapNegotiator` với ordered offer snapshot, generation/request correlation, DIN-over-TLS filtering, timeout/cancel và fail-closed unoffered selection;
- static one-active `ProtocolAdapterFactory` cùng DIN 70121 và ISO 15118-2 EIM request-generating semantic host FSMs; exact pending correlation, energy/permission S/R gates, current-demand/metering loops và retained diagnostics đã có test; generated EXI/wire mapping và normative timers vẫn ở G2;
- shared `ProtocolDataFlow` rules đã map các flow session/service/auth, charge-parameter, DC safety/precharge, power start/loop/stop, metering và post-charge vào request C/S, context S/R và HLC queued event; target/permission limit intersection và required availability bits fail-closed;
- DIN/ISO-2 DC permission-expiry matrix đã cover precharge, power start, first current-demand và ongoing-loop renewal; mỗi request phụ thuộc permission đều đọc lại S/R sample hiện tại;
- cùng C/S/S/R pattern cho coordinator, permission/fault/BPT/state, CP/PP, energy, PLC, hardware, HLC facade, SDP, TCP/TLS và V2GTP; nominal host flow đã đọc snapshot qua ports;
- hardware host model đã tách command acceptance khỏi measured feedback, correlate command ID/result, loại stale feedback và fail `HW_MISMATCH` khi lock/contactor feedback không khớp; exact electrical criteria/deadline vẫn cần HARA/target;
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

## Linux stub update — 26/10/2026

Implemented a generic C11/POSIX surrounding environment at `simulation/linux/`, with a separate TCP station process and OS/RTE/service stubs. See its README for coverage and limits. The smoke flow exchanges only SessionSetup semantics and exercises host permission/hardware/stop. Full continuous socket protocol flow still requires nonblocking cyclic scheduling integration and generated EXI mapping; OP-001…OP-011 remain open.

## SAP EXI update — 26/10/2026

Implemented an opt-in pinned libcbv2g SAP host backend with bounded neutral objects and fixed upstream-vector/malformed-input tests. See `integration/protocol/adapters/cbv2g/README.md`. Next: recover exact schema/generator provenance and independent vectors; then add separate DIN/ISO-2 message backends and wire mapping. Target/legal/safety gates remain open.

## Current DC wire update — 26/10/2026

This update supersedes the SAP-only and custom-frame future-work entries above.
Host implementation now includes separate DIN/ISO-2 codecs, real SAP/V2GTP/EXI
socket sessions for both DC EIM profiles, continuous nonblocking supervision,
coordinator-owned protocol stopping, malformed/sanitizer tests and a separate
pinned independent-codec comparison. See
[the evidence and closure inventory](docs/architecture/EXI_WIRE_IMPLEMENTATION_STATUS.md).

The next production gates require external evidence: exact schema/generator/
license provenance and editions (OP-006/007), supplier ARXML/BSW signatures
(OP-001/002/003/008), target scheduling/resources (OP-004/011), and approved
safety/physical sequencing (OP-005). None is frozen by this host implementation.
Independent host extensions can broaden services/schedules, UDP/IPv6 SDP and
coverage-guided fuzzing; current support is the documented closed DC EIM profile.

## Feature-completion request — 26/10/2026, in progress

Assessment gating, ServiceDetail, pending CPD, minor-deviation policy, bounded
schedule lists and dynamic-limit enforcement now extend the host profile.
Continue the remaining ordered implementation inventory in
[FEATURE_COMPLETION_WORKLOG.md](docs/architecture/FEATURE_COMPLETION_WORKLOG.md).
Do not interpret this increment or existing passing tests as full AC/PnC/ISO-20/
BPT/security/session-resumption support.

A standalone real IPv6 UDP SDP provider and IPv6 TCP follow-up now exist with
host tests (`simulation/linux/SDP_IPV6.md`). Next network step is asynchronous
integration into the EXI runner, then scoped multicast/link-local and TLS.

The `station6`/`evcc6` runner now integrates real UDP discovery before IPv6 TCP
and SAP/EXI, for both DIN and ISO-2. This supersedes the preceding pending-runner
integration note. Multicast/link-local interface handling, retries and TLS remain.
