# Quy ước interface kiểu AUTOSAR trước giai đoạn ARXML

**Revision:** Draft 0.6 — 20/09/2026
**Status:** `PROVISIONAL_ASR_STYLE` — dùng cho host/SIL và implementation-first; không phải generated RTE contract

## 1. Mục tiêu

Cho phép các subcomponent giao tiếp theo semantics AUTOSAR Classic ngay khi chưa chọn vendor ARXML/toolchain. Logic nghiệp vụ không gọi trực tiếp implementation của component khác; nó đi qua port handle có context và operation/read/write function.

Khi formalize, vendor có thể thay function-port binding bằng `Rte_Call`, `Rte_Read`, `Rte_Write`, generated types và runnable events mà không đổi ownership hoặc luồng nghiệp vụ.

## 2. Quy tắc chọn interface

| Trường hợp | Kiểu tạm | Semantics |
|---|---|---|
| Command hoặc request/response đồng bộ | C/S | Client gọi operation trên required port; server sở hữu validation và state mutation. |
| Authoritative state/snapshot | S/R | Một sender sở hữu DataElement; receiver đọc/copy snapshot, không giữ pointer vào mutable state của sender. |
| Event bất đồng bộ | Queued S/R | Callback chỉ copy bounded payload vào queue rồi notify owner runnable; owner xử lý generation/correlation. |
| Init/bind/configuration nội bộ | Local operation | Không quảng bá như RTE interface nếu không qua boundary SWC. |
| BSW/vendor API | Adapter port | Project type không lẫn với generated BSW type; adapter thực hiện mapping. |

## 3. Hình dạng port C11

```c
typedef struct {
    void *context;
    OpenV2g_Result (*operation)(
        void *context,
        const RequestType *request,
        ResultType *result);
} ExampleClientServerPort;

typedef struct {
    void *context;
    OpenV2g_Result (*read)(void *context, StatusType *status);
} ExampleSenderReceiverPort;
```

Một port instance đại diện P-port ở server và được copy/bind thành R-port ở client. `context` chỉ là host/SIL replacement cho generated RTE routing; không được đưa vào ARXML datatype.

## 4. Quy tắc dữ liệu và ownership

- Request/result directions phải thể hiện trong chữ ký; không dựa vào tên P/R mơ hồ từ bảng nguồn.
- Client không include private server header và không dereference state của server.
- S/R reader nhận bản copy; pointer/buffer view chỉ hợp lệ trong lifetime ghi rõ.
- Mọi buffer có pointer kèm length/capacity; không chuyển ownership ngầm.
- Request bất đồng bộ mang session generation và request/correlation ID.
- Một state domain chỉ có một writer; receiver không ghi ngược vào sample đã đọc.
- Missing/unconnected required port trả `REJECTED` hoặc `NOT_IMPLEMENTED`, không giả định thành công.
- Status khởi tạo phải phân biệt với dữ liệu valid; sequence counter tăng khi owner publish thay đổi.

## 5. Mapping hiện tại

### 5.1 ChargingCoordinator và supporting services

| Boundary | C/S | S/R / queued S/R |
|---|---|---|
| `ChargingCoordinator` | request energy transfer, stop | charge permission; complete coordinator status remains pending |
| `ChargeStateMachine` | dispatch là local owner operation | coordinator state |
| `PermissionManager` | evaluate/revoke là local owner operation | charge permission |
| `FaultCoordinator` | begin/raise/clear là local owner operation | blocking fault status |
| `BptCoordinator` | direction request là local owner operation trong G1 | BPT permission request |
| `CpPpManager` | qualification/publish là local producer operation | qualified CP/PP |
| `EnergyDataProvider` | validate/publish là local producer operation | energy snapshot |
| `PlcLinkManager` | start, stop và modem/SLAC/association/IPv6 callbacks | PLC link status |
| `ChargeHardwareAdapter` | prepare, enable, request safe state; apply correlated measured feedback | authoritative measured hardware status; retained command result |

### 5.2 ISO15118Chrg transport/facade

| Boundary | C/S | S/R / queued S/R |
|---|---|---|
| `ISO15118Chrg` façade | start, stop; take bounded semantic protocol request | HLC status; queued HLC events + overflow; protocol diagnostic |
| `SdpClient` | start, request sent, response, cancel | SDP status/selected endpoint |
| `TcpTlsSession` | connect, bounded send, correlated TX result, TCP connected, TLS result, close | transport/security status including bounded queue usage |
| `V2gtpCodec` | begin, consume, get frame, reset, encode | frame is C/S result view with bounded lifetime |
| `ExiCodec` | encode, decode, reset workspace | retained EXI diagnostic |
| `SchemaRegistry` | get descriptor, get limits, compatibility check | authoritative registry status |
| `SapNegotiator` | negotiate, receive response, cancel, timer expiry; required typed message channel | authoritative SAP state/diagnostic status |
| `ProtocolAdapterFactory` | create, destroy, reset static adapter | authoritative factory status |
| DIN/ISO-2 protocol adapters | start, stop, decoded semantic response, timer expiry; required semantic message channel | authoritative protocol state and retained diagnostic; required energy/permission snapshots read by S/R; events copied by HLC façade to queued S/R |

