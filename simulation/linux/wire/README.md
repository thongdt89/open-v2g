# Linux V2GTP/EXI DC EIM host sessions

Revision: Draft 0.1 — 26/10/2026. Status: `IMPLEMENTED_HOST`, `VERIFIED_HOST`.
This extends the custom-frame smoke environment with separate EVCC and station
processes exchanging actual generated SAP, DIN and ISO-2 EXI inside V2GTP.
It is an executable development profile, not a compliant EVSE or production EVCC.

## Build and run

Use the pinned external libcbv2g checkout described in
[the adapter guide](../../../integration/protocol/adapters/cbv2g/README.md).
The legacy `OPENV2G_ENABLE_CBV2G_SAP_POC` option now selects the three separate
SAP/DIN/ISO-2 backends; it remains OFF by default for compatibility.

```sh
cmake -S . -B build-exi -DCMAKE_BUILD_TYPE=Debug \
  -DOPENV2G_ENABLE_CBV2G_SAP_POC=ON \
  -DOPENV2G_CBV2G_SOURCE_DIR="$PWD/out/deps/libcbv2g"
cmake --build build-exi --parallel
ctest --test-dir build-exi --output-on-failure
```

Start these in separate terminals, station first (within its 30-second accept
window):

```sh
./build-exi/simulation/linux/wire/openv2g_linux_exi station 15118 iso2
./build-exi/simulation/linux/wire/openv2g_linux_exi evcc 15118 iso2
```

Replace `iso2` with `din` in **both** terminals for the DIN run. These are real
loopback TCP processes, with one connection and one session per invocation.
The original `openv2g_linux_stub` example is retained as a custom-frame smoke
fixture. `openv2g_linux_exi` is the new generated-wire executable.

## Data path and ownership

```text
qualified CP/PP -> coordinator -> simulated PLC/link readiness
  -> injected SDP endpoint -> actual TCP socket + correlated transport-ready
  -> SapNegotiator request -> project SAP mapper -> EXI -> V2GTP -> station
  <- recorded wire SchemaID selection <- SAP EXI <- V2GTP
  -> selected DIN or ISO-2 semantic owner
  -> typed neutral DTO -> separate generated schema backend -> V2GTP -> station
  <- TcpIp RX -> HLC connection/generation checks -> V2GTP frame
  <- bounded EXI candidate -> validate kind + session -> local pending correlation
  -> HLC protocol owner -> status/events -> coordinator-owned permission/hardware
```

Generated protocol structs never cross project ports. SAP offers keep an explicit
wire-ID-to-offered-schema table; wire IDs are never cast to project schema IDs.
Only one request is outstanding. Pending generation, connection and request ID
are retained locally: they are not added to standardized EXI payloads. SessionID
and response kind are validated before the semantic owner is called. A
PowerDelivery response has no START/STOP discriminator; the pending request
provides that context and both use the shared generated PowerDelivery response.
Unsupported or unsolicited/coalesced extra frames fail closed.

`TcpTlsSession` TX uses a bounded copying lower port. The nonblocking pump reports
correlated completion only after all bytes are sent. RX passes through the
existing `ISO15118Chrg_OnTransportBytes` connection/generation guard and framing
port. Bounded immutable frame data is decoded before releasing its storage.
The single cooperative owner calls coordinator ticks on every polling turn;
`poll` requests a maximum 1-ms wait. CP/energy providers issue explicit fresh
mock samples every 10 ms. Permission expiry is checked **before** renewal.
Cached hardware/link/peer evidence is not restamped as newly measured data.
OS scheduling jitter, decoder CPU time and WCET are not proven by the poll period.

On a normal stop, the coordinator's new `BeginProtocolStop` operation publishes
DENY and requests the host hardware safe state while retaining HLC/PLC for the
bounded PowerDeliveryStop/WeldingDetection/SessionStop tail. The host checks
safe/current-zero feedback before continuing. Permission cannot be renewed in
STOPPING. Any timeout/fault invokes the existing full coordinated teardown.
This host sequence is not a vehicle electrical safe-stop specification.

## Executed profile

Both schemas run:

1. SAP negotiation.
2. SessionSetup, ServiceDiscovery, external-payment selection and EIM authorization.
3. ChargeParameterDiscovery, CableCheck and PreCharge.
4. PowerDelivery START and three CurrentDemand exchanges.
5. Permission revoke / host safe-state, PowerDelivery STOP, WeldingDetection and SessionStop.

The three-loop budget is local application policy, not a peer wire flag.
Received service ID, schedule tuple ID and session ID are retained. Energy
limits are intersected with peer maxima and the schedule power before further
requests. Physical values use checked signed value/multiplier/unit conversion;
nonrepresentable values and unsupported units are rejected instead of rounded.

Deliberate closed-profile settings:

