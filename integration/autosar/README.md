# AUTOSAR integration boundary

Status: `BLOCKED_FOR_TARGET` under the provisional baseline.

This directory is reserved for selected AUTOSAR Classic and supplier artifacts or handwritten adapters around them. Do not add copied or guessed standardized layouts.

Required inputs before implementation:

- selected AUTOSAR Classic release and ECU extract;
- supplier `ISO15118Chrg` ARXML and generated headers;
- operation-by-operation `ChargeControl`/`ChargeParameters` signature mapping;
- generated TcpIp/SoAd/PduR, Csm/Crypto/KeyM, NvM and Dem APIs;
- runnable, event, task, queue, exclusive-area and timing configuration;
- checksums and compatibility evidence.

Target adapters shall translate supplier types at this boundary. Supplier headers must not leak into the host-domain core.