`ISO15118Chrg` gọi SDP, TCP/TLS và V2GTP qua required C/S ports, đồng thời đọc SDP endpoint qua S/R port. Test integration truyền CP/PP, energy, PLC, HLC và hardware snapshots bằng S/R read ports thay vì truy cập trực tiếp state của producer.

### 5.3 EXI/schema operation-level mapping

| Atomic contract | Kiểu triển khai tạm | Port |
|---|---|---|
| `EXI-IF-001 Encode` | C/S | `OpenV2g_ExiCodecCsPort.encode` |
| `EXI-IF-002 Decode` | C/S | `OpenV2g_ExiCodecCsPort.decode` |
| `EXI-IF-003 ResetWorkspace` | C/S | `OpenV2g_ExiCodecCsPort.reset_workspace` |
| Encode/decode result | C/S output parameter | `OpenV2g_ExiResult` |
| `EXI-IF-006 ExiDiagnostic` | S/R retained sample | `OpenV2g_ExiDiagnosticSrPort.read` |
| `SCH-IF-001 GetSchemaDescriptor` | C/S | `OpenV2g_SchemaRegistryCsPort.get_schema_descriptor` |
| `SCH-IF-002 GetSchemaLimits` | C/S | `OpenV2g_SchemaRegistryCsPort.get_schema_limits` |
| `SCH-IF-003 CheckProtocolCompatibility` | C/S | `OpenV2g_SchemaRegistryCsPort.check_protocol_compatibility` |
| `SCH-IF-004 SchemaRegistryStatus` | S/R authoritative snapshot | `OpenV2g_SchemaRegistryStatusSrPort.read` |

`ExiCodec` dùng required C/S và S/R ports của `SchemaRegistry` ở runtime. Backend bind/generator metadata vẫn là local configuration operation.

`SapNegotiator` gửi một bounded typed offer snapshot qua required C/S channel. Trong host/SIL, `ISO15118Chrg` giữ mailbox một phần tử để test; target mapper phải encode snapshot bằng `EXI_SCHEMA_SAP`, đóng V2GTP và chuyển response đã decode về operation tương ứng.

DIN/ISO-2 adapter phát `OpenV2g_ProtocolTxRequest` qua required C/S channel. Mỗi request mang generation, exact correlation ID, protocol/schema/mode và bản copy energy/permission khi bước đó cần. Mapper lấy request bằng `OpenV2g_Iso15118Chrg_TakeProtocolRequest`, map sang generated schema object, encode/đóng V2GTP, rồi đưa decoded response về `OpenV2g_Iso15118Chrg_OnProtocolMessage`. Chỉ response khớp pending generation/correlation/message kind được chấp nhận. Các enum/payload này là semantic project contract, không phải wire message ID hay generated DIN/ISO datatype.

`ProtocolDataFlow` định nghĩa một rule cho mỗi protocol/mode/message: flow case, energy fields relevant/required, permission/action, metering evidence và HLC event category. Adapter dùng rule này trước khi gọi required message channel; HLC giữ bản request đã dispatch và dùng chính bản đó để tạo `HlcEvent`, tránh tin action/power do peer hoặc mapper cung cấp.

Transport TX ở host giữ fixed-capacity metadata của request đang chờ, không giữ payload pointer. Lower `TcpTx` seam phải copy hoặc consume payload trong operation call; ownership TP/IF thực tế chỉ được chốt khi có generated SoAd/TcpIp/PduR contract.

## 6. Mapping khi có AUTOSAR vendor

Formalization sau này phải thực hiện operation-by-operation, không chuyển function table thành ARXML bằng script mù:

1. Chốt server/client ownership và SWC allocation.
2. Thay `size_t`, raw pointer và metadata string bằng exact AUTOSAR implementation/application types hoặc bounded project types.
3. Chọn synchronous/asynchronous C/S semantics, runnable event và timeout.
4. Chọn S/R invalidation, queued/nonqueued, init value, E2E và `maxAge`.
5. Sinh ARXML/RTE, sau đó viết compile adapter/static mapping tests.
6. Giữ host ports làm test seam; target build gọi generated `Rte_*` qua adapter.

Việc hoãn ARXML không cho phép bỏ qua ownership, bounds, freshness, queue, generation hoặc failure semantics.
