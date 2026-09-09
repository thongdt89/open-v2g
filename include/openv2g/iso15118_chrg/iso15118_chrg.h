#ifndef OPENV2G_ISO15118_CHRG_H
#define OPENV2G_ISO15118_CHRG_H

#include <stdbool.h>
#include <stdint.h>

#include "openv2g/common/result.h"
#include "openv2g/common/types.h"
#include "openv2g/iso15118_chrg/sdp_client.h"
#include "openv2g/iso15118_chrg/subsystems.h"

typedef struct {
    bool din_70121_dc_eim;
    bool iso_15118_2_dc_eim;
    bool iso_15118_2_ac_eim;
    bool iso_15118_2_pnc;
    bool iso_15118_20;
    bool bpt;
} OpenV2g_HlcFeatureConfig;

typedef struct {
    uint32_t generation;
    OpenV2g_HlcSessionStatus status;
    OpenV2g_HlcFeatureConfig features;
    OpenV2g_SdpClient sdp_client;
    OpenV2g_Subsystem tcp_tls_session;
    OpenV2g_Subsystem v2gtp_codec;
    OpenV2g_Subsystem exi_codec;
    OpenV2g_Subsystem schema_registry;
    OpenV2g_Subsystem sap_negotiator;
    OpenV2g_Subsystem protocol_adapter_factory;
    OpenV2g_Subsystem din_70121_adapter;
    OpenV2g_Subsystem iso_15118_2_adapter;
    OpenV2g_Subsystem iso_15118_20_adapter;
    OpenV2g_Subsystem pki_security_adapter;
    OpenV2g_Subsystem certificate_manager;
} OpenV2g_Iso15118Chrg;

void OpenV2g_Iso15118Chrg_Init(
    OpenV2g_Iso15118Chrg *hlc,
    const OpenV2g_HlcFeatureConfig *features);

OpenV2g_Result OpenV2g_Iso15118Chrg_Start(
    OpenV2g_Iso15118Chrg *hlc,
    uint32_t generation,
    OpenV2g_TimeUs now);

OpenV2g_Result OpenV2g_Iso15118Chrg_OnSdpComplete(
    OpenV2g_Iso15118Chrg *hlc,
    uint32_t generation,
    OpenV2g_TimeUs now,
    bool endpoint_valid);

OpenV2g_Result OpenV2g_Iso15118Chrg_OnTransportReady(
    OpenV2g_Iso15118Chrg *hlc,
    uint32_t generation,
    OpenV2g_TimeUs now);

OpenV2g_Result OpenV2g_Iso15118Chrg_OnProtocolSelected(
    OpenV2g_Iso15118Chrg *hlc,
    uint32_t generation,
    OpenV2g_TimeUs now,
    OpenV2g_ProtocolId protocol,
    OpenV2g_SchemaId schema);

OpenV2g_Result OpenV2g_Iso15118Chrg_Stop(
    OpenV2g_Iso15118Chrg *hlc,
    uint32_t generation,
    OpenV2g_TimeUs now,
    OpenV2g_ReasonCode reason);

#endif
