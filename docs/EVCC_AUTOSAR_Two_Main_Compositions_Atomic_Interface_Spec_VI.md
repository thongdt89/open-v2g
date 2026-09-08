# EVCC AUTOSAR - Mô tả chi tiết hai composition chính

**Tài liệu:** Atomic Interface & Sub-component Specification  
**Phạm vi:** EVCC phía xe trên AUTOSAR Classic  
**Composition:** `ChargingCoordinator`, `ISO15118Chrg`  
**Protocol baseline:** DIN SPEC 70121, ISO 15118-2 EIM  
**Mở rộng:** ISO 15118-2 PnC, ISO 15118-20, AC-BPT/DC-BPT  
**Phiên bản:** Revision 3 - 08/09/2026

> **Architecture decision:** `ChargingCoordinator` là owner duy nhất của charging progression và `ChargePermission`. `ISO15118Chrg` sở hữu HLC session và protocol behavior. Output của communication stack luôn là request/evidence, không phải actuator command.

## 1. Phạm vi và quy tắc specification

Tài liệu này thay thế cách mô tả interface dạng nhóm bằng catalogue **atomic**. Mỗi dòng interface chỉ biểu diễn đúng một communication item:

- Một `SenderReceiverInterface::DataElement`.
- Một `ClientServerInterface::Operation`.
- Một BSW callback hoặc indication.
- Một operation của local C/C++ interface giữa các sub-component.

### 1.1 Quy tắc atomic interface

| Quy tắc | Yêu cầu |
|---|---|
| Atomic item | Không gộp nhiều data element hoặc operation trong một dòng. |
| Direction | Xác định theo component đang được mô tả: `P` = provided, `R` = required, `CB` = callback/indication. |
| Semantic producer | Ghi rõ component tạo ra semantic value, kể cả khi AUTOSAR C/S parameter direction có thể khác. |
| Authority | Event không phải nguồn state duy nhất; mỗi state domain phải có authoritative nonqueued status. |
| Validity | Async item phải có `sessionGeneration`, `timestamp`, `sequenceCounter` và `quality/result/reason` trực tiếp hoặc trong envelope. |
| Cancellation | Stop/cancel idempotent; item của generation cũ bị loại và chẩn đoán. |
| Bounded data | Array, string, queue, payload, certificate chain và schedule points có capacity cố định/cấu hình. |
| ABI | Tên đề xuất trong tài liệu chưa thay thế ARXML. Exact signature của standardized interface phải lấy từ AUTOSAR release và supplier-generated artifacts đã freeze. |

### 1.2 Common communication envelope

Các S/R data element và event type của project nên sử dụng common header sau hoặc semantics tương đương:

```c
typedef struct {
    uint32 sessionGeneration;  /* 0 = invalid */
    uint32 sequenceCounter;
    uint64 timestamp;          /* monotonic time */
    DataQualityType quality;   /* INVALID, INIT, VALID, DEGRADED, STALE */
    ReasonCodeType reason;
} CommEnvelopeType;
```

`commandId`, `requestId`, `eventId`, `permissionId`, `snapshotId` và `linkGeneration` được thêm vào payload tương ứng để correlate operation/result.

### 1.3 Supporting compositions nằm ngoài phạm vi

- `CpPpManager`: CP/PP acquisition và qualification.
- `EvEnergyDataProvider`: energy/capability snapshots.
- `ChargeHardwareAdapter`: contactor, precharge, lock, discharge và bidirectional hardware execution.
- `PlcLinkManager`: modem, PEV-SLAC, association và IPv6 readiness.
- `PlatformServices`: TcpIp/SoAd, Csm/KeyM/HSM, NvM, diagnostics và OS abstraction.

> `PEV-SLAC` thuộc `PlcLinkManager`. `SDP` thuộc `ISO15118Chrg` và chỉ bắt đầu sau khi PLC link cùng IPv6 link-local đã sẵn sàng.

## 2. Composition map

| Composition | Sub-components | Ownership |
|---|---|---|
| `ChargingCoordinator` | `ChargeStateMachine`, `PermissionManager`, `BptCoordinator`, `FaultCoordinator` | Charging lifecycle, energy permission, safe-stop coordination |
| `ISO15118Chrg` | `HlcSessionAdapter`, `HlcSessionSupervisor`, `SdpClient`, `TcpTlsSession`, `V2gtpCodec`, `ExiCodec`, `SchemaRegistry`, `SapNegotiator`, `ProtocolAdapterFactory`, protocol adapters, `PkiSecurityAdapter`, `CertificateManager` | HLC lifecycle, transport, codec, protocol, security evidence |

---

# Part A - ChargingCoordinator composition

## 3. Composition responsibility

`ChargingCoordinator` hợp nhất qualified CP/PP, energy/safety snapshot, PLC/HLC state và measured hardware feedback. Composition chỉ cho phép tiến triển khi các evidence bắt buộc còn mới, hợp lệ, đúng generation và nhất quán.

### 3.1 External atomic interface catalogue

| ID | Interface type | Port / interface | Atomic element / operation | Dir. | Semantic producer -> consumer | Payload type | Trigger / contract |
|---|---|---|---|---|---|---|---|
| CC-EXT-001 | S/R DE | `Rp_CpPpStatus / If_CpPpStatus` | `QualifiedCpPp` | R | `CpPpManager -> ChargingCoordinator` | `QualifiedCpPpType` | Nonqueued cyclic/on-change; reject invalid/stale/generation mismatch. |
| CC-EXT-002 | S/R DE | `Rp_EnergySnapshot / If_EvEnergyStatus` | `EnergySnapshot` | R | `EvEnergyDataProvider -> ChargingCoordinator` | `EvEnergySnapshotType` | Atomic snapshot; maximum age equals strictest required field. |
| CC-EXT-003 | S/R DE | `Rp_PlcLinkStatus / If_PlcLinkStatus` | `LinkStatus` | R | `PlcLinkManager -> ChargingCoordinator` | `Plm_LinkStatusType` | Authoritative link state; `LINK_READY` is prerequisite, not permission. |
| CC-EXT-004 | S/R DE | `Rp_HlcStatus / If_HlcSessionStatus` | `HlcSessionStatus` | R | `HlcSessionAdapter -> ChargingCoordinator` | `HlcSessionStatusType` | Authoritative nonqueued HLC state. |
| CC-EXT-005 | S/R DE | `Rp_HlcEvent / If_HlcEvent` | `HlcEvent` | R | `HlcSessionAdapter -> ChargingCoordinator` | `HlcEventType` | Queued transition/request event; overflow requires status resync. |
| CC-EXT-006 | S/R DE | `Rp_HwStatus / If_ChargeHardwareStatus` | `HardwareStatus` | R | `ChargeHardwareAdapter -> ChargingCoordinator` | `ChargeHardwareStatusType` | Measured authoritative state; not command echo. |
| CC-EXT-007 | S/R DE | `Rp_HwResult / If_HwCommandResult` | `HardwareCommandResult` | R | `ChargeHardwareAdapter -> ChargingCoordinator` | `HwCommandResultType` | Queued or per-operation result; correlate `commandId`. |
| CC-EXT-008 | S/R DE | `Pp_ChargePermission / If_ChargePermission` | `ChargePermission` | P | `ChargingCoordinator -> HLC/HW consumers` | `ChargePermissionType` | Nonqueued; bounded by `validUntil`; expiry means `DENY`. |
| CC-EXT-009 | C/S OP | `Pp_HwControl / If_HwControl` | `PrepareHardware` | P | `ChargingCoordinator -> ChargeHardwareAdapter` | `HwPrepareRequestType` | Async acceptance; idempotent by `commandId`. |
| CC-EXT-010 | C/S OP | `Pp_HwControl / If_HwControl` | `EnableEnergyPath` | P | `ChargingCoordinator -> ChargeHardwareAdapter` | `HwEnableRequestType` | Requires current permission and expected measured pre-state. |
| CC-EXT-011 | C/S OP | `Pp_HwControl / If_HwControl` | `StopEnergyPath` | P | `ChargingCoordinator -> ChargeHardwareAdapter` | `HwStopRequestType` | Idempotent controlled stop. |
| CC-EXT-012 | C/S OP | `Pp_HwControl / If_HwControl` | `RequestSafeState` | P | `ChargingCoordinator -> ChargeHardwareAdapter` | `HwSafeStateRequestType` | Highest-priority nonblocking request. |
| CC-EXT-013 | C/S OP | `Pp_PlcControl / Plm_LinkControl` | `StartLink` | P | `ChargingCoordinator -> PlcLinkManager` | `Plm_LinkStartRequestType` | One accepted request per session generation. |
| CC-EXT-014 | C/S OP | `Pp_PlcControl / Plm_LinkControl` | `StopLink` | P | `ChargingCoordinator -> PlcLinkManager` | `Plm_LinkStopRequestType` | Idempotent; invalidates current link context. |
| CC-EXT-015 | C/S OP | `Pp_HlcControl / If_HlcLifecycle` | `StartHlc` | P | `ChargingCoordinator -> HlcSessionAdapter` | `HlcStartRequestType` | Requires qualified CP/link current generation. |
| CC-EXT-016 | C/S OP | `Pp_HlcControl / If_HlcLifecycle` | `StopHlc` | P | `ChargingCoordinator -> HlcSessionAdapter` | `HlcStopRequestType` | Normal/emergency reason; does not block for network close. |
| CC-EXT-017 | C/S OP | `Pp_HlcControl / If_HlcLifecycle` | `RenegotiateHlc` | P | `ChargingCoordinator -> HlcSessionAdapter` | `HlcRenegotiateRequestType` | Only in allowed active protocol states. |
| CC-EXT-018 | S/R DE | `Pp_CoordinatorStatus / If_CoordinatorStatus` | `CoordinatorStatus` | P | `ChargingCoordinator -> Vehicle application/diagnostics` | `CoordinatorStatusType` | Authoritative lifecycle state with reason/freshness. |

## 4. `ChargeStateMachine`

### 4.1 Responsibility

Owner duy nhất của vehicle charging lifecycle: plug-in, PLC setup, HLC negotiation, hardware readiness, energy transfer, stop và fault-latched state.

### 4.2 Atomic internal interfaces

| ID | Interface kind | Atomic element / operation | Dir. | Producer -> consumer | Type | Contract |
|---|---|---|---|---|---|---|
| CSM-IF-001 | S/R DE | `PermissionDecision` | R | `PermissionManager -> ChargeStateMachine` | `PermissionDecisionType` | Current generation; `DENY` dominates all progression events. |
| CSM-IF-002 | S/R DE | `BptStatus` | R | `BptCoordinator -> ChargeStateMachine` | `BptStatusType` | Required only for enabled BPT session. |
| CSM-IF-003 | S/R DE | `BlockingFaultStatus` | R | `FaultCoordinator -> ChargeStateMachine` | `BlockingFaultStatusType` | Severity, reaction, latch và clear condition. |
| CSM-IF-004 | Local OP | `RequestPermissionEvaluation` | P | `ChargeStateMachine -> PermissionManager` | `PermissionEvalRequestType` | Requests evaluation; does not imply allow. |
| CSM-IF-005 | Local OP | `RequestBptMode` | P | `ChargeStateMachine -> BptCoordinator` | `BptModeRequestType` | Select/import/export/zero transition request. |
| CSM-IF-006 | Local OP | `RequestFaultReaction` | P | `ChargeStateMachine -> FaultCoordinator` | `FaultReactionRequestType` | Correlates active cause and desired stop category. |
| CSM-IF-007 | S/R DE | `CoordinatorState` | P | `ChargeStateMachine -> composition peers` | `CoordinatorStateType` | Authoritative nonqueued state. |
| CSM-IF-008 | S/R DE | `CoordinatorEvent` | P | `ChargeStateMachine -> diagnostics/application` | `CoordinatorEventType` | Queued event; never sole state source. |

### 4.3 Behavior and state model

| State | Main activity | Allowed transition |
|---|---|---|
| `UNPLUGGED` | Invalidate session; permission denied; request open/safe state | Qualified connect -> `CONNECTED_IDLE` |
| `CONNECTED_IDLE` | Create `sessionGeneration`; validate vehicle availability | HLC trigger -> `LINK_SETUP` |
| `LINK_SETUP` | Request PLC link; supervise deadline | `LinkReady` -> `HLC_NEGOTIATION`; fault -> `STOPPING` |
| `HLC_NEGOTIATION` | Start HLC; evaluate negotiated service | Accepted parameters -> `READY_FOR_ENERGY` |
| `READY_FOR_ENERGY` | Coordinate cable check/precharge/readiness | Valid peer request + permission -> `ENERGY_TRANSFER` |
| `ENERGY_TRANSFER` | Publish bounded permission; supervise all evidence | Stop/stale/loss/fault -> `STOPPING`/`FAULT_LATCHED` |
| `STOPPING` | Deny permission before hardware/HLC/PLC teardown | Safe confirmation -> connected/unplugged state |
| `FAULT_LATCHED` | Maintain safe state and diagnostic evidence | Configured reset/unplug + cause cleared -> `UNPLUGGED` |

**Timing/concurrency:** một owner runnable tuần tự hóa transition. Callback chỉ copy bounded data và signal runnable.  
**Fault handling:** CP disconnect, isolation/interlock failure, stale safety snapshot hoặc actuator mismatch pre-empt normal progression.  
**Dependencies:** `PermissionManager`, `BptCoordinator`, `FaultCoordinator`, external composition ports, RTE, OS/SchM.

## 5. `PermissionManager`

### 5.1 Responsibility

Đánh giá hard invariants và tạo `ChargePermission`. Component không gọi actuator và không sở hữu lifecycle state.

### 5.2 Atomic internal interfaces

