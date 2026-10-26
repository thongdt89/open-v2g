#include "openv2g/exi_cbv2g_dc.h"
#include <string.h>
OpenV2g_ExiDcDocument OpenV2g_ExiDc_Default(OpenV2g_ProtocolMessageKind kind, bool response)
{
    OpenV2g_ExiDcDocument value;
    (void)memset(&value, 0, sizeof(value));
    value.kind = kind; value.response = response; value.accepted = true;
    value.new_session = kind == OPENV2G_PROTOCOL_MESSAGE_SESSION_SETUP;
    value.ready = true; value.isolation_valid = true;
    value.evcc_id[0] = 0x84u; value.evcc_id[1] = 0x87u; value.evcc_id[2] = 0x72u;
    value.evcc_id[3] = 0x60u; value.evcc_id[4] = 0xf3u; value.evcc_id[5] = 0x58u;
    (void)memcpy(value.evse_id, "HOST01", 6u); value.evse_id_length = 6u;
    value.service_id = 1u; value.schedule_id = 1u; value.soc_permille = 500u;
    value.voltage_mv = 400000u; value.current_ma = 50000;
    value.max_voltage_mv = 500000u; value.max_current_ma = 100000;
    value.max_power_w = 30000; value.schedule_power_w = 30000;
    value.schedule_duration_s = 3600u;
    return value;
}
