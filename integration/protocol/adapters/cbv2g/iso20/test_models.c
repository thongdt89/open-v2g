#include "test.h"
#include <stdbool.h>
#include <string.h>
#include "cbv2g/iso_20/iso20_CommonMessages_Encoder.h"
#include "cbv2g/iso_20/iso20_CommonMessages_Decoder.h"
#include "cbv2g/iso_20/iso20_AC_Encoder.h"
#include "cbv2g/iso_20/iso20_AC_Decoder.h"
#include "cbv2g/iso_20/iso20_DC_Encoder.h"
#include "cbv2g/iso_20/iso20_DC_Decoder.h"
/* These are internal PROTOCOL_GENERATED qualification objects, never PROJECT/RTE ABI. */
static struct iso20_exiDocument common, common_read;
static struct iso20_ac_exiDocument ac, ac_read;
static struct iso20_dc_exiDocument dc, dc_read;
static bool complete(const exi_bitstream_t *stream, const uint8_t *bytes, size_t length)
{
    unsigned padding;
    if (length == 0u || exi_bitstream_get_length(stream) != length || stream->byte_pos*8u+stream->bit_count > length*8u) { return false; }
    padding = stream->bit_count < 8u ? (1u << (8u-stream->bit_count))-1u : 0u;
    return (bytes[length-1u]&padding) == 0u;
}
int main(void)
{
    uint8_t bytes[4097] = {0}; exi_bitstream_t stream; size_t length, cut;
    init_iso20_exiDocument(&common); common.SessionSetupReq_isUsed = 1u;
    common.SessionSetupReq.Header.SessionID.bytesLen = 8u; common.SessionSetupReq.Header.TimeStamp = 1700000000u;
    common.SessionSetupReq.EVCCID.charactersLen = 12u; (void)memcpy(common.SessionSetupReq.EVCCID.characters,"AABBCCDDEEFF",12u);
    exi_bitstream_init(&stream,bytes,sizeof(bytes)-1u,0u,NULL);
    TEST_ASSERT(encode_iso20_exiDocument(&stream,&common) == 0); length = exi_bitstream_get_length(&stream);
    exi_bitstream_init(&stream,bytes,length,0u,NULL); init_iso20_exiDocument(&common_read);
    TEST_ASSERT(decode_iso20_exiDocument(&stream,&common_read) == 0 && complete(&stream,bytes,length) && common_read.SessionSetupReq_isUsed && common_read.SessionSetupReq.Header.SessionID.bytesLen == 8u && common_read.SessionSetupReq.EVCCID.charactersLen == 12u);
    for (cut = 0u; cut < length; ++cut) { exi_bitstream_init(&stream,bytes,cut,0u,NULL); init_iso20_exiDocument(&common_read); TEST_ASSERT((decode_iso20_exiDocument(&stream,&common_read) != 0 || !complete(&stream,bytes,cut))); }
    init_iso20_ac_exiDocument(&ac); ac.AC_ChargeParameterDiscoveryReq_isUsed = 1u;
    ac.AC_ChargeParameterDiscoveryReq.Header.SessionID.bytesLen = 8u;
    ac.AC_ChargeParameterDiscoveryReq.AC_CPDReqEnergyTransferMode_isUsed = 1u;
    ac.AC_ChargeParameterDiscoveryReq.AC_CPDReqEnergyTransferMode.EVMaximumChargePower.Value = 7360;
    ac.AC_ChargeParameterDiscoveryReq.AC_CPDReqEnergyTransferMode.EVMinimumChargePower.Value = 1380;
    exi_bitstream_init(&stream,bytes,sizeof(bytes)-1u,0u,NULL); TEST_ASSERT(encode_iso20_ac_exiDocument(&stream,&ac) == 0); length = exi_bitstream_get_length(&stream);
    exi_bitstream_init(&stream,bytes,length,0u,NULL); init_iso20_ac_exiDocument(&ac_read);
    TEST_ASSERT(decode_iso20_ac_exiDocument(&stream,&ac_read) == 0 && complete(&stream,bytes,length) && ac_read.AC_ChargeParameterDiscoveryReq_isUsed && ac_read.AC_ChargeParameterDiscoveryReq.AC_CPDReqEnergyTransferMode.EVMaximumChargePower.Value == 7360);
    exi_bitstream_init(&stream,bytes,length,0u,NULL); init_iso20_dc_exiDocument(&dc_read); TEST_ASSERT(decode_iso20_dc_exiDocument(&stream,&dc_read) != 0);
    for (cut = 0u; cut < length; ++cut) { exi_bitstream_init(&stream,bytes,cut,0u,NULL); init_iso20_ac_exiDocument(&ac_read); TEST_ASSERT((decode_iso20_ac_exiDocument(&stream,&ac_read) != 0 || !complete(&stream,bytes,cut))); }
    init_iso20_dc_exiDocument(&dc); dc.DC_PreChargeReq_isUsed = 1u; dc.DC_PreChargeReq.Header.SessionID.bytesLen = 8u;
    dc.DC_PreChargeReq.EVPresentVoltage.Value = 350; dc.DC_PreChargeReq.EVTargetVoltage.Value = 400;
    exi_bitstream_init(&stream,bytes,sizeof(bytes)-1u,0u,NULL); TEST_ASSERT(encode_iso20_dc_exiDocument(&stream,&dc) == 0); length = exi_bitstream_get_length(&stream);
    exi_bitstream_init(&stream,bytes,length,0u,NULL); init_iso20_dc_exiDocument(&dc_read);
    TEST_ASSERT(decode_iso20_dc_exiDocument(&stream,&dc_read) == 0 && complete(&stream,bytes,length) && dc_read.DC_PreChargeReq_isUsed && dc_read.DC_PreChargeReq.EVTargetVoltage.Value == 400);
    for (cut = 0u; cut < length; ++cut) { exi_bitstream_init(&stream,bytes,cut,0u,NULL); init_iso20_dc_exiDocument(&dc_read); TEST_ASSERT((decode_iso20_dc_exiDocument(&stream,&dc_read) != 0 || !complete(&stream,bytes,cut))); }
    return 0;
}
