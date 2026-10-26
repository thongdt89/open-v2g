#include "openv2g/exi_cbv2g_sap.h"
#include "test.h"
#include <string.h>

/* Upstream fixed vectors: tests/app_handshake/app_handshake.cpp at pinned
 * commit. Apache-2.0, chargebyte GmbH / EVerest contributors. Regression
 * vectors from this codec are not independent conformance evidence.
 */
static const uint8_t request_vector[] = {
    0x80, 0x00, 0xf3, 0xab, 0x93, 0x71, 0xd3, 0x4b, 0x9b, 0x79, 0xd3, 0x9b, 0xa3,
    0x21, 0xd3, 0x4b, 0x9b, 0x79, 0xd1, 0x89, 0xa9, 0x89, 0x89, 0xc1, 0xd1, 0x69,
    0x91, 0x81, 0xd2, 0x0a, 0x18, 0x01, 0x00, 0x00, 0x04, 0x00, 0x40
};
static const uint8_t response_vector[] = {0x80, 0x40, 0x00, 0x40};

typedef struct {
    OpenV2g_SchemaRegistry registry;
    OpenV2g_ExiCodec codec;
    OpenV2g_ExiCodecCsPort port;
    OpenV2g_ExiSapDocument candidate;
    OpenV2g_ExiSapDocument output;
    uint8_t workspace[OPENV2G_EXI_SAP_WORKSPACE_SIZE];
    uint8_t encoded[OPENV2G_EXI_SAP_PAYLOAD_MAX + 1u];
} Fixture;

