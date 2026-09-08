| **ARCHITECTURE DECISION** ChargingCoordinator là owner duy nhất của charging progression và ChargePermission. ISO15118Chrg là owner của HLC session và protocol behavior; mọi output của communication stack chỉ là request/evidence, không phải lệnh actuator. |
|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|

| **Thuộc tính** | **Giá trị**                                                                                     |
|----------------|-------------------------------------------------------------------------------------------------|
| Phạm vi        | EVCC phía xe; DIN SPEC 70121, ISO 15118-2, mở rộng ISO 15118-20/BPT                             |
| Nền tảng       | AUTOSAR Classic; production C/C++; tài nguyên và thời gian thực được giới hạn                   |
| Mục đích       | Baseline mô tả composition/sub-component cho SAD, SRS, RTE catalogue và implementation planning |
| Nguồn baseline | EVCC_AUTOSAR_Charging_Architecture Rev.4 và EVCC_AUTOSAR_Project_Overview_VI Rev.1              |

# 1. Phạm vi và quy tắc chung

Tài liệu này mô tả hai composition trung tâm của charging stack ở mức đủ để phân rã software component, xác định ownership và làm đầu vào cho interface design/ARXML. Các supporting composition như CpPpManager, EvEnergyDataProvider, ChargeHardwareAdapter, PlcLinkManager và PlatformServices chỉ được nhắc tại boundary; chúng không bị nhập vào hai composition chính.

| **Quy tắc**       | **Áp dụng**                                                                                                                     |
|-------------------|---------------------------------------------------------------------------------------------------------------------------------|
| State ownership   | Mỗi state domain có đúng một owner; component khác chỉ nhận event hoặc qualified snapshot.                                      |
| Validity          | Mọi dữ liệu/event bất đồng bộ mang sessionGeneration, timestamp, sequenceCounter và result/reason.                              |
| Safety boundary   | Chỉ ChargingCoordinator phát ChargePermission; HLC không gọi trực tiếp contactor, lock, precharge, OBC/inverter hoặc discharge. |
| Cancellation      | Stop/cancel idempotent; callback/PDU của generation cũ bị loại và chẩn đoán.                                                    |
| Bounded resources | Queue, payload, EXI workspace, certificate chain và schedule points có giới hạn cấu hình.                                       |
| Configuration     | Protocol/timer/retry/feature dùng ECUC, generated constants hoặc NvM có kiểm soát; không dùng file/env/CLI trên ECU.            |

# 2. Bản đồ composition

| **Composition**     | **Sub-components chính**                                                                                              | **Ownership**                                                       |
|---------------------|-----------------------------------------------------------------------------------------------------------------------|---------------------------------------------------------------------|
| ChargingCoordinator | ChargeStateMachine; PermissionManager; BptCoordinator; FaultCoordinator                                               | Charging progression, energy permission, safe-stop coordination     |
| ISO15118Chrg        | HlcSessionSupervisor; SDP; TCP/TLS; V2GTP; EXI; SchemaRegistry; SAP; protocol adapters; security/certificate services | HLC session, protocol sequencing, transport/codec/security evidence |

| **KHÔNG GỘP BOUNDARY** PEV-SLAC thuộc PlcLinkManager, không thuộc ISO15118Chrg. SDP thuộc ISO15118Chrg vì là IPv6/UDP service discovery sau khi PLC link và IPv6 đã sẵn sàng. |
|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|

# 3. Composition 1 - ChargingCoordinator

ChargingCoordinator là safety-related application composition điều phối toàn bộ vòng đời charging phía xe. Composition không xử lý frame, EXI hay modem command. Nó hợp nhất qualified evidence và chỉ cho phép tiến triển khi các điều kiện CP/PP, energy, isolation/interlock, PLC/HLC và hardware feedback còn mới, hợp lệ và nhất quán.

| **External port**          | **Hướng** | **Dữ liệu / mục đích**                             |
|----------------------------|-----------|----------------------------------------------------|
| Rp_CpPpStatus              | Required  | QualifiedCpPp từ CpPpManager                       |
| Rp_EnergySnapshot          | Required  | EvEnergySnapshot nguyên tử từ EvEnergyDataProvider |
| Rp_PlcLinkStatus           | Required  | PlcLinkStatus và linkGeneration từ PlcLinkManager  |
| Rp_HlcEvent / Rp_HlcStatus | Required  | Semantic request/evidence từ HlcSessionAdapter     |
| Rp_HwResult / Rp_HwStatus  | Required  | Measured feedback từ ChargeHardwareAdapter         |
| Pp_ChargePermission        | Provided  | Quyền bounded theo action/mode/limits/validUntil   |
| Pp_HwControl               | Provided  | Prepare/Enable/Stop/SafeState với commandId        |
| Pp_PlcControl              | Provided  | StartLink/StopLink theo sessionGeneration          |
| Pp_HlcControl              | Provided  | StartSession/StopSession/Renegotiate               |

## 3.1 ChargeStateMachine

**Trách nhiệm:** Owner duy nhất của vehicle charging lifecycle. Nó điều phối các bước từ plug-in tới link setup, HLC negotiation, readiness, energy transfer và teardown; không tự đánh giá chi tiết mọi điều kiện an toàn mà yêu cầu kết quả từ PermissionManager/FaultCoordinator.

| **BOUNDARY RULE** Không một sub-component nào ngoài ChargeStateMachine được tự ý thay đổi coordinator lifecycle state. |
|------------------------------------------------------------------------------------------------------------------------|

### Interface contract

| **Interface / dữ liệu**                      | **Hướng** | **Contract chính**                                             |
|----------------------------------------------|-----------|----------------------------------------------------------------|
| CpPpEvidence, LinkStatus, HlcEvent, HwResult | IN        | Chỉ dùng dữ liệu current generation, valid và fresh.           |
| PermissionDecision                           | IN        | Kết quả ALLOW/DENY có reason và validity.                      |
| Start/Stop PLC, HLC; HwCommand               | OUT       | Lệnh idempotent, có commandId và deadline.                     |
| CoordinatorState                             | OUT       | Authoritative nonqueued status; event chỉ hỗ trợ reaction/log. |

### Behavior

- Tạo sessionGeneration mới cho mỗi qualified plug session và invalidates mọi context trước đó.

- Giữ thứ tự PLC link -\> HLC -\> hardware readiness -\> permission; không cho event đến sớm bỏ qua state.

- Khi dừng, thu hồi permission trước, sau đó yêu cầu hardware safe/stop rồi teardown HLC và PLC.

- Tách normal stop, emergency stop và fault-latched để tránh recovery ngoài ý muốn.

### State model rút gọn

