#include "openv2g/iso15118_chrg/session_context.h"
#include "test.h"
#include <string.h>
static int expect(OpenV2g_SessionContext *context, OpenV2g_ProtocolMessageKind kind, uint32_t correlation)
{
    OpenV2g_ProtocolTxRequest request = {0};
    request.generation = context->generation; request.protocol = OPENV2G_PROTOCOL_ISO_15118_2; request.schema = OPENV2G_SCHEMA_ISO_15118_2;
    request.mode = OPENV2G_MODE_DC; request.kind = kind; request.correlation_id = correlation; request.timestamp = context->last_response_time > 100u + correlation ? context->last_response_time : 100u + correlation;
    TEST_ASSERT(OpenV2g_SessionContext_Expect(context, &request) == OPENV2G_RESULT_OK);
    return 0;
}
int main(void)
{
    OpenV2g_SessionContext context, before;
    OpenV2g_SessionResponse response = {0};
    TEST_ASSERT(OpenV2g_SessionContext_Begin(&context, 7u, OPENV2G_PROTOCOL_ISO_15118_2, OPENV2G_SCHEMA_DIN_70121, OPENV2G_MODE_DC) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(OpenV2g_SessionContext_Begin(&context, 7u, OPENV2G_PROTOCOL_ISO_15118_2, OPENV2G_SCHEMA_ISO_15118_2, OPENV2G_MODE_DC) == OPENV2G_RESULT_OK);
    TEST_ASSERT(expect(&context, OPENV2G_PROTOCOL_MESSAGE_SESSION_SETUP, 1u) == 0);
    response.generation = 7u; response.correlation_id = 1u; response.schema = OPENV2G_SCHEMA_ISO_15118_2;
    response.kind = OPENV2G_PROTOCOL_MESSAGE_SESSION_SETUP; response.accepted = true; response.new_session = true;
    response.session_id[7] = 7u; response.peer_id[0] = 'S'; response.peer_id_length = 1u;
    before = context; response.generation = 6u;
    TEST_ASSERT(OpenV2g_SessionContext_Validate(&context, &response, 101u) == OPENV2G_RESULT_REJECTED && memcmp(&context,&before,sizeof(context)) == 0);
    response.generation = 7u; response.schema = OPENV2G_SCHEMA_DIN_70121;
    TEST_ASSERT(OpenV2g_SessionContext_Validate(&context, &response, 101u) == OPENV2G_RESULT_REJECTED);
    response.schema = OPENV2G_SCHEMA_ISO_15118_2; response.peer_id_length = OPENV2G_SESSION_PEER_ID_MAX + 1u;
    TEST_ASSERT(OpenV2g_SessionContext_Validate(&context, &response, 101u) == OPENV2G_RESULT_REJECTED);
    response.peer_id_length = 1u;
    TEST_ASSERT(OpenV2g_SessionContext_Validate(&context, &response, 101u) == OPENV2G_RESULT_OK && context.established);
    TEST_ASSERT(OpenV2g_SessionContext_Validate(&context, &response, 101u) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(OpenV2g_SessionContext_Consume(&context, response.kind, 2u) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(OpenV2g_SessionContext_Consume(&context, response.kind, 1u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(expect(&context, OPENV2G_PROTOCOL_MESSAGE_SERVICE_DISCOVERY, 2u) == 0);
    response.kind = OPENV2G_PROTOCOL_MESSAGE_SERVICE_DISCOVERY; response.correlation_id = 2u; response.service_id = 1u;
    response.session_id[7] = 8u;
    TEST_ASSERT(OpenV2g_SessionContext_Validate(&context, &response, 102u) == OPENV2G_RESULT_REJECTED);
    response.session_id[7] = 7u;
    TEST_ASSERT(OpenV2g_SessionContext_Validate(&context, &response, 102u) == OPENV2G_RESULT_OK && context.service_id == 1u);
    TEST_ASSERT(OpenV2g_SessionContext_Consume(&context, response.kind, 2u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(expect(&context, OPENV2G_PROTOCOL_MESSAGE_CHARGE_PARAMETER, 3u) == 0);
    response.kind = OPENV2G_PROTOCOL_MESSAGE_CHARGE_PARAMETER; response.correlation_id = 3u;
    response.schedule_id = 2u; response.schedule_duration_s = 1u; response.schedule_power_w = 10000;
    TEST_ASSERT(OpenV2g_SessionContext_Validate(&context, &response, 103u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_SessionContext_Consume(&context, response.kind, 3u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(expect(&context, OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND, 4u) == 0);
    response.kind = OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND; response.correlation_id = 4u; response.schedule_id = 3u;
    TEST_ASSERT(OpenV2g_SessionContext_Validate(&context, &response, 104u) == OPENV2G_RESULT_REJECTED);
    response.schedule_id = 2u; response.peer_id[0] = 'X';
    TEST_ASSERT(OpenV2g_SessionContext_Validate(&context, &response, 104u) == OPENV2G_RESULT_REJECTED);
    response.peer_id[0] = 'S';
    TEST_ASSERT(OpenV2g_SessionContext_Validate(&context, &response, context.schedule_valid_until) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(OpenV2g_SessionContext_Validate(&context, &response, 104u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_SessionContext_Consume(&context, response.kind, 4u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(expect(&context, OPENV2G_PROTOCOL_MESSAGE_SESSION_STOP, 5u) == 0);
    response.kind = OPENV2G_PROTOCOL_MESSAGE_SESSION_STOP; response.correlation_id = 5u;
    TEST_ASSERT(OpenV2g_SessionContext_Validate(&context, &response, 105u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_SessionContext_Consume(&context, response.kind, 5u) == OPENV2G_RESULT_OK);
    {
        OpenV2g_RetainedSession retained = {0}, expired;
        OpenV2g_Endpoint endpoint = {0};
        endpoint.address[15] = 1u; endpoint.port = 15118u; endpoint.transport = OPENV2G_TRANSPORT_TCP;
        TEST_ASSERT(OpenV2g_SessionContext_Retain(&context, &endpoint, 106u, 100u, &retained) == OPENV2G_RESULT_OK);
        expired = retained;
        TEST_ASSERT(OpenV2g_SessionContext_Begin(&context, 8u, OPENV2G_PROTOCOL_ISO_15118_2, OPENV2G_SCHEMA_ISO_15118_2, OPENV2G_MODE_DC) == OPENV2G_RESULT_OK);
        TEST_ASSERT(OpenV2g_SessionContext_Restore(&context, &expired, &endpoint, expired.expires) == OPENV2G_RESULT_REJECTED);
        endpoint.security = OPENV2G_SECURITY_TLS12;
        TEST_ASSERT(OpenV2g_SessionContext_Restore(&context, &retained, &endpoint, 150u) == OPENV2G_RESULT_REJECTED && retained.valid);
        endpoint.security = OPENV2G_SECURITY_NONE;
        TEST_ASSERT(OpenV2g_SessionContext_Restore(&context, &retained, &endpoint, 150u) == OPENV2G_RESULT_OK && !retained.valid && context.resume_pending);
        TEST_ASSERT(OpenV2g_SessionContext_Restore(&context, &retained, &endpoint, 150u) == OPENV2G_RESULT_REJECTED);
        TEST_ASSERT(expect(&context, OPENV2G_PROTOCOL_MESSAGE_SESSION_SETUP, 1u) == 0);
        response.generation = 8u; response.kind = OPENV2G_PROTOCOL_MESSAGE_SESSION_SETUP; response.correlation_id = 1u;
        response.new_session = false; response.resumed_session = true; response.peer_id[0] = 'X';
        TEST_ASSERT(OpenV2g_SessionContext_Validate(&context, &response, 151u) == OPENV2G_RESULT_REJECTED);
        response.peer_id[0] = 'S';
        TEST_ASSERT(OpenV2g_SessionContext_Validate(&context, &response, 151u) == OPENV2G_RESULT_OK && context.resumed);
    }
    OpenV2g_SessionContext_Cancel(&context); OpenV2g_SessionContext_Cancel(&context);
    TEST_ASSERT(OpenV2g_SessionContext_Validate(&context, &response, 104u) == OPENV2G_RESULT_REJECTED && !context.established);
    return 0;
}
