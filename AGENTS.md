# V2G EVCC AUTOSAR — Instructions for Codex

## Read first

1. `PROJECT_CONTEXT.md`
2. `DECISIONS.md`
3. `docs/INDEX.md`
4. The relevant sections of `docs/EVCC_AUTOSAR_Two_Main_Compositions_Atomic_Interface_Spec_VI.md` (document Revision 3).
5. `NEXT_STEPS.md` when asked to continue without a more specific task.

These instructions describe the exported V2G project context. Follow the user's current task and preserve applicable existing repository instructions.

## Scope and evidence

- Target: vehicle-side EVCC on AUTOSAR Classic. Baseline rollout: DIN SPEC 70121 and ISO 15118-2 EIM; later ISO-2 PnC, ISO 15118-20 and AC/DC BPT.
- Produce Markdown for specifications and retain technical names and existing requirement/interface IDs.
- Distinguish documented design intent, proposed changes, implemented behavior, verified results and formally frozen artifacts.
- This package includes no production repository, frozen supplier ARXML, licensed standards package, target build configuration or test execution evidence. Do not imply otherwise.
- Source references to standards or upstream repositories do not establish a selected supplier baseline or current upstream support. Verify exact editions and repository commits when a task depends on them.
- Do not invent approval, owner names, requirement IDs from unavailable systems, test results, normative timer values or safety allocations.

## Architecture invariants from the source baseline

- The two main compositions are `ChargingCoordinator` and `ISO15118Chrg`.
- `ChargeStateMachine` owns vehicle charging progression. `PermissionManager` within `ChargingCoordinator` owns `ChargePermission`.
- HLC outputs are requests/evidence. No direct HLC-to-contactor, lock, precharge, converter or discharge command path.
- `PlcLinkManager` owns PEV-SLAC and link readiness. `ISO15118Chrg::SdpClient` owns SDP. Do not conflate them.
- Preserve one writer per state domain and one owner per timer; callbacks copy bounded data and notify the owner runnable.
- Validate generation, freshness, quality, correlation IDs and buffer lifetime on asynchronous paths. Stop/cancel is idempotent; stale-generation callbacks cannot revive sessions.
- Permission expiry or invalid required evidence means DENY. Revoke permission before coordinated safe-state action and HLC/PLC teardown; exact physical sequencing and deadlines require the vehicle safety concept.
- BPT reversal needs measured zero confirmation and a new permission; protocol capability is not proof of vehicle powertrain capability.
- Use explicit schema selection for SAP, DIN, ISO-2, ISO-20 Common/AC/DC. A common EXI service may host separate generated schema implementations.

## Interfaces and datatypes

- One row per S/R DataElement, C/S Operation, callback or local operation.
- State perspective, semantic producer/consumer, parameter directions, type, trigger, freshness, queue and failure semantics explicitly.
- The source contains P/R ambiguities for some C/S caller rows. Resolve against actual server ownership and generated ARXML before generation; do not blindly transform the tables into RTE ports.
- Preserve standardized `ChargeControl` / `ChargeParameters` ABI. Keep project metadata in the project facade/envelopes; do not extend imported AUTOSAR types.
- Distinguish `PROJECT`, `AUTOSAR_IMPORTED`, `BSW_IMPORTED`, `PROTOCOL_GENERATED`. Import exact supplier-generated signatures when available.
- Check units, sign conventions, invalid/init values, nested type resolution and bounded capacities. Catalogue presence does not prove buildable ARXML/C types.

## Working and verification

- Use the latest atomic specification for the two main compositions; consult the detailed composition document for behavior and the architecture document for supporting modules. Report conflicts instead of silently merging incompatible definitions.
- Preserve source snapshots under `docs/` and originals under `references/` during a read-only handoff review. For requested edits, make changes traceable with revision notes.
- SwitchEV/EVerest references are supporting research/test references, not automatic production AUTOSAR implementations or normative authorities.
- No build/test commands have been established in this package. Inspect the actual repository/toolchain before selecting commands. Clearly label host mocks and provisional configurations.
- For implementation, prioritize meaningful tests for stale/late callbacks, cancel races, permission expiry, queue overflow, protocol/schema mismatch and bounded parsers, according to the changed scope.
- Continue work that can be completed with available information. Keep missing external decisions explicit rather than silently freezing arbitrary values.