| ID | Interface kind | Atomic element / operation | Dir. | Producer -> consumer | Type | Contract |
|---|---|---|---|---|---|---|
| PM-IF-001 | S/R DE | `CoordinatorState` | R | `ChargeStateMachine -> PermissionManager` | `CoordinatorStateType` | Evaluation context; current generation only. |
| PM-IF-002 | S/R DE | `BptPermissionRequest` | R | `BptCoordinator -> PermissionManager` | `BptPermissionRequestType` | Direction, power limits, zero-crossing state. |
| PM-IF-003 | S/R DE | `BlockingFaultStatus` | R | `FaultCoordinator -> PermissionManager` | `BlockingFaultStatusType` | Blocking fault forces `DENY`. |
| PM-IF-004 | Local OP | `EvaluatePermission` | R | `ChargeStateMachine -> PermissionManager` | `PermissionEvalRequestType` | Nonblocking evaluation trigger. |
| PM-IF-005 | S/R DE | `PermissionDecision` | P | `PermissionManager -> ChargeStateMachine` | `PermissionDecisionType` | `ALLOW/DENY`, action, limits, reason, validity. |
| PM-IF-006 | S/R DE | `ChargePermission` | P | `PermissionManager -> HLC/HW boundary` | `ChargePermissionType` | Atomic update; `permissionId`, `validUntil`. |

### 5.3 Evaluation rules

1. Validate generation and freshness of every required evidence.
2. Evaluate hard invariants before protocol/policy/fallback.
3. Calculate limits as intersection of vehicle, battery, thermal, cable, negotiated charger and topology limits.
4. Publish `DENY` on missing, stale, contradictory or out-of-range evidence.
5. Revoke permission before teardown or direction change.

**States:** `DENIED -> QUALIFYING -> ALLOWED -> REVOKING -> DENIED`; blocking fault enters `BLOCKED`.  
**Timing:** execute at least once per coordinator safety cycle; `validUntil` uses monotonic time.  
**Dependencies:** qualified external evidence, `ChargeStateMachine`, `BptCoordinator`, `FaultCoordinator`.

## 6. `BptCoordinator`

### 6.1 Responsibility

Điều phối BPT policy, direction, limits, ramp-to-zero và direction reversal cho AC-BPT/DC-BPT, V2G/V2H/V2L.

### 6.2 Atomic internal interfaces

| ID | Interface kind | Atomic element / operation | Dir. | Producer -> consumer | Type | Contract |
|---|---|---|---|---|---|---|
| BPT-IF-001 | S/R DE | `BptCapability` | R | `EvEnergyDataProvider -> BptCoordinator` | `BptCapabilityType` | Supported topology, import/export bounds, grid modes. |
| BPT-IF-002 | S/R DE | `NegotiatedBptService` | R | `HlcSessionAdapter -> BptCoordinator` | `NegotiatedBptServiceType` | Selected AC-BPT/DC-BPT service and control mode. |
| BPT-IF-003 | S/R DE | `BptPowerRequest` | R | `HlcSessionAdapter -> BptCoordinator` | `BptPowerRequestType` | Peer request/evidence; not authorization. |
| BPT-IF-004 | S/R DE | `VehicleBptPolicy` | R | `Vehicle policy SWC -> BptCoordinator` | `VehicleBptPolicyType` | V2G/V2H/V2L enable, reserve SOC, mobility need. |
| BPT-IF-005 | C/S OP | `RequestBptMode` | R | `ChargeStateMachine -> BptCoordinator` | `BptModeRequestType` | Request target direction/mode. |
| BPT-IF-006 | S/R DE | `BptPermissionRequest` | P | `BptCoordinator -> PermissionManager` | `BptPermissionRequestType` | Allowed direction/ramp/limits; still requires permission evaluation. |
| BPT-IF-007 | S/R DE | `BptStatus` | P | `BptCoordinator -> ChargeStateMachine` | `BptStatusType` | Authoritative mode/direction/zero-crossing status. |
| BPT-IF-008 | S/R DE | `BptEvent` | P | `BptCoordinator -> FaultCoordinator/diagnostics` | `BptEventType` | Direction/limit/reversal/failure event. |

### 6.3 Behavior

- Dùng một sign convention nội bộ duy nhất; conversion chỉ tại boundary.
- Direction reversal bắt buộc đi qua measured `0 kW` và `permissionId` mới.
- Enforce minimum SOC, mobility reserve, BMS/inverter/OBC và thermal limits.
- Link/permission loss yêu cầu ramp về zero/safe state.

**States:** `DISABLED`, `AVAILABLE`, `IMPORT`, `RAMP_TO_ZERO`, `EXPORT`, `BLOCKED`.  
**Dependencies:** energy/BPT capability provider, ISO-20 adapter, `PermissionManager`, hardware feedback và vehicle policy.

## 7. `FaultCoordinator`

### 7.1 Responsibility

Chuẩn hóa fault/event đa domain thành severity, reaction, latch và recovery condition thống nhất.

### 7.2 Atomic internal interfaces

| ID | Interface kind | Atomic element / operation | Dir. | Producer -> consumer | Type | Contract |
|---|---|---|---|---|---|---|
| FC-IF-001 | S/R DE | `CpPpDiagnostic` | R | `CpPpManager -> FaultCoordinator` | `CpPpDiagnosticType` | Qualified reason; no raw ADC. |
| FC-IF-002 | S/R DE | `EnergyDataDiagnostic` | R | `EvEnergyDataProvider -> FaultCoordinator` | `EnergyDataDiagnosticType` | Stale/range/consistency reason. |
| FC-IF-003 | S/R DE | `PlcLinkDiagnostic` | R | `PlcLinkManager -> FaultCoordinator` | `Plm_DiagnosticType` | Modem/SLAC/link reason normalized at PLC boundary. |
| FC-IF-004 | S/R DE | `HlcDiagnostic` | R | `HlcSessionAdapter -> FaultCoordinator` | `HlcDiagnosticType` | Protocol/transport/security reason; no secret/raw payload. |
| FC-IF-005 | S/R DE | `HardwareDiagnostic` | R | `ChargeHardwareAdapter -> FaultCoordinator` | `HardwareDiagnosticType` | Mismatch, timeout, welded contactor, discharge fault. |
| FC-IF-006 | S/R DE | `PlatformSupervisionStatus` | R | `WdgM/platform -> FaultCoordinator` | `PlatformSupervisionStatusType` | Watchdog/resource/mode evidence. |
| FC-IF-007 | C/S OP | `RequestFaultReaction` | R | `ChargeStateMachine -> FaultCoordinator` | `FaultReactionRequestType` | Correlate cause và requested stop category. |
| FC-IF-008 | S/R DE | `BlockingFaultStatus` | P | `FaultCoordinator -> CSM/PermissionManager` | `BlockingFaultStatusType` | Authoritative severity, latch, reaction, clear condition. |
| FC-IF-009 | S/R DE | `SafeStopRequest` | P | `FaultCoordinator -> ChargeStateMachine` | `SafeStopRequestType` | Controlled/emergency stop request. |
| FC-IF-010 | S/R DE | `FaultEvent` | P | `FaultCoordinator -> diagnostics` | `FaultEventType` | Root cause + secondary evidence reference. |

### 7.3 Fault taxonomy

`PRECONDITION`, `STALE_DATA`, `LINK_LOSS`, `TIMEOUT`, `SEQUENCE`, `FORMAT`, `SECURITY`, `RESOURCE`, `PEER_REJECT`, `CANCELLED`, `INTERNAL`.

**States:** `CLEAR`, `ACTIVE_RECOVERABLE`, `ACTIVE_LATCHED`, `CLEAR_PENDING`.  
**Fault policy:** debounce chỉ áp dụng khi recovery; không trì hoãn immediate safety reaction.  
**Dependencies:** all diagnostic producers, Dem/Det, WdgM, NvM policy, RTE.

---

# Part B - ISO15118Chrg composition

## 8. Composition responsibility and AUTOSAR boundary

`ISO15118Chrg` sở hữu HLC lifecycle từ SDP tới protocol session termination. Standardized AUTOSAR boundary được giữ nguyên; project metadata được bổ sung tại `HlcSessionAdapter` thay vì sửa `ISO15118Chrg_*` data layout.

### 8.1 `ChargeControl` atomic operations

| ID | Port / interface | Atomic operation | Dir. from ISO15118Chrg | Semantic caller -> callee | Parameter | Contract |
|---|---|---|---|---|---|---|
| HLC-ASR-CC-001 | `Pp_ChargeControlPort / ChargeControl` | `SetChargeState` | P | `HlcSessionAdapter -> ISO15118Chrg` | `StateMachineControl` | Accept only supported lifecycle command in valid module state. |
| HLC-ASR-CC-002 | `Pp_ChargeControlPort / ChargeControl` | `SetCpLineStatus` | P | `HlcSessionAdapter -> ISO15118Chrg` | `CpLineStatus` | Qualified active/inactive only; raw CP voltage/PWM prohibited. |

`SetChargeState` shall support only values present in the frozen AUTOSAR/supplier interface, normally including the semantics `START`, `RENEGOTIATE`, `NORMAL_STOP` và `EMERGENCY_STOP`.

### 8.2 `ChargeParameters` atomic operations

> IN/OUT parameter direction must follow the selected generated RTE API. The table below identifies the **semantic producer** to prevent EV/EVSE parameters from being wired backwards.

| ID | Port / interface | Atomic operation | Dir. | Semantic producer | Primary information | Atomic contract |
|---|---|---|---|---|---|---|
| HLC-ASR-CP-001 | `Rp_ChargeParametersPort / ChargeParameters` | `SupportedAppProtocolParams` | R | `ISO15118Chrg <-> HlcSessionAdapter` | Enabled app protocols and selected schema | Bounded configured list; no runtime schema discovery. |
| HLC-ASR-CP-002 | Same | `SessionSetupParams` | R | `ISO15118Chrg <-> HlcSessionAdapter` | EVCC/SECC session identity | Bind to active generation; validate ID length/value. |
| HLC-ASR-CP-003 | Same | `SessionStopParams` | R | `ISO15118Chrg <-> HlcSessionAdapter` | Stop response/charging session intent | Idempotent; cannot reactivate stopped context. |
| HLC-ASR-CP-004 | Same | `ServiceDiscoveryParams` | R | `ISO15118Chrg <-> HlcSessionAdapter` | Offered energy/payment/VAS services | Validate bounded services and enum ranges. |
| HLC-ASR-CP-005 | Same | `ServiceDetailParams` | R | `ISO15118Chrg <-> HlcSessionAdapter` | Service parameter sets | Validate service ID, set ID, capacity and occurrence. |
| HLC-ASR-CP-006 | Same | `PaymentServiceSelectionParams` | R | `ISO15118Chrg <-> HlcSessionAdapter` | Selected payment option and services | Selected items must be subset of offered list. |
| HLC-ASR-CP-007 | Same | `PaymentDetailsParams` | R | `ISO15118Chrg <-> HlcSessionAdapter` | eMAID, certificate references, challenge | No private key material; length bounded. |
| HLC-ASR-CP-008 | Same | `AuthorizationParams` | R | `ISO15118Chrg <-> HlcSessionAdapter` | EIM/PnC authorization status | Authorization is protocol evidence, not energy permission. |
| HLC-ASR-CP-009 | Same | `ChargeParameterDiscoveryParams` | R | `ISO15118Chrg <-> HlcSessionAdapter` | EV/EVSE capabilities, limits, schedules | Read one atomic energy snapshot; explicit units/availability. |
| HLC-ASR-CP-010 | Same | `CableCheckParams` | R | `ISO15118Chrg <-> HlcSessionAdapter` | DC isolation/cable-check request/status | Peer request upward; readiness gated by permission/hardware evidence. |
| HLC-ASR-CP-011 | Same | `PreChargeParams` | R | `ISO15118Chrg <-> HlcSessionAdapter` | Target/present DC voltage and progress | Validate unit/range/age; no contactor command. |
| HLC-ASR-CP-012 | Same | `PowerDeliveryParams` | R | `ISO15118Chrg <-> HlcSessionAdapter` | START/STOP/RENEGOTIATE and profile | READY/START response requires fresh `ChargePermission`. |
| HLC-ASR-CP-013 | Same | `ChargingStatusParams` | R | `ISO15118Chrg <-> HlcSessionAdapter` | AC charging loop values/status | Reject stale snapshot; bounded loop response. |
| HLC-ASR-CP-014 | Same | `CurrentDemandParams` | R | `ISO15118Chrg <-> HlcSessionAdapter` | DC target/present voltage/current/power | Explicit sign/units; values limited by permission. |
| HLC-ASR-CP-015 | Same | `MeteringReceiptParams` | R | `ISO15118Chrg <-> HlcSessionAdapter` | Metering receipt and signature evidence | Bounded signed data; security adapter owns crypto. |
| HLC-ASR-CP-016 | Same | `WeldingDetectionParams` | R | `ISO15118Chrg <-> HlcSessionAdapter` | DC post-charge voltage/check evidence | Hardware decision remains outside HLC. |
| HLC-ASR-CP-017 | Same | `SeccConnectionParams` | R | `ISO15118Chrg -> HlcSessionAdapter` | Endpoint, IP assignment, TLS outcome | Diagnostic/session evidence only; not authorization. |
| HLC-ASR-CP-018 | Same | `SlacStateParams` | R | `PlcLinkManager via adapter -> ISO15118Chrg` | Qualified link-found/readiness evidence | Derived only from valid current `Plm_LinkStatusType`. |
| HLC-ASR-CP-019 | Same | `StateParams` | R | `ISO15118Chrg -> HlcSessionAdapter` | Module state/substate/protocol step | Map into authoritative `HlcSessionStatus`. |
| HLC-ASR-CP-020 | Same | `ErrorNotification` | R | `ISO15118Chrg -> HlcSessionAdapter` | Standard error + context | Map deterministically to stable project reason and Dem event. |

