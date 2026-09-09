#include "openv2g/charging_coordinator/bpt_coordinator.h"
#include "openv2g/charging_coordinator/charge_state_machine.h"
#include "openv2g/charging_coordinator/charging_coordinator.h"
#include "openv2g/charging_coordinator/fault_coordinator.h"
#include "openv2g/charging_coordinator/permission_manager.h"
#include "openv2g/common/result.h"
#include "openv2g/common/types.h"
#include "openv2g/common/validation.h"
#include "openv2g/config.h"
#include "openv2g/iso15118_chrg/iso15118_chrg.h"
#include "openv2g/iso15118_chrg/sdp_client.h"
#include "openv2g/iso15118_chrg/subsystems.h"
#include "openv2g/iso15118_chrg/v2gtp_codec.h"
#include "openv2g/platform/platform_services.h"
#include "openv2g/support/charge_hardware_adapter.h"
#include "openv2g/support/cp_pp_manager.h"
#include "openv2g/support/energy_data_provider.h"
#include "openv2g/support/plc_link_manager.h"
#include "openv2g/version.h"

int main(void)
{
    return 0;
}