| **State**        | **Hoạt động / điều kiện chuyển**                                                                |
|------------------|-------------------------------------------------------------------------------------------------|
| UNPLUGGED        | Qualified connect -\> CONNECTED_IDLE; giữ permission DENY.                                      |
| CONNECTED_IDLE   | Tạo generation/context; HLC trigger -\> LINK_SETUP.                                             |
| LINK_SETUP       | Start PLC; LinkReady -\> HLC_NEGOTIATION; timeout/fault -\> STOPPING.                           |
| HLC_NEGOTIATION  | Start HLC; accepted parameters -\> READY_FOR_ENERGY.                                            |
| READY_FOR_ENERGY | Cable check/precharge/hardware readiness; valid start request + permission -\> ENERGY_TRANSFER. |
| ENERGY_TRANSFER  | Supervise evidence; stop/stale/link loss/fault -\> STOPPING hoặc FAULT_LATCHED.                 |
| STOPPING         | Deny permission, request safe state, close HLC/PLC; confirmation -\> CONNECTED_IDLE/UNPLUGGED.  |
| FAULT_LATCHED    | Safe state; chỉ thoát khi cause cleared và reset/unplug policy thỏa mãn.                        |

**Timing & concurrency:** Một owner runnable tuần tự hóa mọi state transition. Safety cycle và deadline được cấu hình theo HARA/WCET; event queue overflow phải resynchronize từ authoritative status.

**Fault handling:** Invalid CP, stale safety data, isolation/interlock failure hoặc actuator mismatch tạo high-priority stop. Late event không được mở lại session.

**Dependencies:** PermissionManager, FaultCoordinator, BptCoordinator, RTE/OS-SchM, CpPpManager, PlcLinkManager, HlcSessionAdapter, EvEnergyDataProvider và ChargeHardwareAdapter.

## 3.2 PermissionManager

**Trách nhiệm:** Tính toán và phát quyết định cho phép/không cho phép action năng lượng dựa trên hard invariants và qualified evidence. Đây là nơi tạo ChargePermission, nhưng lifecycle state vẫn do ChargeStateMachine sở hữu.

| **BOUNDARY RULE** PermissionManager không gọi actuator. Nó chỉ phát quyền có thời hạn; HardwareAdapter chỉ nhận command từ coordinator control path. |
|------------------------------------------------------------------------------------------------------------------------------------------------------|

### Interface contract

| **Interface / dữ liệu** | **Hướng** | **Contract chính**                                                            |
|-------------------------|-----------|-------------------------------------------------------------------------------|
| QualifiedCpPp           | IN        | CP/PP state, cable capability, quality, age.                                  |
| EvEnergySnapshot        | IN        | BMS/OBC/thermal/isolation/interlock, availability và limits.                  |
| Link/HLC/HW evidence    | IN        | Current-generation readiness và measured feedback.                            |
| BptPermissionRequest    | IN        | Mode/direction/limits từ BptCoordinator.                                      |
| ChargePermission        | OUT       | allow, allowedAction, mode, bounded limits, validUntil, reason, permissionId. |

### Behavior

- Đánh giá hard invariants trước policy/fallback: plug valid, interlock/isolation valid, no blocking fault, snapshot fresh, hardware state plausible.

- Giao limits bằng giao của vehicle limits, cable/charger negotiated limits, thermal limits và topology limits.

- Dùng fail-silent default: thiếu hoặc mâu thuẫn evidence tương đương DENY.

- Tăng permissionId/generation khi action, direction hoặc giới hạn an toàn thay đổi đáng kể.

### State model rút gọn

| **State**  | **Hoạt động / điều kiện chuyển**                                                |
|------------|---------------------------------------------------------------------------------|
| DENIED     | Default; chờ bộ evidence đầy đủ và không có blocking fault.                     |
| QUALIFYING | Kiểm tra validity/age/consistency và action preconditions.                      |
| ALLOWED    | Phát permission bounded; expiry hoặc evidence change -\> REVOKING.              |
| REVOKING   | Phát DENY trước teardown; xác nhận publication -\> DENIED.                      |
| BLOCKED    | Fault latched/policy block; chỉ clear theo FaultCoordinator recovery condition. |

**Timing & concurrency:** Chạy cùng hoặc nhanh hơn safety-cycle của ChargeStateMachine. validUntil dùng monotonic time; expiry được consumer hiểu là DENY mà không cần event.

**Fault handling:** Bất kỳ input invalid/stale/out-of-range đều thu hồi permission. Không giữ last-known-good cho quyết định năng lượng.

**Dependencies:** EvEnergyDataProvider, CpPpManager, HlcSessionAdapter, PlcLinkManager, ChargeHardwareAdapter, FaultCoordinator và calibrated safety policy.

## 3.3 BptCoordinator

**Trách nhiệm:** Điều phối policy và invariant riêng cho bidirectional power transfer (AC-BPT/DC-BPT, V2G/V2H/V2L), biến yêu cầu công suất hai chiều thành mode/direction/limits an toàn cho PermissionManager và ChargeStateMachine.

### Interface contract

| **Interface / dữ liệu**             | **Hướng** | **Contract chính**                                                  |
|-------------------------------------|-----------|---------------------------------------------------------------------|
| BptCapability + EnergySnapshot      | IN        | Khả năng inverter/OBC/BMS, SOC, thermal, charge/discharge limits.   |
| NegotiatedBptService / PowerRequest | IN        | Request/evidence từ ISO 15118-20 adapter.                           |
| Vehicle/Grid policy                 | IN        | Allowed use case, min SOC, departure/mobility need, grid mode.      |
| BptPermissionRequest                | OUT       | Direction, max import/export power, ramp/zero-crossing constraints. |
| BptStatus/Event                     | OUT       | Mode, direction, limiting reason, reversal progress.                |

### Behavior

- Phân biệt sign convention nội bộ rõ ràng và chuyển đổi đúng một lần tại boundary.

- Kiểm tra capability của cả vehicle hardware và selected ISO-20 service; protocol support không tự suy ra powertrain support.

- Direction reversal phải qua 0 kW, có measured confirmation và permission mới.

- Áp dụng mobility reserve/min SOC, thermal derating, battery/inverter/OBC limits và use-case policy.

### State model rút gọn

| **State**    | **Hoạt động / điều kiện chuyển**                        |
|--------------|---------------------------------------------------------|
| DISABLED     | Không quảng bá/không chấp nhận BPT.                     |
| AVAILABLE    | Capabilities valid; chờ negotiated BPT service.         |
| IMPORT       | Power vào xe; limits được cập nhật theo snapshot.       |
| RAMP_TO_ZERO | Thu công suất về zero trước stop/reversal.              |
| EXPORT       | Power ra ngoài; giám sát SOC/mobility/grid constraints. |
| BLOCKED      | Capability/policy/safety condition không hợp lệ.        |

