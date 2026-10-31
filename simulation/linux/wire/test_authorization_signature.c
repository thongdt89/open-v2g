#include "wire.h"
#include "test.h"
#include <stdio.h>
#include <string.h>
static int save(const char *folder, const char *name, const uint8_t *bytes, size_t length)
{
    char path[512]; FILE *file;
    TEST_ASSERT(snprintf(path,sizeof(path),"%s/%s",folder,name) > 0);
    file = fopen(path,"wb"); TEST_ASSERT(file != NULL && fwrite(bytes,1u,length,file) == length && fclose(file) == 0); return 0;
}
int main(int argc, char **argv)
{
    Ov2g_Crypto signer, verifier; OvWireCodec codec;
    OpenV2g_ExiDcDocument value = OpenV2g_ExiDc_Default(OPENV2G_PROTOCOL_MESSAGE_AUTHORIZATION,false), changed, decoded;
    OpenV2g_V2gtpFrameView frame; uint8_t bytes[OV_WIRE_FRAME_MAX]; size_t length; uint32_t version;
    char path[512]; unsigned i;
    TEST_ASSERT(argc == 2); Ov2g_CryptoInit(&signer); Ov2g_CryptoInit(&verifier);
    TEST_ASSERT(Ov2g_CryptoBeginGeneration(&signer,7u) == OPENV2G_RESULT_OK && Ov2g_CryptoBeginGeneration(&verifier,7u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(snprintf(path,sizeof(path),"%s/contract.key",argv[1]) > 0 && Ov2g_CryptoLoadPrivateKey(&signer,1u,path) == OPENV2G_RESULT_OK);
    TEST_ASSERT(snprintf(path,sizeof(path),"%s/contract.pem",argv[1]) > 0 && Ov2g_CryptoLoadPublicCertificate(&verifier,1u,path) == OPENV2G_RESULT_OK);
    TEST_ASSERT(snprintf(path,sizeof(path),"%s/client.pem",argv[1]) > 0 && Ov2g_CryptoLoadPublicCertificate(&verifier,2u,path) == OPENV2G_RESULT_OK);
    value.has_challenge = true; value.signature.body_id_length = 4u; (void)memcpy(value.signature.body_id,"auth",4u);
    value.session_id[7] = 7u; for (i = 0u; i < 16u; ++i) { value.challenge[i] = (uint8_t)(i+1u); }
    TEST_ASSERT(OvWireSignAuthorization(&signer,7u,1u,1u,&value,Ov2g_OsNow()+500000u,&version) && version == 1u && value.has_signature);
    TEST_ASSERT(OvWireVerifyAuthorization(&verifier,7u,1u,1u,&value,Ov2g_OsNow()+500000u));
    TEST_ASSERT(!OvWireVerifyAuthorization(&verifier,7u,2u,3u,&value,Ov2g_OsNow()+500000u));
    changed = value; changed.challenge[0] ^= 1u;
    TEST_ASSERT(!OvWireVerifyAuthorization(&verifier,7u,1u,5u,&changed,Ov2g_OsNow()+500000u));
    changed = value; changed.signature.value[0] ^= 1u;
    TEST_ASSERT(!OvWireVerifyAuthorization(&verifier,7u,1u,7u,&changed,Ov2g_OsNow()+500000u));
    TEST_ASSERT(!OvWireVerifyAuthorization(&verifier,6u,1u,9u,&value,Ov2g_OsNow()+500000u));
    TEST_ASSERT(OvWireCodec_Init(&codec,7u));
    TEST_ASSERT(OvWireEncode(&codec,7u,OPENV2G_SCHEMA_ISO_15118_2,4u,&value,sizeof(value),bytes,&length));
    TEST_ASSERT(save(argv[1],"auth.exi",bytes+OPENV2G_V2GTP_HEADER_SIZE,length-OPENV2G_V2GTP_HEADER_SIZE) == 0);
    frame.payload = bytes+OPENV2G_V2GTP_HEADER_SIZE; frame.payload_length = (uint32_t)(length-OPENV2G_V2GTP_HEADER_SIZE); frame.payload_type = 0x8001u; frame.generation = 7u;
    TEST_ASSERT(OvWireDecode(&codec,7u,OPENV2G_SCHEMA_ISO_15118_2,4u,&frame,&decoded,sizeof(decoded)) && decoded.has_challenge && decoded.has_signature);
    TEST_ASSERT(OvWireVerifyAuthorization(&verifier,7u,1u,11u,&decoded,Ov2g_OsNow()+500000u));
    TEST_ASSERT(OpenV2g_Cbv2gIso2_AuthorizationFragment(&value,bytes,sizeof(bytes),&length) == OPENV2G_RESULT_OK && save(argv[1],"auth-body.exi",bytes,length) == 0);
    TEST_ASSERT(OpenV2g_Cbv2gIso2_SignedInfoFragment(&value.signature,bytes,sizeof(bytes),&length) == OPENV2G_RESULT_OK && save(argv[1],"auth-info.exi",bytes,length) == 0);
    TEST_ASSERT(save(argv[1],"auth.sig",value.signature.value,64u) == 0 && save(argv[1],"auth-digest.bin",value.signature.digest,32u) == 0);
    (void)memset(bytes,0xa5,sizeof(bytes));
    TEST_ASSERT(OpenV2g_Cbv2gIso2_AuthorizationFragment(&value,bytes,1u,&length) == OPENV2G_RESULT_REJECTED && length == 0u && bytes[0] == 0xa5u);
    {
        OpenV2g_ExiDcDocument details = OpenV2g_ExiDc_Default(OPENV2G_PROTOCOL_MESSAGE_PAYMENT_DETAILS,false); FILE *file; size_t size;
        TEST_ASSERT(snprintf(path,sizeof(path),"%s/contract.der",argv[1]) > 0); file = fopen(path,"rb"); TEST_ASSERT(file != NULL);
        size = fread(details.contract.chain[0].bytes,1u,sizeof(details.contract.chain[0].bytes),file); TEST_ASSERT(size > 0u && size <= 800u && fclose(file) == 0);
        details.contract.present = true; details.contract.chain_count = 1u; details.contract.chain[0].length = (uint16_t)size;
        details.contract.identity_length = 15u; (void)memcpy(details.contract.identity,"DEABC1234567890",15u);
        details.contract.key_handle = 77u; details.contract.key_version = 99u; details.contract.version = 42u;
        TEST_ASSERT(OvWireEncode(&codec,7u,OPENV2G_SCHEMA_ISO_15118_2,17u,&details,sizeof(details),bytes,&length));
        TEST_ASSERT(save(argv[1],"payment-details.exi",bytes+OPENV2G_V2GTP_HEADER_SIZE,length-OPENV2G_V2GTP_HEADER_SIZE) == 0);
        frame.payload = bytes+OPENV2G_V2GTP_HEADER_SIZE; frame.payload_length = (uint32_t)(length-OPENV2G_V2GTP_HEADER_SIZE);
        TEST_ASSERT(OvWireDecode(&codec,7u,OPENV2G_SCHEMA_ISO_15118_2,17u,&frame,&decoded,sizeof(decoded)) && decoded.contract.key_handle == 0u && decoded.contract.key_version == 0u && decoded.contract.version == 0u);
        details = OpenV2g_ExiDc_Default(OPENV2G_PROTOCOL_MESSAGE_PAYMENT_DETAILS,true); details.has_challenge = true;
        (void)memcpy(details.challenge,value.challenge,16u); details.peer_time_s = 1700000000;
        TEST_ASSERT(OvWireEncode(&codec,7u,OPENV2G_SCHEMA_ISO_15118_2,117u,&details,sizeof(details),bytes,&length));
        TEST_ASSERT(save(argv[1],"payment-details-response.exi",bytes+OPENV2G_V2GTP_HEADER_SIZE,length-OPENV2G_V2GTP_HEADER_SIZE) == 0);
    }
    Ov2g_CryptoClose(&signer); Ov2g_CryptoClose(&verifier); return 0;
}
