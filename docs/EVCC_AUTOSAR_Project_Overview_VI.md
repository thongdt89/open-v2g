**TỔNG QUAN DỰ ÁN EVCC AUTOSAR**

Kiến trúc, phạm vi triển khai và kế hoạch phát triển DIN / ISO 15118 / BPT

**Phạm vi:** EVCC phía xe trên AUTOSAR Classic

**Baseline:** DIN SPEC 70121 và ISO 15118-2 EIM

**Mở rộng:** ISO 15118-2 PnC, ISO 15118-20 và bidirectional power transfer

**Phiên bản tài liệu:** Revision 1 - 07/09/2026

**Trạng thái:** Project planning baseline

| **KHUYẾN NGHỊ CHÍNH** Phát triển theo từng release: hoàn thiện nền tảng an toàn và DIN/ISO-2 EIM trước; sau đó bổ sung PnC, ISO-20 và BPT bằng các protocol adapter độc lập. SwitchEV được sử dụng làm executable reference và test oracle, không phải production AUTOSAR implementation. |
|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|

# 1. Mục tiêu tài liệu

Tài liệu này tổng hợp phạm vi kỹ thuật, cấu trúc implementation composition, kế hoạch release, work package, tiêu chí nghiệm thu và nguồn lực cần thiết để phát triển EVCC charging stack trên AUTOSAR Classic.

- Thiết lập một kiến trúc có ownership rõ ràng giữa vehicle safety, HLC, PLC và hardware execution.

- Hỗ trợ DIN SPEC 70121, ISO 15118-2 và khả năng mở rộng ISO 15118-20/BPT.

- Duy trì interface ổn định khi thay đổi EXI engine, modem PLC, TCP/IP hoặc protocol edition.

- Cho phép traceability từ requirement tới module, state machine và verification evidence.

# 2. Giả định và ranh giới

| **Chủ đề**       | **Giả định dự án**                                                                           |
|------------------|----------------------------------------------------------------------------------------------|
| Platform         | AUTOSAR Classic trên charging ECU/domain controller; IPv6 link-local qua HomePlug Green PHY. |
| Safety           | ASIL allocation phải được xác nhận bằng HARA và functional safety concept của xe.            |
| Protocol rollout | R2: DIN/ISO-2 EIM; R3: ISO-2 PnC; R4: ISO-20; R5: BPT.                                       |
| Reference code   | SwitchEV iso15118/pyslac dùng cho message model, trace, timer và differential test.          |
| Production code  | C/C++ theo AUTOSAR; tài nguyên được giới hạn; không phụ thuộc Python/asyncio/POSIX.          |

# 3. Nguyên tắc kiến trúc

| **ARCHITECTURE DECISION** Một owner cho mỗi state domain: ChargingCoordinator sở hữu quyền năng lượng; ISO15118Chrg sở hữu HLC session; PlcLinkManager sở hữu SLAC/link; ChargeHardwareAdapter sở hữu thực thi actuator và feedback. |
|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|

| **Nguyên tắc**     | **Quy tắc thực hiện**                                                                                          |
|--------------------|----------------------------------------------------------------------------------------------------------------|
| Safety boundary    | Mọi output từ communication stack chỉ là request/evidence. Chỉ ChargingCoordinator được phát ChargePermission. |
| State ownership    | Mỗi state chỉ do một module ghi; module khác nhận event hoặc qualified snapshot.                               |
| Validity           | Event bất đồng bộ phải có sessionGeneration, timestamp, sequenceCounter và result/reason.                      |
| Cancellation       | Stop/cancel phải idempotent; callback thuộc session cũ không được làm sống lại session.                        |
| Bounded resources  | Queue, EXI payload, certificate chain và schedule points có giới hạn cấu hình rõ ràng.                         |
| Platform isolation | Không để POSIX socket, Python object, raw MME hoặc hardware register vượt qua owning module.                   |

# 4. Implementation composition

