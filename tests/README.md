# Host/SIL tests

The tests in this directory exercise provisional logical behavior and hard architecture invariants. They use no supplier, AUTOSAR, network, crypto, PLC or actuator implementation.

Evidence classification: `VERIFIED_HOST` only.

Priority extensions:

- cancel during SDP/TCP/TLS/SAP;
- late/reordered callback after reconnect;
- queue overflow and status resynchronization;
- fragmented/coalesced V2GTP stream input;
- malformed or unoffered schema;
- permission expiry during every energy-related state;
- hardware command/result mismatch;
- BPT measured-zero/new-permission checks while feature remains disabled.