### 8.3 Lower BSW atomic callbacks and operations

| ID | Boundary | Atomic callback / operation | Dir. | Owner | Contract |
|---|---|---|---|---|---|
| HLC-BSW-001 | Lifecycle | `ISO15118Chrg_Init` | R | `EcuM/BswM -> ISO15118Chrg` | Initialize static pools/config before accepting RTE calls. |
| HLC-BSW-002 | Link | `ISO15118Chrg_DataLinkIndication` | CB | `EthIf/EthTrcv -> ISO15118Chrg` | Inactive immediately invalidates communication evidence. |
| HLC-BSW-003 | IP | `RequestIpAddrAssignment` | R | `ISO15118Chrg -> SoAd/TcpIp` | Explicit configured local-address/interface reference. |
| HLC-BSW-004 | IP | `LocalIpAddrAssignmentChg` | CB | `TcpIp -> ISO15118Chrg` | Current interface/generation only. |
| HLC-BSW-005 | Socket | `SoConModeChg` | CB | `SoAd -> ISO15118Chrg` | Correlate configured socket connection and active generation. |
| HLC-BSW-006 | UDP IF | `IfRxIndication` | CB | `PduR/SoAd -> SDP/V2GTP` | One bounded UDP PDU; validate before use. |
| HLC-BSW-007 | UDP IF | `IfTxConfirmation` | CB | `PduR/SoAd -> SDP` | Correlate current SDP request. |
| HLC-BSW-008 | TCP TP | `StartOfReception` | CB | `PduR/SoAd -> V2GTP path` | Validate available length/capacity; return BUSY/OVFL/NOT_OK as required. |
| HLC-BSW-009 | TCP TP | `CopyRxData` | CB | `PduR/SoAd -> V2GTP path` | Copy/lease bounded bytes; no caller-buffer retention. |
| HLC-BSW-010 | TCP TP | `TpRxIndication` | CB | `PduR/SoAd -> transport` | Complete/error indication for current connection handle. |
| HLC-BSW-011 | TCP TP | `CopyTxData` | CB | `PduR/SoAd -> transport` | Provide bounded current TX segment. |
| HLC-BSW-012 | TCP TP | `TpTxConfirmation` | CB | `PduR/SoAd -> transport` | Correlate TX job/session generation. |
| HLC-BSW-013 | Crypto | `CryptoJobFinished` | CB | `Csm/Crypto/HSM -> security owner` | Correlate job ID; no FSM transition inside callback. |

## 9. `HlcSessionAdapter` - project ASW façade

### 9.1 Responsibility

Façade duy nhất map coordinator lifecycle, energy snapshot, permission và PLC state vào standardized `ChargeControl`/`ChargeParameters` ABI.

### 9.2 Atomic ASW interfaces

| ID | Interface type | Port / interface | Atomic element / operation | Dir. | Producer -> consumer | Type / contract |
|---|---|---|---|---|---|---|
| HSA-IF-001 | C/S OP | `Pp_HlcLifecycle / If_HlcLifecycle` | `StartHlc` | P | `ChargingCoordinator -> HlcSessionAdapter` | `HlcStartRequestType`; validate CP/link/generation, then call `SetCpLineStatus` and `SetChargeState`. |
| HSA-IF-002 | C/S OP | Same | `StopHlc` | P | `ChargingCoordinator -> HlcSessionAdapter` | `HlcStopRequestType`; normal/emergency, idempotent, nonblocking. |
| HSA-IF-003 | C/S OP | Same | `RenegotiateHlc` | P | `ChargingCoordinator -> HlcSessionAdapter` | `HlcRenegotiateRequestType`; active protocol state only. |
| HSA-IF-004 | S/R DE | `Pp_HlcSessionStatus / If_HlcSessionStatus` | `HlcSessionStatus` | P | `HlcSessionAdapter -> ChargingCoordinator` | Authoritative nonqueued status; generation/state/protocol/security/quality/reason/time/counter. |
| HSA-IF-005 | S/R DE | `Pp_HlcEvent / If_HlcEvent` | `HlcEvent` | P | `HlcSessionAdapter -> ChargingCoordinator` | Queued event; category, step, evidence ID and reason. |
| HSA-IF-006 | S/R DE | `Pp_HlcDiagnostic / If_HlcDiagnostic` | `HlcDiagnostic` | P | `HlcSessionAdapter -> FaultCoordinator` | Stable reason + Dem reference; no raw secret/payload. |
| HSA-IF-007 | S/R DE | `Rp_EvChargeContext / If_EvChargeContext` | `EvChargeSnapshot` | R | `EvEnergyDataProvider -> HlcSessionAdapter` | Read once per parameter callback; reject stale/generation mismatch. |
| HSA-IF-008 | S/R DE | `Rp_ChargePermission / If_ChargePermission` | `ChargePermission` | R | `ChargingCoordinator -> HlcSessionAdapter` | Required before returning READY/START/progress. |
| HSA-IF-009 | S/R DE | `Rp_PlcLinkStatus / If_PlcLinkStatus` | `LinkStatus` | R | `PlcLinkManager -> HlcSessionAdapter` | Map valid link to `SlacStateParams`; loss triggers emergency close. |
| HSA-IF-010 | S/R DE | `Pp_NegotiatedService / If_NegotiatedService` | `NegotiatedService` | P | `HlcSessionAdapter -> coordinator/BPT` | Protocol-neutral service and mode evidence. |
| HSA-IF-011 | S/R DE | `Pp_HlcPowerRequest / If_HlcPowerRequest` | `HlcPowerRequest` | P | `HlcSessionAdapter -> coordinator/BPT` | Peer power request/evidence; never permission. |

**State model:** `IDLE -> BOUND -> ACTIVE -> STOPPING -> IDLE`; failures pass through `ERROR`.  
**Rule:** standard data types are not locally extended; project metadata remains in project interfaces above.

## 10. `HlcSessionSupervisor`

### 10.1 Atomic internal interfaces

| ID | Interface | Atomic operation / element | Dir. | Producer -> consumer | Contract |
|---|---|---|---|---|---|
| HSS-IF-001 | `IHlcSessionControl` | `StartSession` | R | `HlcSessionAdapter -> Supervisor` | Create one context keyed by generation. |
| HSS-IF-002 | Same | `StopSession` | R | `HlcSessionAdapter -> Supervisor` | Idempotent close with terminal reason. |
| HSS-IF-003 | Same | `RenegotiateSession` | R | `HlcSessionAdapter -> Supervisor` | Allowed protocol state only. |
| HSS-IF-004 | `ISdpClient` | `DiscoverEndpoint` | R | `Supervisor -> SdpClient` | Start after current LinkReady + IPv6 readiness. |
| HSS-IF-005 | `ISdpClient` | `CancelDiscovery` | R | `Supervisor -> SdpClient` | Cancel current generation only. |
| HSS-IF-006 | `ITransportSession` | `Connect` | R | `Supervisor -> TcpTlsSession` | Use immutable current SDP result. |
| HSS-IF-007 | `ITransportSession` | `Close` | R | `Supervisor -> TcpTlsSession` | Idempotent; erase transient state. |
| HSS-IF-008 | `ISapNegotiator` | `Negotiate` | R | `Supervisor -> SapNegotiator` | Active transport required. |
| HSS-IF-009 | `IProtocolAdapter` | `StartProtocol` | R | `Supervisor -> selected adapter` | Exactly one adapter active. |
| HSS-IF-010 | `IProtocolAdapter` | `StopProtocol` | R | `Supervisor -> selected adapter` | Cancels message timers and context. |
| HSS-IF-011 | `IHlcStatusSink` | `SessionStatus` | P | `Supervisor -> HlcSessionAdapter` | Authoritative common lifecycle status. |
| HSS-IF-012 | `IHlcEventSink` | `SessionEvent` | P | `Supervisor -> HlcSessionAdapter` | Normalized transition/completion/failure event. |

**State model:** `IDLE -> WAIT_LINK -> SDP -> TCP_TLS -> SAP -> PROTOCOL_SESSION -> CLOSING -> IDLE`; error enters `SESSION_ERROR` then `CLOSING`.  
**Timer ownership:** overall setup/session timer only; lower-layer timers stay with lower owner.

## 11. `SdpClient`

### 11.1 Atomic interfaces

| ID | Interface | Atomic operation / element | Dir. | Type | Contract |
|---|---|---|---|---|---|
| SDP-IF-001 | `ISdpClient` | `DiscoverEndpoint` | P | `SdpRequestType` | Async current-generation request. |
| SDP-IF-002 | `ISdpClient` | `CancelDiscovery` | P | `SdpCancelType` | Idempotent cancellation. |
| SDP-IF-003 | `ISdpResultSink` | `EndpointResult` | P | `SdpResultType` | Immutable address/port/transport/security result. |
| SDP-IF-004 | `ISdpStatus` | `SdpStatus` | P | `SdpStatusType` | Authoritative discovery state. |
| SDP-IF-005 | `ISdpDiagnostic` | `SdpDiagnostic` | P | `SdpDiagnosticType` | Timeout/version/type/endpoint/security reason. |
| SDP-IF-006 | `ILinkContext` | `LinkContext` | R | `LinkContextType` | Interface index, IPv6 scope/readiness and generation. |
| SDP-IF-007 | `IUdpSocket` | `OpenUdp` | R | `UdpOpenRequestType` | Configured PLC interface only. |
| SDP-IF-008 | `IUdpSocket` | `SendDatagram` | R | `UdpTxRequestType` | Exactly one SDP request per UDP packet. |
| SDP-IF-009 | `IUdpSocket` | `CloseUdp` | R | `UdpCloseRequestType` | Idempotent cleanup. |
| SDP-IF-010 | `IUdpSocketSink` | `DatagramReceived` | R | `UdpRxIndicationType` | Validate generation/interface/payload before accept. |

**State model:** `IDLE -> BUILD_REQUEST -> SEND -> WAIT_RESPONSE -> VALIDATE_ENDPOINT -> COMPLETE`; timeout/cancel enter `FAILED`/`CANCELLED`.  
**Timing:** retry interval minimum 250 ms in current baseline; retry count and overall deadline are ECUC-controlled.

## 12. `TcpTlsSession`

### 12.1 Atomic interfaces

| ID | Interface | Atomic operation / element | Dir. | Type | Contract |
|---|---|---|---|---|---|
| TTS-IF-001 | `ITransportSession` | `Connect` | P | `TransportConnectRequestType` | Endpoint from current SDP result only. |
| TTS-IF-002 | Same | `Send` | P | `TransportTxRequestType` | Bounded buffer, handle, generation, deadline. |
| TTS-IF-003 | Same | `Close` | P | `TransportCloseRequestType` | Idempotent; cancels connect/handshake/I/O. |
| TTS-IF-004 | `ITransportStatus` | `TransportStatus` | P | `TransportStatusType` | Authoritative connection/security state. |
| TTS-IF-005 | `ITransportRxSink` | `BytesReceived` | P | `TransportRxIndicationType` | Ordered stream bytes; no V2GTP boundary assumption. |
| TTS-IF-006 | `ITransportTxSink` | `SendResult` | P | `TransportTxResultType` | Correlate TX request ID. |
| TTS-IF-007 | `ISocketAdapter` | `OpenTcp` | R | `TcpOpenRequestType` | Nonblocking SoAd/TcpIp open. |
| TTS-IF-008 | Same | `SendTcp` | R | `TcpTxRequestType` | Bounded TX/backpressure contract. |
| TTS-IF-009 | Same | `CloseTcp` | R | `TcpCloseRequestType` | Release current socket context. |
| TTS-IF-010 | `ITlsAdapter` | `StartHandshake` | R | `TlsHandshakeRequestType` | Apply selected security policy; DIN forbidden. |
| TTS-IF-011 | Same | `CloseTls` | R | `TlsCloseRequestType` | Erase transient TLS session state. |
| TTS-IF-012 | `ITlsStatusSink` | `TlsResult` | R | `TlsResultType` | Peer/cipher/certificate evidence; no private key. |

**State model:** `CLOSED -> TCP_CONNECTING -> TCP_ESTABLISHED -> TLS_HANDSHAKE/ACTIVE_PLAIN -> ACTIVE_SECURE -> CLOSING`.  
**Fault rule:** TLS/security failure does not retry plaintext unless an independently authorized policy explicitly allows a different session path.

## 13. `V2gtpCodec`

### 13.1 Atomic interfaces

| ID | Interface | Atomic operation / element | Dir. | Type | Contract |
|---|---|---|---|---|---|
| V2GTP-IF-001 | `IV2gtpCodec` | `EncodeFrame` | P | `V2gtpEncodeRequestType` | One payload type + one bounded payload -> one frame. |
| V2GTP-IF-002 | Same | `ConsumeBytes` | P | `V2gtpByteSpanType` | Incremental stream parsing. |
| V2GTP-IF-003 | Same | `ResetDecoder` | P | `V2gtpResetType` | Clear partial frame for current connection. |
| V2GTP-IF-004 | `IV2gtpFrameSink` | `ValidatedFrame` | P | `V2gtpFrameViewType` | Immutable bounded-lifetime view. |
| V2GTP-IF-005 | `IV2gtpDiagnostic` | `V2gtpDiagnostic` | P | `V2gtpDiagnosticType` | Version/inverse/type/length/overflow reason. |

**State model:** `AWAIT_HEADER -> VALIDATE_HEADER -> AWAIT_PAYLOAD -> FRAME_READY`; invalid data enters `REJECT`.  
**Resource rule:** payload length is validated before buffer reservation or EXI decode.