| **Composition**       | **Module bên trong**                                                                                  | **Ownership**                                 |
|-----------------------|-------------------------------------------------------------------------------------------------------|-----------------------------------------------|
| ChargingCoordinator   | ChargeStateMachine; PermissionManager; BptCoordinator; FaultCoordinator                               | Quyền tiến triển charging và quyền năng lượng |
| CpPpManager           | CpStateEvaluator; PpCapabilityEvaluator; CpPpHwIf                                                     | CP/PP acquisition, debounce và qualification  |
| EvEnergyDataProvider  | EnergySnapshotProvider; BptCapabilityProvider; EnergyDataValidator                                    | Snapshot BMS/OBC/thermal/isolation            |
| ChargeHardwareAdapter | ContactorAdapter; PrechargeAdapter; ConnectorLockAdapter; DischargeAdapter; BidirectionalPowerAdapter | Actuator command và measured feedback         |
| ISO15118Chrg          | SDP; TCP/TLS; V2GTP; EXI; SAP; protocol adapters; security; session supervisor                        | HLC session và protocol behavior              |
| PlcLinkManager        | LinkSupervisor; PevSlac; PlcModemIf                                                                   | Modem, SLAC, association và IPv6 readiness    |
| PlatformServices      | TcpIpSoAdAdapter; CsmKeymHsmAdapter; CredentialStore; DiagnosticAdapter; OsSchedulerAdapter           | AUTOSAR BSW/platform abstraction              |

# 5. Ánh xạ scope tới Composition::Module

| **No.** | **Scope**              | **Primary Composition::Module**       | **Supporting module**                                             |
|---------|------------------------|---------------------------------------|-------------------------------------------------------------------|
| 1       | V2G Transport Layer    | ISO15118Chrg::TcpTlsSession           | V2gtpCodec; TcpIpSoAdAdapter; PkiCryptoAdapter                    |
| 2       | EXI Codec              | ISO15118Chrg::ExiCodec                | SchemaRegistry                                                    |
| 3a      | SDP - SECC discovery   | ISO15118Chrg::SdpClient               | TcpIpSoAdAdapter; LinkSupervisor                                  |
| 3b      | PEV-SLAC pairing       | PlcLinkManager::PevSlac               | PlcModemIf                                                        |
| 4       | V2G State Machine      | ISO15118Chrg::ProtocolFsm             | DIN, ISO-2 và ISO-20 adapter                                      |
| 5       | Basic ISO-2 PnC        | Iso15118_2Adapter::PncHandler         | PkiSecurityAdapter; CredentialStore                               |
| 11      | ISO-20 BPT Service     | Iso15118_20Adapter::BptServiceHandler | BptCapabilityProvider; BptCoordinator                             |
| 12      | BPT Charge Loop        | Iso15118_20Adapter::BptChargeLoop     | BptCoordinator; EnergySnapshotProvider; BidirectionalPowerAdapter |
| 13      | Enhanced ISO-20 PnC    | Iso15118_20Adapter::PncHandler        | CertificateManager; PkiSecurityAdapter; CredentialStore           |
| 15      | Enhanced Message Layer | ISO15118Chrg::Iso15118_20MessageModel | ExiCodec; SchemaRegistry; V2gtpCodec                              |
| 16      | Enhanced Security      | ISO15118Chrg::PkiSecurityAdapter      | TcpTlsSession; CertificateManager; Csm/KeyM/HSM                   |
| 17      | Error and Recovery     | ISO15118Chrg::HlcSessionSupervisor    | FaultCoordinator; DiagnosticAdapter; protocol adapters            |

| **LƯU Ý** SDP và SLAC phải là hai module khác nhau. SDP là IPv6/UDP service discovery trong ISO15118Chrg; PEV-SLAC là link-layer matching trong PlcLinkManager. |
|-----------------------------------------------------------------------------------------------------------------------------------------------------------------|

# 6. Kiến trúc EXI đa giao thức