**Timing & concurrency:** Power limits được refresh theo snapshot; ramp và zero-crossing deadline là configuration. Mọi callback phải mang sessionGeneration.

**Fault handling:** Link/permission loss yêu cầu công suất về zero/safe state. Không đảo chiều nếu chưa có zero confirmation và permissionId mới.

**Dependencies:** EvEnergyDataProvider::BptCapabilityProvider, ISO15118Chrg::Iso15118_20Adapter, PermissionManager, FaultCoordinator và ChargeHardwareAdapter::BidirectionalPowerAdapter.

## 3.4 FaultCoordinator

**Trách nhiệm:** Chuẩn hóa fault/event từ các domain, xác định severity, latching, reaction và recovery precondition; cung cấp một blocking-fault view nhất quán cho coordinator thay vì để từng module tự diễn giải mã lỗi của nhau.

### Interface contract

| **Interface / dữ liệu**            | **Hướng** | **Contract chính**                                                 |
|------------------------------------|-----------|--------------------------------------------------------------------|
| CpPp/Energy/PLC/HLC/HW diagnostics | IN        | Reason code ổn định + detailed Dem reference.                      |
| Watchdog/platform status           | IN        | Supervision, resource và mode failures.                            |
| BlockingFaultStatus                | OUT       | Severity, reaction, latch, cause, generation, clear condition.     |
| SafeStopRequest                    | OUT       | Normal/controlled/emergency classification cho ChargeStateMachine. |
| DiagnosticEvent                    | OUT       | Dem mapping và service information không chứa secret/raw payload.  |

### Behavior

- Map lỗi về taxonomy: PRECONDITION, STALE_DATA, LINK_LOSS, TIMEOUT, SEQUENCE, FORMAT, SECURITY, RESOURCE, PEER_REJECT, CANCELLED, INTERNAL.

- Ưu tiên fault safety-related hơn communication retry/fallback policy.

- Duy trì root cause đầu tiên và secondary evidence mà không làm mất nguyên nhân ban đầu.

- Tách automatic recovery, session restart, unplug-reset và service-reset.

### State model rút gọn

| **State**          | **Hoạt động / điều kiện chuyển**                                            |
|--------------------|-----------------------------------------------------------------------------|
| CLEAR              | Không có blocking fault.                                                    |
| ACTIVE_RECOVERABLE | Yêu cầu stop; có thể recover khi fresh evidence trở lại và policy cho phép. |
| ACTIVE_LATCHED     | Giữ safe state; cần unplug/reset/service condition.                         |
| CLEAR_PENDING      | Xác nhận cause đã hết trong debounce/qualification window.                  |

**Timing & concurrency:** Fault reaction được phát trong safety-cycle; debounce chỉ dùng cho recovery, không trì hoãn immediate safety reaction.

**Fault handling:** Không tự clear fault latched do message/event biến mất. Queue overflow phải được phát hiện và đồng bộ lại từ status.

**Dependencies:** Dem/Det, WdgM, RTE, NvM fault policy, ChargeStateMachine và tất cả diagnostic producers.

# 4. Composition 2 - ISO15118Chrg

ISO15118Chrg là communication-service composition sở hữu HLC session, từ SECC discovery đến transport, SAP negotiation và protocol FSM. Public AUTOSAR boundary giữ ChargeControlPort (provided) và ChargeParametersPort (required) theo release/supplier baseline. Metadata dự án như sessionGeneration, freshness và semantic HlcEvent được bổ sung tại HlcSessionAdapter, không sửa layout của ISO15118Chrg\_\* normative types.

| **Boundary**                    | **Contract**                                                                                                                             |
|---------------------------------|------------------------------------------------------------------------------------------------------------------------------------------|
| ChargeControlPort (Provided)    | SetChargeState và SetCpLineStatus; chỉ nhận qualified state từ ASW.                                                                      |
| ChargeParametersPort (Required) | SupportedAppProtocol, session/service/payment/auth/charge parameters, cable check, precharge, power delivery, charge loops, state/error. |
| DataLinkIndication              | BSW callback; inactive lập tức invalidates communication evidence.                                                                       |
| SoAd/PduR/TcpIp                 | UDP SDP, TCP TP path, IPv6 assignment; không rò POSIX socket.                                                                            |
| Crypto/Csm/KeyM/HSM             | TLS, certificate validation, signing; private key chỉ là handle.                                                                         |
| HlcSessionAdapter               | Project ASW facade chuyển lifecycle/snapshot/permission/link thành normative calls và semantic events.                                   |

## 4.1 HlcSessionAdapter (ASW façade)

**Trách nhiệm:** Façade xác định boundary giữa application compositions và ISO15118Chrg BSW. Nó bảo toàn ABI AUTOSAR, thêm project metadata ở interface riêng và là điểm duy nhất map coordinator lifecycle/link/snapshot/permission vào ChargeControl/ChargeParameters.

| **BOUNDARY RULE** Đây là project SWC đi kèm composition, không phải nơi thay đổi normative ISO15118Chrg ABI. |
|--------------------------------------------------------------------------------------------------------------|

### Interface contract

| **Interface / dữ liệu**             | **Hướng** | **Contract chính**                                                            |
|-------------------------------------|-----------|-------------------------------------------------------------------------------|
| StartHlc / StopHlc / Renegotiate    | IN        | sessionGeneration, commandId, policy, linkGeneration, reason.                 |
| EvChargeSnapshot / ChargePermission | IN        | Read once per callback; kiểm tra age/generation/expiry.                       |
| PlcLinkStatus                       | IN        | Map current LinkReady vào SlacStateParams; loss -\> emergency close.          |
| ChargeControlPort                   | OUT       | SetCpLineStatus; SetChargeState START/RENEGOTIATE/NORMAL_STOP/EMERGENCY_STOP. |
| ChargeParametersPort                | I/O       | Map semantic vehicle context sang normative parameter operations.             |
| HlcSessionStatus / HlcEvent         | OUT       | Authoritative status + bounded queued events cho ChargingCoordinator.         |

### Behavior

- Reject StartHlc nếu CP/link evidence không valid/current-generation.

- Không block chờ network close; command được accept bất đồng bộ và status phản ánh tiến triển.

- Gate READY/START/progress responses bằng fresh ChargePermission.

- Normalize protocol-specific state/error về event taxonomy ổn định cho coordinator.

### State model rút gọn

| **State** | **Hoạt động / điều kiện chuyển**                    |
|-----------|-----------------------------------------------------|
| IDLE      | Không bind generation.                              |
| BOUND     | Bind generation/link/context; invoke START.         |
| ACTIVE    | Serve ChargeParameters và publish status/events.    |
| STOPPING  | Map stop, invalidate readiness; chờ terminal state. |
| ERROR     | Publish one terminal reason rồi về IDLE.            |