## 14. `ExiCodec`

### 14.1 Atomic interfaces

| ID | Interface | Atomic operation / element | Dir. | Type | Contract |
|---|---|---|---|---|---|
| EXI-IF-001 | `IExiCodec` | `Encode` | P | `ExiEncodeRequestType` | Explicit `schemaId`, typed input, caller-provided output buffer. |
| EXI-IF-002 | Same | `Decode` | P | `ExiDecodeRequestType` | Explicit `schemaId`, bounded input and workspace. |
| EXI-IF-003 | Same | `ResetWorkspace` | P | `ExiResetRequestType` | Zero transient state after failure/cancel. |
| EXI-IF-004 | `IExiResultSink` | `EncodeResult` | P | `ExiEncodeResultType` | Bytes produced or structured reason. |
| EXI-IF-005 | Same | `DecodeResult` | P | `ExiDecodeResultType` | Fully validated typed object or error; never partial delivery. |
| EXI-IF-006 | `IExiDiagnostic` | `ExiDiagnostic` | P | `ExiDiagnosticType` | Schema/range/grammar/size/signature reason. |
| EXI-IF-007 | `ISchemaRegistry` | `GetSchemaDescriptor` | R | `SchemaLookupRequestType` | Exact selected schema only. |

**State model:** `IDLE -> VALIDATE_REQUEST -> SELECT_SCHEMA -> ENCODE/DECODE -> VALIDATE_RESULT -> COMPLETE/REJECT`.  
**Rule:** decoder never infers protocol from untrusted content.

## 15. `SchemaRegistry`

### 15.1 Atomic interfaces

| ID | Interface | Atomic operation / element | Dir. | Type | Contract |
|---|---|---|---|---|---|
| SCH-IF-001 | `ISchemaRegistry` | `GetSchemaDescriptor` | P | `SchemaLookupRequestType` | Return immutable descriptor or `UNKNOWN_SCHEMA`. |
| SCH-IF-002 | Same | `GetSchemaLimits` | P | `SchemaLimitsRequestType` | Maximum payload, collection and workspace sizes. |
| SCH-IF-003 | Same | `CheckProtocolCompatibility` | P | `SchemaCompatibilityRequestType` | Validate protocol/version/service vs schema. |
| SCH-IF-004 | `ISchemaStatus` | `SchemaRegistryStatus` | P | `SchemaRegistryStatusType` | Init/integrity/compatibility status. |

Required schema IDs:

| Schema ID | Owner/consumer | Scope |
|---|---|---|
| `EXI_SCHEMA_SAP` | `SapNegotiator` | `SupportedAppProtocolReq/Res` |
| `EXI_SCHEMA_DIN_70121` | `Din70121Adapter` | DIN DC messages |
| `EXI_SCHEMA_ISO_15118_2` | `Iso15118_2Adapter` | ISO-2 AC/DC, EIM/PnC |
| `EXI_SCHEMA_ISO_20_COMMON` | ISO-20 common FSM | Common session/auth/service/schedule |
| `EXI_SCHEMA_ISO_20_AC` | ISO-20 AC/AC-BPT FSM | AC parameters and charge loop |
| `EXI_SCHEMA_ISO_20_DC` | ISO-20 DC/DC-BPT FSM | Cable check, precharge, DC loop, welding |

**State model:** `UNINITIALIZED -> VALIDATING -> READY`; integrity/configuration error enters `FAILED`.  
**Rule:** BPT uses AC or DC schema corresponding to selected service; it does not use a separate EXI engine.

## 16. `SapNegotiator`

### 16.1 Atomic interfaces

| ID | Interface | Atomic operation / element | Dir. | Type | Contract |
|---|---|---|---|---|---|
| SAP-IF-001 | `ISapNegotiator` | `Negotiate` | P | `SapNegotiateRequestType` | Current transport/security/energy context. |
| SAP-IF-002 | Same | `CancelNegotiation` | P | `SapCancelType` | Idempotent current generation. |
| SAP-IF-003 | `IProtocolConfig` | `EnabledProtocolList` | R | `EnabledProtocolListType` | Versioned, priority ordered, bounded. |
| SAP-IF-004 | `ISapMessageChannel` | `SendSapRequest` | R | `SapTxRequestType` | Uses `EXI_SCHEMA_SAP`. |
| SAP-IF-005 | `ISapMessageSink` | `SapResponse` | R | `SapRxIndicationType` | Validate selected schema against exact offered list. |
| SAP-IF-006 | `IProtocolSelectionSink` | `ProtocolSelection` | P | `ProtocolSelectionType` | Immutable protocol ID + schema ID. |
| SAP-IF-007 | `ISapDiagnostic` | `SapDiagnostic` | P | `SapDiagnosticType` | No-match/malformed/timeout/policy reason. |

**State model:** `IDLE -> BUILD_OFFERS -> ENCODE_SEND -> WAIT_RESPONSE -> VALIDATE_SELECTION -> ADAPTER_READY`; error enters `FAILED`.  
**Rule:** DIN over TLS và unoffered schema are rejected; fallback never leaves the configured offer list.

## 17. `ProtocolAdapterFactory`

### 17.1 Atomic interfaces

| ID | Interface | Atomic operation / element | Dir. | Type | Contract |
|---|---|---|---|---|---|
| PAF-IF-001 | `IProtocolAdapterFactory` | `CreateAdapter` | P | `AdapterCreateRequestType` | Validated protocol/schema/context; one active adapter. |
| PAF-IF-002 | Same | `DestroyAdapter` | P | `AdapterDestroyRequestType` | Idempotent zero/release. |
| PAF-IF-003 | Same | `ResetAdapter` | P | `AdapterResetRequestType` | Clear session-specific state before reuse. |
| PAF-IF-004 | `IAdapterFactoryStatus` | `FactoryStatus` | P | `AdapterFactoryStatusType` | Resource/configuration/integrity status. |

**Resource rule:** use static pool or compile-time variants; no reflection, dynamic module loading or unbounded allocation.

## 18. Common protocol-adapter contract

Mỗi DIN/ISO adapter triển khai riêng cùng atomic contract; adapter không chia sẻ protocol state ownership.

| ID | Interface | Atomic operation / element | Dir. | Contract |
|---|---|---|---|---|
| PAD-IF-001 | `IProtocolAdapter` | `StartProtocol` | P | Initialize selected protocol/session context. |
| PAD-IF-002 | Same | `StopProtocol` | P | Cancel timers and enter controlled terminal path. |
| PAD-IF-003 | Same | `RenegotiateProtocol` | P | Only if standard/state/policy permits. |
| PAD-IF-004 | Same | `OnMessage` | P | One fully decoded typed message; sequence checked before advance. |
| PAD-IF-005 | Same | `OnTimerExpired` | P | One timer ID with one owner. |
| PAD-IF-006 | `IEvChargeContext` | `GetCapabilities` | R | Immutable static/session capabilities. |
| PAD-IF-007 | Same | `GetEnergySnapshot` | R | Atomic current-generation snapshot. |
| PAD-IF-008 | Same | `GetChargePermission` | R | Fresh permission before energy-ready response. |
| PAD-IF-009 | `IProtocolMessageChannel` | `SendProtocolMessage` | R | Typed message -> explicit schema -> EXI/V2GTP. |
| PAD-IF-010 | `IHlcEventSink` | `ProtocolEvent` | P | Normalized request/state/completion event. |
| PAD-IF-011 | `IProtocolStatusSink` | `ProtocolStatus` | P | Authoritative protocol state. |
| PAD-IF-012 | `IProtocolDiagnostic` | `ProtocolDiagnostic` | P | Negative response/sequence/timer/security reason. |

## 19. `Din70121Adapter`

### 19.1 State model

`SESSION_SETUP -> SERVICE_DISCOVERY -> SERVICE_PAYMENT_SELECTION -> CONTRACT_AUTHENTICATION -> CHARGE_PARAMETER_DISCOVERY -> CABLE_CHECK -> PRECHARGE -> POWER_DELIVERY_START -> CURRENT_DEMAND -> POWER_DELIVERY_STOP -> WELDING_DETECTION -> SESSION_STOP -> TERMINATE`.

### 19.2 Adapter-specific atomic items

| ID | Interface | Atomic element | Dir. | Contract |
|---|---|---|---|---|
| DIN-IF-001 | `IProtocolConfig` | `Din70121Config` | R | DC-only, non-TLS, schema/timer/limit profile. |
| DIN-IF-002 | `IProtocolStatusSink` | `DinProtocolStatus` | P | Current DIN state and response processing status. |
| DIN-IF-003 | `IHlcEventSink` | `DinCableCheckRequest` | P | Semantic request; no hardware action. |
| DIN-IF-004 | Same | `DinPreChargeRequest` | P | Target/evidence upward; permission-gated response. |
| DIN-IF-005 | Same | `DinPowerDeliveryRequest` | P | START/STOP request evidence. |
| DIN-IF-006 | Same | `DinCurrentDemandRequest` | P | Target current/voltage request evidence. |
| DIN-IF-007 | Same | `DinWeldingDetectionRequest` | P | Post-charge check request evidence. |

**Fault handling:** unexpected/negative response, sequence error, permission loss hoặc timeout enters common controlled close path.

## 20. `Iso15118_2Adapter`

### 20.1 State model

Common: `SESSION_SETUP -> SERVICE_DISCOVERY/DETAIL -> PAYMENT_SERVICE_SELECTION -> EIM AUTHORIZATION` hoặc `PNC CERTIFICATE/PAYMENT_DETAILS/AUTHORIZATION -> CHARGE_PARAMETER_DISCOVERY`.

- AC: `POWER_DELIVERY -> CHARGING_STATUS <-> METERING_RECEIPT -> SESSION_STOP`.
- DC: `CABLE_CHECK -> PRECHARGE -> POWER_DELIVERY -> CURRENT_DEMAND <-> METERING_RECEIPT -> WELDING_DETECTION -> SESSION_STOP`.

### 20.2 Adapter-specific atomic items

| ID | Interface | Atomic element | Dir. | Contract |
|---|---|---|---|---|
| ISO2-IF-001 | `IProtocolConfig` | `Iso15118_2Config` | R | AC/DC mode, EIM/PnC, timer and collection limits. |
| ISO2-IF-002 | `IHlcEventSink` | `Iso2ServiceSelection` | P | Selected service/payment evidence. |
| ISO2-IF-003 | Same | `Iso2AuthorizationResult` | P | EIM/PnC protocol result; not energy permission. |
| ISO2-IF-004 | Same | `Iso2ChargeParameterResult` | P | Negotiated mode/limits/schedule evidence. |
| ISO2-IF-005 | Same | `Iso2CableCheckRequest` | P | DC-only semantic request. |
| ISO2-IF-006 | Same | `Iso2PreChargeRequest` | P | DC-only semantic request. |
| ISO2-IF-007 | Same | `Iso2PowerDeliveryRequest` | P | START/STOP/RENEGOTIATE evidence. |
| ISO2-IF-008 | Same | `Iso2ChargingStatusRequest` | P | AC loop request/evidence. |
| ISO2-IF-009 | Same | `Iso2CurrentDemandRequest` | P | DC loop request/evidence. |
| ISO2-IF-010 | Same | `Iso2MeteringReceiptRequest` | P | Signed receipt request/evidence. |
| ISO2-IF-011 | Same | `Iso2WeldingDetectionRequest` | P | DC post-charge check evidence. |

## 21. `Iso15118_20Adapter`

### 21.1 Internal sub-FSM ownership

| Sub-FSM | Owned states/functions | Schema |
|---|---|---|
| `CommonSessionFsm` | Session setup, authorization setup, authorization, service discovery/detail/selection, schedule exchange, session stop | `ISO_20_COMMON` |
| `AcChargeFsm` | AC parameter discovery, power delivery, scheduled/dynamic AC loop | `ISO_20_AC` |
| `DcChargeFsm` | DC parameter discovery, cable check, precharge, power delivery, DC loop, welding detection | `ISO_20_DC` |
| `AcBptServiceHandler` | AC-BPT capability/control mapping and loop semantics | `ISO_20_AC` |
| `DcBptServiceHandler` | DC-BPT capability/control mapping and loop semantics | `ISO_20_DC` |
| `PncHandler` | Authorization/certificate/signature orchestration | Common + security services |

### 21.2 Adapter-specific atomic items

| ID | Interface | Atomic element | Dir. | Contract |
|---|---|---|---|---|
| ISO20-IF-001 | `IProtocolConfig` | `Iso15118_20Config` | R | Enabled services, control modes, TLS/PKI, timer/resource limits. |
| ISO20-IF-002 | `IHlcEventSink` | `Iso20AuthorizationMethod` | P | Selected EIM/PnC method evidence. |
| ISO20-IF-003 | Same | `Iso20SelectedService` | P | AC/DC/AC-BPT/DC-BPT and control mode. |
| ISO20-IF-004 | Same | `Iso20ScheduleResult` | P | Bounded schedule/control constraints. |
| ISO20-IF-005 | Same | `Iso20CableCheckRequest` | P | DC/DC-BPT only. |
| ISO20-IF-006 | Same | `Iso20PreChargeRequest` | P | DC/DC-BPT only. |
| ISO20-IF-007 | Same | `Iso20PowerDeliveryRequest` | P | START/STOP/RENEGOTIATE semantic evidence. |
| ISO20-IF-008 | Same | `Iso20AcPowerRequest` | P | AC/AC-BPT loop request; explicit sign and limits. |
| ISO20-IF-009 | Same | `Iso20DcPowerRequest` | P | DC/DC-BPT loop request; explicit sign and limits. |
| ISO20-IF-010 | Same | `Iso20WeldingDetectionRequest` | P | DC/DC-BPT post-charge evidence. |
| ISO20-IF-011 | `IPkiSecurity` | `RequestCertificateInstallation` | R | Production implementation required before enabling path. |

