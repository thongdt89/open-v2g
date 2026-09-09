#ifndef OPENV2G_SUPPORT_CP_PP_MANAGER_H
#define OPENV2G_SUPPORT_CP_PP_MANAGER_H

#include "openv2g/common/result.h"
#include "openv2g/common/types.h"

typedef struct {
    OpenV2g_QualifiedCpPp qualified;
    bool has_value;
} OpenV2g_CpPpManager;

void OpenV2g_CpPpManager_Init(OpenV2g_CpPpManager *manager);
OpenV2g_Result OpenV2g_CpPpManager_PublishQualified(
    OpenV2g_CpPpManager *manager,
    const OpenV2g_QualifiedCpPp *value);

#endif
