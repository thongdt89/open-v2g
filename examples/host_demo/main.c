#include <stdio.h>

#include "openv2g/config.h"
#include "openv2g/iso15118_chrg/iso15118_chrg.h"
#include "openv2g/version.h"

int main(void)
{
    OpenV2g_Iso15118Chrg hlc;

    OpenV2g_Iso15118Chrg_Init(&hlc, NULL);
    (void)printf("open-v2g %d.%d.%d (%s)\n",
                 OPENV2G_VERSION_MAJOR,
                 OPENV2G_VERSION_MINOR,
                 OPENV2G_VERSION_PATCH,
                 OPENV2G_VERSION_STATUS);
    (void)printf("DIN 70121 DC EIM: %s\n",
                 hlc.features.din_70121_dc_eim ? "enabled" : "disabled");
    (void)printf("ISO 15118-2 DC EIM: %s\n",
                 hlc.features.iso_15118_2_dc_eim ? "enabled" : "disabled");
    (void)printf("PnC/ISO-20/BPT: %s/%s/%s\n",
                 hlc.features.iso_15118_2_pnc ? "enabled" : "disabled",
                 hlc.features.iso_15118_20 ? "enabled" : "disabled",
                 hlc.features.bpt ? "enabled" : "disabled");
    return 0;
}
