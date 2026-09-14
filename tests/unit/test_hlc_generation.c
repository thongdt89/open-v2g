#include "openv2g/iso15118_chrg/iso15118_chrg.h"
#include "test.h"
#include "test_support.h"

int main(void)
{
    OpenV2g_Iso15118Chrg hlc;
    OpenV2g_Iso15118Chrg unattached_hlc;
    OpenV2g_Iso15118Chrg failed_hlc;
    OpenV2g_V2gtpFrameView frame;
    OpenV2g_HlcEvent event;
    const uint8_t payload[] = {0x01u, 0x02u};
    uint8_t storage[16];
    uint8_t failed_storage[16];
    uint8_t encoded[16];
    size_t encoded_length;
    size_t consumed;
    OpenV2g_Endpoint endpoint;

    endpoint = OpenV2g_TestEndpoint(OPENV2G_SECURITY_NONE);

    OpenV2g_Iso15118Chrg_Init(&hlc, NULL);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_AttachV2gtpStorage(
                    &hlc,
                    storage,
                    sizeof(storage)) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_Start(&hlc, 1u, 100u) ==
                OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(hlc.status.state == OPENV2G_HLC_SDP);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_PopEvent(&hlc, 1u, &event) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(event.category == OPENV2G_HLC_EVENT_STATE_CHANGED);
    TEST_ASSERT(event.env.session_generation == 1u);

    TEST_ASSERT(OpenV2g_Iso15118Chrg_OnSdpComplete(&hlc, 2u, 110u, &endpoint) ==
                OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(hlc.status.state == OPENV2G_HLC_SDP);

    TEST_ASSERT(OpenV2g_Iso15118Chrg_OnSdpComplete(&hlc, 1u, 110u, &endpoint) ==
                OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(hlc.status.state == OPENV2G_HLC_TRANSPORT);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_OnTransportReady(&hlc, 1u, 120u) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(hlc.status.state == OPENV2G_HLC_SAP);

    TEST_ASSERT(OpenV2g_V2gtp_Encode(
                    0x8001u,
                    payload,
                    (uint32_t)sizeof(payload),
                    encoded,
                    sizeof(encoded),
                    &encoded_length) == OPENV2G_RESULT_OK);
    consumed = 99u;
    TEST_ASSERT(OpenV2g_Iso15118Chrg_OnTransportBytes(
                    &hlc,
                    2u,
                    121u,
                    encoded,
                    encoded_length,
                    &consumed,
                    &frame) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(consumed == 0u);
    TEST_ASSERT(hlc.status.state == OPENV2G_HLC_SAP);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_OnTransportBytes(
                    &hlc,
                    1u,
                    121u,
                    encoded,
                    encoded_length,
                    &consumed,
                    &frame) == OPENV2G_RESULT_OK);
    TEST_ASSERT(consumed == encoded_length);
    TEST_ASSERT(frame.payload_type == 0x8001u);
    TEST_ASSERT(frame.payload_length == sizeof(payload));
    TEST_ASSERT(OpenV2g_Iso15118Chrg_ReleaseV2gtpFrame(&hlc, 1u) ==
                OPENV2G_RESULT_OK);

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

    OpenV2g_Iso15118Chrg_Init(&unattached_hlc, NULL);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_Start(&unattached_hlc, 2u, 200u) ==
                OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_OnSdpComplete(
                    &unattached_hlc,
                    2u,
                    210u,
                    &endpoint) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_OnTransportReady(
                    &unattached_hlc,
                    2u,
                    220u) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(unattached_hlc.status.state == OPENV2G_HLC_ERROR);

    OpenV2g_Iso15118Chrg_Init(&failed_hlc, NULL);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_AttachV2gtpStorage(
                    &failed_hlc,
                    failed_storage,
                    sizeof(failed_storage)) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_Start(&failed_hlc, 3u, 300u) ==
                OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_OnSdpComplete(
                    &failed_hlc,
                    3u,
                    310u,
                    &endpoint) == OPENV2G_RESULT_ACCEPTED);
    failed_hlc.exi_codec.state = OPENV2G_SUBSYSTEM_DISABLED;
    TEST_ASSERT(OpenV2g_Iso15118Chrg_OnTransportReady(
                    &failed_hlc,
                    3u,
                    320u) == OPENV2G_RESULT_NOT_IMPLEMENTED);
    TEST_ASSERT(failed_hlc.status.state == OPENV2G_HLC_ERROR);
    TEST_ASSERT(failed_hlc.schema_registry.state == OPENV2G_SUBSYSTEM_IDLE);
    return 0;
}