**BPT rule:** zero-power crossing and direction reversal are vehicle-control decisions mediated by `BptCoordinator` and a new `ChargePermission`; protocol support alone is insufficient.

## 22. `PkiSecurityAdapter`

### 22.1 Atomic interfaces

| ID | Interface | Atomic operation / element | Dir. | Type | Contract |
|---|---|---|---|---|---|
| PKI-IF-001 | `IPkiSecurity` | `ValidatePeerCertificateChain` | P | `CertChainValidationRequestType` | Length/policy/time bounded; returns evidence/reason. |
| PKI-IF-002 | Same | `VerifySignature` | P | `SignatureVerifyRequestType` | Validate profile/algorithm/input bounds first. |
| PKI-IF-003 | Same | `CreateSignature` | P | `SignatureCreateRequestType` | Private key handle only. |
| PKI-IF-004 | Same | `ValidateChallenge` | P | `ChallengeValidationRequestType` | Bind challenge to active session/generation. |
| PKI-IF-005 | Same | `CancelSecurityJob` | P | `SecurityCancelRequestType` | Idempotent by job ID. |
| PKI-IF-006 | `ISecurityResultSink` | `CertificateValidationResult` | P | `CertValidationResultType` | Stable status/reason; no raw private data. |
| PKI-IF-007 | Same | `SignatureResult` | P | `SignatureResultType` | Correlated async result. |
| PKI-IF-008 | `ICertificateManager` | `GetCertificateReference` | R | `CertificateRefRequestType` | Returns versioned handle/public chain reference. |
| PKI-IF-009 | `ICryptoAdapter` | `SubmitCryptoJob` | R | `CryptoJobRequestType` | Maps to Csm/Crypto/HSM. |
| PKI-IF-010 | `ICryptoResultSink` | `CryptoJobResult` | R | `CryptoJobResultType` | Job ID + generation validation. |

**State model:** `IDLE -> VALIDATE_REQUEST -> WAIT_CRYPTO -> VALIDATE_RESULT -> COMPLETE/FAILED/CANCELLED`.  
**Security rule:** no implicit downgrade; secrets/raw key material are excluded from logs, events and freeze frames.

## 23. `CertificateManager`

### 23.1 Atomic interfaces

| ID | Interface | Atomic operation / element | Dir. | Type | Contract |
|---|---|---|---|---|---|
| CERT-IF-001 | `ICertificateManager` | `GetCertificateReference` | P | `CertificateRefRequestType` | Versioned handle, usage and validity metadata. |
| CERT-IF-002 | Same | `GetIdentityReference` | P | `IdentityRefRequestType` | eMAID/public identity reference; bounded. |
| CERT-IF-003 | Same | `InstallCertificate` | P | `CertificateInstallRequestType` | Stage, validate and atomically commit. |
| CERT-IF-004 | Same | `UpdateCertificate` | P | `CertificateUpdateRequestType` | Preserve previous consistent version until commit. |
| CERT-IF-005 | Same | `DeleteCertificate` | P | `CertificateDeleteRequestType` | Policy-controlled; key/reference consistency required. |
| CERT-IF-006 | Same | `RollbackCertificateUpdate` | P | `CertificateRollbackRequestType` | Restore last consistent version. |
| CERT-IF-007 | `ICertificateStatus` | `CertificateStatus` | P | `CertificateStatusType` | Available/valid/expired/update-pending/failed. |
| CERT-IF-008 | `ICertificateEvent` | `CertificateEvent` | P | `CertificateEventType` | Update/commit/rollback/failure event. |
| CERT-IF-009 | `IKeyMAdapter` | `StoreCertificateReference` | R | `KeyMStoreRequestType` | No private-key byte transfer. |
| CERT-IF-010 | `INvMAdapter` | `WriteCertificateRecord` | R | `NvMWriteRequestType` | Transactional, CRC/integrity protected. |

**State model:** `INIT -> READY -> UPDATE_PENDING -> COMMITTING -> READY`; failure uses `ROLLBACK`, then `DEGRADED/FAILED` if recovery is unsuccessful.  
**Policy:** PnC-to-EIM fallback is explicit vehicle policy, not automatic security recovery.

---

# Part C - Cross-composition contracts

## 24. Nominal interaction sequence

1. `CpPpManager::QualifiedCpPp` indicates qualified connect/HLC trigger.
2. `ChargeStateMachine` creates a new `sessionGeneration`.
3. `ChargingCoordinator::StartLink` requests `PlcLinkManager` to establish PEV-SLAC/link.
4. `PlcLinkManager::LinkStatus` reaches valid `LINK_READY` with `linkGeneration` and IPv6 readiness.
5. `ChargingCoordinator::StartHlc` calls `HlcSessionAdapter`.
6. Adapter maps qualified CP and lifecycle control into `SetCpLineStatus` and `SetChargeState`.
7. Supervisor runs SDP, TCP/TLS, SAP and exactly one protocol adapter.
8. Adapter reports peer request using an atomic `HlcEvent`/specific semantic data element.
9. `PermissionManager` evaluates all current evidence and publishes bounded `ChargePermission`.
10. Only then may HLC return READY/START semantics and coordinator command hardware execution.

## 25. Stop and fault sequence

1. Detect CP invalid, stale energy/safety data, link loss, permission expiry, protocol/security failure or hardware mismatch.
2. `PermissionManager` publishes `DENY`/expired permission.
3. `ChargeStateMachine` invokes `RequestSafeState` or `StopEnergyPath` as required.
4. `HlcSessionAdapter::StopHlc` maps normal/emergency stop into `ChargeControl::SetChargeState`.
5. `HlcSessionSupervisor` cancels active protocol, SAP/SDP timers and transport.
6. `ChargingCoordinator::StopLink` invalidates PLC link context.
7. Late callbacks/events are rejected by generation and cannot reopen the session.

## 26. Data ownership matrix

| State/data | Owner | Consumer | Authority rule |
|---|---|---|---|
| `CoordinatorState` | `ChargeStateMachine` | Composition/application | Authoritative nonqueued state |
| `ChargePermission` | `PermissionManager` within `ChargingCoordinator` | HLC/HW boundary | Bounded validity; expiry = deny |
| `BptStatus` | `BptCoordinator` | CSM/permission/hardware integration | Direction and zero-crossing authority |
| `BlockingFaultStatus` | `FaultCoordinator` | CSM/PermissionManager | Severity/latch/recovery authority |
| `HlcSessionStatus` | `HlcSessionAdapter` | `ChargingCoordinator` | Authoritative normalized HLC state |
| `TransportStatus` | `TcpTlsSession` | `HlcSessionSupervisor` | Connection/security state |
| Protocol state | Selected protocol adapter | Supervisor/adapter façade | Exactly one active adapter |
| `SchemaDescriptor` | `SchemaRegistry` | EXI/SAP/adapters | Explicit immutable schema selection |
| Certificate/key reference | `CertificateManager`/KeyM/HSM | Security adapter | Private key never leaves HSM |

## 27. Acceptance criteria

- [ ] Mỗi interface row tương ứng đúng một S/R data element, C/S operation, callback hoặc local operation.
- [ ] Không còn interface row dạng `Prepare/Enable/Stop` hoặc `Status/Event` bị gộp.
- [ ] Mỗi atomic item xác định direction, semantic producer/consumer, type và validation contract.
- [ ] Exact standardized AUTOSAR signatures được trace tới frozen ARXML/generated headers.
- [ ] Không có direct HLC-to-actuator path trong code, RTE wiring hoặc configuration.
- [ ] Mỗi state domain và timer có đúng một owner.
- [ ] Mọi async path vượt qua generation mismatch, late/reordered event, cancel và queue-overflow tests.
- [ ] `ChargePermission` được kiểm tra trước mọi HLC response biểu diễn readiness/START energy progress.
- [ ] DIN, ISO-2 và ISO-20 dùng adapter/schema riêng; không overload standardized enumeration hoặc ABI.
- [ ] EXI/V2GTP/SAP/certificate/MME inputs có bounds, negative tests và fuzz corpus.
- [ ] BPT direction reversal phải có measured zero confirmation và permission mới.
- [ ] Stop path thu hồi permission trước, đưa hardware về safe state và teardown HLC/PLC idempotently.
- [x] Mọi project-owned datatype được khai báo trong Datatype Catalogue với category, fields/base type, constraint và invalid/init rule.
- [x] Mọi datatype chuẩn/supplier-owned được đánh dấu `IMPORTED`; layout không bị định nghĩa lại trong project namespace.

## 28. Open items cần freeze

1. AUTOSAR Classic release và supplier `ISO15118Chrg` ARXML/generated header.
2. Exact parameter direction/signature của từng `ChargeParameters` operation.
3. Project ARXML naming convention cho `Pp_`, `Rp_`, interface và data type.
4. Runnable mapping, event source, queue depth, exclusive area và task priority.
5. HARA-derived safe-state, ASIL allocation và maximum controlled-stop latency.
6. Protocol edition/corrigenda, timers, retry budgets và response-code mapping.
7. EXI product, code-generation flow, maximum workspace/payload/collection và WCET.
8. PLC modem/TcpIp/TLS capability, PKI provisioning và BPT vehicle use cases.

## 29. Datatype design rules

### 29.1 Ownership classes

| Class | Definition owner | Project action |
|---|---|---|
| `PROJECT` | EVCC project | Define complete `ApplicationDataType`, `ImplementationDataType`, `DataConstr`, `CompuMethod`, init value and ARXML mapping. |
| `AUTOSAR_IMPORTED` | Selected AUTOSAR release/supplier | Import exact generated type; do not copy, extend or renumber fields/enums. |
| `BSW_IMPORTED` | AUTOSAR BSW/vendor stack | Reference generated BSW type/API and isolate it behind an adapter. |
| `PROTOCOL_GENERATED` | Licensed DIN/ISO schema generation | Keep generated type in protocol adapter/EXI boundary; do not expose across project RTE interfaces. |

### 29.2 Base implementation types

| Type | Category | Implementation | Range / invalid |
|---|---|---|---|
| `ApplicationDataType` | AUTOSAR METAMODEL ELEMENT | Defines physical/semantic view, unit, CompuMethod and DataConstr | Not instantiated in C; mapped to an ImplementationDataType in data mapping. |
| `ImplementationDataType` | AUTOSAR METAMODEL ELEMENT | Defines generated implementation representation and sub-elements | Exact generated layout is frozen by ARXML/header baseline. |
| `BooleanType` | VALUE | `boolean`/`uint8` | `FALSE=0`, `TRUE=1`; other values invalid on receive. |
| `UInt8Type` | VALUE | `uint8` | `0..255`. |
| `UInt16Type` | VALUE | `uint16` | `0..65535`. |
| `UInt32Type` | VALUE | `uint32` | `0..4294967295`. |
| `UInt64Type` | VALUE | `uint64` | `0..18446744073709551615`. |
| `SInt16Type` | VALUE | `sint16` | `-32768..32767`. |
| `SInt32Type` | VALUE | `sint32` | `-2147483648..2147483647`. |
| `SessionGenerationType` | TYPE_REFERENCE | `UInt32Type` | `1..UINT32_MAX`; `0=INVALID`. |
| `SequenceCounterType` | TYPE_REFERENCE | `UInt32Type` | Wrap-safe modulo counter; no invalid representation. |
| `MonotonicTimeUsType` | TYPE_REFERENCE | `UInt64Type` | Microseconds from monotonic ECU timebase; `0=NOT_AVAILABLE` before timebase start. |
| `Identifier32Type` | TYPE_REFERENCE | `UInt32Type` | `1..UINT32_MAX`; `0=INVALID`. |
| `DataPtrType` | TYPE_REFERENCE | Toolchain `uintptr` or typed local pointer | `0/NULL=INVALID`; local interface only, never serialized or exposed over RTE/network. |
| `Length16Type` | TYPE_REFERENCE | `UInt16Type` | `0..capacity`. |
| `Length32Type` | TYPE_REFERENCE | `UInt32Type` | `0..capacity`. |

### 29.3 Physical-value types and units

Physical quantities use integer SI-derived units at the project boundary. Protocol exponent/multiplier conversion occurs only inside the selected protocol adapter.

| Type | Unit / implementation | DataConstr | Invalid representation |
|---|---|---|---|
| `SocPermilleType` | 0.1 %, `uint16` | `0..1000` | `65535` |
| `VoltageMilliVoltType` | mV, `uint32` | `0..1500000` | `UINT32_MAX` |
| `SignedVoltageMilliVoltType` | mV, `sint32` | `-1500000..1500000` | `INT32_MIN` |
| `CurrentMilliAmpereType` | mA, `sint32` | `-3000000..3000000` | `INT32_MIN` |
| `PowerWattType` | W, `sint32` | `-1500000..1500000` | `INT32_MIN` |
| `EnergyWattHourType` | Wh, `uint32` | `0..10000000` | `UINT32_MAX` |
| `TemperatureDeciCType` | 0.1 °C, `sint16` | `-500..2000` | `INT16_MIN` |
| `ResistanceKiloOhmType` | kΩ, `uint32` | `0..10000000` | `UINT32_MAX` |
| `CurrentAmpereType` | A, `uint16` | `0..1000` | `65535` |
| `DutyPermilleType` | 0.1 %, `uint16` | `0..1000` | `65535` |
| `FrequencyHertzType` | Hz, `uint16` | `0..10000` | `65535` |
| `PowerRampWattPerSecondType` | W/s, `uint32` | `0..3000000` | `UINT32_MAX` |