**Timing & concurrency:** RTE server runnable không block trên SDP/TCP/TLS/EXI/NvM. Status update nguyên tử; queue event bounded và recoverable từ status.

**Fault handling:** Permission expiry, CP invalid, LinkLost hoặc generation mismatch lập tức invalidates readiness và yêu cầu close.

**Dependencies:** ChargingCoordinator, EvEnergyDataProvider, PlcLinkManager, ISO15118Chrg standard ports và RTE.

## 4.2 HlcSessionSupervisor

**Trách nhiệm:** Owner của HLC lifecycle và orchestration các common services: link readiness, SDP, transport, SAP, adapter execution và closing. Supervisor không sở hữu protocol message sequence chi tiết.

### Interface contract

| **Interface / dữ liệu** | **Hướng** | **Contract chính**                                                    |
|-------------------------|-----------|-----------------------------------------------------------------------|
| SessionControl          | IN        | Start/Stop/Renegotiate + generation/policy.                           |
| LinkContext             | IN        | Qualified PLC/IPv6 readiness.                                         |
| Service results         | IN        | SDP, transport, SAP và adapter completion/error.                      |
| HlcStatus/Event         | OUT       | Semantic lifecycle, negotiated protocol, security và terminal reason. |
| ServiceControl          | OUT       | Start/cancel lower service theo current context.                      |

### Behavior

- Tạo context theo sessionGeneration; chỉ dispatch một active path.

- Chạy đúng thứ tự link -\> SDP -\> TCP/TLS -\> SAP -\> protocol adapter.

- Timer thuộc lowest layer có đủ thông tin; supervisor chỉ quản overall setup/session deadline.

- Closing hủy timer, socket, adapter, transient security/session state theo thứ tự idempotent.

### State model rút gọn

| **State**        | **Hoạt động / điều kiện chuyển**                 |
|------------------|--------------------------------------------------|
| IDLE             | Start -\> WAIT_LINK.                             |
| WAIT_LINK        | LinkReady -\> SDP; timeout -\> ERROR.            |
| SDP              | Endpoint valid -\> TRANSPORT.                    |
| TRANSPORT        | Connected/security ready -\> SAP.                |
| SAP              | Protocol selected -\> PROTOCOL_SESSION.          |
| PROTOCOL_SESSION | Run selected adapter; stop/complete -\> CLOSING. |
| CLOSING          | Release resources -\> IDLE.                      |
| ERROR            | Publish reason -\> CLOSING.                      |

**Timing & concurrency:** Overall session deadlines dùng monotonic clock; mọi lower callback được copy bounded rồi xử lý trong owner runnable.

**Fault handling:** Malformed message, timer expiry, link/permission loss hoặc cancel đi qua một controlled close path và chỉ publish một terminal reason.

**Dependencies:** HlcSessionAdapter, SdpClient, TcpTlsSession, SapNegotiator, ProtocolAdapterFactory, Dem và OS/SchM.

## 4.3 SdpClient

**Trách nhiệm:** Khám phá endpoint IPv6 của SECC và transport-security mode qua UDP trên PLC link-local interface.

### Interface contract

| **Interface / dữ liệu** | **Hướng** | **Contract chính**                                        |
|-------------------------|-----------|-----------------------------------------------------------|
| DiscoverEndpoint        | IN        | generation, interface, transport/security policy.         |
| UdpSocket service       | I/O       | SoAd/TcpIp UDP datagram.                                  |
| SdpResult               | OUT       | IPv6 address, port, transport, security, validity/reason. |
| SdpDiag                 | OUT       | Timeout, invalid header/payload/endpoint/policy.          |

### Behavior

- Chỉ chạy sau LinkReady và IPv6 readiness.

- Bind đúng interface/scope; validate V2GTP header và SDP fields.

- Retry/backoff bounded; cancel ngay khi LinkLost/StopSession.

- DIN chỉ chấp nhận TCP không TLS; downgrade khác chỉ theo explicit policy.

### State model rút gọn

| **State**        | **Hoạt động / điều kiện chuyển**            |
|------------------|---------------------------------------------|
| IDLE             | Discover -\> BUILD_REQUEST.                 |
| BUILD_REQUEST    | Policy valid -\> SEND.                      |
| WAIT_RESPONSE    | Frame -\> VALIDATE; timer -\> RETRY/FAILED. |
| VALIDATE         | Endpoint/security valid -\> COMPLETE.       |
| COMPLETE         | Return immutable result -\> IDLE.           |
| FAILED/CANCELLED | Return reason và cleanup -\> IDLE.          |

**Timing & concurrency:** SDP retry interval tối thiểu 250 ms theo baseline requirement; retry count/overall timeout ECUC-controlled.

**Fault handling:** Reject wrong interface, generation, payload type hoặc invalid transport/security. Không cache endpoint qua plug session.

**Dependencies:** SoAd/TcpIp, EthSM/interface status, V2gtpCodec và OS timers.

## 4.4 TcpTlsSession

**Trách nhiệm:** Cung cấp stream transport bounded/cancellable và optional TLS; che giấu SoAd/TcpIp, TLS supplier và crypto internals khỏi protocol FSM.

### Interface contract

| **Interface / dữ liệu**   | **Hướng** | **Contract chính**                                           |
|---------------------------|-----------|--------------------------------------------------------------|
| Connect(endpoint, policy) | IN        | Async; trả connectionHandle current generation.              |
| Send / Receive            | I/O       | Bounded buffers, deadline và backpressure.                   |
| Close                     | IN        | Idempotent close reason.                                     |
| TransportStatus/Evidence  | OUT       | Connected/security/cipher/peer result; không có private key. |
| SoAd/TcpIp + Crypto       | Required  | AUTOSAR network và Csm/KeyM/HSM operations.                  |

### Behavior

- Chỉ kết nối endpoint từ current SDP result.

- Thực thi TLS theo negotiated protocol/policy; DIN không TLS.

- Không giả định receive indication trùng V2GTP boundary.

- Giới hạn TX/RX queue và cancel connect/handshake/I/O khi stop/link loss.

### State model rút gọn

| **State**           | **Hoạt động / điều kiện chuyển**                |
|---------------------|-------------------------------------------------|
| CLOSED              | Connect -\> TCP_CONNECTING.                     |
| TCP_CONNECTING      | Connected -\> TCP_ESTABLISHED.                  |
| TCP_ESTABLISHED     | No TLS -\> ACTIVE_PLAIN; TLS -\> TLS_HANDSHAKE. |
| TLS_HANDSHAKE       | Success -\> ACTIVE_SECURE; failure -\> ERROR.   |
| ACTIVE_PLAIN/SECURE | Send/receive; close/loss -\> CLOSING.           |
| CLOSING             | Release socket/crypto -\> CLOSED.               |
| ERROR               | Invalidate handle -\> CLOSING.                  |

