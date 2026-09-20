# Ma trận truy vết interface tới host/SIL implementation

**Revision:** Draft 0.10 — 20/09/2026
**Status:** `VERIFIED_HOST_MAPPING` — không phải ARXML/RTE, target build hoặc bằng chứng conformance

## 1. Phạm vi kiểm kê

Nguồn chuẩn để kiểm kê là `EVCC_AUTOSAR_Two_Main_Compositions_Atomic_Interface_Spec_VI.md` Rev.3. Tài liệu đó có 218 hàng atomic interface:

- `CC-EXT-*`: 18;
- `HLC-ASR-*`: 22;
- `HLC-BSW-*`: 13;
- các interface nội bộ `*-IF-*`: 165.

Ma trận dưới đây bao phủ toàn bộ các dải ID đó theo subcomponent. Nó không thay đổi ID nguồn và không chuyển thiết kế thành artifact đã freeze.

## 2. Quy ước trạng thái

| Trạng thái | Ý nghĩa |
|---|---|
| `IMPLEMENTED_HOST` | Có logical C11 contract, source và test host cho hành vi cốt lõi trong G1. |
| `PARTIAL_HOST` | Có owner/seam hoặc một phần hành vi; chưa đủ toàn bộ atomic operations. |
| `STUB_HOST` | Chỉ có lifecycle/feature placeholder để kiểm tra wiring; không có protocol implementation. |
| `DISABLED_G1` | Có extension point nhưng fail-disabled trong active G1 scope. |
| `BLOCKED_G2` | Cần supplier/platform/licensed artifact; không được tự dựng ABI hoặc normative behavior. |

## 3. ChargingCoordinator composition

| Atomic IDs | Subcomponent/boundary | Host mapping | Verification | Trạng thái và khoảng trống |
|---|---|---|---|---|
| `CC-EXT-001…018` | `ChargingCoordinator` external boundary | `charging_coordinator.h/.c`; typed link/HLC/hardware ports; provider/status/event inputs; permission output | `openv2g_test_nominal_flow`, `openv2g_test_hlc_event_bridge`, `openv2g_test_hardware_gate` | `PARTIAL_HOST`: 001–006, 008–016 có logical path; 007 command-result correlation, 017 renegotiate và 018 public coordinator status chưa hiện thực. HLC event overflow bắt buộc status resync và revoke permission. |
| `CSM-IF-001…008` | `ChargeStateMachine` | deterministic owner state machine; coordinator dispatches local events; state exposed by S/R copy port | nominal flow, permission, BPT, ASR-style ports | `PARTIAL_HOST`: progression owner đã có; event queue riêng, đầy đủ BPT/fault local-operation payload chưa có. |
| `PM-IF-001…006` | `PermissionManager` | fail-closed evaluation, freshness/generation, fault/BPT gates, permission ID/expiry/revoke; permission S/R port | `openv2g_test_permission`, nominal flow, ASR-style ports | `IMPLEMENTED_HOST` cho G1 DC EIM; limit intersection và timing là provisional, chưa phải safety baseline. |
| `BPT-IF-001…008` | `BptCoordinator` | typed permission request and S/R port; generation, measured-zero và new-permission reversal guards | `openv2g_test_bpt_coordinator`, permission, ASR-style ports | `DISABLED_G1`: guard cốt lõi có test; capability, vehicle policy, negotiated service, power request, status/event đầy đủ bị giữ ngoài active scope. |
| `FC-IF-001…010` | `FaultCoordinator` | blocking status owner/S/R port, session reset, raise/latch, unplug clear; feeds permission and stop paths | permission, nominal flow, ASR-style ports | `PARTIAL_HOST`: blocking path đã nối; diagnostic-specific inputs, safe-stop/event records và Dem mapping cần G2/G3. |

## 4. ISO15118Chrg composition