The physical maxima above are project interface safety bounds, not guaranteed vehicle capability. Runtime limits shall be smaller than or equal to both the datatype bound and current vehicle capability.

### 29.4 Bounded collection types

| Type | Definition | Constraint |
|---|---|---|
| `ByteBufferRefType` | `{dataRef:DataPtrType, length:Length32Type, capacity:Length32Type}` | `length <= capacity`; `dataRef != NULL` when `length > 0`; lifetime explicitly stated by API. |
| `ByteArray64Type` | `{length:Length16Type, data:uint8[64]}` | `length <= 64`. |
| `ByteArray512Type` | `{length:Length16Type, data:uint8[512]}` | `length <= 512`. |
| `ByteArray4096Type` | `{length:Length16Type, data:uint8[4096]}` | `length <= 4096`. |
| `Ipv6AddressType` | `uint8[16]` | Network byte order; all-zero address invalid for remote endpoint. |
| `String16Type` | `{length:uint8, data:uint8[16]}` | UTF-8/ASCII profile selected by owning interface; no implicit NUL requirement. |
| `String32Type` | `{length:uint8, data:uint8[32]}` | `length <= 32`. |
| `String64Type` | `{length:uint8, data:uint8[64]}` | `length <= 64`. |
| `ProtocolOfferArrayType` | `{length:uint8, item:ProtocolOfferType[8]}` | `length <= 8`; priority values unique. |
| `ServiceIdArrayType` | `{length:uint8, item:uint16[16]}` | `length <= 16`; no duplicate service ID. |
| `ScheduleEntryArrayType` | `{length:uint8, item:ScheduleEntryType[64]}` | `length <= 64`; ordered and non-overlapping intervals. |
| `CertificateChainType` | `{length:uint8, cert:ByteArray4096Type[5]}` | Maximum 5 certificates, each maximum 4096 bytes; tighter protocol/vendor bound may apply. |

### 29.5 Common configuration capacity symbols

| Symbol | Baseline value | Configuration rule |
|---|---:|---|
| `CFG_PROTOCOL_OFFER_MAX` | 8 | `1..8` |
| `CFG_SERVICE_MAX` | 16 | `1..16` |
| `CFG_SCHEDULE_ENTRY_MAX` | 64 | `1..64` |
| `CFG_V2GTP_PAYLOAD_MAX` | 65536 bytes | `1024..65536`; must fit static pool and EXI WCET budget. |
| `CFG_EXI_WORKSPACE_MAX` | 131072 bytes | Generated-schema measurement shall prove sufficiency. |
| `CFG_CERT_CHAIN_MAX` | 5 | `1..5` |
| `CFG_CERT_SIZE_MAX` | 4096 bytes | Must also satisfy KeyM/Crypto vendor limitation. |
| `CFG_SIGNATURE_SIZE_MAX` | 512 bytes | Covers enabled algorithm set only. |

Capacity symbols are ECUC/generated constants. Changing them requires memory/WCET and negative-test requalification.

## 30. Common enumerations

All project enums use `uint8` unless the generated ARXML selects another implementation base. Unknown values received across a boundary are rejected and mapped to `FORMAT` or `INTERNAL` as appropriate.

| Enum | Numeric values |
|---|---|
| `DataQualityType` | `INVALID=0`, `INIT=1`, `VALID=2`, `DEGRADED=3`, `STALE=4` |
| `ReasonCodeType` | `NONE=0`, `PRECONDITION=1`, `STALE_DATA=2`, `LINK_LOSS=3`, `TIMEOUT=4`, `SEQUENCE=5`, `FORMAT=6`, `SECURITY=7`, `RESOURCE=8`, `PEER_REJECT=9`, `CANCELLED=10`, `INTERNAL=11`, `HW_MISMATCH=12`, `ISOLATION=13`, `INTERLOCK=14`, `PERMISSION_EXPIRED=15`, `CONFIGURATION=16` |
| `ReturnCodeType` | `OK=0`, `ACCEPTED=1`, `BUSY=2`, `REJECTED=3`, `NOT_OK=4`, `OVERFLOW=5` |
| `PlugStateType` | `UNKNOWN=0`, `UNPLUGGED=1`, `CONNECTED=2`, `LOCKED=3` |
| `CpStateType` | `INVALID=0`, `A=1`, `B=2`, `C=3`, `D=4`, `E=5`, `F=6` |
| `PwmModeType` | `NONE=0`, `ANALOG_PWM=1`, `HLC_DIGITAL=2`, `INVALID=3` |
| `CoordinatorStateEnumType` | `UNPLUGGED=0`, `CONNECTED_IDLE=1`, `LINK_SETUP=2`, `HLC_NEGOTIATION=3`, `READY_FOR_ENERGY=4`, `ENERGY_TRANSFER=5`, `STOPPING=6`, `FAULT_LATCHED=7` |
| `PermissionDecisionEnumType` | `DENY=0`, `ALLOW=1` |
| `AllowedActionType` | `NONE=0`, `PREPARE=1`, `ENERGIZE=2`, `IMPORT_POWER=3`, `EXPORT_POWER=4`, `RAMP_TO_ZERO=5`, `DISCHARGE=6` |
| `ChargeModeType` | `NONE=0`, `AC=1`, `DC=2`, `AC_BPT=3`, `DC_BPT=4` |
| `PowerDirectionType` | `NONE=0`, `IMPORT_TO_EV=1`, `EXPORT_FROM_EV=2`, `ZERO_TRANSITION=3` |
| `BptStateEnumType` | `DISABLED=0`, `AVAILABLE=1`, `IMPORT=2`, `RAMP_TO_ZERO=3`, `EXPORT=4`, `BLOCKED=5` |
| `FaultSeverityType` | `INFO=0`, `WARNING=1`, `CONTROLLED_STOP=2`, `EMERGENCY_STOP=3` |
| `FaultLatchType` | `NOT_LATCHED=0`, `SESSION_LATCHED=1`, `UNPLUG_LATCHED=2`, `SERVICE_LATCHED=3` |
| `StopCategoryType` | `NORMAL=0`, `CONTROLLED=1`, `EMERGENCY=2` |
| `HlcStateEnumType` | `IDLE=0`, `WAIT_LINK=1`, `SDP=2`, `TRANSPORT=3`, `SAP=4`, `PROTOCOL_SESSION=5`, `CLOSING=6`, `ERROR=7` |
| `HlcEventCategoryType` | `STATE_CHANGED=0`, `PARAMETERS_ACCEPTED=1`, `PEER_REQUEST=2`, `AUTH_RESULT=3`, `SESSION_COMPLETE=4`, `WARNING=5`, `FAILURE=6` |
| `ProtocolIdType` | `NONE=0`, `DIN_70121=1`, `ISO_15118_2=2`, `ISO_15118_20=3` |
| `SchemaIdType` | `NONE=0`, `SAP=1`, `DIN_70121=2`, `ISO_15118_2=3`, `ISO_20_COMMON=4`, `ISO_20_AC=5`, `ISO_20_DC=6` |
| `SecurityModeType` | `NONE=0`, `TLS12=1`, `TLS13=2` |
| `SecurityStateType` | `NOT_REQUESTED=0`, `NEGOTIATING=1`, `ESTABLISHED=2`, `FAILED=3` |
| `TransportStateType` | `CLOSED=0`, `CONNECTING=1`, `PLAIN_ACTIVE=2`, `TLS_HANDSHAKE=3`, `SECURE_ACTIVE=4`, `CLOSING=5`, `ERROR=6` |
| `TransportProtocolType` | `TCP=0`, `UDP=1` |
| `PlcLinkStateType` | `DOWN=0`, `MODEM_INIT=1`, `SLAC_MATCHING=2`, `ASSOCIATING=3`, `IP_CONFIGURING=4`, `LINK_READY=5`, `STOPPING=6`, `FAULT=7` |
| `HardwareStateType` | `SAFE=0`, `PREPARING=1`, `READY=2`, `ENERGIZED=3`, `STOPPING=4`, `FAULT=5` |
| `CommandResultStateType` | `ACCEPTED=0`, `COMPLETED=1`, `FAILED=2`, `CANCELLED=3`, `TIMEOUT=4` |
| `SdpStateType` | `IDLE=0`, `SENDING=1`, `WAIT_RESPONSE=2`, `COMPLETE=3`, `FAILED=4`, `CANCELLED=5` |
| `CertificateStateType` | `UNAVAILABLE=0`, `VALID=1`, `EXPIRED=2`, `UPDATE_PENDING=3`, `FAILED=4` |
| `CryptoOperationType` | `VERIFY_CHAIN=0`, `VERIFY_SIGNATURE=1`, `CREATE_SIGNATURE=2`, `VALIDATE_CHALLENGE=3` |

## 31. Common structures

### 31.1 `CommEnvelopeType`

`CommEnvelopeType` is a project `STRUCTURE` whose complete sub-elements are defined below. Its initial value is `{sessionGeneration=0, sequenceCounter=0, timestamp=0, quality=INIT, reason=NONE}`.

| Type | Category | Implementation | Invalid/init rule |
|---|---|---|---|
| `CommEnvelopeType` | STRUCTURE | Five fields listed in the following table | Generation `0` invalid; initial quality `INIT`; publication updates all fields atomically. |

| Field | Type | Constraint / meaning |
|---|---|---|
| `sessionGeneration` | `SessionGenerationType` | `0` invalid; changes for every qualified plug session. |
| `sequenceCounter` | `SequenceCounterType` | Increment on every atomic publication. |
| `timestamp` | `MonotonicTimeUsType` | Timestamp of semantic source data. |
| `quality` | `DataQualityType` | Explicit validity/freshness state. |
| `reason` | `ReasonCodeType` | `NONE` when valid with no issue. |

### 31.2 Reusable headers

| Type | Fields | Invalid/init rule |
|---|---|---|
| `CommandHeaderType` | `env:CommEnvelopeType; commandId:Identifier32Type; deadline:MonotonicTimeUsType` | Invalid when generation or command ID is `0`; deadline must be later than source timestamp. |
| `RequestHeaderType` | `env:CommEnvelopeType; requestId:Identifier32Type; deadline:MonotonicTimeUsType` | Same validation as command header. |
| `ResultHeaderType` | `env:CommEnvelopeType; correlationId:Identifier32Type; result:CommandResultStateType; returnCode:ReturnCodeType` | Correlation ID must match a current outstanding request. |
| `EventHeaderType` | `env:CommEnvelopeType; eventId:Identifier32Type; evidenceId:Identifier32Type` | Event ID `0` invalid; event is not authoritative state. |
| `PowerLimitsType` | `maxImportPower:PowerWattType; maxExportPower:PowerWattType; maxChargeCurrent:CurrentMilliAmpereType; maxDischargeCurrent:CurrentMilliAmpereType; maxVoltage:VoltageMilliVoltType; rampLimit:PowerRampWattPerSecondType` | Negative import/export maxima invalid; unavailable quantity uses its type invalid value. |
| `EndpointType` | `address:Ipv6AddressType; port:uint16; transport:TransportProtocolType; security:SecurityModeType; interfaceId:uint16` | Port `0`, all-zero address or unsupported transport/security combination invalid. |
| `ScheduleEntryType` | `startTime_s:uint32; duration_s:uint32; power:PowerWattType` | Duration > 0; addition shall not overflow; entries ordered. |
| `ProtocolOfferType` | `protocol:ProtocolIdType; major:uint8; minor:uint8; schema:SchemaIdType; priority:uint8; namespace:String64Type` | Priority unique; schema/protocol compatibility required. |

## 32. Complete project datatype catalogue

The following catalogue is normative for **project-owned interface design**. Field order shown here is the logical order; generated memory alignment/padding follows the selected AUTOSAR toolchain and shall be verified by ARXML/header diff.

### 32.1 Coordinator, energy, PLC and hardware datatypes

