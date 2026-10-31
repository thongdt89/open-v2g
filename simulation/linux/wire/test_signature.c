#include "wire.h"
#include "test.h"
#include <stdio.h>
#include <string.h>
#include "cbv2g/iso_2/iso2_msgDefDatatypes.h"
#include "cbv2g/iso_2/iso2_msgDefDecoder.h"
#include "cbv2g/iso_2/iso2_msgDefEncoder.h"
#include "cbv2g/common/exi_bitstream.h"
static int write_file(const char *folder, const char *name, const uint8_t *bytes, size_t length)
{
    char path[512]; FILE *file;
    TEST_ASSERT(snprintf(path,sizeof(path),"%s/%s",folder,name) > 0);
    file = fopen(path,"wb"); TEST_ASSERT(file != NULL);
    TEST_ASSERT(fwrite(bytes,1u,length,file) == length && fclose(file) == 0); return 0;
}
static int profile_restrictions(OvWireCodec *codec, const uint8_t *input, size_t length)
{
    struct iso2_exiDocument original, changed;
    exi_bitstream_t stream;
    uint8_t bytes[OPENV2G_EXI_DC_WORKSPACE_SIZE] = {0}, workspace[OPENV2G_EXI_DC_WORKSPACE_SIZE];
    OpenV2g_ExiDcDocument candidate, output, untouched;
    OpenV2g_ExiDecodeRequest decode = {OPENV2G_SCHEMA_ISO_15118_2, OPENV2G_PROTOCOL_MESSAGE_METERING_RECEIPT,
        bytes, length, &candidate, &output, sizeof(output), workspace, sizeof(workspace)};
    OpenV2g_ExiResult result;
    unsigned variant;
    (void)memcpy(bytes,input,length); (void)memset(&original,0,sizeof(original));
    exi_bitstream_init(&stream,bytes,length,0u,NULL);
    TEST_ASSERT(decode_iso2_exiDocument(&stream,&original) == 0);
    for (variant = 0u; variant < 7u; ++variant) {
        struct iso2_SignatureType *signature;
        changed = original; signature = &changed.V2G_Message.Header.Signature;
        if (variant == 0u) { signature->SignedInfo.SignatureMethod.Algorithm.characters[0] = 'x'; }
        if (variant == 1u) { signature->SignedInfo.Reference.array[0].URI.characters[2] = '2'; }
        if (variant == 2u) { signature->SignedInfo.Reference.arrayLen = 2u; signature->SignedInfo.Reference.array[1] = signature->SignedInfo.Reference.array[0]; }
        if (variant == 3u) { signature->SignatureValue.CONTENT.bytesLen = 63u; }
        if (variant == 4u) { signature->SignedInfo.Reference.array[0].DigestValue.bytesLen = 31u; }
        if (variant == 5u) { changed.V2G_Message.Body.MeteringReceiptReq.Id_isUsed = 0u; }
        if (variant == 6u) { signature->Id_isUsed = 1u; signature->Id.charactersLen = 1u; signature->Id.characters[0] = 'x'; }
        (void)memset(bytes,0,sizeof(bytes)); exi_bitstream_init(&stream,bytes,OPENV2G_EXI_DC_PAYLOAD_MAX,0u,NULL);
        TEST_ASSERT(encode_iso2_exiDocument(&stream,&changed) == 0);
        decode.input_length = exi_bitstream_get_length(&stream);
        (void)memset(&output,0x5a,sizeof(output)); untouched = output;
        TEST_ASSERT(OpenV2g_ExiCodec_Decode(&codec->codec,1u,&decode,&result) == OPENV2G_RESULT_REJECTED);
        TEST_ASSERT(memcmp(&output,&untouched,sizeof(output)) == 0);
    }
    return 0;
}
int main(int argc, char **argv)
{
    Ov2g_Crypto crypto;
    OpenV2g_ExiDcDocument document = OpenV2g_ExiDc_Default(OPENV2G_PROTOCOL_MESSAGE_METERING_RECEIPT,false), decoded, bad;
    OvWireCodec codec;
    OpenV2g_ExiResult result;
    uint8_t bytes[OPENV2G_EXI_DC_PAYLOAD_MAX];
    OpenV2g_ExiEncodeRequest encode = {OPENV2G_SCHEMA_ISO_15118_2, OPENV2G_PROTOCOL_MESSAGE_METERING_RECEIPT, &document,sizeof(document),bytes,sizeof(bytes)};
    OpenV2g_ExiDecodeRequest decode = {0};
    OpenV2g_ExiDcDocument candidate;
    uint8_t workspace[OPENV2G_EXI_DC_WORKSPACE_SIZE];
    uint32_t version;
    size_t length;
    char key[512], public[512], wrong[512];
    TEST_ASSERT(argc == 2);
    TEST_ASSERT(snprintf(key,sizeof(key),"%s/client.key",argv[1]) > 0);
    TEST_ASSERT(snprintf(public,sizeof(public),"%s/client.pem",argv[1]) > 0);
    TEST_ASSERT(snprintf(wrong,sizeof(wrong),"%s/server.pem",argv[1]) > 0);
    Ov2g_CryptoInit(&crypto);
    TEST_ASSERT(Ov2g_CryptoLoadPrivateKey(&crypto,1u,key) == OPENV2G_RESULT_OK);
    TEST_ASSERT(Ov2g_CryptoLoadPublicCertificate(&crypto,2u,public) == OPENV2G_RESULT_OK);
    TEST_ASSERT(Ov2g_CryptoLoadPublicCertificate(&crypto,3u,wrong) == OPENV2G_RESULT_OK);
    TEST_ASSERT(Ov2g_CryptoBeginGeneration(&crypto,1u) == OPENV2G_RESULT_OK);
    document.session_id[0] = 7u; document.has_meter_info = true;
    document.meter_info.id_length = 6u; (void)memcpy(document.meter_info.id,"METER1",6u);
    document.meter_info.has_reading = true; document.meter_info.reading_wh = 12345u;
    document.signature.body_id_length = 2u; (void)memcpy(document.signature.body_id,"m1",2u);
    TEST_ASSERT(OvWireSignReceipt(&crypto,1u,1u,1u,&document,Ov2g_OsNow()+500000u,&version) && version != 0u);
    TEST_ASSERT(OvWireVerifyReceipt(&crypto,1u,2u,3u,&document,Ov2g_OsNow()+500000u));
    TEST_ASSERT(!OvWireVerifyReceipt(&crypto,1u,3u,5u,&document,Ov2g_OsNow()+500000u));
    TEST_ASSERT(OvWireCodec_Init(&codec,1u));
    TEST_ASSERT(OpenV2g_ExiCodec_Encode(&codec.codec,1u,&encode,&result) == OPENV2G_RESULT_OK);
    TEST_ASSERT(write_file(argv[1],"receipt.exi",bytes,result.encoded_length) == 0);
    TEST_ASSERT(profile_restrictions(&codec, bytes, result.encoded_length) == 0);
    decode = (OpenV2g_ExiDecodeRequest){OPENV2G_SCHEMA_ISO_15118_2, OPENV2G_PROTOCOL_MESSAGE_METERING_RECEIPT,bytes,result.encoded_length,
        &candidate,&decoded,sizeof(decoded),workspace,sizeof(workspace)};
    TEST_ASSERT(OpenV2g_ExiCodec_Decode(&codec.codec,1u,&decode,&result) == OPENV2G_RESULT_OK && decoded.has_signature);
    TEST_ASSERT(OvWireVerifyReceipt(&crypto,1u,2u,7u,&decoded,Ov2g_OsNow()+500000u));
    bad = decoded; ++bad.meter_info.reading_wh;
    TEST_ASSERT(!OvWireVerifyReceipt(&crypto,1u,2u,9u,&bad,Ov2g_OsNow()+500000u));
    bad = decoded; bad.signature.value[0] ^= 1u;
    TEST_ASSERT(!OvWireVerifyReceipt(&crypto,1u,2u,11u,&bad,Ov2g_OsNow()+500000u));
    bad = decoded; ++bad.signature.body_id[1];
    TEST_ASSERT(!OvWireVerifyReceipt(&crypto,1u,2u,13u,&bad,Ov2g_OsNow()+500000u));
    TEST_ASSERT(!OvWireVerifyReceipt(&crypto,2u,2u,15u,&decoded,Ov2g_OsNow()+500000u));
    TEST_ASSERT(!OvWireVerifyReceipt(&crypto,1u,2u,17u,&decoded,Ov2g_OsNow()-1u));
    TEST_ASSERT(OpenV2g_Cbv2gIso2_ReceiptFragment(&document,bytes,sizeof(bytes),&length) == OPENV2G_RESULT_OK);
    TEST_ASSERT(write_file(argv[1],"receipt-body.exi",bytes,length) == 0);
    TEST_ASSERT(write_file(argv[1],"receipt-digest.bin",document.signature.digest,32u) == 0);
    TEST_ASSERT(OpenV2g_Cbv2gIso2_SignedInfoFragment(&document.signature,bytes,sizeof(bytes),&length) == OPENV2G_RESULT_OK);
    TEST_ASSERT(write_file(argv[1],"receipt-info.exi",bytes,length) == 0);
    TEST_ASSERT(write_file(argv[1],"receipt.sig",document.signature.value,64u) == 0);
    Ov2g_CryptoClose(&crypto); return 0;
}