**Timing & concurrency:** Tách connect, handshake và I/O deadlines. BSW callbacks không advance FSM trực tiếp; chỉ signal owner runnable.

**Fault handling:** Security failure không retry plaintext trừ khi có một policy path được authorize riêng. Error invalidates handle và queued callbacks.

**Dependencies:** SoAd, TcpIp, EthSM, Csm, KeyM, Crypto/HSM, SchM và Dem.

## 4.5 V2gtpCodec

**Trách nhiệm:** Frame/parse V2GTP và bảo vệ upper layer khỏi version, inverse-version, payload-type và length không hợp lệ.

### Interface contract

| **Interface / dữ liệu** | **Hướng** | **Contract chính**                                           |
|-------------------------|-----------|--------------------------------------------------------------|
| EncodeFrame             | IN/OUT    | payloadType + bounded payload -\> complete frame/size error. |
| ConsumeBytes            | IN        | Stream fragments current handle/generation.                  |
| ValidatedFrame          | OUT       | Immutable bounded-lifetime payload view.                     |
| CodecDiag               | OUT       | VERSION/TYPE/LENGTH/OVERFLOW/FORMAT reason.                  |

### Behavior

- Validate fixed header trước khi reserve payload buffer.

- Hỗ trợ partial header, fragmented payload và nhiều frame trong một indication.

- Payload type phải phù hợp phase SAP/selected protocol.

- Không log sensitive payload theo default.

### State model rút gọn

| **State**       | **Hoạt động / điều kiện chuyển**                       |
|-----------------|--------------------------------------------------------|
| AWAIT_HEADER    | Accumulate fixed header.                               |
| VALIDATE_HEADER | Valid -\> AWAIT_PAYLOAD; invalid -\> REJECT.           |
| AWAIT_PAYLOAD   | Exact bounded length -\> FRAME_READY.                  |
| FRAME_READY     | Deliver view -\> AWAIT_HEADER.                         |
| REJECT          | Discard defined scope; policy quyết định resync/close. |

**Timing & concurrency:** Incremental parser có WCET theo byte budget; declared length không được gây dynamic unbounded allocation.

**Fault handling:** Unsupported type, inconsistent header hoặc oversize length tạo deterministic reject và thông thường close session.

**Dependencies:** TcpTlsSession, configured payload limits, Dem/Det, SAP và protocol adapters.

## 4.6 ExiCodec

**Trách nhiệm:** Encode/decode schema-informed message sau khi protocol/schema đã được chọn rõ ràng; cung cấp bounded typed result hoặc structured error.

### Interface contract

| **Interface / dữ liệu** | **Hướng** | **Contract chính**                          |
|-------------------------|-----------|---------------------------------------------|
| ExiEncode               | IN/OUT    | schemaId + typed message + output buffer.   |
| ExiDecode               | IN/OUT    | schemaId + byte view -\> typed object.      |
| SchemaRegistry          | Required  | Immutable grammar/generated-type metadata.  |
| ExiDiag                 | OUT       | SCHEMA/RANGE/GRAMMAR/SIZE/SIGNATURE reason. |

### Behavior

- Không tự đoán protocol/schema từ untrusted payload.

- Validate occurrence count, string length, numeric range và optional field presence.

- Dùng caller-provided/fixed workspace; zero transient state sau reject.

- Golden-vector/differential test với SwitchEV reference ở semantic level.

### State model rút gọn

| **State**       | **Hoạt động / điều kiện chuyển**             |
|-----------------|----------------------------------------------|
| IDLE            | Request -\> VALIDATE_REQUEST.                |
| SELECT_SCHEMA   | Resolve explicit schema/profile.             |
| ENCODE/DECODE   | Run bounded generated grammar.               |
| VALIDATE_RESULT | Constraints valid -\> COMPLETE; else REJECT. |
| COMPLETE/REJECT | Return result/reason, cleanup -\> IDLE.      |

**Timing & concurrency:** Đo WCET/workspace với largest enabled messages và maximum schedule points; không unbounded heap growth sau init.

**Fault handling:** Partially decoded data không bao giờ được chuyển cho FSM. Grammar/constraint/buffer/signature error rejects toàn message.

**Dependencies:** Generated schema code, SchemaRegistry, fixed pools, selected EXI product, Csm signature hooks và Dem/Det.

## 4.7 SchemaRegistry

**Trách nhiệm:** Quản lý mapping bất biến từ protocol selection tới đúng EXI grammar, namespace, generated types, resource budget và validation profile.

### Interface contract

| **Interface / dữ liệu** | **Hướng** | **Contract chính**                                        |
|-------------------------|-----------|-----------------------------------------------------------|
| GetSchema(schemaId)     | Provided  | Read-only descriptor, version, namespace và codec vtable. |
| Schema configuration    | IN        | Generated/ECUC feature set; không runtime discovery.      |
| Schema self-test result | OUT       | Integrity/version/compatibility status.                   |

### Behavior

- Duy trì các ID riêng: SAP, DIN_70121, ISO_15118_2, ISO_20_COMMON, ISO_20_AC và ISO_20_DC.

- Không mix typed grammars giữa adapter; BPT dùng ISO-20 AC hoặc DC schema tương ứng.

- Kiểm tra version/hash/feature compatibility khi init.

- Công bố per-schema maximum message, collection và workspace sizes.

### State model rút gọn

| **State**     | **Hoạt động / điều kiện chuyển**                                |
|---------------|-----------------------------------------------------------------|
| UNINITIALIZED | Load generated table.                                           |
| VALIDATING    | Check uniqueness/integrity/capacities.                          |
| READY         | Serve immutable descriptors.                                    |
| FAILED        | Disable affected protocol/schema và report configuration fault. |

**Timing & concurrency:** Lookup deterministic O(1)/bounded; registry immutable sau init để tránh race và runtime schema switch.

**Fault handling:** Unknown/duplicate/incompatible schemaId chặn protocol offer hoặc adapter creation; không fallback bằng đoán namespace.

**Dependencies:** Generated EXI artifacts, ECUC/build configuration, ExiCodec, SapNegotiator và ProtocolAdapterFactory.

## 4.8 SapNegotiator

**Trách nhiệm:** Thực hiện SupportedAppProtocol exchange, quảng bá ordered enabled offers và chọn đúng một protocol adapter/schema hợp lệ.

### Interface contract

