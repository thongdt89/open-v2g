# PEV-SLAC owner and typed host peer

Draft 0.1 — 31/10/2026. Implemented portable C owner and deterministic surrounding
peer/key-reference simulation. This is not raw Ethernet/HomePlug MME encoding,
a modem driver, physical attenuation evidence or protocol conformance.

`PlcLinkManager` now owns `PevSlac`. Binding is optional while the link is DOWN;
legacy semantic tests remain unbound. Every actual EXI runner configuration
explicitly enables the typed peer before SDP. The matching owner exchanges
parameter request/confirmation, configured start indications and sound sequence,
attenuation indication/response and match request/confirmation. Association and
IPv6 readiness remain separate PlcLinkManager operations after matching. SDP
remains owned by ISO15118Chrg and starts only after complete link readiness.

The bounded PROJECT modem seam carries an eight-byte RunID, source/destination
MACs, generation/freshness metadata, sound counters, up to 58 attenuation groups,
and an opaque installed-key reference plus public NID. It never carries raw NMK
bytes. These enum values are project message kinds, not HomePlug MMTYPE constants.
The platform must bind incoming metadata to the captured receive generation,
without relabeling late frames as belonging to the current session.

- RX callbacks validate basic bounds and enqueue owned copies into eight slots.
  They do not change matching state or link readiness. Overflow is latched and
  causes failure on the next owner turn. Owner/ingress calls must be serialized;
  supplier exclusive areas and real concurrent callback binding are not provided.
- Owner turns validate generation, RunID, selected peer, destination, quality,
  freshness, stage, sound/group counts and attenuation policy. At most one queued
  frame or one outbound frame is processed per turn. Unexpected/late frames
  cannot advance a new generation or revive a cancelled session.
- A single total budget caps all stage deadlines. Parameter/match retransmissions
  retain the RunID and original deadlines; attempts and intervals are bounded.
  Clock rewind, timeout, rejection and invalid key references fail closed.
- Only a validated current installed key reference can produce MATCHED.
  Association/IPv6 callbacks recheck it, and the EXI supervisor keeps checking it
  while the link is established. Matching never directly grants ChargePermission.
- Established-link failure invalidates the matching proof and link evidence
  immediately. Provider teardown is deferred to coordinated Stop after permission
  revocation. Matching-stage failure can cancel its job because no energy
  authority exists. Stop/cancel is idempotent; a stale-generation cancel cannot
  affect a new run. Reusing the preceding RunID is rejected.

The host peer verifies outbound sequence/counters, produces a bounded attenuation
sample and models installed key handle/version liveness. A same-run invalidated
key cannot be reinstalled by a replayed match request. New generations use new
RunIDs and matching/association again, including pause/resume. Its fixed key
handle/NID and deterministic RunIDs are fixture data, not provisioning or a
cryptographic nonce policy. No private key material is printed or persisted.

The provisional fixture sends three start indications and ten sounds, with 58
groups, attenuation value 7 and configurable acceptance bound 30. Stage/total
budgets are 250 ms/1 s, retry interval 50 ms and at most three transmissions.
These are explicit test configuration choices, not normative timers, modem
capabilities or vehicle safety thresholds. The model has one candidate peer and
no physical pacing between repeated owner turns; raw MME framing, PHY timing,
multi-candidate channel qualification and modem association/IP acquisition need
their actual platform/profile implementation. No cached measurement is restamped
by the matching monitor merely to keep a long charging session alive.

`openv2g_test_pev_slac` exercises copied ingress, matching, noisy/rejected peers,
dropped-request recovery, bounded retries, total/stage expiry, overflow, old
RunID/generation/cancel, key invalidation/replay and clock rewind. It checks the
optional NULL-config initialization path. All socket flows now run the typed
matching exchange first; a link-key-loss scenario during energy start verifies
DENY and measured simulated safe hardware before teardown. See
[verification evidence](../../docs/architecture/evidence/slac_host_verification.json).