- DIN namespace `urn:din:70121:2012:MsgDef`; ISO-2 namespace
  `urn:iso:15118:2:2013:MsgDef`; host SAP version 2.0 for both. These are explicit
  test configuration, not a frozen normative edition or compatibility claim.
- Plain TCP only. SDP, PLC/SLAC and electrical surroundings are injected; no
  UDP discovery, actual PLC modem, TLS or PnC is provided by this executable.
- One DC-core charge service, external payment, one flat schedule tuple/interval,
  no tariffs, signatures, metering receipts, AC, ISO-20 or BPT. Broader valid
  protocol features are outside this profile and must not be inferred as supported.
- Host station supplies 500 V / 100 A / 30 kW limits and a 30-kW flat schedule.
  Requests use test energy-provider data; this is not a physical precharge model.
- 4096-byte EXI payload bound, 4097-byte guarded decode workspace; one bounded
  TX frame. Request timeout 500 ms, mock energy max age 50 ms, permission TTL
  100 ms, provider refresh 10 ms. These are **host settings**, not standard timers
  or safety allocations. A long session can stop on unchanged link/hardware
  evidence becoming stale; no fresh sample is fabricated to hide that condition.
- Fail-closed bounds/unsupported branches are a development profile, not proof
  that all valid DIN/ISO messages interoperate. Generated grammar/schema
  provenance and production redistribution approvals remain open.

## Verification and remaining gates

`openv2g_test_linux_wire` uses ephemeral loopback ports and separate station
processes for both nominal schema sessions and faults: unoffered SAP ID,
malformed frame, request timeout, disconnect, wrong SessionID, negative response,
wrong response kind, stale energy while waiting, replayed SAP during a protocol exchange, wrong schedule/EVSE identity, and
wrong-schema EXI. It verifies final permission DENY and cancelled TX/events.
The stale-energy case reaches host energy transfer first; supervision stops it
while socket reception remains pending. Existing host-simulation tests cover
idempotent protocol stop, retained HLC/PLC, rejected permission renewal and
ordered final teardown.

The full local suite passes 24/24 with the optional external oracle configured;
23/23 without it. All 23 candidate tests pass with ASan/UBSan/LSan after the
expanded malformed corpus. Remote CI is configured; no remote run is claimed.
See [verification/provenance record](../../../docs/architecture/EXI_WIRE_IMPLEMENTATION_STATUS.md)
for evidence and external closure requirements.

## Feature increment — Draft 0.2, 26/10/2026

The current normal session adds ServiceDetail after discovery: **15 exchanges**
after SAP, with three CurrentDemand loops. Pending-CPD regression adds a repeated
CPD exchange. Separate DIN/ISO-2 mapping now retains bounded multi-interval
schedules and minimum/dynamic limits. The host intersects reductions and clamps
subsequent targets against current and power limits. SAP minor deviation is an
explicit compatibility policy (`allow_sap_minor_deviation`).

Wire sessions require a coordinator-owned physical assessment before transfer.
The surrounding assessment provider is a deterministic fixture: nonzero target
matching peer precharge voltage plus READY state; zero peer/local voltage/current
and open contactors for welding. It is not an independent inlet/battery voltage
measurement model or an approved vehicle safety decision. Assessment evidence
must match generation, correlation, freshness and current hardware sequence.
Legacy semantic fixtures leave this new opt-in gate disabled.

ServiceDetail currently accepts up to five parameter sets, each containing one
integer or string parameter. ISO-2 pause/resume and customer-waiting fields are
preserved by the mapper; retained-session resume is not implemented. Full scope
and remaining work: [feature work log](../../../docs/architecture/FEATURE_COMPLETION_WORKLOG.md).

For GCC line/branch coverage configure `-DOPENV2G_ENABLE_COVERAGE=ON`, build and
run CTest, then run:

```sh
python3 tools/coverage_report.py build-exi --output /tmp/openv2g-coverage.json
```

Use a fresh build directory for each measurement to avoid accumulating prior
execution counters. Coverage percentages do not establish protocol conformance.

## Real IPv6 SDP path — Draft 0.3, 26/10/2026

`station6 PORT din|iso2` and `evcc6 PORT din|iso2` add real unicast IPv6 UDP
SDP before IPv6 TCP connection and the same generated-EXI flow. This removes
endpoint injection for the IPv6 path. Cooperative coordinator supervision also
runs while discovery is pending. The CLI uses UDP and TCP on `::1:PORT`; the
station advertises its TCP endpoint. [Detailed provider/run guide](../SDP_IPV6.md).
Both protocols have complete IPv6 discovery/socket regression sessions, in
addition to the legacy IPv4/fault scenarios (35 wire sessions total).
Multicast/link-local, physical PLC and actual TLS remain open.