Các adapter có thể sử dụng chung một interface EXI, nhưng cần schema, namespace, generated types và validation rules khác nhau. Decoder không được tự suy luận protocol từ untrusted payload.

| **Adapter**             | **Schema ID**            | **Phạm vi**                                         |
|-------------------------|--------------------------|-----------------------------------------------------|
| SapNegotiator           | EXI_SCHEMA_SAP           | SupportedAppProtocolReq/Res trước khi chọn protocol |
| Din70121Adapter         | EXI_SCHEMA_DIN_70121     | DIN namespace, DC messages và physical constraints  |
| Iso15118_2Adapter       | EXI_SCHEMA_ISO_15118_2   | AC/DC, EIM/PnC, schedule và metering                |
| ISO-20 CommonSessionFsm | EXI_SCHEMA_ISO_20_COMMON | Authorization, service, schedule và session control |
| ISO-20 AC / AC-BPT      | EXI_SCHEMA_ISO_20_AC     | AC charge parameters và AC charge loop              |
| ISO-20 DC / DC-BPT      | EXI_SCHEMA_ISO_20_DC     | Cable check, precharge, DC charge loop, welding     |

- Một ExiCodec service ổn định; nhiều generated schema implementation.

- BPT không cần EXI engine riêng: AC-BPT dùng ISO-20 AC schema, DC-BPT dùng ISO-20 DC schema.

- XML signature do PkiSecurityAdapter xử lý; EXI cung cấp encoded bytes đúng cho signed elements.

- Worst-case workspace và execution time phải được đo cho message lớn nhất được hỗ trợ.

# 7. Chiến lược release

| **Release** | **Phạm vi**              | **Mục tiêu nghiệm thu**                                                  |
|-------------|--------------------------|--------------------------------------------------------------------------|
| R0          | Architecture baseline    | Requirement, interface, state ownership và safety boundary được baseline |
| R1          | PLC + network foundation | PEV-SLAC, modem, IPv6, SDP và TCP hoạt động                              |
| R2          | DIN / ISO-2 EIM          | Sạc DC cơ bản với EIM và safe-stop hoàn chỉnh                            |
| R3          | ISO-2 PnC                | TLS 1.2, contract certificate và Plug & Charge                           |
| R4          | ISO-20 charging          | Common session, AC/DC, scheduled/dynamic control                         |
| R5          | ISO-20 BPT               | AC-BPT/DC-BPT và vehicle bidirectional control                           |
| R6          | Production release       | Conformance, interoperability, safety và cybersecurity evidence          |

# 8. Kế hoạch tổng thể

| **Giai đoạn**            | **Thời gian** | **Work package** | **Kết quả**                          |
|--------------------------|---------------|------------------|--------------------------------------|
| 0\. Requirement baseline | 4-6 tuần      | WP0              | Architecture và requirement baseline |
| 1\. Platform foundation  | 6-10 tuần     | WP1-WP3          | PLC, TCP/IP, EXI/V2GTP foundation    |
| 2\. DIN/ISO-2 EIM MVP    | 10-14 tuần    | WP4-WP6          | Phiên sạc DC EIM hoàn chỉnh          |
| 3\. ISO-2 PnC            | 8-12 tuần     | WP7              | TLS/PnC với secure key operation     |
| 4\. ISO-20 charging      | 12-16 tuần    | WP8              | AC/DC ISO-20 một chiều               |
| 5\. ISO-20 BPT           | 12-18 tuần    | WP9              | Charging/discharging có kiểm soát    |
| 6\. Qualification        | 12-16 tuần    | WP10             | SOP candidate                        |

Tổng thời gian mục tiêu là 15-18 tháng với đội 10-12 người và các work package chạy chồng lấn. Với đội 5-6 người, kế hoạch thực tế nên là 20-24 tháng.

# 9. Critical path

1.  Lựa chọn EXI product và xác nhận code-generation/qualification strategy.