| Datatype | Category | Complete definition | Constraint / invalid rule |
|---|---|---|---|
| `QualifiedCpPpType` | STRUCTURE | `env; plugState:PlugStateType; cpState:CpStateType; cableCurrent:CurrentAmpereType; pwmMode:PwmModeType; duty:DutyPermilleType; frequency:FrequencyHertzType; hlcTrigger:BooleanType` | `quality=VALID` requires plausible CP/PP and cable capability. |
| `EvEnergySnapshotType` | STRUCTURE | `env; snapshotId:Identifier32Type; soc:SocPermilleType; dcVoltage:VoltageMilliVoltType; dcCurrent:CurrentMilliAmpereType; batteryTemp:TemperatureDeciCType; isolation:ResistanceKiloOhmType; interlockClosed:BooleanType; limits:PowerLimitsType; targetVoltage:VoltageMilliVoltType; targetCurrent:CurrentMilliAmpereType; availabilityMask:uint32` | Snapshot ID nonzero; required-field availability and age validated together. |
| `Plm_LinkStatusType` | STRUCTURE | `env; linkGeneration:SessionGenerationType; state:PlcLinkStateType; modemReady:BooleanType; slacMatched:BooleanType; associated:BooleanType; ipv6Ready:BooleanType; peerMac:ByteArray64Type` | `LINK_READY` requires all four readiness booleans true. |
| `HlcSessionStatusType` | STRUCTURE | `env; state:HlcStateEnumType; protocol:ProtocolIdType; schema:SchemaIdType; security:SecurityStateType; selectedMode:ChargeModeType; permissionUsed:Identifier32Type; protocolStep:uint16` | Unknown state/protocol/schema rejected; permission ID zero means none used. |
| `HlcEventType` | STRUCTURE | `header:EventHeaderType; category:HlcEventCategoryType; protocol:ProtocolIdType; protocolStep:uint16; requestedAction:AllowedActionType; requestedPower:PowerWattType` | Peer request remains evidence only. |
| `ChargeHardwareStatusType` | STRUCTURE | `env; state:HardwareStateType; connectorLocked:BooleanType; prechargeActive:BooleanType; positiveContactorClosed:BooleanType; negativeContactorClosed:BooleanType; dischargeActive:BooleanType; measuredVoltage:VoltageMilliVoltType; measuredCurrent:CurrentMilliAmpereType` | Energized state requires consistent measured/contact feedback. |
| `HwCommandResultType` | STRUCTURE | `header:ResultHeaderType; measuredState:HardwareStateType; measuredVoltage:VoltageMilliVoltType; measuredCurrent:CurrentMilliAmpereType` | Result correlation ID equals issued command ID. |
| `ChargePermissionType` | STRUCTURE | `env; permissionId:Identifier32Type; decision:PermissionDecisionEnumType; action:AllowedActionType; mode:ChargeModeType; direction:PowerDirectionType; limits:PowerLimitsType; validUntil:MonotonicTimeUsType` | `ALLOW` requires nonzero ID, validUntil > timestamp and non-NONE action. |
| `HwPrepareRequestType` | STRUCTURE | `header:CommandHeaderType; mode:ChargeModeType; expectedState:HardwareStateType` | Only accepted from safe/idle hardware state. |
| `HwEnableRequestType` | STRUCTURE | `header:CommandHeaderType; permissionId:Identifier32Type; action:AllowedActionType; limits:PowerLimitsType; expectedState:HardwareStateType` | Permission must be fresh/current and action compatible. |
| `HwStopRequestType` | STRUCTURE | `header:CommandHeaderType; stopCategory:StopCategoryType; dischargeRequired:BooleanType` | Always accepted idempotently for current/older active command. |
| `HwSafeStateRequestType` | STRUCTURE | `header:CommandHeaderType; stopCategory:StopCategoryType; cause:ReasonCodeType` | Highest-priority request; emergency cause cannot be downgraded. |
| `Plm_LinkStartRequestType` | STRUCTURE | `header:CommandHeaderType; cpState:CpStateType; interfaceId:uint16; protocolPolicyId:uint16` | One active request per session. |
| `Plm_LinkStopRequestType` | STRUCTURE | `header:CommandHeaderType; stopCategory:StopCategoryType; cause:ReasonCodeType` | Idempotent; invalidates link generation. |
| `HlcStartRequestType` | STRUCTURE | `header:CommandHeaderType; linkGeneration:SessionGenerationType; protocolPolicyId:uint16; requestedMode:ChargeModeType` | CP/link generation must match header generation. |
| `HlcStopRequestType` | STRUCTURE | `header:CommandHeaderType; stopCategory:StopCategoryType; cause:ReasonCodeType` | Nonblocking and idempotent. |
| `HlcRenegotiateRequestType` | STRUCTURE | `header:CommandHeaderType; cause:ReasonCodeType; requestedMode:ChargeModeType` | Active protocol session and policy permission required. |
| `CoordinatorStatusType` | STRUCTURE | `env; state:CoordinatorStateEnumType; mode:ChargeModeType; direction:PowerDirectionType; activePermissionId:Identifier32Type; activeFaultId:Identifier32Type` | Initial state `UNPLUGGED`, quality `INIT`. |
| `PermissionDecisionType` | STRUCTURE | `env; decision:PermissionDecisionEnumType; action:AllowedActionType; mode:ChargeModeType; direction:PowerDirectionType; limits:PowerLimitsType; validUntil:MonotonicTimeUsType` | `DENY` is safe default; limits zeroed on deny. |
| `BptStatusType` | STRUCTURE | `env; state:BptStateEnumType; direction:PowerDirectionType; requestedPower:PowerWattType; measuredPower:PowerWattType; limits:PowerLimitsType; zeroConfirmed:BooleanType` | Direction change requires `zeroConfirmed=TRUE`. |
| `BlockingFaultStatusType` | STRUCTURE | `env; faultId:Identifier32Type; severity:FaultSeverityType; latch:FaultLatchType; stopCategory:StopCategoryType; sourceDomain:uint8; clearAllowed:BooleanType` | `faultId=0` only when no active fault. |
| `PermissionEvalRequestType` | STRUCTURE | `header:RequestHeaderType; requestedAction:AllowedActionType; requestedMode:ChargeModeType; requestedDirection:PowerDirectionType` | Triggers evaluation only; no authorization semantics. |
| `BptModeRequestType` | STRUCTURE | `header:RequestHeaderType; targetMode:ChargeModeType; targetDirection:PowerDirectionType; targetPower:PowerWattType` | BPT mode required for export. |
| `FaultReactionRequestType` | STRUCTURE | `header:RequestHeaderType; faultId:Identifier32Type; requestedStop:StopCategoryType` | Cannot request weaker reaction than active severity policy. |
| `CoordinatorStateType` | STRUCTURE | `env; state:CoordinatorStateEnumType; previousState:CoordinatorStateEnumType; transitionId:Identifier32Type` | Only `ChargeStateMachine` writes. |
| `CoordinatorEventType` | STRUCTURE | `header:EventHeaderType; fromState:CoordinatorStateEnumType; toState:CoordinatorStateEnumType; trigger:uint16` | Queue overflow requires state resync. |
| `BptCapabilityType` | STRUCTURE | `env; acBptSupported:BooleanType; dcBptSupported:BooleanType; v2gSupported:BooleanType; v2hSupported:BooleanType; v2lSupported:BooleanType; limits:PowerLimitsType; minimumSoc:SocPermilleType` | Export support requires nonzero export limit. |
| `NegotiatedBptServiceType` | STRUCTURE | `env; mode:ChargeModeType; controlMode:uint8; serviceId:uint16; scheduleTupleId:uint16` | Mode must be `AC_BPT` or `DC_BPT`. |
| `BptPowerRequestType` | STRUCTURE | `env; direction:PowerDirectionType; targetPower:PowerWattType; targetEnergy:EnergyWattHourType; scheduleTupleId:uint16` | Export sign and direction must agree. |
| `VehicleBptPolicyType` | STRUCTURE | `env; allowedUseCaseMask:uint8; minimumSoc:SocPermilleType; departureTime_s:uint32; requiredMobilityEnergy:EnergyWattHourType; gridMode:uint8` | Policy mask zero disables BPT. |
| `BptPermissionRequestType` | STRUCTURE | `env; direction:PowerDirectionType; action:AllowedActionType; limits:PowerLimitsType; zeroCrossingRequired:BooleanType` | Still requires `PermissionManager` decision. |
| `BptEventType` | STRUCTURE | `header:EventHeaderType; state:BptStateEnumType; direction:PowerDirectionType; limitingReason:ReasonCodeType` | Event is informational/evidence only. |
| `CpPpDiagnosticType` | STRUCTURE | `env; cpState:CpStateType; measuredPositive_mV:VoltageMilliVoltType; measuredNegative_mV:SignedVoltageMilliVoltType; faultMask:uint32` | Raw measurements remain diagnostic; not used directly by HLC. |
| `EnergyDataDiagnosticType` | STRUCTURE | `env; snapshotId:Identifier32Type; missingMask:uint32; staleMask:uint32; rangeErrorMask:uint32; consistencyErrorMask:uint32` | Any required-field mask invalidates snapshot. |
| `Plm_DiagnosticType` | STRUCTURE | `env; linkGeneration:SessionGenerationType; modemCode:uint16; slacCode:uint16; retryCount:uint8` | Vendor detail mapped without exposing raw MME. |
| `HlcDiagnosticType` | STRUCTURE | `env; protocol:ProtocolIdType; protocolStep:uint16; standardError:uint16; demEventId:uint16` | No secret, certificate body or EXI payload. |
| `HardwareDiagnosticType` | STRUCTURE | `env; state:HardwareStateType; mismatchMask:uint32; driverCode:uint16; demEventId:uint16` | Welded/mismatch faults require blocking reaction. |
| `PlatformSupervisionStatusType` | STRUCTURE | `env; entityId:uint16; supervisionState:uint8; deadlineMissCount:uint16; resourceMask:uint32` | Failed supervision is never represented as last-known-good. |
| `SafeStopRequestType` | STRUCTURE | `header:RequestHeaderType; stopCategory:StopCategoryType; faultId:Identifier32Type; cause:ReasonCodeType` | Emergency category pre-empts controlled/normal stop. |
| `FaultEventType` | STRUCTURE | `header:EventHeaderType; faultId:Identifier32Type; severity:FaultSeverityType; latch:FaultLatchType; sourceDomain:uint8; demEventId:uint16` | Preserve first root cause; secondary evidence referenced separately. |

### 32.2 HLC lifecycle and protocol-selection datatypes

| Datatype | Category | Complete definition | Constraint / invalid rule |
|---|---|---|---|
| `LinkContextType` | STRUCTURE | `env; linkGeneration:SessionGenerationType; interfaceId:uint16; ipv6Ready:BooleanType; localAddress:Ipv6AddressType` | IPv6-ready requires nonzero link generation and valid local address. |
| `ProtocolSelectionType` | STRUCTURE | `env; protocol:ProtocolIdType; schema:SchemaIdType; security:SecurityModeType; mode:ChargeModeType` | Compatibility validated by `SchemaRegistry`. |
| `EnabledProtocolListType` | STRUCTURE | `env; offers:ProtocolOfferArrayType` | Length `1..CFG_PROTOCOL_OFFER_MAX`; deterministic priority. |
| `AdapterCreateRequestType` | STRUCTURE | `header:RequestHeaderType; selection:ProtocolSelectionType; contextId:Identifier32Type` | One adapter per context. |
| `AdapterDestroyRequestType` | STRUCTURE | `header:RequestHeaderType; adapterId:Identifier32Type` | Idempotent. |
| `AdapterResetRequestType` | STRUCTURE | `header:RequestHeaderType; adapterId:Identifier32Type; cause:ReasonCodeType` | Zero transient protocol state. |
| `AdapterFactoryStatusType` | STRUCTURE | `env; activeAdapterId:Identifier32Type; activeProtocol:ProtocolIdType; poolUsed:uint8; poolCapacity:uint8` | `poolUsed <= poolCapacity`; capacity generated. |

### 32.3 SDP and UDP datatypes

| Datatype | Category | Complete definition | Constraint / invalid rule |
|---|---|---|---|
| `SdpRequestType` | STRUCTURE | `header:RequestHeaderType; link:LinkContextType; requestedTransport:TransportProtocolType; requestedSecurity:SecurityModeType; retryLimit:uint8` | Link/IPv6 ready required; DIN requires TCP/NONE. |
| `SdpCancelType` | TYPE_REFERENCE | `RequestHeaderType` | Correlation identifies active discovery. |
| `SdpResultType` | STRUCTURE | `header:ResultHeaderType; endpoint:EndpointType; retryCount:uint8` | Endpoint valid only when result completed/OK. |
| `SdpStatusType` | STRUCTURE | `env; state:SdpStateType; activeRequestId:Identifier32Type; retryCount:uint8` | Authoritative state. |
| `SdpDiagnosticType` | STRUCTURE | `env; activeRequestId:Identifier32Type; rejectedField:uint8; retryCount:uint8; endpoint:EndpointType` | Endpoint zeroed when failure occurred before validation. |
| `UdpOpenRequestType` | STRUCTURE | `header:RequestHeaderType; interfaceId:uint16; localPort:uint16; multicastAddress:Ipv6AddressType` | Interface must equal current link context. |
| `UdpTxRequestType` | STRUCTURE | `header:RequestHeaderType; endpoint:EndpointType; payload:ByteBufferRefType` | Payload length bounded by configured UDP PDU. |
| `UdpCloseRequestType` | TYPE_REFERENCE | `RequestHeaderType` | Idempotent. |
| `UdpRxIndicationType` | STRUCTURE | `env; socketId:Identifier32Type; remote:EndpointType; payload:ByteBufferRefType` | Buffer lifetime limited to callback/lease contract. |

### 32.4 TCP/TLS transport datatypes

| Datatype | Category | Complete definition | Constraint / invalid rule |
|---|---|---|---|
| `TransportConnectRequestType` | STRUCTURE | `header:RequestHeaderType; endpoint:EndpointType; security:SecurityModeType; credentialRef:Identifier32Type` | Endpoint/security must match SDP/SAP policy. |
| `TransportTxRequestType` | STRUCTURE | `header:RequestHeaderType; connectionId:Identifier32Type; payload:ByteBufferRefType` | Current connection/generation; bounded payload. |
| `TransportCloseRequestType` | STRUCTURE | `header:RequestHeaderType; connectionId:Identifier32Type; cause:ReasonCodeType` | Idempotent. |
| `TransportStatusType` | STRUCTURE | `env; connectionId:Identifier32Type; state:TransportStateType; security:SecurityStateType; endpoint:EndpointType; txQueueUsed:uint16; rxQueueUsed:uint16` | Queue usage <= configured capacity. |
| `TransportRxIndicationType` | STRUCTURE | `env; connectionId:Identifier32Type; payload:ByteBufferRefType` | Ordered stream span; no message-boundary promise. |
| `TransportTxResultType` | STRUCTURE | `header:ResultHeaderType; connectionId:Identifier32Type; transferredLength:Length32Type` | Transferred length <= requested length. |
| `TcpOpenRequestType` | STRUCTURE | `header:RequestHeaderType; endpoint:EndpointType; soConId:uint16` | Configured SoCon only. |
| `TcpTxRequestType` | STRUCTURE | `header:RequestHeaderType; soConId:uint16; payload:ByteBufferRefType` | Bounded TP/IF ownership contract. |
| `TcpCloseRequestType` | STRUCTURE | `header:RequestHeaderType; soConId:uint16; abort:BooleanType` | Idempotent; abort true for emergency/link loss. |
| `TlsHandshakeRequestType` | STRUCTURE | `header:RequestHeaderType; connectionId:Identifier32Type; security:SecurityModeType; credentialRef:Identifier32Type; peerName:String64Type` | TLS mode cannot be NONE; key is referenced only. |
| `TlsCloseRequestType` | STRUCTURE | `header:RequestHeaderType; connectionId:Identifier32Type` | Erase transient TLS state. |
| `TlsResultType` | STRUCTURE | `header:ResultHeaderType; connectionId:Identifier32Type; security:SecurityStateType; cipherSuite:uint16; peerCertificateRef:Identifier32Type` | No private key or full certificate body. |

