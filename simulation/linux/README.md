# Linux BSW stub environment

Revision: Draft 0.1 — 26/10/2026. Status: `IMPLEMENTED_HOST` / `VERIFIED_HOST`.
This is a generic C11/POSIX environment around the existing project-domain EVCC.
It requires CMake and a Linux C compiler, with no AUTOSAR SDK or external library.

## Build and run

From the repository root:

```sh
cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Debug
cmake --build build-linux --parallel
ctest --test-dir build-linux --output-on-failure
```

Linux stubs default ON when simulation is enabled. Disable them with
`-DOPENV2G_BUILD_LINUX_STUBS=OFF`; disable all simulation with
`-DOPENV2G_BUILD_SIMULATION=OFF`. Windows builds do not select this directory.

In one terminal, start the station:

```sh
./build-linux/simulation/linux/openv2g_linux_stub station 15118
```

In another terminal, within 30 seconds, run the EVCC:

```sh
./build-linux/simulation/linux/openv2g_linux_stub evcc 15118
```

The station serves one connection and exits. Both commands return zero on the
nominal smoke flow. Use `station 15118 reject` or `station 15118 stale` for
negative responses; the EVCC then returns nonzero and leaves permission DENY.
A failed connect/listen also returns nonzero. The EVCC never starts a hidden
in-process station. TCP binds/connects to IPv4 loopback; no remote host lookup.

## Boundary coverage

The names below identify responsibilities, **not supplier API signatures**.
No `Rte_*`, `TcpIp_*` or imported AUTOSAR layout is guessed or exported.

| Surrounding module/domain | Stub implementation and interaction | Limits |
|---|---|---|
| EcuM/BswM lifecycle | `Ov2g_EnvironmentInit` initializes components, services and OS; `EnvironmentRun` coordinates startup and stop | Single smoke run per initialization; no ECU state/arbitration configuration |
| RTE / SchM | Existing typed C/S calls and S/R snapshot ports in `OpenV2g_HostSimulation`; `rte.c` dispatches semantic RX to the owner | Single-thread cooperative ownership, no generated RTE, partitions, ISR or preemption |
| OS / alarms | `os.c`: monotonic clock, 16-entry event/alarm metadata queue, due-time dispatch, bounded dispatch turn, generation cancel | One-shot alarms; caller drives dispatch; context lifetime belongs to caller; no threads or scheduler priority emulation |
| TcpIp | `tcpip.c`: nonblocking POSIX TCP listen/connect/accept/read/write, partial I/O, absolute deadlines, EOF and SIGPIPE handling | IPv4 loopback TCP; no full AUTOSAR socket API or UDP/IPv6 implementation |
| SoAd / PduR responsibility | `soad.c` frames test messages; `rte.c` binds the real `TcpTlsSession` lower TX C/S port before connect and supplies correlated TX completion; bounded RX copy is queued to owner | Project semantic envelope, not real socket-adapter routing tables or V2GTP/EXI mapping |
| Eth/EthIf/EthSM, modem, PLC | Existing provider callbacks synthesize modem-ready, SLAC matched, association and IPv6 readiness | No raw MME, Ethernet frames, modem or network-management emulation |
| SDP / SAP peers | Existing host fixture supplies endpoint and correlated SAP selection | Local injection; no UDP discovery or SAP EXI on the socket |
| CP/PP, BMS, OBC and actuator surroundings | Existing S/R providers supply qualified CP/PP and energy; host hardware adapter supplies measured-state fake | No electrical plant model; hardware remains reachable only through coordinator |
| Dem / Det responsibility | `services.c`: bounded 16-event diagnostic sink with overflow flag; runtime failure records project event 1 | Event 1 is a local test identifier; no imported DEM event IDs or DTC handling |
| NvM / MemIf / Fee responsibility | Four bounded 256-byte RAM blocks, exact-length read, unwritten-block rejection | RAM-only; resets on init, no persistence, redundancy or transaction emulation |
| Csm / Crypto / KeyM / HSM | Crypto job port returns `NOT_IMPLEMENTED`, never reports successful security evidence | No keys, certificates or crypto; TLS/PnC cannot be proven by these stubs |
| EXI generated implementation | Existing typed codec/schema seam retained | No generated backend supplied; semantic socket bridge intentionally bypasses EXI |

`Ov2g_ClockPort`, diagnostic and crypto port factories can be bound by host
consumers. Diagnostics are connected to runtime failure reporting. NvM and crypto
are independently tested service seams; current EIM logic has no credential/NvM
consumer to connect. They do not make disabled PKI/certificate functionality active.

## Executed flow and wire format

```text
EcuM-like init -> project RTE wiring -> CP/PP -> PLC readiness (injected)
  -> SDP/TCP readiness/SAP (injected) -> TakeProtocolRequest(SessionSetup)
  -> TcpTlsSession Send -> POSIX TCP -> external station
  -> correlated TX completion -> bounded TCP RX -> decode/validate
  -> OS event -> HLC owner -> coordinator HLC status
  -> refresh energy/hardware -> permission -> host hardware transfer -> stop
```

This is a **SessionSetup socket smoke test plus a host permission/hardware
exercise**, not a complete charging protocol session. The existing fixture
permits that host exercise before the complete cable-check/precharge protocol
exchange. Do not interpret its `host transfer=yes` message as peer readiness,
actual energy transfer, or complete DIN/ISO negotiation. Full semantic protocol
state-machine tests remain in `tests/unit/test_protocol_adapters.c`.

The fixed 24-byte test frame contains 8 ASCII bytes `OV2GST01`, followed by four
unsigned 32-bit big-endian integers: generation, correlation, project message
kind, accepted (0/1). Requests set accepted=0. No C structure is serialized.
The decoder validates magic/version, nonzero IDs, message range and boolean
range before publishing. The EVCC additionally matches all pending IDs and kind.
The peer returns deliberately fragmented bytes to exercise exact-length intake.
RX failure, rejection or mismatch stops the flow and revokes permission before
hardware/HLC/PLC teardown. The station intentionally implements no real EVSE.

Socket I/O consumes buffers synchronously under a finite deadline (500 ms per
EVCC exchange); this can block the cooperative caller until that deadline. No
periodic charging loop runs during this smoke exchange. For a continuous
energized-session simulator, integrate nonblocking I/O polling with periodic
coordinator ticks and evidence refresh before allowing sustained transfer.
The stub timeouts/capacities are host test settings, not normative timers,
safety deadlines, runnable mappings or formally frozen configuration.

## Verification

On 26/10/2026, Linux GCC 13.3 Debug with `-Wall -Wextra -Wpedantic -Werror`
built the portable core and Linux environment. CTest passed 19/19 executables,
including all 18 existing host tests and `openv2g_test_linux_bsw`.
The Linux test exercises OS due times, queue overflow, generation cancellation,
RAM block bounds, diagnostics overflow, fail-closed crypto, malformed framing,
socket timeout/partial EOF, fragmented loopback response, peer rejection,
stale generation and disconnect, and final permission DENY / cancelled events.
Loopback tests need local socket permission; the review sandbox blocked `send`,
so socket execution used the approved elevated test command.

Evidence is host-only. Supplier ABI, target scheduling/WCET, actual TLS,
protocol conformance, HIL and vehicle safety remain unverified.