2.  Xác nhận modem PLC, raw MME access và QCA700x/vendor-driver integration.

3.  Xác nhận IPv6/TLS capability của AUTOSAR TcpIp/SoAd/Crypto stack.

4.  Chốt PnC PKI, HSM và OEM certificate provisioning process.

5.  Chốt BPT use case: AC-BPT/DC-BPT, V2G/V2H/V2L và grid-following/grid-forming.

# 10. Work-package plan

## WP0 - Requirements và kiến trúc

**Thời gian dự kiến:** 4-6 tuần

- Chốt standard edition, AUTOSAR release và feature matrix.

- Định nghĩa RTE ports, service interfaces, data types, timeout và resource limits.

- Baseline state machine, fault reaction và traceability requirement-module-test.

**Exit criteria:** Không còn interface không có owner; không có direct HLC-to-actuator path; ICD/ARXML draft sẵn sàng.

## WP1 - AUTOSAR platform foundation

**Thời gian dự kiến:** 6-8 tuần

- Cấu hình Ethernet/EthIf/EthSM/TcpIp/SoAd và IPv6 link-local.

- Tích hợp OS timers, NvM, Csm/KeyM/Crypto/HSM, Dem/Det và watchdog.

- Thiết lập memory/stack/execution-time measurement baseline.

**Exit criteria:** TCP client và crypto operation chạy trên target; network lifecycle được supervisor kiểm soát.

## WP2 - PLC modem và PEV-SLAC

**Thời gian dự kiến:** 8-12 tuần

- Phát triển vendor modem adapter; reset/boot/MME/network-key/link control.

- Triển khai PEV-SLAC FSM, peer/RunID filtering, attenuation và NMK/NID.

- Association, IPv6 readiness, retry, cancellation và teardown.

**Exit criteria:** SLAC interoperable với tối thiểu ba EVSE/chipset; link loss được báo tức thời; late message bị loại.

## WP3 - V2GTP, SAP và multi-schema EXI

**Thời gian dự kiến:** 8-12 tuần

- V2GTP incremental parser với bounded buffers.

- SAP negotiation và explicit schema selection.

- Generated schemas cho SAP, DIN, ISO-2, ISO-20 Common/AC/DC.

- Golden vectors, differential test và fuzzing.

**Exit criteria:** Codec không tự đoán schema; không có unbounded allocation; WCET/workspace được đo.

## WP4 - Coordinator và hardware integration

**Thời gian dự kiến:** 8-10 tuần

- Vehicle charging FSM, evidence qualification và ChargePermission.

- Cable check, precharge, contactor, lock, discharge và measured feedback.

- Safe stop khi CP/PLC/TCP/BMS/hardware không hợp lệ.

**Exit criteria:** Mọi actuator action cần coordinator permission; stale evidence thu hồi permission xác định.

## WP5 - DIN SPEC 70121 adapter

**Thời gian dự kiến:** 6-8 tuần

- SessionSetup tới CurrentDemand, WeldingDetection và SessionStop.

- Timer, negative response, sequence validation và controlled termination.

- Interoperability và trace comparison với executable reference.

**Exit criteria:** Phiên sạc DC DIN EIM hoàn chỉnh với ít nhất ba DC charger.

## WP6 - ISO 15118-2 EIM

**Thời gian dự kiến:** 6-10 tuần

- AC/DC mode, service/payment selection, EIM authorization và schedules.

- ChargingStatus, CurrentDemand, metering receipt và renegotiation.

- Protocol-neutral HlcEvent mapping lên ChargingCoordinator.

**Exit criteria:** DIN và ISO-2 dùng chung semantic interface nhưng không dùng chung protocol-state ownership.

## WP7 - ISO 15118-2 PnC

**Thời gian dự kiến:** 8-12 tuần

- TLS 1.2 profile, SECC certificate-chain validation và contract certificate installation.

- PaymentDetails, challenge signing, eMAID và secure HSM operations.