| **Interface / dữ liệu** | **Hướng** | **Contract chính**                                                   |
|-------------------------|-----------|----------------------------------------------------------------------|
| EnabledProtocols        | IN        | Priority, namespace, major/minor, schemaId, transport compatibility. |
| Negotiate               | IN        | Current transport/security/energy context.                           |
| SAP message channel     | I/O       | V2GTP + EXI_SCHEMA_SAP.                                              |
| ProtocolSelection       | OUT       | Validated immutable ProtocolId + schemaId.                           |
| AdapterFactory          | Required  | Create selected adapter only after validation.                       |

### Behavior

- Build deterministic offer list từ configuration và session context.

- Forbid DIN over TLS và các protocol/transport combination không hợp lệ.

- Validate response code/schemaId đúng với offered list.

- Phân biệt NO_PROTOCOL_MATCH, malformed response và transport timeout.

### State model rút gọn

| **State**          | **Hoạt động / điều kiện chuyển**            |
|--------------------|---------------------------------------------|
| IDLE               | Start -\> BUILD_OFFERS.                     |
| BUILD_OFFERS       | Nonempty -\> ENCODE_SEND; empty -\> FAILED. |
| WAIT_RESPONSE      | Decode response -\> VALIDATE_SELECTION.     |
| VALIDATE_SELECTION | Valid -\> ADAPTER_READY; else FAILED.       |
| ADAPTER_READY      | Bind schema/adapter -\> IDLE.               |
| FAILED             | Report reason và close/cleanup.             |

**Timing & concurrency:** SAP request/response timer do negotiator sở hữu; overall cancellation do HlcSessionSupervisor.

**Fault handling:** Unoffered schemaId, unsupported namespace hoặc invalid transport combination kết thúc negotiation, không fallback ngoài configured list.

**Dependencies:** ExiCodec SAP grammar, V2gtpCodec, TcpTlsSession, SchemaRegistry, versioned protocol configuration và ProtocolAdapterFactory.

## 4.9 ProtocolAdapterFactory

**Trách nhiệm:** Tạo đúng một deterministic protocol adapter cho selection đã xác thực, bind đúng message model/schema/configuration và quản lifecycle bộ nhớ của adapter.

### Interface contract

| **Interface / dữ liệu**    | **Hướng** | **Contract chính**                                        |
|----------------------------|-----------|-----------------------------------------------------------|
| Create(selection, context) | IN        | Validated ProtocolId/schemaId, mode/security, generation. |
| AdapterHandle              | OUT       | IProtocolAdapter handle với bounded storage.              |
| Destroy/Reset              | IN        | Idempotent cleanup.                                       |
| FactoryDiag                | OUT       | Unsupported/config/resource/integrity reason.             |

### Behavior

- Dùng static pool/compile-time variants; không reflection/dynamic module loading.

- Reject selection không match build feature, schema hoặc transport policy.

- Khởi tạo adapter với immutable context; không copy secret/private key.

- Đảm bảo một active adapter trên một session context.

### State model rút gọn

| **State**  | **Hoạt động / điều kiện chuyển**            |
|------------|---------------------------------------------|
| READY      | No active adapter.                          |
| CREATING   | Validate selection/resource rồi initialize. |
| ACTIVE     | Handle owned by supervisor.                 |
| DESTROYING | Zero context/release pool -\> READY.        |
| FAILED     | Report reason; remain READY after cleanup.  |

**Timing & concurrency:** Create/destroy bounded, nonblocking; pool capacity và concurrent-session count là configuration.

**Fault handling:** Allocation/config mismatch chặn session trước protocol FSM; không tái sử dụng dirty adapter state.

**Dependencies:** SapNegotiator, SchemaRegistry, DIN/ISO-2/ISO-20 adapters, static memory pools và build/ECUC configuration.

## 4.10 Din70121Adapter

**Trách nhiệm:** Triển khai legacy DIN SPEC 70121 EVCC DC charging sequence trên non-TLS TCP, đồng thời xuất semantic HlcEvent giống các adapter mới hơn.

### Interface contract

| **Interface / dữ liệu**  | **Hướng** | **Contract chính**                                |
|--------------------------|-----------|---------------------------------------------------|
| ProtocolControl          | IN        | Start/Stop + negotiated DIN context.              |
| EvContext/Permission     | IN        | Protocol-neutral SI values và bounded permission. |
| Typed DIN MessageChannel | I/O       | DIN schema qua EXI/V2GTP.                         |
| HlcEvent/ProtocolDiag    | OUT       | Normalized request/state/failure.                 |

### Behavior

- Chỉ hỗ trợ DC và TCP không TLS.

- Enforce message order/timers và map SI values sang DIN physical values.

- CableCheck/Precharge/PowerDelivery/Stop chỉ tạo request/evidence upward.

- Chỉ gửi readiness/energy-flow indication khi permission valid.

### State model rút gọn

| **State**                 | **Hoạt động / điều kiện chuyển**          |
|---------------------------|-------------------------------------------|
| SESSION_SETUP             | -\> SERVICE_DISCOVERY.                    |
| SERVICE/PAYMENT_SELECTION | External payment -\> AUTHENTICATION.      |
| AUTHENTICATION            | Accepted -\> CHARGE_PARAMETER_DISCOVERY.  |
| CABLE_CHECK               | Finished -\> PRECHARGE.                   |
| PRECHARGE                 | Voltage matched -\> POWER_DELIVERY_START. |
| CURRENT_DEMAND            | Loop; stop -\> POWER_DELIVERY_STOP.       |
| WELDING_DETECTION         | Finished -\> SESSION_STOP.                |
| PROTOCOL_FAILURE          | Normalize error -\> common close path.    |

**Timing & concurrency:** DIN response timers/message-loop limits nằm trong generated protocol configuration.

**Fault handling:** Unexpected/negative response, permission loss, sequence error hoặc timeout tạo protocol failure và controlled close.

**Dependencies:** DIN EXI schema, common message channel, EvChargeContext, ChargePermission và protocol timers.

## 4.11 Iso15118_2Adapter

**Trách nhiệm:** Triển khai ISO 15118-2 AC/DC, EIM baseline và optional PnC; tách protocol FSM khỏi transport, EXI, crypto và vehicle control.

### Interface contract

| **Interface / dữ liệu** | **Hướng** | **Contract chính**                                |
|-------------------------|-----------|---------------------------------------------------|
| ProtocolControl         | IN        | Start/Stop + ISO-2 context/mode.                  |
| EvContext/Permission    | IN        | Read-only snapshots và permission.                |
| ISO-2 MessageChannel    | I/O       | ISO-2 typed messages.                             |
| PkiCrypto               | Required  | Certificate/signature service cho PnC.            |
| HlcEvent                | OUT       | Service/auth/schedule/energy requests normalized. |

### Behavior

- Chọn AC hoặc DC branch theo negotiated energy-transfer mode.