| Atomic IDs | Subcomponent/boundary | Host mapping | Verification | Trạng thái và khoảng trống |
|---|---|---|---|---|
| `HLC-ASR-CC-001…002` | Standardized `ChargeControl` | isolated behind future `integration/autosar` adapter | public-header compile only | `BLOCKED_G2`: cần exact supplier ARXML/generated signatures; không dùng host API làm ABI thay thế. |
| `HLC-ASR-CP-001…020` | Standardized `ChargeParameters` | project logical status/types remain outside imported ABI | public-header compile only | `BLOCKED_G2`: parameter directions, nested types and enum mapping require supplier artifacts. |
| `HLC-BSW-001…013` | EcuM/Eth/TcpIp/SoAd/PduR/Crypto callbacks | host lifecycle, byte-stream and crypto seams only | V2GTP, HLC generation/TLS tests | `BLOCKED_G2`: no generated callback signatures, socket/PDU config, crypto jobs or target correlation evidence. |
| `HSA-IF-001…011` | `HlcSessionAdapter` façade | C/S start/stop and bounded protocol-request take operations; S/R HLC status, queued event/overflow and retained protocol diagnostic; coordinator HLC required port | HLC generation, event queue/bridge, nominal flow, protocol adapters, ASR-style ports | `PARTIAL_HOST`: start/stop/status/event and protocol request/diagnostic paths exist; renegotiate, link facade mapping and complete service/power payloads remain. |
| `HSS-IF-001…012` | `HlcSessionSupervisor` | single `ISO15118Chrg` owner sequences SDP → transport/TLS → V2GTP/schema/EXI/SAP → adapter and stop rollback | HLC generation, HLC TLS | `PARTIAL_HOST`: lifecycle/cancel/late-generation/rollback are present; renegotiate and real lower owners remain. |
| `SDP-IF-001…010` | `SdpClient` | C/S lifecycle/callback port; S/R status/endpoint; bounded generation, request ID and idempotent cancel | `openv2g_test_sdp_client`, HLC generation, ASR-style ports | `PARTIAL_HOST`: discovery lifecycle is tested; UDP socket, LinkContext, timers/retries and diagnostic publication require G2/config input. |
| `TTS-IF-001…012` | `TcpTlsSession` | C/S connect/send/TX-result/callback/close port; S/R transport status; fixed-capacity TX metadata queue; generation + connection/request-ID/deadline correlation, backpressure, cancel/reconnect late-callback rejection and no implicit downgrade | `openv2g_test_tcp_tls_session`, `openv2g_test_hlc_tls`, HLC generation, ASR-style ports | `PARTIAL_HOST`: connect/send/close/status and correlated HLC RX/TX lifecycle exist; real SoAd/TcpIp/PduR and TLS adapter binding remains `BLOCKED_G2`. |
| `V2GTP-IF-001…005` | `V2gtpCodec` | C/S encode/begin/consume/frame/reset port, immutable caller-owned frame view and bounded capacity | `openv2g_test_v2gtp_codec`, HLC generation, ASR-style ports | `PARTIAL_HOST`: 001–004 are implemented; explicit diagnostic publication for 005 is represented only by result/HLC reason. |
| `EXI-IF-001…007` | `ExiCodec` | provisional C/S encode/decode/reset port, S/R diagnostic sample, typed pluggable backend; explicit schema/message, caller-owned bounded buffers/workspace, generation guard and no-partial decode publication | `openv2g_test_exi_codec`, dependency-failure branch in HLC generation | `PARTIAL_HOST`: ASR-style project adapter contract is implemented; production generated backend, licensed schema, golden vectors and target WCET remain `BLOCKED_G2`. |
| `SCH-IF-001…004` | `SchemaRegistry` | provisional C/S descriptor/limits/compatibility port and S/R authoritative status; immutable descriptor, availability and integrity masks | `openv2g_test_exi_codec`, schema/protocol mismatch branch | `PARTIAL_HOST`: registry/port behavior is implemented; real descriptors/checksums and integrity evidence require generated artifacts. |
| `SAP-IF-001…007` | `SapNegotiator` | typed negotiate/response/cancel/timer C/S port, ordered bounded offer snapshot, required message-channel port and authoritative S/R status | SAP negotiator, HLC generation/TLS, nominal flow | `PARTIAL_HOST`: generation/request correlation, TLS filtering, no-match, timeout and unoffered schema fail closed; generated SAP EXI objects, namespaces/version mapping and normative response-code mapping remain `BLOCKED_G2`. |
| `PAF-IF-001…004` | `ProtocolAdapterFactory` | static DIN/ISO-2 instances, typed create/destroy/reset C/S operations, one-active ownership and S/R factory status | protocol adapters, HLC generation, nominal flow | `IMPLEMENTED_HOST` for active G1 variants; ISO-20 registry entry and generated schema-integrity binding remain outside G1. |
| `PAD-IF-001…012` | Common protocol adapter contract | typed start/stop/decoded-response/timer C/S port, shared protocol data-flow rules, required outbound semantic message channel, energy/permission S/R reads, authoritative status and retained diagnostic; HLC retains dispatched request provenance for normalized events | protocol data flow, protocol adapters, nominal flow | `PARTIAL_HOST`: mode/message applicability, required field availability, limit intersection, exact pending generation/correlation/kind, metering token, timer/resource/peer failures, diagnostics and permission re-read at every dependent request/loop are implemented; renegotiation and generated message mapping remain. |
| `DIN-IF-001…007` | `Din70121Adapter` | DC/non-TLS guard; request-generating semantic sequence SessionSetup → discovery/selection/auth → parameters → cable/precharge/power → current-demand loop → stop/welding/session-stop; energy and permission gates before dependent requests | protocol adapters, SAP/HLC selection path | `PARTIAL_HOST`: complete baseline DC EIM semantic host loop and expiry matrix through precharge/power/current-demand renewal are exercised; DIN generated types, EXI mapping, physical-value conversion and edition-specific timers/response codes remain `BLOCKED_G2`. |
| `ISO2-IF-001…011` | `Iso15118_2Adapter` | EIM mode guard and request-generating AC/DC semantic branches; tested DC sequence includes current-demand/metering-receipt loops, permission/context gates and controlled stop | protocol adapters, nominal SAP→ISO-2 flow | `PARTIAL_HOST`: complete baseline DC EIM semantic host loop and expiry matrix through precharge/power/current-demand renewal are exercised; generated ISO-2 types/EXI mapping, schedules/limits, exact timers and PnC/security behavior remain `BLOCKED_G2` or disabled. |
| `ISO20-IF-001…011` | `Iso15118_20Adapter` | feature gate defaults disabled | rejection test in HLC generation | `DISABLED_G1`; Common/AC/DC schemas and BPT/schedule behavior are outside active scope. |
| `PKI-IF-001…010` | `PkiSecurityAdapter` | key-handle oriented platform seam; feature disabled | TLS negative path verifies no plaintext fallback | `DISABLED_G1` / `BLOCKED_G2`: no certificate verification, signing, trust/time or Csm/HSM binding. |
| `CERT-IF-001…010` | `CertificateManager` | lifecycle placeholder; feature disabled | feature gate only | `DISABLED_G1` / `BLOCKED_G2`: no OEM provisioning/update/revocation or NvM transaction. |