- Certificate validity, NvM transaction/rollback và PnC-to-EIM fallback policy.

**Exit criteria:** Private key không rời HSM; certificate update an toàn; negative security tests hoàn thành.

## WP8 - ISO 15118-20 charging

**Thời gian dự kiến:** 12-16 tuần

- AuthorizationSetup, services, schedule exchange và certificate integration.

- Scheduled/dynamic AC/DC charge parameters và loops.

- TLS 1.3, service renegotiation và pause/resume.

**Exit criteria:** Không còn production-flow NotImplemented; Common/AC/DC schema được chọn đúng; limits được enforce.

## WP9 - ISO 15118-20 BPT

**Thời gian dự kiến:** 12-18 tuần

- AC-BPT/DC-BPT capabilities, BPT channel và mobility needs.

- Schedule tuples, sign convention, 0 kW crossing và direction reversal.

- BMS/inverter limits, V2G/V2H/V2L policy và bidirectional safe stop.

**Exit criteria:** Không đổi chiều công suất nếu chưa có permission mới; link loss đưa công suất về zero/safe state.

## WP10 - Qualification và release

**Thời gian dự kiến:** 12-16 tuần

- Static analysis, unit/MC-DC theo safety allocation, SIL/HIL và fuzzing.

- DIN/ISO conformance, multi-vendor interoperability và fault injection.

- Cybersecurity validation, performance, robustness và release evidence.

**Exit criteria:** Tất cả release gates đóng; SOP candidate có traceability và verification evidence đầy đủ.

# 11. Verification strategy

| **Mức**          | **Mục tiêu**                                                 | **Evidence điển hình**                                     |
|------------------|--------------------------------------------------------------|------------------------------------------------------------|
| Unit             | State transitions, timers, validator, codec và error mapping | Boundary tests, coverage, static analysis                  |
| Differential     | So sánh semantic behavior với SwitchEV                       | EXI vectors, messages, state transitions, timeout outcomes |
| SIL              | RTE interface và concurrency                                 | Late/reordered event, cancellation, stale snapshot         |
| Fuzz             | Parser robustness                                            | Malformed EXI, V2GTP, SAP, certificate và MME              |
| HIL              | Electrical/timing behavior                                   | CP/PP, modem reset, link loss, contactor và precharge      |
| Conformance      | Protocol correctness                                         | DIN/ISO test suite và negative cases                       |
| Interoperability | Ecosystem compatibility                                      | Nhiều EVSE, PLC chipset, PKI và backend                    |
| Safety           | Fault control và safe-state timing                           | Fault injection, watchdog loss, resource exhaustion        |
| Cybersecurity    | Secure communication                                         | TLS downgrade, replay, certificate và key lifecycle        |

# 12. Release gates

- Không tồn tại direct HLC-to-actuator path trong code, RTE wiring hoặc configuration.

- Mọi chuyển tiếp tới energy transfer có evidence mới và nhất quán từ CP/PP, interlock, BMS, link, HLC và hardware.

- Mọi asynchronous path vượt qua generation-token, timeout, cancellation và late-event tests.

- EXI/V2GTP/SLAC parser vượt qua bounds, malformed-input và fuzz campaigns.

- Safe-stop latency được đo khi link loss, modem reset, ECU load và diagnostic fault injection.

- Protocol conformance và interoperability targets được đáp ứng cho release tương ứng.

# 13. Nguồn lực đề xuất

| **Nhóm**            | **FTE** | **Phạm vi**                                                |
|---------------------|---------|------------------------------------------------------------|
| System/architecture | 2       | Requirements, state ownership, safety concept và interface |
| PLC/network         | 2       | Modem, PEV-SLAC, Ethernet, SDP và TCP                      |
| Codec/protocol      | 3       | EXI, V2GTP, DIN, ISO-2 và ISO-20                           |
| Security            | 1-2     | TLS, PKI, HSM và certificate lifecycle                     |
| Vehicle integration | 2       | Coordinator, BMS, OBC/inverter và hardware                 |
| Verification        | 2-3     | SIL/HIL, conformance, fuzzing và interoperability          |

