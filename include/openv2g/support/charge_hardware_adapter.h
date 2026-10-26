#ifndef OPENV2G_SUPPORT_CHARGE_HARDWARE_ADAPTER_H
#define OPENV2G_SUPPORT_CHARGE_HARDWARE_ADAPTER_H

#include <stdbool.h>
#include <stdint.h>

#include "openv2g/common/result.h"
#include "openv2g/common/types.h"

typedef enum {
    OPENV2G_HW_COMMAND_ACCEPTED = 0,
    OPENV2G_HW_COMMAND_COMPLETED = 1,
    OPENV2G_HW_COMMAND_FAILED = 2,
    OPENV2G_HW_COMMAND_CANCELLED = 3
} OpenV2g_HardwareCommandResultState;

typedef struct {
    uint32_t generation;
    uint32_t command_id;
    OpenV2g_TimeUs timestamp;
    bool connector_locked;
    bool precharge_active;
    bool positive_contactor_closed;
    bool negative_contactor_closed;
    bool discharge_active;
    uint32_t measured_voltage_mv;
    int32_t measured_current_ma;
} OpenV2g_ChargeHardwareFeedback;

typedef struct {
    OpenV2g_CommEnvelope env;
    uint32_t command_id;
    OpenV2g_HardwareCommandResultState result;
    OpenV2g_HardwareState measured_state;
    uint32_t measured_voltage_mv;
    int32_t measured_current_ma;
} OpenV2g_HardwareCommandResult;

typedef struct {
    OpenV2g_ChargeHardwareStatus status;
    OpenV2g_HardwareCommandResult last_result;
    uint32_t generation;
    uint32_t next_command_id;
    uint32_t pending_command_id;
    uint32_t accepted_permission_id;
    OpenV2g_HardwareState expected_state;
    OpenV2g_ChargeMode prepared_mode;
    bool host_ac_enabled; /* Simulation capability only; default false. */
    bool host_fake;
    bool auto_feedback;
} OpenV2g_ChargeHardwareAdapter;

typedef struct {
    void *context;
    OpenV2g_Result (*prepare)(
        void *context,
        uint32_t generation,
        OpenV2g_TimeUs now,
        OpenV2g_ChargeMode mode);
    OpenV2g_Result (*enable)(
        void *context,
        const OpenV2g_ChargePermission *permission,
        OpenV2g_TimeUs now);
    OpenV2g_Result (*request_safe_state)(
        void *context,
        uint32_t generation,
        OpenV2g_TimeUs now,
        OpenV2g_ReasonCode reason);
} OpenV2g_ChargeHardwareControlCsPort;

typedef struct {
    void *context;
    OpenV2g_Result (*read)(
        void *context,
        OpenV2g_ChargeHardwareStatus *status);
} OpenV2g_ChargeHardwareStatusSrPort;

typedef struct {
    void *context;
    OpenV2g_Result (*read)(
        void *context,
        OpenV2g_HardwareCommandResult *result);
} OpenV2g_HardwareCommandResultSrPort;

void OpenV2g_ChargeHardwareAdapter_InitHostFake(OpenV2g_ChargeHardwareAdapter *adapter);
OpenV2g_Result OpenV2g_ChargeHardwareAdapter_SetAutoFeedback(
    OpenV2g_ChargeHardwareAdapter *adapter,
    bool enabled);
OpenV2g_Result OpenV2g_ChargeHardwareAdapter_Prepare(
    OpenV2g_ChargeHardwareAdapter *adapter,
    uint32_t generation,
    OpenV2g_TimeUs now,
    OpenV2g_ChargeMode mode);
OpenV2g_Result OpenV2g_ChargeHardwareAdapter_Enable(
    OpenV2g_ChargeHardwareAdapter *adapter,
    const OpenV2g_ChargePermission *permission,
    OpenV2g_TimeUs now);
OpenV2g_Result OpenV2g_ChargeHardwareAdapter_RequestSafeState(
    OpenV2g_ChargeHardwareAdapter *adapter,
    uint32_t generation,
    OpenV2g_TimeUs now,
    OpenV2g_ReasonCode reason);
OpenV2g_Result OpenV2g_ChargeHardwareAdapter_ApplyMeasuredFeedback(
    OpenV2g_ChargeHardwareAdapter *adapter,
    const OpenV2g_ChargeHardwareFeedback *feedback);
OpenV2g_Result OpenV2g_ChargeHardwareAdapter_CreateControlClientServerPort(
    OpenV2g_ChargeHardwareAdapter *adapter,
    OpenV2g_ChargeHardwareControlCsPort *port);
OpenV2g_Result OpenV2g_ChargeHardwareAdapter_CreateStatusSenderReceiverPort(
    OpenV2g_ChargeHardwareAdapter *adapter,
    OpenV2g_ChargeHardwareStatusSrPort *port);
OpenV2g_Result OpenV2g_ChargeHardwareAdapter_CreateResultSenderReceiverPort(
    OpenV2g_ChargeHardwareAdapter *adapter,
    OpenV2g_HardwareCommandResultSrPort *port);

#endif