- EIM triển khai trước; PnC được cô lập qua PkiSecurityAdapter/CertificateManager.

- Schedule/profile/certificate collection đều bounded.

- DIN và ISO-2 có thể chung semantic vocabulary nhưng không chung protocol state ownership.

### State model rút gọn

| **State**                       | **Hoạt động / điều kiện chuyển**                                 |
|---------------------------------|------------------------------------------------------------------|
| SESSION_SETUP/SERVICE_DISCOVERY | Discover services/payment/mode.                                  |
| PAYMENT_SELECTION               | EIM -\> AUTHORIZATION; PnC -\> CERT/PAYMENT_DETAILS.             |
| AUTHORIZATION                   | Accepted -\> CHARGE_PARAMETER_DISCOVERY.                         |
| AC PATH                         | POWER_DELIVERY -\> CHARGING_STATUS/METERING_RECEIPT.             |
| DC PATH                         | CABLE_CHECK -\> PRECHARGE -\> POWER_DELIVERY -\> CURRENT_DEMAND. |
| SESSION_STOP                    | Close/pause -\> TERMINATE.                                       |
| PROTOCOL_FAILURE                | Normalize sequence/security/timeout -\> close.                   |

**Timing & concurrency:** Message/sequence timers theo configured ISO-2 profile; collection bounds và certificate-chain limit explicit.

**Fault handling:** Authorization, certificate, signature, permission, sequence và timeout failures được map thành stable reason trước close.

**Dependencies:** ISO-2 EXI model, common channel, PkiSecurityAdapter, CertificateManager/NvM, EvChargeContext và HlcSessionSupervisor.

## 4.12 Iso15118_20Adapter

**Trách nhiệm:** Triển khai ISO 15118-20 common session và dispatch sang service-specific AC, DC, AC-BPT hoặc DC-BPT sub-FSM; giữ nguyên coordinator safety boundary.

### Interface contract

| **Interface / dữ liệu**     | **Hướng** | **Contract chính**                                |
|-----------------------------|-----------|---------------------------------------------------|
| ProtocolControl             | IN        | Start/Stop + service/security configuration.      |
| EvContext/Permission        | IN        | Capabilities/snapshot, gồm BPT khi enabled.       |
| Common/AC/DC MessageChannel | I/O       | Schema selection explicit theo phase/service.     |
| PkiCrypto                   | Required  | TLS 1.3, certificate/signature operations.        |
| HlcEvent                    | OUT       | Service/schedule/control-loop request normalized. |

### Behavior

- CommonSessionFsm sở hữu setup/auth/service selection; service FSM sở hữu charge loop.

- Chỉ quảng bá vehicle-supported service/control mode.

- Bound schedule/service parameter collections và validate control-loop values.

- CertificateInstallation là implementation item riêng; không coi reference NotImplemented path là production complete.

- BPT request đi qua BptCoordinator và permission model như mọi action năng lượng khác.

### State model rút gọn

| **State**           | **Hoạt động / điều kiện chuyển**                                                     |
|---------------------|--------------------------------------------------------------------------------------|
| COMMON SETUP        | SESSION_SETUP -\> AUTHORIZATION_SETUP/AUTHORIZATION -\> SERVICE_DISCOVERY/SELECTION. |
| PARAMETER DISCOVERY | AC/AC-BPT hoặc DC/DC-BPT parameters -\> SCHEDULE_EXCHANGE.                           |
| DC PRE-ENERGY       | CABLE_CHECK -\> PRECHARGE.                                                           |
| POWER_DELIVERY      | START -\> selected AC/DC charge loop.                                                |
| AC/DC CHARGE LOOP   | Scheduled/dynamic control; stop/renegotiate -\> POWER_DELIVERY.                      |
| DC WELDING          | Post-charge -\> SESSION_STOP.                                                        |
| SESSION_STOP        | Renegotiate -\> SERVICE_DISCOVERY; terminate -\> release.                            |
| PROTOCOL_FAILURE    | Security/service/schedule/sequence/timeout -\> common close.                         |

**Timing & concurrency:** Timer/control-loop period và resource budget được cấu hình theo từng service; schema switch chỉ tại allowed state boundary.

**Fault handling:** Unsupported service, schedule overflow, stale permission hoặc direction violation kết thúc service/session; không bypass coordinator.

**Dependencies:** ISO-20 Common/AC/DC schemas, TcpTlsSession, PkiSecurityAdapter, CertificateManager, BptCoordinator, EvChargeContext và common message channel.

## 4.13 PkiSecurityAdapter

**Trách nhiệm:** Cô lập TLS/PKI/signature semantics khỏi protocol FSM và map operations sang Csm/KeyM/Crypto/HSM bằng key/certificate handles.

### Interface contract

| **Interface / dữ liệu** | **Hướng** | **Contract chính**                                                  |
|-------------------------|-----------|---------------------------------------------------------------------|
| ValidatePeerChain       | IN/OUT    | Bounded chain + policy -\> validation evidence/reason.              |
| Sign/Verify             | IN/OUT    | Algorithm/profile + digest/data reference; private key handle only. |
| TlsPolicy/Evidence      | I/O       | Allowed/required mode, cipher, peer result.                         |
| CertificateManager      | Required  | Contract/OEM cert references và lifecycle.                          |
| SecurityDiag            | OUT       | Stable reason; no secret/raw key logging.                           |

### Behavior

- Kiểm tra length/range/policy trước crypto processing.

- Không xuất private key; protocol structures chỉ giữ handle/public material.

- Tách transport TLS handshake với XML/message signature operation.

- Áp dụng anti-replay/challenge/session binding theo selected protocol profile.

### State model rút gọn

| **State**        | **Hoạt động / điều kiện chuyển**         |
|------------------|------------------------------------------|
| IDLE             | Operation -\> VALIDATE_REQUEST.          |
| WAIT_CRYPTO      | Async Csm/HSM job.                       |
| VALIDATE_RESULT  | Check policy/session binding.            |
| COMPLETE         | Return bounded evidence.                 |
| FAILED/CANCELLED | Zero transient buffers và return reason. |

**Timing & concurrency:** Crypto operations bất đồng bộ, có deadline riêng; caller không block. Chain/signature sizes bounded.

**Fault handling:** Policy mismatch, invalid chain/signature, HSM failure hoặc timeout là security failure; không tự downgrade.

**Dependencies:** Csm, Crypto, KeyM, HSM, TcpTlsSession, CertificateManager, ISO-2/ISO-20 adapters và Dem.

## 4.14 CertificateManager

**Trách nhiệm:** Quản lifecycle metadata và reference của OEM/contract certificates, eMAID, chain và provisioning/update transaction; không sở hữu private-key bytes.

### Interface contract