# 14. Rủi ro và quyết định cần đóng

| **Rủi ro**                 | **Mức**    | **Tác động**                                                     | **Biện pháp**                                            |
|----------------------------|------------|------------------------------------------------------------------|----------------------------------------------------------|
| EXI product chưa chọn      | Cao        | Ảnh hưởng footprint, interoperability và qualification           | Prototype 2 ứng viên; benchmark trên target trước R1     |
| PEV-SLAC reference hạn chế | Cao        | pyslac chủ yếu EVSE-side, không phải production PEV              | Tự phát triển/adopt PEV FSM; test đa chipset             |
| TLS/HSM capability         | Cao        | TLS 1.3/cipher/key operation có thể thiếu trong vendor stack     | PoC Csm/KeyM/HSM trong WP1                               |
| ISO-20 PnC chưa hoàn chỉnh | Cao        | Reference có CertificateInstallation gap và OCSP chưa hoàn thiện | Tách CertificateManager; xây negative test sớm           |
| BPT vehicle safety         | Cao        | Protocol support không đồng nghĩa powertrain support             | HARA bổ sung; BptCoordinator và independent power limits |
| Resource exhaustion        | Trung bình | Schedule/certificate/EXI message lớn                             | Compile-time limits, backpressure và fuzzing             |
| Interoperability           | Trung bình | Khác biệt EVSE/PLC/PKI vendor                                    | Testival và multi-vendor bench từ R2                     |

# 15. Definition of Done

| **ĐỊNH NGHĨA HOÀN THÀNH** Xe duy trì trạng thái an toàn khi bất kỳ communication component nào bị mất, trễ, reset, malformed, compromised hoặc internally inconsistent. Charging/discharging chỉ được tiến triển khi tất cả evidence bắt buộc còn mới, hợp lệ và nhất quán. |
|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|

# 16. Tài liệu tham chiếu

| **Nguồn**         | **Cách sử dụng**                                            | **Giới hạn / hành động**                                          |
|-------------------|-------------------------------------------------------------|-------------------------------------------------------------------|
| DIN SPEC 70121    | DIN EVCC message sequence, timers và DC charging behavior   | Baseline implementation; cần licensed edition chính thức          |
| ISO 15118-2       | HLC AC/DC, EIM/PnC, schedule, metering và TLS profile       | Baseline R2/R3; xác nhận edition và corrigenda                    |
| ISO 15118-3       | PLC link-layer và PEV-SLAC requirements                     | Nguồn chuẩn cho PevSlac; không thay bằng pyslac behavior          |
| ISO 15118-20      | Common session, AC/DC, scheduled/dynamic và BPT             | Baseline R4/R5; cần feature-by-feature compliance matrix          |
| AUTOSAR Classic   | RTE, TcpIp/SoAd, Csm/KeyM/Crypto, NvM, Dem và lifecycle     | Xác nhận release/vendor trước khi freeze ARXML                    |
| SwitchEV iso15118 | Executable HLC reference, schemas, message models và traces | Test oracle; không phải safety-qualified production code          |
| SwitchEV pyslac   | SLAC message examples và EVSE-oriented behavior             | Chỉ dùng tham khảo/test; PEV production FSM phải phát triển riêng |

# 17. Baseline artifacts cần duy trì

- System requirement specification và feature-compliance matrix theo từng protocol release.

- Software architecture specification, RTE interface catalogue và ARXML baseline.

- State-transition tables, timer/retry configuration và unified reason-code taxonomy.

- EXI schema-generation package, golden vectors và parser negative corpus.

- Safety/cybersecurity work products, verification plan và requirement-test traceability.

- Interoperability matrix theo EVSE, PLC chipset, certificate chain và backend condition.
