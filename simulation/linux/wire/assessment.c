#include "wire.h"
#include <string.h>
/* This surrounding provider is a deterministic host plant assessment. Exact
 * equality/zero are fixture criteria, not vehicle safety tolerances. Real
 * deployments supply a separately reviewed assessment port. */
static OpenV2g_Result assess(void *context, OpenV2g_AssessmentKind kind,
    uint32_t generation, uint32_t correlation_id, OpenV2g_TimeUs now,
    uint32_t target_voltage_mv, uint32_t peer_voltage_mv,
    const OpenV2g_ChargeHardwareStatus *hardware, OpenV2g_PhysicalAssessment *evidence)
{
    (void)context;
    if (hardware == NULL || evidence == NULL || correlation_id == 0u || generation == 0u ||
        hardware->env.session_generation != generation || hardware->env.quality != OPENV2G_QUALITY_VALID) {
        return OPENV2G_RESULT_REJECTED;
    }
    (void)memset(evidence, 0, sizeof(*evidence));
    evidence->env.session_generation = generation; evidence->env.timestamp = now;
    evidence->env.sequence_counter = correlation_id; evidence->env.quality = OPENV2G_QUALITY_VALID;
    evidence->kind = kind; evidence->correlation_id = correlation_id;
    evidence->hardware_sequence = hardware->env.sequence_counter;
    if (kind == OPENV2G_ASSESS_PRECHARGE) {
        evidence->passed = target_voltage_mv != 0u && peer_voltage_mv == target_voltage_mv &&
            hardware->state == OPENV2G_HW_READY;
    } else if (kind == OPENV2G_ASSESS_AC_READINESS) {
        /* Deterministic host OBC readiness; real vehicle supplies assessment. */
        evidence->passed = hardware->state == OPENV2G_HW_READY && hardware->connector_locked &&
            !hardware->positive_contactor_closed && !hardware->negative_contactor_closed && hardware->measured_current_ma == 0;
    } else if (kind == OPENV2G_ASSESS_WELDING) {
        evidence->passed = peer_voltage_mv == 0u && hardware->measured_voltage_mv == 0u &&
            hardware->measured_current_ma == 0 && hardware->state == OPENV2G_HW_SAFE &&
            !hardware->positive_contactor_closed && !hardware->negative_contactor_closed;
    } else { return OPENV2G_RESULT_REJECTED; }
    return OPENV2G_RESULT_OK;
}
OpenV2g_PhysicalAssessmentCsPort OvWireHostAssessmentPort(void)
{
    OpenV2g_PhysicalAssessmentCsPort port = {NULL, assess}; return port;
}
