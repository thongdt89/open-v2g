# Host surrounding-component simulation

Status: `HOST_SIMULATION_ONLY`. This directory does not contain supplier
AUTOSAR ABI, generated RTE/BSW bindings, licensed EXI schemas or physical
actuator models.

`openv2g_host_sim` assembles the existing project-side components behind their
C/S and S/R port contracts and exposes controlled environment inputs for:

- qualified CP/PP connection data;
- BMS/OBC-style energy snapshots;
- PLC modem, PEV-SLAC, association and IPv6-ready callbacks;
- SDP endpoint, TCP readiness and SAP protocol selection;
- correlated EVSE protocol responses;
- host-fake hardware safe, prepare and energize feedback.

Explicit link-loss and measured-hardware-feedback injection APIs support
negative flow testing without bypassing the corresponding component owner.

The simulator preserves `sessionGeneration`, request correlation, bounded
storage and the coordinator-owned permission/stop sequence. Tests can modify a
default CP/PP or energy sample before injection to cover stale generation,
invalid quality, missing availability bits, interlock faults and conflicting
limits.

The `openv2g_flow_simulator` example drives a nominal ISO 15118-2 DC EIM host
flow and prints the ordered operation trace. It is a deterministic architecture
test aid, not a protocol-conformance or electrical plant simulator.

## Windows GCC build

Install a 64-bit MinGW-w64 or MSYS2 GCC toolchain with either Ninja or
`mingw32-make`, then run from PowerShell:

```powershell
.\scripts\build_gcc_windows.ps1
```

If local policy blocks unsigned PowerShell scripts, invoke it explicitly for
this process:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build_gcc_windows.ps1
```

The script locates GCC/CMake, configures a separate `build-gcc-windows`
directory, enables strict warnings, builds core/simulation/examples/tests and
runs CTest. Override the build directory or configuration when needed:

```powershell
.\scripts\build_gcc_windows.ps1 -BuildDirectory out\gcc-debug -BuildType Debug
```