## 5. Supporting boundaries outside the 218-row catalogue

| Boundary | Connected host behavior | Evidence | Remaining dependency |
|---|---|---|---|
| `CpPpManager` | qualified CP/PP S/R copy port into coordinator-facing flow | nominal flow, ASR-style ports | ADC/PWM/EthTrcv/vendor mapping and timing |
| `EvEnergyDataProvider` | atomic energy snapshot S/R copy port into coordinator/permission | nominal flow, permission, ASR-style ports | real BMS/OBC/inverter data, E2E/freshness budget |
| `PlcLinkManager` | C/S control/callback port and S/R status for modem-ready → SLAC → association → IPv6 | nominal flow, ASR-style ports | modem/raw MME/PEV capability and TcpIp/SoAd binding |
| `ChargeHardwareAdapter` | C/S permission-gated control and S/R measured status | hardware gate, nominal flow, ASR-style ports | HARA, physical feedback, command IDs/results, target drivers |
| Platform services | clock/diagnostic/crypto function-port seams | header compile | OS/RTE/Dem/Csm/KeyM/NvM generated configuration |

## 6. Compatibility conclusion

The G1 logical data path is connected and host-verified for:

`CpPp/Energy → ChargingCoordinator → PlcLinkManager → ISO15118Chrg(SDP → TCP/TLS → V2GTP → correlated SAP selection → static DIN/ISO-2 request-generating semantic adapter ↔ energy/permission S/R) → HLC status/event → PermissionManager → hardware fake`.

The structural rule “HLC has no actuator control port” remains true. Event overflow revokes permission and requires a status snapshot at or after the overflow time before queued events are accepted again. TLS failure leaves transport in error and never activates plaintext.

Compatibility cannot yet be claimed for AUTOSAR RTE/BSW, supplier `ChargeControl`/`ChargeParameters`, EXI-generated types, modem drivers, protocol conformance or physical hardware. Those boundaries remain explicitly `BLOCKED_G2`/G3.

## 7. Reproducible host evidence

Configuration/build used for this draft:

```powershell
cmake -S . -B build-host -A x64
cmake --build build-host --config Debug
```

Clean MSVC `/W4 /WX` build passed and all 17 host test executables passed on 20/09/2026. This includes DIN/ISO-2 data-flow projection, complete DC EIM semantic request/response loops, TCP/TLS cancel/reconnect and bounded TX correlation, fake-backend EXI and cross-library ASR-style port tests and is `VERIFIED_HOST` evidence only.