| **Interface / dữ liệu**       | **Hướng** | **Contract chính**                                       |
|-------------------------------|-----------|----------------------------------------------------------|
| GetCertificateRef/GetIdentity | Provided  | Versioned handle, usage, validity, chain summary.        |
| Install/Update/Delete         | Provided  | Transactional request từ verified protocol path/policy.  |
| KeyM/NvM/HSM                  | Required  | Persistent records, key handles, atomic commit/rollback. |
| CertificateStatus/Event       | OUT       | Available/valid/expired/update-pending/failed + reason.  |

### Behavior

- Validate ownership, chain, validity, usage và binding trước commit.

- Thực hiện two-phase/transactional update để giữ last-known-consistent record khi power loss.

- Tách certificate metadata khỏi HSM key object và hạn chế log.

- PnC-to-EIM fallback là vehicle policy, không phải automatic security recovery.

### State model rút gọn

| **State**       | **Hoạt động / điều kiện chuyển**              |
|-----------------|-----------------------------------------------|
| INIT            | Load/verify records.                          |
| READY           | Serve current refs.                           |
| UPDATE_PENDING  | Stage and validate new record.                |
| COMMITTING      | Atomic NvM/KeyM commit.                       |
| ROLLBACK        | Restore previous consistent version.          |
| DEGRADED/FAILED | Disable affected PnC identity; report reason. |

**Timing & concurrency:** NvM/KeyM jobs async và deadline-supervised. Validity uses trusted time policy defined by platform/security concept.

**Fault handling:** Partial/invalid update không thay active identity. Expired/untrusted chain chặn PnC; private key remains in HSM.

**Dependencies:** KeyM, NvM, Crypto/HSM, trusted-time service, PkiSecurityAdapter và ISO-2/ISO-20 PnC handlers.

# 5. Luồng phối hợp giữa hai composition

1.  CpPpManager phát qualified connect/HLC trigger; ChargingCoordinator tạo sessionGeneration.

2.  ChargeStateMachine yêu cầu PlcLinkManager thiết lập PEV-SLAC/link. Chỉ LinkReady current-generation mới được chấp nhận.

3.  Coordinator gọi HlcSessionAdapter StartHlc; adapter map vào ISO15118Chrg ChargeControlPort.

4.  HlcSessionSupervisor chạy SDP -\> TCP/TLS -\> SAP -\> selected protocol adapter.

5.  Protocol adapter nhận peer request và phát semantic HlcEvent; không gọi phần cứng.

6.  PermissionManager kết hợp request với CP/PP, energy, isolation/interlock, link và measured hardware evidence.

7.  Nếu đủ điều kiện, Coordinator phát bounded ChargePermission và command tới ChargeHardwareAdapter; HLC chỉ phản hồi READY/START khi permission còn hiệu lực.

8.  Khi fault/stop/stale/link loss: permission bị thu hồi trước, hardware đi safe state, sau đó HLC và PLC được teardown idempotently.

# 6. Interface và data ownership tối thiểu

| **Data/state**            | **Owner**                                           | **Consumer**                             | **Điều kiện bắt buộc**                                               |
|---------------------------|-----------------------------------------------------|------------------------------------------|----------------------------------------------------------------------|
| CoordinatorState          | ChargeStateMachine                                  | Application/diagnostics                  | Authoritative nonqueued, generation + freshness.                     |
| ChargePermission          | PermissionManager / ChargingCoordinator composition | HlcSessionAdapter, HardwareAdapter       | Bounded validity; expiry = DENY.                                     |
| BptStatus                 | BptCoordinator                                      | Coordinator, energy/hardware integration | Direction và limits nhất quán sign convention.                       |
| BlockingFaultStatus       | FaultCoordinator                                    | ChargeStateMachine, PermissionManager    | Severity/latch/recovery explicit.                                    |
| HlcSessionStatus          | HlcSessionAdapter                                   | ChargingCoordinator                      | Normative state được normalize; event không là nguồn state duy nhất. |
| Transport state           | TcpTlsSession                                       | HlcSessionSupervisor                     | ConnectionHandle + generation; no socket leakage.                    |
| Protocol state            | Selected protocol adapter                           | HlcSessionSupervisor/HlcSessionAdapter   | Exactly one adapter active.                                          |
| Schema selection          | SapNegotiator + SchemaRegistry                      | ExiCodec/adapters                        | Explicit immutable schemaId.                                         |
| Certificate/key reference | CertificateManager/KeyM/HSM                         | Security adapter                         | Private key không rời HSM.                                           |

# 7. Acceptance criteria cho phân rã composition

- Không tồn tại call/RTE connector trực tiếp từ ISO15118Chrg hoặc protocol adapter tới charging actuator.

- Mỗi state domain và timer có đúng một owner; callback chỉ signal owner runnable.

- Mọi async interface vượt qua test generation mismatch, late/reordered event, cancel và queue overflow.

- ChargePermission có validity window và được kiểm tra trước mọi HLC readiness/START indication có thể dẫn tới energy transfer.

- EXI/V2GTP/SAP/certificate inputs có bounds, structured error và negative/fuzz corpus.

- DIN, ISO-2 và ISO-20 dùng adapter/schema riêng; không overload enum/ABI của normative AUTOSAR release.

- BPT direction reversal đi qua zero confirmation và permission mới.

- Mọi stop path thu hồi permission, đưa hardware về safe state và giải phóng HLC/PLC context mà callback cũ không thể phục hồi session.

# 8. Các điểm cần freeze ở bước thiết kế tiếp theo

- AUTOSAR Classic release/vendor ISO15118Chrg ARXML và exact generated ChargeControl/ChargeParameters signatures.

- RTE port catalogue, runnable mapping, queue depth, exclusive area và E2E strategy cho từng ECU boundary.

- HARA-derived safe state, ASIL allocation, controlled-stop latency và fault latching/recovery policy.

- Protocol editions/corrigenda, timer values, retry budgets và response-code mapping.

- EXI product, generated-schema workflow, maximum message/collection/workspace và WCET.

- PLC modem/TcpIp/TLS capability, PnC PKI provisioning và BPT use-case/powertrain support.

# 9. Tài liệu nguồn và tính áp dụng

Nội dung được tổng hợp từ hai baseline nội bộ đã phân tích: EVCC_AUTOSAR_Charging_Architecture Revision 4 (07/09/2026) và EVCC_AUTOSAR_Project_Overview_VI Revision 1 (07/09/2026). Các tên port nội bộ trong tài liệu này là proposed design names. Với interface chuẩn, implementation phải lấy exact definition từ AUTOSAR release và supplier-generated ARXML đã được project freeze. Timer chi tiết và safety reaction cuối cùng phải được xác nhận từ licensed DIN/ISO standards, HARA và ECU timing analysis.
