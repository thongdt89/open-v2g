# Current implementation architecture

Revision: Draft 0.1 — 26/10/2026. Source baseline: commit `70ced6a`.
Scope: current C11/Linux host implementation, including the opt-in generated
EXI candidate. No target AUTOSAR configuration or formal freeze is implied.

Legend: **HOST** = implemented project logic verified on host; **SIM** = host
surrounding model or service stub; **CANDIDATE** = pinned external generated
code, opt-in; **OPEN** = production integration/evidence unavailable.
Solid edges describe the current host paths. Dotted edges describe dependencies,
configuration or future integration, not implemented production wiring.

## 1. System and composition view

```mermaid
flowchart TB
    subgraph vehicle["EVCC Linux process — HOST / SIM"]
        direction TB
        inputs["SIM providers<br/>qualified CP/PP + energy snapshots"]
        plc["PlcLinkManager — HOST<br/>SIM modem / SLAC / IPv6 readiness"]

        subgraph cc["ChargingCoordinator — HOST"]
            csm["ChargeStateMachine<br/>charging progression owner"]
            pm["PermissionManager<br/>ChargePermission owner"]
            fault["FaultCoordinator<br/>blocking fault / latch"]
            bpt["BptCoordinator<br/>guards tested; BPT disabled"]
            fault --> pm
            bpt --> pm
            pm --> csm
        end

        subgraph hlc["ISO15118Chrg — HOST"]
            facade["HLC facade / supervisor<br/>status + queued events"]
            sap["SapNegotiator<br/>offered-schema selection"]
            factory["ProtocolAdapterFactory<br/>one active adapter"]
            adapters["Din70121Adapter / Iso15118_2Adapter<br/>DC EIM semantic state machines"]
            transport["SdpClient + TcpTlsSession<br/>V2GTP receive parser"]
            facade --> sap
            sap --> factory
            factory --> adapters
            facade <--> transport
        end

        hw["ChargeHardwareAdapter — SIM<br/>correlated measured-feedback fake"]
        runner["Linux wire runner — HOST<br/>pending context + DTO mapping<br/>nonblocking pump / supervision"]
        codec["Concrete ExiCodec + SchemaRegistry — HOST<br/>separate generated backends: CANDIDATE"]
        socket["POSIX TCP — HOST<br/>IPv4 loopback, plaintext"]
        services["Available Linux environment services<br/>RTE-style C/S + S/R wiring<br/>OS events / clock / Dem / RAM NvM stubs<br/>not all services bound; crypto unavailable"]

        inputs --> csm
        inputs --> pm
        csm -->|"start / stop"| plc
        plc -->|"readiness evidence"| csm
        csm -->|"start / stop HLC"| facade
        facade -->|"requests / status / evidence"| csm
        pm -->|"bounded permission"| adapters
        pm -->|"permission checked at enable"| hw
        csm -->|"prepare / enable / safe-state"| hw
        hw -->|"feedback"| csm
        facade <-->|"pending SAP / protocol DTOs<br/>decoded results"| runner
        runner <--> codec
        runner -->|"framed TX via TcpTlsSession"| socket
        socket -->|"RX through HLC generation / connection guard"| transport
        runner -->|"tick before renewal"| csm
        services -.-> runner
    end

    station["Station process — SIM<br/>SAP + DIN / ISO-2 EXI + V2GTP<br/>closed DC EIM peer profile"]
    socket <-->|"actual TCP bytes"| station
```

HLC supplies requests/evidence and consumes permission. It has no direct actuator
control path. The coordinator remains the owner of energy progression and stop.
The supporting PLC manager owns SLAC/link readiness; SDP belongs to HLC.

The diagram represents logical responsibilities. The current C implementation
combines facade/supervisor behavior in `iso15118_chrg.c`; it does not assert
separate generated SWCs, runnables or AUTOSAR partitions.

## 2. Actual generated-code and network path

```mermaid
flowchart TB
    subgraph app["HOST application / semantic owners"]
        owners["SapNegotiator / selected DIN or ISO-2 adapter"]
        context["Local pending context<br/>generation + connection + request ID<br/>offered wire-ID map / SessionID / EVSE ID / schedule ID"]
        owners <--> context
    end

    subgraph bridge["Linux runner: simulation/linux/wire/"]
        mapping["client.c / station.c<br/>project-neutral SAP / DC DTOs"]
        facade["codec.c<br/>explicit schema + expected message kind"]
        bounded["Concrete ExiCodec + SchemaRegistry<br/>bounded candidate / output / workspace"]
        mapping <--> facade
        facade <--> bounded
    end

    subgraph generated["Opt-in external libcbv2g candidate — pinned / hash checked"]
        sap["exi_cbv2g_sap.c<br/>appHand generated model"]
        din["exi_cbv2g_din.c<br/>DIN generated model"]
        iso["exi_cbv2g_iso2.c<br/>ISO-2 generated model"]
    end

    tx["V2GTP encode<br/>TcpTlsSession.Send<br/>bounded copying TX queue"]
    tcp["io.c / POSIX socket<br/>nonblocking send / recv / poll<br/>correlated TX completion"]
    rx["ISO15118Chrg.OnTransportBytes<br/>generation + connection validation<br/>incremental V2GTP RX framing"]
    peer["Separate station process<br/>actual EXI payloads inside V2GTP"]

    context <--> mapping
    bounded <--> sap
    bounded <--> din
    bounded <--> iso
    facade -->|"encoded EXI"| tx
    tx --> tcp
    tcp <-->|"loopback TCP"| peer
    tcp --> rx
    rx -->|"bounded immutable payload view"| facade
```