### 32.5 V2GTP, EXI, schema and SAP datatypes

| Datatype | Category | Complete definition | Constraint / invalid rule |
|---|---|---|---|
| `V2gtpEncodeRequestType` | STRUCTURE | `header:RequestHeaderType; payloadType:uint16; payload:ByteBufferRefType; output:ByteBufferRefType` | Input/output length <= configured maximum; no overlap unless codec supports it. |
| `V2gtpByteSpanType` | STRUCTURE | `env; connectionId:Identifier32Type; bytes:ByteBufferRefType` | Span length bounded; ordered per connection. |
| `V2gtpResetType` | STRUCTURE | `header:RequestHeaderType; connectionId:Identifier32Type` | Clears partial header/payload. |
| `V2gtpFrameViewType` | STRUCTURE | `env; connectionId:Identifier32Type; payloadType:uint16; payload:ByteBufferRefType` | View lifetime explicitly bounded; payload already header-validated. |
| `V2gtpDiagnosticType` | STRUCTURE | `env; connectionId:Identifier32Type; payloadType:uint16; declaredLength:uint32; rejectedField:uint8` | Oversize length rejected before allocation. |
| `ExiEncodeRequestType` | STRUCTURE | `header:RequestHeaderType; schema:SchemaIdType; messageTypeId:uint16; typedObjectRef:DataPtrType; output:ByteBufferRefType` | Typed object lifetime covers call/job; schema explicit. |
| `ExiDecodeRequestType` | STRUCTURE | `header:RequestHeaderType; schema:SchemaIdType; expectedMessageTypeId:uint16; input:ByteBufferRefType; outputObjectRef:DataPtrType; workspace:ByteBufferRefType` | Input/workspace bounded; schema never inferred. |
| `ExiResetRequestType` | TYPE_REFERENCE | `RequestHeaderType` | Zero transient codec workspace. |
| `ExiEncodeResultType` | STRUCTURE | `header:ResultHeaderType; schema:SchemaIdType; encodedLength:Length32Type` | Encoded length <= output capacity. |
| `ExiDecodeResultType` | STRUCTURE | `header:ResultHeaderType; schema:SchemaIdType; messageTypeId:uint16; decodedObjectRef:DataPtrType` | Object exposed only after full validation. |
| `ExiDiagnosticType` | STRUCTURE | `env; schema:SchemaIdType; messageTypeId:uint16; bitOffset:uint32; rejectedConstraint:uint16` | No partial decoded payload exported. |
| `SchemaLookupRequestType` | STRUCTURE | `header:RequestHeaderType; schema:SchemaIdType` | Schema ID not NONE. |
| `SchemaLimitsRequestType` | STRUCTURE | `header:RequestHeaderType; schema:SchemaIdType; maxPayload:uint32; maxWorkspace:uint32; maxCollection:uint16` | Returned values <= compiled capacities. |
| `SchemaCompatibilityRequestType` | STRUCTURE | `header:RequestHeaderType; protocol:ProtocolIdType; schema:SchemaIdType; mode:ChargeModeType` | Exact compatibility matrix match required. |
| `SchemaRegistryStatusType` | STRUCTURE | `env; ready:BooleanType; enabledSchemaMask:uint32; integrityErrorMask:uint32` | Failed schema is not offered by SAP. |
| `SapNegotiateRequestType` | STRUCTURE | `header:RequestHeaderType; enabled:EnabledProtocolListType; transport:TransportProtocolType; security:SecurityModeType; mode:ChargeModeType` | Offers prefiltered and bounded. |
| `SapCancelType` | TYPE_REFERENCE | `RequestHeaderType` | Current negotiation only. |
| `SapTxRequestType` | STRUCTURE | `header:RequestHeaderType; payload:ByteBufferRefType` | Encoded with SAP schema only. |
| `SapRxIndicationType` | STRUCTURE | `env; payload:ByteBufferRefType` | SAP payload type and grammar validated. |
| `SapDiagnosticType` | STRUCTURE | `env; activeRequestId:Identifier32Type; offeredCount:uint8; selectedSchema:SchemaIdType; responseCode:uint8` | Unoffered schema invalid. |

### 32.6 Security and certificate datatypes

| Datatype | Category | Complete definition | Constraint / invalid rule |
|---|---|---|---|
| `CertChainValidationRequestType` | STRUCTURE | `header:RequestHeaderType; chain:CertificateChainType; policyId:uint16; trustedTime_s:uint64` | Chain/count/size bounded before crypto processing. |
| `SignatureVerifyRequestType` | STRUCTURE | `header:RequestHeaderType; algorithm:uint16; keyOrCertRef:Identifier32Type; signedData:ByteBufferRefType; signature:ByteArray512Type` | Algorithm enabled; input/signature bounded. |
| `SignatureCreateRequestType` | STRUCTURE | `header:RequestHeaderType; algorithm:uint16; privateKeyHandle:Identifier32Type; dataToSign:ByteBufferRefType` | Handle only; private key bytes prohibited. |
| `ChallengeValidationRequestType` | STRUCTURE | `header:RequestHeaderType; challenge:ByteArray64Type; expectedSessionId:ByteArray64Type` | Bound to active generation/session. |
| `SecurityCancelRequestType` | STRUCTURE | `header:RequestHeaderType; securityJobId:Identifier32Type` | Idempotent. |
| `CertValidationResultType` | STRUCTURE | `header:ResultHeaderType; certificateRef:Identifier32Type; validFrom_s:uint64; validTo_s:uint64; trustAnchorRef:Identifier32Type` | Result valid only if trusted time is available/policy permits. |
| `SignatureResultType` | STRUCTURE | `header:ResultHeaderType; signature:ByteArray512Type` | Signature length compatible with algorithm. |
| `CryptoJobRequestType` | STRUCTURE | `header:RequestHeaderType; operation:CryptoOperationType; keyHandle:Identifier32Type; input:ByteBufferRefType; output:ByteBufferRefType` | No secret input export; buffers bounded. |
| `CryptoJobResultType` | STRUCTURE | `header:ResultHeaderType; cryptoJobId:Identifier32Type; outputLength:Length32Type` | Correlation and generation checked before delivery. |
| `CertificateRefRequestType` | STRUCTURE | `header:RequestHeaderType; usage:uint8; identityId:Identifier32Type` | Usage must be enabled by policy. |
| `IdentityRefRequestType` | STRUCTURE | `header:RequestHeaderType; identityType:uint8; identity:String32Type` | eMAID/identity syntax validated by protocol owner. |
| `CertificateInstallRequestType` | STRUCTURE | `header:RequestHeaderType; identity:String32Type; chain:CertificateChainType; privateKeyHandle:Identifier32Type; transactionId:Identifier32Type` | Stage/validate before commit; no private key bytes. |
| `CertificateUpdateRequestType` | STRUCTURE | `header:RequestHeaderType; currentCertificateRef:Identifier32Type; newChain:CertificateChainType; newPrivateKeyHandle:Identifier32Type; transactionId:Identifier32Type` | Preserve current record until atomic commit. |
| `CertificateDeleteRequestType` | STRUCTURE | `header:RequestHeaderType; certificateRef:Identifier32Type; deleteKey:BooleanType; transactionId:Identifier32Type` | Policy/ownership check required. |
| `CertificateRollbackRequestType` | STRUCTURE | `header:RequestHeaderType; transactionId:Identifier32Type; targetVersion:uint16` | Target version must be last verified consistent record. |
| `CertificateStatusType` | STRUCTURE | `env; certificateRef:Identifier32Type; state:CertificateStateType; version:uint16; validFrom_s:uint64; validTo_s:uint64; privateKeyHandle:Identifier32Type` | Private key handle may be nonzero; key bytes never present. |
| `CertificateEventType` | STRUCTURE | `header:EventHeaderType; certificateRef:Identifier32Type; transactionId:Identifier32Type; state:CertificateStateType` | Event is not current-state authority. |
| `KeyMStoreRequestType` | STRUCTURE | `header:RequestHeaderType; certificateRef:Identifier32Type; keyHandle:Identifier32Type; chain:CertificateChainType` | KeyM ownership and chain consistency required. |
| `NvMWriteRequestType` | STRUCTURE | `header:RequestHeaderType; blockId:uint16; transactionId:Identifier32Type; record:ByteBufferRefType; crc:uint32` | Record bounded; atomic commit/rollback policy. |

## 33. Imported standardized and BSW datatype mapping

These types are intentionally **not redefined**. Completeness means their exact origin and compatibility check are defined.

| Interface/API | Ownership | Required imported artifact | Project compatibility evidence |
|---|---|---|---|
| `ChargeControl::SetChargeState` parameter `StateMachineControl` | `AUTOSAR_IMPORTED` | Selected AUTOSAR/supplier `ISO15118Chrg` ARXML + generated `Rte_ISO15118Chrg.h` | ARXML/header checksum, enum-value static assertions and wrapper unit test. |
| `ChargeControl::SetCpLineStatus` parameter `CpLineStatus` | `AUTOSAR_IMPORTED` | Same baseline | Static mapping from qualified CP active/inactive only. |
| Every `ChargeParameters::*Params` argument and return type | `AUTOSAR_IMPORTED` | Exact supplier-generated interface for selected release | Operation-by-operation signature diff; no hand-copied layout. |
| `EthTrcv_LinkStateType` | `BSW_IMPORTED` | `EthTrcv` generated header | Adapter maps active/inactive to project link evidence. |
| SoAd/TcpIp socket and address types | `BSW_IMPORTED` | Selected SoAd/TcpIp vendor headers | Compile-time API adapter test; no POSIX type crosses boundary. |
| PduR buffer/TP types | `BSW_IMPORTED` | PduR generated headers | `StartOfReception/CopyRxData/CopyTxData` signature and buffer ownership tests. |
| Csm/Crypto/KeyM job and key types | `BSW_IMPORTED` | Vendor Csm/Crypto/KeyM generated headers | Job ID/key handle width and callback correlation tests. |
| DIN/ISO typed message structures | `PROTOCOL_GENERATED` | Licensed schema + selected EXI generator output | Golden vectors, generated-code checksum and schema-version matrix. |

## 34. Datatype initialization, invalidation and compatibility

### 34.1 Initialization rules

- All structures are zero-initialized before owner-specific initialization.
- `sessionGeneration=0`, `Identifier32Type=0`, enum value `0` where named `NONE/UNKNOWN/INVALID`, and quantity-specific invalid values represent unavailable data.
- Published S/R data starts with `quality=INIT` and changes to `VALID`, `DEGRADED`, `STALE` or `INVALID` explicitly.
- Unused array elements are zeroed; consumers iterate only to validated `length`.

### 34.2 Freshness and invalidation

- Datatype carries source timestamp and quality; interface configuration supplies `maxAge`.
- Consumer calculates age with wrap-safe monotonic-time logic.
- `timestamp=0`, generation mismatch, `quality != VALID` where a hard invariant requires valid data, or `age > maxAge` invalidates the item.
- Last-known-good safety data is never reused after invalidation/expiry.

### 34.3 Version compatibility

Each project `ApplicationDataType` shall have:

```text
shortName
semanticVersion.major
semanticVersion.minor
compatibilityId
generatedArxmlChecksum
```

- Major change: field removal/reorder, base-type/range/unit change or enum renumbering.
- Minor change: backward-compatible optional field/enum addition when all consumers explicitly tolerate it.
- Same-ECU RTE mapping is frozen by ARXML/header diff; cross-ECU mapping additionally requires E2E/data-transformation compatibility.

## 35. Datatype-to-interface completion rules

An interface is datatype-complete only when all checks below pass:

- [x] Project payload type exists in Section 32 or is a documented alias to a complete common type.
- [x] Every nested project type resolves to a base, enum, bounded collection or complete structure.
- [x] Unit, sign, physical range and invalid representation are defined for physical values.
- [x] All arrays/strings/buffers have fixed maximum capacity and validated runtime length.
- [x] Requests/results contain generation plus command/request/job correlation.
- [x] Async status/event contains timestamp, sequence counter, quality and reason through `CommEnvelopeType`.
- [x] Imported AUTOSAR/BSW/protocol types identify their authoritative generated artifact and validation evidence.
- [ ] Exact AUTOSAR/supplier imported signatures have been attached after release/vendor freeze.
- [ ] Concrete per-interface `maxAge`, queue depth and runnable timing have been assigned by timing/HARA work products.

## 36. Source baseline and applicability

Nội dung được phát triển từ:

- `EVCC_AUTOSAR_Charging_Architecture`, Revision 4, 07/09/2026.
- `EVCC_AUTOSAR_Project_Overview_VI`, Revision 1, 07/09/2026.
- `EVCC_AUTOSAR_Two_Main_Compositions_Detailed_Description_VI`, Revision 1, 08/09/2026.

Port/interface/type names không thuộc standardized AUTOSAR contract được xem là **proposed project design names**. Exact normative types và parameter directions phải được lấy từ selected AUTOSAR release, supplier ARXML và licensed DIN/ISO standards. Timer và safety reaction cuối cùng phải được xác nhận bằng HARA, software timing analysis và ECU integration constraints.
