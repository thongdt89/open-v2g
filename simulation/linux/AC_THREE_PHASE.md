# Balanced three-phase ISO-2 host profile

Draft 0.1 — 31/10/2026. Explicit host simulation; no actual OBC/phase measurement,
vehicle capability allocation, normative policy or conformance claim.

The `iso2-ac3` CLI profile exchanges actual ISO-2 AC three-phase mode selection,
charge parameters, PowerDelivery, ChargingStatus and controlled stopping over
IPv4 or discovered IPv6 TCP, with optional TLS1.2/1.3. Existing `iso2-ac` remains
single-phase. Example station/client pair:

```sh
./build-exi/simulation/linux/wire/openv2g_linux_exi station6 15118 iso2-ac3
./build-exi/simulation/linux/wire/openv2g_linux_exi evcc6 15118 iso2-ac3
```

`OvWireClient_SetAcPhaseLimits` explicitly selects one or three phases before
the first generation and copies three surrounding-component current caps. The
provisional host range is 6000..32000 mA per active phase; inactive phases must
have zero capacity. This fixture range is not a frozen vehicle or standard value.
Two phases, invalid capacities and changes during a generation are rejected.
Balanced operation uses the minimum active-phase cap, preserving each individual
constraint without inventing phase-specific fields in the ISO-2 schema. It does
not implement asymmetric phase dispatch.

The project energy envelope now carries `ac_phases` (0 legacy/one, 1 or 3).
AC voltage is phase-to-neutral and current is per phase. Power limits remain total
vehicle watts. Permission carries the same project phase metadata. Imported
AUTOSAR types and generated protocol types are unchanged. Permission rejects
invalid phase counts and three-phase metadata on a DC selection; a three-phase
permission additionally needs a qualified target-voltage sample. It caps each
phase's current by total intersected power divided by phase voltage and phase
count. HLC target projection checks phase consistency and uses the same total
power convention without changing the original vehicle snapshot. Products are
computed without overflowing int64 at full uint32/int32 input bounds.

The generated mapper serializes phase count only where ISO-2 carries a transfer
mode: the offered ServiceDiscovery mode and CPD request. AC CPD responses contain
a per-phase scalar current and nominal voltage, without a phase count. The
consumer derives total power using the validated session mode. It never treats
that absent field as a newly negotiated mode. An unoffered/mismatched phase mode
stops before energy enable. Nominal voltage, phase balance, power factor and OBC
qualification remain explicit host assumptions, not inferred physical evidence.

Tests cover three-phase round trips/truncation, invalid phase encoding, real
unequal-cap socket charging, mismatched mode rejection, per-phase permission
projection from a total 10 kW cap, phase mismatch and full-width integer bounds.
Two added EXI cases independently decode/re-encode identically with the pinned
separate oracle; all previous 25 feature references and 50 baseline references
remain unchanged. Four TLS CLI pairs now cover DC, single-phase AC and three-phase
AC over IPv4/IPv6. Counts and hashes are in
[verification evidence](../../docs/architecture/evidence/ac_three_phase_verification.json).