Generated types stay private to their schema-specific adapters. The runner binds
an actual codec/registry alongside the core facade; the facade's internal
`exi_codec` / `schema_registry` fields still represent lifecycle placeholders.
The concrete backend is not a supplier-generated AUTOSAR service.

Generation, connection and request IDs are local metadata, not custom additions
to standardized payloads. One request is outstanding; decoded kind and SessionID
are checked before forwarding to the owner. SAP wire SchemaID is explicitly
mapped to an offered project schema, never cast to `OpenV2g_SchemaId`.

## 3. Runtime progression and stopping

```mermaid
sequenceDiagram
    participant Env as SIM CP/PP / energy / link
    participant CC as ChargingCoordinator
    participant HLC as HLC / selected protocol owner
    participant IO as Wire runner + EXI/V2GTP + TCP
    participant EVSE as SIM station process
    participant HW as SIM hardware adapter

    Env->>CC: qualified connect + current-generation evidence
    CC->>HLC: start after simulated PLC readiness
    Note over HLC,IO: SDP endpoint / network readiness injected; no real UDP SDP or PLC
    HLC->>IO: pending SAP offer snapshot
    IO->>EVSE: SAP request in EXI/V2GTP
    EVSE-->>IO: selected wire SchemaID
    IO->>HLC: validated offered-schema result
    HLC->>IO: setup / service / EIM auth / parameters
    IO->>EVSE: generated DIN or ISO-2 messages
    EVSE-->>IO: generated responses
    IO->>HLC: checked kind / session / pending context
    HLC-->>CC: status and semantic evidence
    HLC->>IO: cable check / precharge
    CC->>CC: evaluate bounded permission
    CC->>HW: prepare with host measured feedback
    HLC->>IO: PowerDelivery START
    IO->>EVSE: generated request
    EVSE-->>IO: generated response
    CC->>HW: permission-gated enable
    loop Three host current-demand exchanges
        HLC->>IO: pending CurrentDemand request
        IO->>EVSE: EXI/V2GTP request
        Note over CC,IO: Nonblocking wait: coordinator tick before permission renewal; refresh mock evidence
        EVSE-->>IO: EXI/V2GTP response
        IO->>HLC: validated response and local correlation
    end
    CC->>CC: BeginProtocolStop — publish DENY
    CC->>HW: request safe state; check host safe / zero-current feedback
    HLC->>IO: PowerDelivery STOP / WeldingDetection / SessionStop
    IO->>EVSE: bounded generated protocol-stop exchanges
    CC->>HLC: final teardown; cancel protocol / transport
    CC->>Env: stop PLC link
    Note over CC,HW: Fault / timeout / stale evidence invokes full coordinated stop directly
```

This is a host control sequence, not an approved physical electrical stop
sequence. Poll periods, freshness limits and timeout values are provisional host
settings. A late owner turn cannot renew an already expired permission.

## 4. Available verification and production gaps

```mermaid
flowchart LR
    host["HOST core / stubs / generated-wire runner"]
    tests["24 ordinary checks with oracle<br/>23 ASan / UBSan / LSan checks<br/>19 baseline checks without candidate"]
    corpus["Candidate parser tests<br/>truncation / mutation / malformed inputs<br/>12 socket fault scenarios"]
    oracle["Separate test-only OpenV2G process<br/>50 byte-identical decode / re-encode comparisons"]
    ci["CI jobs configured<br/>remote run not claimed"]
    target["OPEN production boundary<br/>supplier ARXML / RTE / BSW / drivers<br/>licensed schema and generator provenance<br/>HARA / physical timing / target resources"]
    disabled["Disabled / unavailable<br/>PnC / ISO-20 / BPT / real TLS / PKI"]

    tests --> host
    corpus --> host
    oracle --> host
    ci -.-> tests
    host -.->|"requires external evidence and integration"| target
    disabled -.-> target
```

The oracle is a separate test executable, not an application dependency. Its
comparisons provide differential evidence, not independently authored normative
golden vectors or physical interoperability proof.

## Source mapping

| Diagram element | Current implementation |
|---|---|
| Coordinator owners | `src/charging_coordinator/` |
| HLC facade/supervisor and semantic protocol owners | `src/iso15118_chrg/` |
| CP/PP, energy, PLC and host hardware boundaries | `src/support/` |
| Composition fixture / typed port wiring | `simulation/src/host_simulation.c` |
| Linux service stubs and custom-frame smoke example | `simulation/linux/src/` |
| Generated-wire host application and TCP pump | `simulation/linux/wire/` |
| Concrete schema-specific generated-code adapters | `integration/protocol/adapters/cbv2g/` |
| Candidate source pin / selected-source checksums | `integration/protocol/generated/libcbv2g/` |
| Independent differential oracle and fixed references | `tests/oracle/` |
| Target AUTOSAR / supplier boundary | `integration/autosar/` — blocked for target |

[Current evidence and closure inventory](EXI_WIRE_IMPLEMENTATION_STATUS.md) ·
[Wire build/run guide](../../simulation/linux/wire/README.md)
