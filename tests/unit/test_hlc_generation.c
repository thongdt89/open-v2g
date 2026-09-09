#include "openv2g/iso15118_chrg/iso15118_chrg.h"
#include "test.h"

int main(void)
{
    OpenV2g_Iso15118Chrg hlc;

    OpenV2g_Iso15118Chrg_Init(&hlc, NULL);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_Start(&hlc, 1u, 100u) ==
                OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(hlc.status.state == OPENV2G_HLC_SDP);

    TEST_ASSERT(OpenV2g_Iso15118Chrg_OnSdpComplete(&hlc, 2u, 110u, true) ==
                OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(hlc.status.state == OPENV2G_HLC_SDP);

    TEST_ASSERT(OpenV2g_Iso15118Chrg_OnSdpComplete(&hlc, 1u, 110u, true) ==
                OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(hlc.status.state == OPENV2G_HLC_TRANSPORT);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_OnTransportReady(&hlc, 1u, 120u) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(hlc.status.state == OPENV2G_HLC_SAP);

    TEST_ASSERT(OpenV2g_Iso15118Chrg_OnProtocolSelected(
                    &hlc,
                    1u,
                    130u,
                    OPENV2G_PROTOCOL_ISO_15118_20,
                    OPENV2G_SCHEMA_ISO_20_COMMON) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(hlc.status.state == OPENV2G_HLC_SAP);

    TEST_ASSERT(OpenV2g_Iso15118Chrg_OnProtocolSelected(
                    &hlc,
                    1u,
                    130u,
                    OPENV2G_PROTOCOL_ISO_15118_2,
                    OPENV2G_SCHEMA_ISO_15118_2) == OPENV2G_RESULT_OK);
    TEST_ASSERT(hlc.status.state == OPENV2G_HLC_PROTOCOL_SESSION);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_Stop(
                    &hlc,
                    1u,
                    140u,
                    OPENV2G_REASON_CANCELLED) == OPENV2G_RESULT_OK);
    TEST_ASSERT(hlc.status.state == OPENV2G_HLC_IDLE);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_Stop(
                    &hlc,
                    1u,
                    141u,
                    OPENV2G_REASON_CANCELLED) == OPENV2G_RESULT_OK);
    return 0;
}
