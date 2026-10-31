# IPv6 SDP host provider

Revision: Draft 0.1 — 26/10/2026. Implemented/tested host fixture.

`openv2g/linux/sdp6.h` provides a nonblocking IPv6 UDP discovery job around the
existing `SdpClient` owner. The owner starts its request; `Ov2g_Sdp6Begin` sends the
bounded packet and marks it sent. The scheduler calls `Ov2g_Sdp6Pump` without
blocking. Completion validates owner generation, request ID, state, deadline,
UDP source address/port, exact V2GTP type/length, transport and configured security
policy before publishing the endpoint. Cancellation closes the socket, is
idempotent and cannot cancel a newer owner request.

The separately callable server fixture answers one packet per turn. Both ends
bind only `::1`. `Ov2g_Tcp6Listen` and `Ov2g_Tcp6Connect` provide raw IPv6 loopback
TCP; tests discover an actual listener, connect and exchange a byte.

Wire encoding uses V2GTP request `0x9000`/2 bytes and response `0x9001`/20 bytes;
the response carries 16-byte IPv6 address, network-order port, security and TCP
selection. Field layout/security values were cross-checked against
[the public ISO 15118-2:2014 copy, §7.10.1/Table 16](https://downloads.regulations.gov/FHWA-2022-0008-0405/attachment_2.pdf).
This reference does not select a production edition or establish conformance.

Security policy is strict equality: the fixture rejects a changed security bit
rather than upgrading/downgrading policy automatically. SDP carries only a TLS
required/not-required bit, not a negotiated TLS version. TLS12/TLS13 endpoint
metadata retain caller configuration; these tests perform **no TLS handshake**.

`openv2g_test_linux_sdp6` covers plain and TLS-bit discovery, real IPv6 UDP/TCP,
exact-size/truncation/oversize checks, malformed header/address/transport,
security mismatch, timeout, cancel, and generation replacement. Its short loop
is a test harness; production scheduling remains owner/supplier configuration.

## Generated-EXI runner integration

`station6`/`evcc6` integrate the provider with both DIN and ISO-2 sessions. The
EVCC first establishes simulated link readiness, then sends UDP discovery while
continuing coordinator supervision. Its completion callback enters the HLC facade
with the validated endpoint; the actual IPv6 TCP connection follows discovery.
The existing transport-ready/generation path then starts SAP and protocol EXI.
Owned discovery/TCP descriptors are closed on stop/error. Legacy `station`/`evcc`
retain the earlier IPv4 injected-endpoint path.

```sh
./build-exi/simulation/linux/wire/openv2g_linux_exi station6 15118 iso2
./build-exi/simulation/linux/wire/openv2g_linux_exi evcc6 15118 iso2
```

Use separate terminals, station first; replace `iso2` with `din` in both commands
for DIN. The CLI uses the same numeric UDP and TCP port on `::1`; tests also
exercise discovery of a different ephemeral TCP port. These are real UDP/TCP
sockets, with simulated CP/PLC/hardware. Remaining network work includes multicast/
link-local interface selection, retry/processing budgets, genuine TLS/credentials
and PLC integration. The provider is a unicast loopback fixture and does not
claim on-cable discovery or PLC/SLAC support.

## Scoped host extension — 31/10/2026

The provider now additionally accepts explicitly scoped link-local unicast and
`ff02::1` all-nodes multicast discovery destinations. A nonzero `interface_id`
is mandatory for those addresses; arbitrary global/multicast destinations are
rejected. `Ov2g_Sdp6ListenScoped` binds a UDP listener and can join the all-nodes
group on that interface. Responses must advertise a link-local address matching
the responding peer, retain the selected scope and requested security, and arrive
on the selected kernel ingress interface (`IPV6_PKTINFO`). A payload cannot invent
its ingress interface. Loopback remains available for deterministic tests.

`Ov2g_Sdp6ConfigureRetries` selects one through eight total sends and a nonzero
interval before Begin. Retries keep the original generation/request and aggregate
deadline; exhaustion never resets that deadline. Clock rewind, cancel/new owner
request, bad packet/source/scope/security and timeout fail closed. Default remains
one send. `OvWireClient_SetDiscoveryEndpoint` exposes destination and bounded retry
configuration to the EXI runner before its first generation. Explicit TLS config
continues to choose requested SDP security; no plaintext retry is introduced.

`Ov2g_Tcp6Connect` and `Ov2g_Tcp6ListenEndpoint` preserve the interface scope for
link-local TCP. They accept loopback or scoped link-local endpoints; global and
unscoped link-local endpoints are rejected. Caller-provided interface, listening
address and route must actually exist in the host network.

Tests join the IPv6 group on `lo`, verify kernel ingress scope with real UDP,
drop an initial request and recover on retry, exhaust the bounded send count,
check the aggregate timeout and validate multicast/link-local response payloads.
Managed TLS receipt sessions also use the runner's scoped loopback configuration.
These tests do **not** establish real ff02 multicast delivery to a physical
interface or actual fe80 TCP connectivity: this environment provides only the
loopback fixture. Those paths are implemented but real interface/route execution
remains unverified. No modem/SLAC responsibility moves into SdpClient.
