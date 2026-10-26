#ifndef OPENV2G_PHYSICAL_ASSESSMENT_H
#define OPENV2G_PHYSICAL_ASSESSMENT_H
#include "openv2g/common/types.h"
#include "openv2g/common/result.h"
/* PROJECT evidence; thresholds are caller/vehicle configuration, never normative defaults. */
typedef enum { OPENV2G_ASSESS_PRECHARGE = 1, OPENV2G_ASSESS_WELDING = 2 } OpenV2g_AssessmentKind;
typedef struct {
    OpenV2g_CommEnvelope env;
    OpenV2g_AssessmentKind kind;
    uint32_t correlation_id;
    uint32_t hardware_sequence;
    bool passed;
} OpenV2g_PhysicalAssessment;
typedef struct {
    void *context;
    OpenV2g_Result (*assess)(void *context, OpenV2g_AssessmentKind kind,
        uint32_t generation, uint32_t correlation_id, OpenV2g_TimeUs now,
        uint32_t target_voltage_mv, uint32_t peer_voltage_mv,
        const OpenV2g_ChargeHardwareStatus *measured, OpenV2g_PhysicalAssessment *evidence);
} OpenV2g_PhysicalAssessmentCsPort;
#endif