static OpenV2g_Result encode(Fixture *fixture, const OpenV2g_ExiSapDocument *document,
                             size_t capacity, OpenV2g_ExiResult *result)
{
    OpenV2g_ExiEncodeRequest request = {
        OPENV2G_SCHEMA_SAP, document->message_type_id, document,
        sizeof(*document), fixture->encoded, capacity
    };
    return fixture->port.encode(fixture->port.context, 1u, &request, result);
}
static OpenV2g_Result decode(Fixture *fixture, uint16_t expected,
                             const uint8_t *input, size_t length)
{
    OpenV2g_ExiResult result;
    OpenV2g_ExiDecodeRequest request = {
        OPENV2G_SCHEMA_SAP, expected, input, length,
        &fixture->candidate, &fixture->output, sizeof(fixture->output),
        fixture->workspace, sizeof(fixture->workspace)
    };
    return fixture->port.decode(fixture->port.context, 1u, &request, &result);
}
static int initialize(Fixture *fixture)
{
    (void)memset(fixture, 0, sizeof(*fixture));
    OpenV2g_SchemaRegistry_Init(&fixture->registry);
    OpenV2g_ExiCodec_Init(&fixture->codec, &fixture->registry);
    TEST_ASSERT(OpenV2g_Cbv2gSap_Bind(&fixture->codec) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_ExiCodec_CreateClientServerPort(&fixture->codec, &fixture->port) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_ExiCodec_Start(&fixture->codec, 1u) == OPENV2G_RESULT_OK);
    return 0;
}
static int test_vectors(void)
{
    Fixture fixture;
    OpenV2g_ExiResult result;
    OpenV2g_ExiSapDocument document;
    const char namespace_text[] = "urn:iso:std:iso:15118:-20:AC";
    TEST_ASSERT(initialize(&fixture) == 0);
    TEST_ASSERT(decode(&fixture, OPENV2G_EXI_SAP_REQUEST, request_vector, sizeof(request_vector)) == OPENV2G_RESULT_OK);
    document = fixture.output;
    TEST_ASSERT(document.body.request.count == 1u);
    TEST_ASSERT(document.body.request.offers[0].namespace_length == sizeof(namespace_text) - 1u);
    TEST_ASSERT(memcmp(document.body.request.offers[0].namespace_text, namespace_text, sizeof(namespace_text) - 1u) == 0);
    TEST_ASSERT(document.body.request.offers[0].version_major == 1u);
    TEST_ASSERT(document.body.request.offers[0].version_minor == 0u);
    TEST_ASSERT(document.body.request.offers[0].wire_schema_id == 1u);
    TEST_ASSERT(document.body.request.offers[0].priority == 1u);
    TEST_ASSERT(encode(&fixture, &document, sizeof(fixture.encoded), &result) == OPENV2G_RESULT_OK);
    TEST_ASSERT(result.encoded_length == sizeof(request_vector));
    TEST_ASSERT(memcmp(fixture.encoded, request_vector, sizeof(request_vector)) == 0);
    TEST_ASSERT(decode(&fixture, OPENV2G_EXI_SAP_RESPONSE, response_vector, sizeof(response_vector)) == OPENV2G_RESULT_OK);
    TEST_ASSERT(fixture.output.body.response.code == OPENV2G_EXI_SAP_SUCCESS);
    TEST_ASSERT(fixture.output.body.response.has_schema_id && fixture.output.body.response.wire_schema_id == 1u);
    document = fixture.output;
    TEST_ASSERT(encode(&fixture, &document, sizeof(fixture.encoded), &result) == OPENV2G_RESULT_OK);
    TEST_ASSERT(result.encoded_length == sizeof(response_vector));
    TEST_ASSERT(memcmp(fixture.encoded, response_vector, sizeof(response_vector)) == 0);
    /* Exact small output capacity and one byte too small exercise the guard. */
    TEST_ASSERT(encode(&fixture, &document, sizeof(response_vector), &result) == OPENV2G_RESULT_OK);
    TEST_ASSERT(encode(&fixture, &document, sizeof(response_vector) - 1u, &result) != OPENV2G_RESULT_OK);
    TEST_ASSERT(result.encoded_length == 0u);
    return 0;
}
static int unchanged_on_failure(Fixture *fixture, const uint8_t *bytes, size_t length,
                                uint16_t expected)
{
    OpenV2g_ExiSapDocument previous;
    uint8_t zero_workspace[OPENV2G_EXI_SAP_WORKSPACE_SIZE] = {0};
    OpenV2g_ExiSapDocument zero_candidate;
    (void)memset(&fixture->output, 0xa5, sizeof(fixture->output));
    previous = fixture->output;
    (void)memset(&zero_candidate, 0, sizeof(zero_candidate));
    TEST_ASSERT(decode(fixture, expected, bytes, length) != OPENV2G_RESULT_OK);
    TEST_ASSERT(memcmp(&fixture->output, &previous, sizeof(previous)) == 0);
    if (length != 0u) {
        TEST_ASSERT(memcmp(&fixture->candidate, &zero_candidate, sizeof(zero_candidate)) == 0);
        TEST_ASSERT(memcmp(fixture->workspace, zero_workspace, sizeof(zero_workspace)) == 0);
    }
    return 0;
}
static int test_negative(void)
{
    Fixture fixture;
    uint8_t altered[sizeof(request_vector) + 1u];
    size_t length;
    const OpenV2g_SchemaDescriptor *descriptor;
    OpenV2g_ExiSapDocument document;
    OpenV2g_ExiResult result;
    TEST_ASSERT(initialize(&fixture) == 0);
    for (length = 0u; length < sizeof(request_vector); ++length) {
        TEST_ASSERT(unchanged_on_failure(&fixture, request_vector, length, OPENV2G_EXI_SAP_REQUEST) == 0);
    }
    for (length = 0u; length < sizeof(response_vector); ++length) {
        TEST_ASSERT(unchanged_on_failure(&fixture, response_vector, length, OPENV2G_EXI_SAP_RESPONSE) == 0);
    }
    TEST_ASSERT(unchanged_on_failure(&fixture, response_vector, sizeof(response_vector), OPENV2G_EXI_SAP_REQUEST) == 0);
    (void)memcpy(altered, request_vector, sizeof(request_vector));
    altered[sizeof(request_vector)] = 0u;
    TEST_ASSERT(unchanged_on_failure(&fixture, altered, sizeof(altered), OPENV2G_EXI_SAP_REQUEST) == 0);
    altered[0] = 0xffu;
    TEST_ASSERT(unchanged_on_failure(&fixture, altered, sizeof(request_vector), OPENV2G_EXI_SAP_REQUEST) == 0);
    (void)memcpy(altered, response_vector, sizeof(response_vector));
    altered[sizeof(response_vector) - 1u] |= 1u;
    TEST_ASSERT(unchanged_on_failure(&fixture, altered, sizeof(response_vector), OPENV2G_EXI_SAP_RESPONSE) == 0);
    TEST_ASSERT(OpenV2g_SchemaRegistry_Get(&fixture.registry, OPENV2G_SCHEMA_DIN_70121, &descriptor) != OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_SchemaRegistry_Get(&fixture.registry, OPENV2G_SCHEMA_ISO_15118_2, &descriptor) != OPENV2G_RESULT_OK);
    (void)memset(&document, 0, sizeof(document));
    document.message_type_id = OPENV2G_EXI_SAP_REQUEST;
    document.body.request.count = OPENV2G_EXI_SAP_OFFERS_MAX + 1u;
    TEST_ASSERT(encode(&fixture, &document, sizeof(fixture.encoded), &result) != OPENV2G_RESULT_OK);
    document.body.request.count = 1u;
    document.body.request.offers[0].namespace_length = OPENV2G_EXI_SAP_NAMESPACE_MAX + 1u;
    TEST_ASSERT(encode(&fixture, &document, sizeof(fixture.encoded), &result) != OPENV2G_RESULT_OK);
    return 0;
}
static int test_bounds_roundtrip(void)
{
    Fixture fixture;
    OpenV2g_ExiSapDocument document;
    OpenV2g_ExiResult result;
    size_t index;
    TEST_ASSERT(initialize(&fixture) == 0);
    (void)memset(&document, 0, sizeof(document));
    document.message_type_id = OPENV2G_EXI_SAP_REQUEST;
    document.body.request.count = OPENV2G_EXI_SAP_OFFERS_MAX;
    for (index = 0u; index < OPENV2G_EXI_SAP_OFFERS_MAX; ++index) {
        OpenV2g_ExiSapOffer *offer = &document.body.request.offers[index];
        offer->namespace_length = OPENV2G_EXI_SAP_NAMESPACE_MAX;
        (void)memset(offer->namespace_text, 'x', sizeof(offer->namespace_text));
        offer->version_major = UINT32_MAX;
        offer->version_minor = 0u;
        offer->priority = (uint8_t)(index + 1u);
        offer->wire_schema_id = (uint8_t)index;
    }
    TEST_ASSERT(encode(&fixture, &document, sizeof(fixture.encoded), &result) == OPENV2G_RESULT_OK);
    TEST_ASSERT(decode(&fixture, OPENV2G_EXI_SAP_REQUEST, fixture.encoded, result.encoded_length) == OPENV2G_RESULT_OK);
    TEST_ASSERT(fixture.output.body.request.count == OPENV2G_EXI_SAP_OFFERS_MAX);
    TEST_ASSERT(fixture.output.body.request.offers[4].namespace_length == OPENV2G_EXI_SAP_NAMESPACE_MAX);
    TEST_ASSERT(fixture.output.body.request.offers[4].version_major == UINT32_MAX);
    document.body.request.offers[1].wire_schema_id = document.body.request.offers[0].wire_schema_id;
    TEST_ASSERT(encode(&fixture, &document, sizeof(fixture.encoded), &result) != OPENV2G_RESULT_OK);
    return 0;
}
static int test_mutations(void)
{
    Fixture fixture;
    uint8_t bytes[sizeof(request_vector)];
    size_t index;
    unsigned bit;
    TEST_ASSERT(initialize(&fixture) == 0);
    /* Deterministic negative corpus: valid mutations may decode, but no run
     * may overrun buffers or publish a partial result on failure. */
    for (index = 0u; index < sizeof(bytes); ++index) {
        for (bit = 0u; bit < 8u; ++bit) {
            OpenV2g_ExiSapDocument before;
            OpenV2g_Result result;
            (void)memcpy(bytes, request_vector, sizeof(bytes));
            bytes[index] ^= (uint8_t)(1u << bit);
            (void)memset(&fixture.output, 0xa5, sizeof(fixture.output));
            before = fixture.output;
            result = decode(&fixture, OPENV2G_EXI_SAP_REQUEST, bytes, sizeof(bytes));
            if (result != OPENV2G_RESULT_OK) {
                TEST_ASSERT(memcmp(&fixture.output, &before, sizeof(before)) == 0);
            }
        }
    }
    return 0;
}
static int test_response_and_lifecycle(void)
{
    Fixture fixture;
    OpenV2g_ExiSapDocument document;
    OpenV2g_ExiResult result;
    OpenV2g_ExiEncodeRequest request;
    unsigned code;
    TEST_ASSERT(initialize(&fixture) == 0);
    (void)memset(&document, 0, sizeof(document));
    document.message_type_id = OPENV2G_EXI_SAP_RESPONSE;
    for (code = 0u; code <= 2u; ++code) {
        document.body.response.code = (OpenV2g_ExiSapResponseCode)code;
        document.body.response.has_schema_id = code != 2u;
        document.body.response.wire_schema_id = 255u;
        TEST_ASSERT(encode(&fixture, &document, sizeof(fixture.encoded), &result) == OPENV2G_RESULT_OK);
        TEST_ASSERT(decode(&fixture, OPENV2G_EXI_SAP_RESPONSE, fixture.encoded, result.encoded_length) == OPENV2G_RESULT_OK);
        TEST_ASSERT(fixture.output.body.response.code == document.body.response.code);
    }
    document.body.response.has_schema_id = true;
    TEST_ASSERT(encode(&fixture, &document, sizeof(fixture.encoded), &result) != OPENV2G_RESULT_OK);
    request.schema = OPENV2G_SCHEMA_SAP;
    request.message_type_id = document.message_type_id;
    request.typed_object = &document; request.typed_object_size = sizeof(document);
    request.output = fixture.encoded; request.output_capacity = sizeof(fixture.encoded);
    TEST_ASSERT(fixture.port.encode(fixture.port.context, 2u, &request, &result) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(OpenV2g_Cbv2gSap_Bind(&fixture.codec) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(OpenV2g_ExiCodec_Stop(&fixture.codec, 1u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(fixture.port.encode(fixture.port.context, 1u, &request, &result) == OPENV2G_RESULT_REJECTED);
    return 0;
}
static int test_random_corpus(void)
{
    Fixture fixture;
    uint8_t bytes[OPENV2G_EXI_SAP_PAYLOAD_MAX];
    uint32_t seed = 0x151182u;
    size_t iteration;
    TEST_ASSERT(initialize(&fixture) == 0);
    for (iteration = 0u; iteration < 5000u; ++iteration) {
        size_t index;
        size_t length = iteration % sizeof(bytes) + 1u;
        OpenV2g_ExiSapDocument before;
        OpenV2g_Result result;
        for (index = 0u; index < length; ++index) {
            seed = seed * 1664525u + 1013904223u;
            bytes[index] = (uint8_t)(seed >> 24u);
        }
        if ((iteration % 2u) == 0u) { bytes[0] = 0x80u; }
        (void)memset(&fixture.output, 0xa5, sizeof(fixture.output));
        (void)memcpy(&before, &fixture.output, sizeof(before));
        result = decode(&fixture, OPENV2G_EXI_SAP_REQUEST, bytes, length);
        if (result != OPENV2G_RESULT_OK) {
            TEST_ASSERT(memcmp(&fixture.output, &before, sizeof(before)) == 0);
        }
    }
    return 0;
}
int main(void)
{
    TEST_ASSERT(test_vectors() == 0);
    TEST_ASSERT(test_negative() == 0);
    TEST_ASSERT(test_bounds_roundtrip() == 0);
    TEST_ASSERT(test_mutations() == 0);
    TEST_ASSERT(test_response_and_lifecycle() == 0);
    TEST_ASSERT(test_random_corpus() == 0);
    return 0;
}
