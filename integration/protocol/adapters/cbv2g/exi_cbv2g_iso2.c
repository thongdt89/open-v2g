/* Project mapper for the pinned iso2 generated model; host DC EIM only. */
#include "openv2g/exi_cbv2g_dc.h"
#include <limits.h>
#include <string.h>
#include "cbv2g/iso_2/iso2_msgDefDatatypes.h"
#include "cbv2g/iso_2/iso2_msgDefEncoder.h"
#include "cbv2g/iso_2/iso2_msgDefDecoder.h"
#include "cbv2g/common/exi_bitstream.h"
#include "cbv2g_pin.h"

static bool physical_out(struct iso2_PhysicalValueType *target, int64_t milli, iso2_unitSymbolType unit)
{
    int multiplier = -3;
    while ((milli > INT16_MAX || milli < INT16_MIN) && multiplier < 3 && milli % 10 == 0) {
        milli /= 10; ++multiplier;
    }
    if (milli > INT16_MAX || milli < INT16_MIN) { return false; }
    target->Value = (int16_t)milli; target->Multiplier = (int8_t)multiplier; target->Unit = unit;
    return true;
}
static bool physical_in(const struct iso2_PhysicalValueType *source, iso2_unitSymbolType unit, int32_t *milli)
{
    int exponent = (int)source->Multiplier + 3;
    int64_t value = source->Value;
    if (source->Unit != unit || exponent < -6 || exponent > 6) { return false; }
    while (exponent > 0) { value *= 10; --exponent; }
    while (exponent < 0) { if (value % 10 != 0) { return false; } value /= 10; ++exponent; }
    if (value < 0 || value > INT32_MAX) { return false; }
    *milli = (int32_t)value; return true;
}
static void ev_status(struct iso2_DC_EVStatusType *status, const OpenV2g_ExiDcDocument *value)
{
    status->EVReady = value->ready; status->EVErrorCode = iso2_DC_EVErrorCodeType_NO_ERROR;
    status->EVRESSSOC = (int8_t)(value->soc_permille / 10u);
}
static void evse_status(struct iso2_DC_EVSEStatusType *status, const OpenV2g_ExiDcDocument *value)
{
    status->EVSEStatusCode = value->ready ? iso2_DC_EVSEStatusCodeType_EVSE_Ready : iso2_DC_EVSEStatusCodeType_EVSE_NotReady;
    status->EVSEIsolationStatus_isUsed = 1u;
    status->EVSEIsolationStatus = value->isolation_valid ? iso2_isolationLevelType_Valid : iso2_isolationLevelType_Fault;
    status->EVSENotification = iso2_EVSENotificationType_None;
}
static bool status_in(const struct iso2_DC_EVSEStatusType *status, OpenV2g_ExiDcDocument *value)
{
    if (status->EVSENotification != iso2_EVSENotificationType_None) { return false; }
    value->ready = status->EVSEStatusCode == iso2_DC_EVSEStatusCodeType_EVSE_Ready;
    value->isolation_valid = status->EVSEIsolationStatus_isUsed && status->EVSEIsolationStatus == iso2_isolationLevelType_Valid;
    return true;
}
static bool request_status_in(const struct iso2_DC_EVStatusType *status, OpenV2g_ExiDcDocument *value)
{
    if (status->EVErrorCode != iso2_DC_EVErrorCodeType_NO_ERROR || status->EVRESSSOC < 0 || status->EVRESSSOC > 100) { return false; }
    value->ready = status->EVReady != 0; value->soc_permille = (uint16_t)status->EVRESSSOC * 10u; return true;
}
#define REQUIRE(expression) do { if (!(expression)) { return false; } } while (0)
#define OUT_PHYS(target, amount, unit) REQUIRE(physical_out(&(target), (amount), iso2_unitSymbolType_##unit))
#define IN_PHYS(source, unit, destination) REQUIRE(physical_in(&(source), iso2_unitSymbolType_##unit, &(destination)))

static bool to_generated(const OpenV2g_ExiDcDocument *value, struct iso2_exiDocument *generated)
{
    struct iso2_BodyType *body = &generated->V2G_Message.Body;
    iso2_responseCodeType code = value->accepted ?
        (value->new_session ? iso2_responseCodeType_OK_NewSessionEstablished : iso2_responseCodeType_OK) : iso2_responseCodeType_FAILED;
    if (value->soc_permille > 1000u || value->voltage_mv > INT32_MAX || value->max_voltage_mv > INT32_MAX ||
        value->current_ma < 0 || value->max_current_ma < 0 || value->max_power_w < 0 || value->schedule_power_w < 0) { return false; }
    generated->V2G_Message.Header.SessionID.bytesLen = 8u;
    (void)memcpy(generated->V2G_Message.Header.SessionID.bytes, value->session_id, 8u);
    if (!value->response) {
        switch (value->kind) {
        case OPENV2G_PROTOCOL_MESSAGE_SESSION_SETUP:
            body->SessionSetupReq_isUsed = 1u;
            body->SessionSetupReq.EVCCID.bytesLen = 6u;
            (void)memcpy(body->SessionSetupReq.EVCCID.bytes, value->evcc_id, 6u); break;
        case OPENV2G_PROTOCOL_MESSAGE_SERVICE_DISCOVERY:
            body->ServiceDiscoveryReq_isUsed = 1u; break;
        case OPENV2G_PROTOCOL_MESSAGE_SERVICE_SELECTION:
            body->PaymentServiceSelectionReq_isUsed = 1u;
            body->PaymentServiceSelectionReq.SelectedPaymentOption = iso2_paymentOptionType_ExternalPayment;
            body->PaymentServiceSelectionReq.SelectedServiceList.SelectedService.arrayLen = 1u;
            body->PaymentServiceSelectionReq.SelectedServiceList.SelectedService.array[0].ServiceID = value->service_id; break;
        case OPENV2G_PROTOCOL_MESSAGE_AUTHORIZATION:
            body->AuthorizationReq_isUsed = 1u; break;
        case OPENV2G_PROTOCOL_MESSAGE_CHARGE_PARAMETER: {
            struct iso2_DC_EVChargeParameterType *parameters = &body->ChargeParameterDiscoveryReq.DC_EVChargeParameter;
            body->ChargeParameterDiscoveryReq_isUsed = 1u;
            body->ChargeParameterDiscoveryReq.DC_EVChargeParameter_isUsed = 1u;
            body->ChargeParameterDiscoveryReq.RequestedEnergyTransferMode = iso2_EnergyTransferModeType_DC_core;
            ev_status(&parameters->DC_EVStatus, value);
            OUT_PHYS(parameters->EVMaximumCurrentLimit, value->max_current_ma, A);
            OUT_PHYS(parameters->EVMaximumVoltageLimit, value->max_voltage_mv, V);
            parameters->EVMaximumPowerLimit_isUsed = 1u;
            OUT_PHYS(parameters->EVMaximumPowerLimit, (int64_t)value->max_power_w * 1000, W);
            break;
        }
        case OPENV2G_PROTOCOL_MESSAGE_CABLE_CHECK:
            body->CableCheckReq_isUsed = 1u; ev_status(&body->CableCheckReq.DC_EVStatus, value); break;
        case OPENV2G_PROTOCOL_MESSAGE_PRECHARGE:
            body->PreChargeReq_isUsed = 1u; ev_status(&body->PreChargeReq.DC_EVStatus, value);
            OUT_PHYS(body->PreChargeReq.EVTargetVoltage, value->voltage_mv, V);
            OUT_PHYS(body->PreChargeReq.EVTargetCurrent, value->current_ma, A); break;
        case OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_START:
        case OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_STOP:
            body->PowerDeliveryReq_isUsed = 1u;
            body->PowerDeliveryReq.ChargeProgress = value->kind == OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_START ? iso2_chargeProgressType_Start : iso2_chargeProgressType_Stop;
            body->PowerDeliveryReq.SAScheduleTupleID = value->schedule_id;
            body->PowerDeliveryReq.DC_EVPowerDeliveryParameter_isUsed = 1u;
            ev_status(&body->PowerDeliveryReq.DC_EVPowerDeliveryParameter.DC_EVStatus, value);
            body->PowerDeliveryReq.DC_EVPowerDeliveryParameter.ChargingComplete = value->kind == OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_STOP; break;
        case OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND:
            body->CurrentDemandReq_isUsed = 1u; ev_status(&body->CurrentDemandReq.DC_EVStatus, value);
            OUT_PHYS(body->CurrentDemandReq.EVTargetVoltage, value->voltage_mv, V);
            OUT_PHYS(body->CurrentDemandReq.EVTargetCurrent, value->current_ma, A); break;
        case OPENV2G_PROTOCOL_MESSAGE_WELDING_DETECTION:
            body->WeldingDetectionReq_isUsed = 1u; ev_status(&body->WeldingDetectionReq.DC_EVStatus, value); break;
        case OPENV2G_PROTOCOL_MESSAGE_SESSION_STOP:
            body->SessionStopReq_isUsed = 1u;
            body->SessionStopReq.ChargingSession = iso2_chargingSessionType_Terminate;
            break;
        default: return false;
        }
    } else {
        switch (value->kind) {
        case OPENV2G_PROTOCOL_MESSAGE_SESSION_SETUP:
            body->SessionSetupRes_isUsed = 1u; body->SessionSetupRes.ResponseCode = code;
            REQUIRE(value->evse_id_length > 0u && value->evse_id_length <= sizeof(value->evse_id) && value->evse_id_length < sizeof(body->SessionSetupRes.EVSEID.characters));
            body->SessionSetupRes.EVSEID.charactersLen = value->evse_id_length;
            (void)memcpy(body->SessionSetupRes.EVSEID.characters, value->evse_id, value->evse_id_length); break;
        case OPENV2G_PROTOCOL_MESSAGE_SERVICE_DISCOVERY:
            body->ServiceDiscoveryRes_isUsed = 1u; body->ServiceDiscoveryRes.ResponseCode = code;
            body->ServiceDiscoveryRes.PaymentOptionList.PaymentOption.arrayLen = 1u;
            body->ServiceDiscoveryRes.PaymentOptionList.PaymentOption.array[0] = iso2_paymentOptionType_ExternalPayment;
            body->ServiceDiscoveryRes.ChargeService.ServiceID = value->service_id;
            body->ServiceDiscoveryRes.ChargeService.ServiceCategory = iso2_serviceCategoryType_EVCharging;
            body->ServiceDiscoveryRes.ChargeService.SupportedEnergyTransferMode.EnergyTransferMode.arrayLen = 1u;
            body->ServiceDiscoveryRes.ChargeService.SupportedEnergyTransferMode.EnergyTransferMode.array[0] = iso2_EnergyTransferModeType_DC_core;
            body->ServiceDiscoveryRes.ChargeService.FreeService = 1; break;
        case OPENV2G_PROTOCOL_MESSAGE_SERVICE_SELECTION:
            body->PaymentServiceSelectionRes_isUsed = 1u; body->PaymentServiceSelectionRes.ResponseCode = code; break;
        case OPENV2G_PROTOCOL_MESSAGE_AUTHORIZATION:
            body->AuthorizationRes_isUsed = 1u; body->AuthorizationRes.ResponseCode = code;
            body->AuthorizationRes.EVSEProcessing = value->ongoing ? iso2_EVSEProcessingType_Ongoing : iso2_EVSEProcessingType_Finished; break;
        case OPENV2G_PROTOCOL_MESSAGE_CHARGE_PARAMETER: {
            struct iso2_DC_EVSEChargeParameterType *parameters = &body->ChargeParameterDiscoveryRes.DC_EVSEChargeParameter;
            struct iso2_SAScheduleTupleType *tuple = &body->ChargeParameterDiscoveryRes.SAScheduleList.SAScheduleTuple.array[0];
            body->ChargeParameterDiscoveryRes_isUsed = 1u; body->ChargeParameterDiscoveryRes.ResponseCode = code;
            body->ChargeParameterDiscoveryRes.EVSEProcessing = value->ongoing ? iso2_EVSEProcessingType_Ongoing : iso2_EVSEProcessingType_Finished;
            body->ChargeParameterDiscoveryRes.DC_EVSEChargeParameter_isUsed = 1u;
            evse_status(&parameters->DC_EVSEStatus, value);
            OUT_PHYS(parameters->EVSEMaximumVoltageLimit, value->max_voltage_mv, V);
            OUT_PHYS(parameters->EVSEMaximumCurrentLimit, value->max_current_ma, A);
            OUT_PHYS(parameters->EVSEMaximumPowerLimit, (int64_t)value->max_power_w * 1000, W);
            OUT_PHYS(parameters->EVSEMinimumVoltageLimit, 0, V);
            OUT_PHYS(parameters->EVSEMinimumCurrentLimit, 0, A);
            OUT_PHYS(parameters->EVSEPeakCurrentRipple, 0, A);
            body->ChargeParameterDiscoveryRes.SAScheduleList_isUsed = 1u;
            body->ChargeParameterDiscoveryRes.SAScheduleList.SAScheduleTuple.arrayLen = 1u;
            tuple->SAScheduleTupleID = value->schedule_id;
            tuple->PMaxSchedule.PMaxScheduleEntry.arrayLen = 1u;
            tuple->PMaxSchedule.PMaxScheduleEntry.array[0].RelativeTimeInterval_isUsed = 1u;
            tuple->PMaxSchedule.PMaxScheduleEntry.array[0].RelativeTimeInterval.duration_isUsed = 1u;
            tuple->PMaxSchedule.PMaxScheduleEntry.array[0].RelativeTimeInterval.duration = value->schedule_duration_s;
            OUT_PHYS(tuple->PMaxSchedule.PMaxScheduleEntry.array[0].PMax, (int64_t)value->schedule_power_w * 1000, W);
            break;
        }
        case OPENV2G_PROTOCOL_MESSAGE_CABLE_CHECK:
            body->CableCheckRes_isUsed = 1u; body->CableCheckRes.ResponseCode = code;
            evse_status(&body->CableCheckRes.DC_EVSEStatus, value);
            body->CableCheckRes.EVSEProcessing = value->ongoing ? iso2_EVSEProcessingType_Ongoing : iso2_EVSEProcessingType_Finished; break;
        case OPENV2G_PROTOCOL_MESSAGE_PRECHARGE:
            body->PreChargeRes_isUsed = 1u; body->PreChargeRes.ResponseCode = code;
            evse_status(&body->PreChargeRes.DC_EVSEStatus, value);
            OUT_PHYS(body->PreChargeRes.EVSEPresentVoltage, value->voltage_mv, V); break;
        case OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_START:
        case OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_STOP:
            body->PowerDeliveryRes_isUsed = 1u; body->PowerDeliveryRes.ResponseCode = code;
            body->PowerDeliveryRes.DC_EVSEStatus_isUsed = 1u;
            evse_status(&body->PowerDeliveryRes.DC_EVSEStatus, value); break;
        case OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND:
            body->CurrentDemandRes_isUsed = 1u; body->CurrentDemandRes.ResponseCode = code;
            evse_status(&body->CurrentDemandRes.DC_EVSEStatus, value);
            OUT_PHYS(body->CurrentDemandRes.EVSEPresentVoltage, value->voltage_mv, V);
            OUT_PHYS(body->CurrentDemandRes.EVSEPresentCurrent, value->current_ma, A);
            REQUIRE(value->evse_id_length > 0u && value->evse_id_length <= sizeof(value->evse_id) && value->evse_id_length < sizeof(body->CurrentDemandRes.EVSEID.characters));
            body->CurrentDemandRes.EVSEID.charactersLen = value->evse_id_length;
            (void)memcpy(body->CurrentDemandRes.EVSEID.characters, value->evse_id, value->evse_id_length);
            body->CurrentDemandRes.SAScheduleTupleID = value->schedule_id;
            break;
        case OPENV2G_PROTOCOL_MESSAGE_WELDING_DETECTION:
            body->WeldingDetectionRes_isUsed = 1u; body->WeldingDetectionRes.ResponseCode = code;
            evse_status(&body->WeldingDetectionRes.DC_EVSEStatus, value);
            OUT_PHYS(body->WeldingDetectionRes.EVSEPresentVoltage, value->voltage_mv, V); break;
        case OPENV2G_PROTOCOL_MESSAGE_SESSION_STOP:
            body->SessionStopRes_isUsed = 1u; body->SessionStopRes.ResponseCode = code; break;
        default: return false;
        }
    }
    return true;
}
static bool from_generated(const struct iso2_exiDocument *generated, OpenV2g_ExiDcDocument *value)
{
    const struct iso2_BodyType *body = &generated->V2G_Message.Body;
    int32_t physical;
    unsigned selected = 0u;
    (void)physical;
    REQUIRE(generated->V2G_Message.Header.SessionID.bytesLen == 8u);
    REQUIRE(!generated->V2G_Message.Header.Signature_isUsed && !generated->V2G_Message.Header.Notification_isUsed);
    (void)memcpy(value->session_id, generated->V2G_Message.Header.SessionID.bytes, 8u);
    if (body->SessionSetupReq_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_SESSION_SETUP; value->response = false;
        REQUIRE(body->SessionSetupReq.EVCCID.bytesLen == 6u); (void)memcpy(value->evcc_id, body->SessionSetupReq.EVCCID.bytes, 6u);
    }
    if (body->SessionSetupRes_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_SESSION_SETUP; value->response = true;
        value->accepted = body->SessionSetupRes.ResponseCode == iso2_responseCodeType_OK || body->SessionSetupRes.ResponseCode == iso2_responseCodeType_OK_NewSessionEstablished;
        value->new_session = body->SessionSetupRes.ResponseCode == iso2_responseCodeType_OK_NewSessionEstablished;
        REQUIRE(body->SessionSetupRes.EVSEID.charactersLen > 0u && body->SessionSetupRes.EVSEID.charactersLen <= sizeof(value->evse_id));
        value->evse_id_length = body->SessionSetupRes.EVSEID.charactersLen;
        (void)memcpy(value->evse_id, body->SessionSetupRes.EVSEID.characters, value->evse_id_length);
    }
    if (body->ServiceDiscoveryReq_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_SERVICE_DISCOVERY; value->response = false;
        REQUIRE(!body->ServiceDiscoveryReq.ServiceScope_isUsed && !body->ServiceDiscoveryReq.ServiceCategory_isUsed);
    }
    if (body->ServiceDiscoveryRes_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_SERVICE_DISCOVERY; value->response = true;
        value->accepted = body->ServiceDiscoveryRes.ResponseCode == iso2_responseCodeType_OK;
        REQUIRE(body->ServiceDiscoveryRes.PaymentOptionList.PaymentOption.arrayLen == 1u && body->ServiceDiscoveryRes.PaymentOptionList.PaymentOption.array[0] == iso2_paymentOptionType_ExternalPayment);
        REQUIRE(body->ServiceDiscoveryRes.ChargeService.SupportedEnergyTransferMode.EnergyTransferMode.arrayLen == 1u && body->ServiceDiscoveryRes.ChargeService.SupportedEnergyTransferMode.EnergyTransferMode.array[0] == iso2_EnergyTransferModeType_DC_core);
        REQUIRE(body->ServiceDiscoveryRes.ChargeService.ServiceCategory == iso2_serviceCategoryType_EVCharging && !body->ServiceDiscoveryRes.ServiceList_isUsed);
        value->service_id = body->ServiceDiscoveryRes.ChargeService.ServiceID;
    }
    if (body->PaymentServiceSelectionReq_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_SERVICE_SELECTION; value->response = false;
        REQUIRE(body->PaymentServiceSelectionReq.SelectedPaymentOption == iso2_paymentOptionType_ExternalPayment && body->PaymentServiceSelectionReq.SelectedServiceList.SelectedService.arrayLen == 1u);
        REQUIRE(!body->PaymentServiceSelectionReq.SelectedServiceList.SelectedService.array[0].ParameterSetID_isUsed);
        value->service_id = body->PaymentServiceSelectionReq.SelectedServiceList.SelectedService.array[0].ServiceID;
    }
    if (body->PaymentServiceSelectionRes_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_SERVICE_SELECTION; value->response = true;
        value->accepted = body->PaymentServiceSelectionRes.ResponseCode == iso2_responseCodeType_OK;
    }
    if (body->AuthorizationReq_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_AUTHORIZATION; value->response = false;
        REQUIRE(!body->AuthorizationReq.Id_isUsed && !body->AuthorizationReq.GenChallenge_isUsed);
    }
    if (body->AuthorizationRes_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_AUTHORIZATION; value->response = true;
        value->accepted = body->AuthorizationRes.ResponseCode == iso2_responseCodeType_OK;
        REQUIRE(body->AuthorizationRes.EVSEProcessing == iso2_EVSEProcessingType_Finished || body->AuthorizationRes.EVSEProcessing == iso2_EVSEProcessingType_Ongoing);
        value->ongoing = body->AuthorizationRes.EVSEProcessing != iso2_EVSEProcessingType_Finished;
    }
    if (body->ChargeParameterDiscoveryReq_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_CHARGE_PARAMETER; value->response = false;
        REQUIRE(body->ChargeParameterDiscoveryReq.DC_EVChargeParameter_isUsed && !body->ChargeParameterDiscoveryReq.AC_EVChargeParameter_isUsed && !body->ChargeParameterDiscoveryReq.EVChargeParameter_isUsed);
        REQUIRE(body->ChargeParameterDiscoveryReq.RequestedEnergyTransferMode == iso2_EnergyTransferModeType_DC_core);
        REQUIRE(request_status_in(&body->ChargeParameterDiscoveryReq.DC_EVChargeParameter.DC_EVStatus, value));
        IN_PHYS(body->ChargeParameterDiscoveryReq.DC_EVChargeParameter.EVMaximumVoltageLimit, V, physical); value->max_voltage_mv = (uint32_t)physical;
        IN_PHYS(body->ChargeParameterDiscoveryReq.DC_EVChargeParameter.EVMaximumCurrentLimit, A, value->max_current_ma);
        REQUIRE(body->ChargeParameterDiscoveryReq.DC_EVChargeParameter.EVMaximumPowerLimit_isUsed);
        IN_PHYS(body->ChargeParameterDiscoveryReq.DC_EVChargeParameter.EVMaximumPowerLimit, W, physical); REQUIRE(physical % 1000 == 0); value->max_power_w = physical / 1000;
    }
    if (body->ChargeParameterDiscoveryRes_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_CHARGE_PARAMETER; value->response = true;
        value->accepted = body->ChargeParameterDiscoveryRes.ResponseCode == iso2_responseCodeType_OK;
        REQUIRE(body->ChargeParameterDiscoveryRes.EVSEProcessing == iso2_EVSEProcessingType_Finished || body->ChargeParameterDiscoveryRes.EVSEProcessing == iso2_EVSEProcessingType_Ongoing);
        value->ongoing = body->ChargeParameterDiscoveryRes.EVSEProcessing != iso2_EVSEProcessingType_Finished;
        REQUIRE(body->ChargeParameterDiscoveryRes.DC_EVSEChargeParameter_isUsed && !body->ChargeParameterDiscoveryRes.AC_EVSEChargeParameter_isUsed && !body->ChargeParameterDiscoveryRes.EVSEChargeParameter_isUsed && !body->ChargeParameterDiscoveryRes.SASchedules_isUsed);
        REQUIRE(status_in(&body->ChargeParameterDiscoveryRes.DC_EVSEChargeParameter.DC_EVSEStatus, value));
        IN_PHYS(body->ChargeParameterDiscoveryRes.DC_EVSEChargeParameter.EVSEMinimumVoltageLimit, V, physical); REQUIRE(physical == 0);
        IN_PHYS(body->ChargeParameterDiscoveryRes.DC_EVSEChargeParameter.EVSEMinimumCurrentLimit, A, physical); REQUIRE(physical == 0);
        REQUIRE(!body->ChargeParameterDiscoveryRes.DC_EVSEChargeParameter.EVSECurrentRegulationTolerance_isUsed);
        REQUIRE(!body->ChargeParameterDiscoveryRes.DC_EVSEChargeParameter.EVSEEnergyToBeDelivered_isUsed);
        IN_PHYS(body->ChargeParameterDiscoveryRes.DC_EVSEChargeParameter.EVSEMaximumVoltageLimit, V, physical); value->max_voltage_mv = (uint32_t)physical;
        IN_PHYS(body->ChargeParameterDiscoveryRes.DC_EVSEChargeParameter.EVSEMaximumCurrentLimit, A, value->max_current_ma);
        IN_PHYS(body->ChargeParameterDiscoveryRes.DC_EVSEChargeParameter.EVSEMaximumPowerLimit, W, physical); REQUIRE(physical % 1000 == 0); value->max_power_w = physical / 1000;
        REQUIRE(body->ChargeParameterDiscoveryRes.SAScheduleList_isUsed && body->ChargeParameterDiscoveryRes.SAScheduleList.SAScheduleTuple.arrayLen == 1u);
        REQUIRE(body->ChargeParameterDiscoveryRes.SAScheduleList.SAScheduleTuple.array[0].PMaxSchedule.PMaxScheduleEntry.arrayLen == 1u);
        REQUIRE(!body->ChargeParameterDiscoveryRes.SAScheduleList.SAScheduleTuple.array[0].SalesTariff_isUsed);
        REQUIRE(body->ChargeParameterDiscoveryRes.SAScheduleList.SAScheduleTuple.array[0].SAScheduleTupleID > 0 && body->ChargeParameterDiscoveryRes.SAScheduleList.SAScheduleTuple.array[0].SAScheduleTupleID <= UINT8_MAX);
        value->schedule_id = (uint8_t)body->ChargeParameterDiscoveryRes.SAScheduleList.SAScheduleTuple.array[0].SAScheduleTupleID;
        REQUIRE(body->ChargeParameterDiscoveryRes.SAScheduleList.SAScheduleTuple.array[0].PMaxSchedule.PMaxScheduleEntry.array[0].RelativeTimeInterval_isUsed);
        REQUIRE(!body->ChargeParameterDiscoveryRes.SAScheduleList.SAScheduleTuple.array[0].PMaxSchedule.PMaxScheduleEntry.array[0].TimeInterval_isUsed);
        REQUIRE(body->ChargeParameterDiscoveryRes.SAScheduleList.SAScheduleTuple.array[0].PMaxSchedule.PMaxScheduleEntry.array[0].RelativeTimeInterval.start == 0u);
        REQUIRE(body->ChargeParameterDiscoveryRes.SAScheduleList.SAScheduleTuple.array[0].PMaxSchedule.PMaxScheduleEntry.array[0].RelativeTimeInterval.duration_isUsed);
        value->schedule_duration_s = body->ChargeParameterDiscoveryRes.SAScheduleList.SAScheduleTuple.array[0].PMaxSchedule.PMaxScheduleEntry.array[0].RelativeTimeInterval.duration;
        IN_PHYS(body->ChargeParameterDiscoveryRes.SAScheduleList.SAScheduleTuple.array[0].PMaxSchedule.PMaxScheduleEntry.array[0].PMax, W, physical); REQUIRE(physical % 1000 == 0); value->schedule_power_w = physical / 1000;
    }
    if (body->CableCheckReq_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_CABLE_CHECK; value->response = false;
        REQUIRE(request_status_in(&body->CableCheckReq.DC_EVStatus, value));
    }
    if (body->CableCheckRes_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_CABLE_CHECK; value->response = true;
        value->accepted = body->CableCheckRes.ResponseCode == iso2_responseCodeType_OK;
        REQUIRE(body->CableCheckRes.EVSEProcessing == iso2_EVSEProcessingType_Finished || body->CableCheckRes.EVSEProcessing == iso2_EVSEProcessingType_Ongoing);
        value->ongoing = body->CableCheckRes.EVSEProcessing != iso2_EVSEProcessingType_Finished;
        REQUIRE(status_in(&body->CableCheckRes.DC_EVSEStatus, value));
    }
    if (body->PreChargeReq_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_PRECHARGE; value->response = false;
        REQUIRE(request_status_in(&body->PreChargeReq.DC_EVStatus, value));
        IN_PHYS(body->PreChargeReq.EVTargetVoltage, V, physical); value->voltage_mv = (uint32_t)physical;
        IN_PHYS(body->PreChargeReq.EVTargetCurrent, A, value->current_ma);
    }
    if (body->PreChargeRes_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_PRECHARGE; value->response = true;
        value->accepted = body->PreChargeRes.ResponseCode == iso2_responseCodeType_OK;
        REQUIRE(status_in(&body->PreChargeRes.DC_EVSEStatus, value));
        IN_PHYS(body->PreChargeRes.EVSEPresentVoltage, V, physical); value->voltage_mv = (uint32_t)physical;
    }
    if (body->PowerDeliveryReq_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_START; value->response = false;
        REQUIRE(!body->PowerDeliveryReq.ChargingProfile_isUsed && !body->PowerDeliveryReq.EVPowerDeliveryParameter_isUsed);
        REQUIRE(body->PowerDeliveryReq.DC_EVPowerDeliveryParameter_isUsed);
        REQUIRE(request_status_in(&body->PowerDeliveryReq.DC_EVPowerDeliveryParameter.DC_EVStatus, value));
        REQUIRE(body->PowerDeliveryReq.ChargeProgress == iso2_chargeProgressType_Start || body->PowerDeliveryReq.ChargeProgress == iso2_chargeProgressType_Stop);
        value->kind = body->PowerDeliveryReq.ChargeProgress == iso2_chargeProgressType_Start ? OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_START : OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_STOP;
        value->schedule_id = body->PowerDeliveryReq.SAScheduleTupleID;
    }
    if (body->PowerDeliveryRes_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_START; value->response = true;
        value->accepted = body->PowerDeliveryRes.ResponseCode == iso2_responseCodeType_OK;
        REQUIRE(body->PowerDeliveryRes.DC_EVSEStatus_isUsed && !body->PowerDeliveryRes.AC_EVSEStatus_isUsed && !body->PowerDeliveryRes.EVSEStatus_isUsed);
        REQUIRE(status_in(&body->PowerDeliveryRes.DC_EVSEStatus, value));
    }
    if (body->CurrentDemandReq_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND; value->response = false;
        REQUIRE(request_status_in(&body->CurrentDemandReq.DC_EVStatus, value));
        IN_PHYS(body->CurrentDemandReq.EVTargetVoltage, V, physical); value->voltage_mv = (uint32_t)physical;
        IN_PHYS(body->CurrentDemandReq.EVTargetCurrent, A, value->current_ma);
    }
    if (body->CurrentDemandRes_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND; value->response = true;
        value->accepted = body->CurrentDemandRes.ResponseCode == iso2_responseCodeType_OK;
        REQUIRE(status_in(&body->CurrentDemandRes.DC_EVSEStatus, value));
        IN_PHYS(body->CurrentDemandRes.EVSEPresentVoltage, V, physical); value->voltage_mv = (uint32_t)physical;
        IN_PHYS(body->CurrentDemandRes.EVSEPresentCurrent, A, value->current_ma);
        REQUIRE(!body->CurrentDemandRes.EVSEMaximumVoltageLimit_isUsed && !body->CurrentDemandRes.EVSEMaximumCurrentLimit_isUsed && !body->CurrentDemandRes.EVSEMaximumPowerLimit_isUsed);
        REQUIRE(!body->CurrentDemandRes.EVSECurrentLimitAchieved && !body->CurrentDemandRes.EVSEVoltageLimitAchieved && !body->CurrentDemandRes.EVSEPowerLimitAchieved);
        REQUIRE(!body->CurrentDemandRes.MeterInfo_isUsed && (!body->CurrentDemandRes.ReceiptRequired_isUsed || !body->CurrentDemandRes.ReceiptRequired));
        value->schedule_id = body->CurrentDemandRes.SAScheduleTupleID;
        REQUIRE(body->CurrentDemandRes.EVSEID.charactersLen > 0u && body->CurrentDemandRes.EVSEID.charactersLen <= sizeof(value->evse_id));
        value->evse_id_length = body->CurrentDemandRes.EVSEID.charactersLen;
        (void)memcpy(value->evse_id, body->CurrentDemandRes.EVSEID.characters, value->evse_id_length);
    }
    if (body->WeldingDetectionReq_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_WELDING_DETECTION; value->response = false;
        REQUIRE(request_status_in(&body->WeldingDetectionReq.DC_EVStatus, value));
    }
    if (body->WeldingDetectionRes_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_WELDING_DETECTION; value->response = true;
        value->accepted = body->WeldingDetectionRes.ResponseCode == iso2_responseCodeType_OK;
        REQUIRE(status_in(&body->WeldingDetectionRes.DC_EVSEStatus, value));
        IN_PHYS(body->WeldingDetectionRes.EVSEPresentVoltage, V, physical); value->voltage_mv = (uint32_t)physical;
    }
    if (body->SessionStopReq_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_SESSION_STOP; value->response = false;
    }
    if (body->SessionStopRes_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_SESSION_STOP; value->response = true;
        value->accepted = body->SessionStopRes.ResponseCode == iso2_responseCodeType_OK;
    }
    REQUIRE(selected == 1u);
    return true;
}

static OpenV2g_Result rejected(OpenV2g_ExiDiagnostic *diagnostic)
{
    if (diagnostic != NULL) { diagnostic->reason = OPENV2G_REASON_FORMAT; }
    return OPENV2G_RESULT_REJECTED;
}
static OpenV2g_Result encode(void *context, uint16_t id, const void *object,
    size_t size, uint8_t *output, size_t capacity, size_t *length, OpenV2g_ExiDiagnostic *diagnostic)
{
    const OpenV2g_ExiDcDocument *value = object;
    struct iso2_exiDocument generated;
    uint8_t guarded[OPENV2G_EXI_DC_PAYLOAD_MAX + 1u] = {0};
    exi_bitstream_t stream;
    int error;
    (void)context;
    if (length != NULL) { *length = 0u; }
    if (value == NULL || size != sizeof(*value) || output == NULL || capacity == 0u || length == NULL ||
        id != (uint16_t)((unsigned)value->kind + (value->response ? OPENV2G_EXI_DC_RESPONSE_OFFSET : 0u))) { return rejected(diagnostic); }
    (void)memset(&generated, 0, sizeof(generated));
    init_iso2_exiDocument(&generated);
    if (!to_generated(value, &generated)) { return rejected(diagnostic); }
    if (capacity > OPENV2G_EXI_DC_PAYLOAD_MAX) { capacity = OPENV2G_EXI_DC_PAYLOAD_MAX; }
    exi_bitstream_init(&stream, guarded, capacity, 0u, NULL);
    error = encode_iso2_exiDocument(&stream, &generated);
    if (error != 0 || exi_bitstream_get_length(&stream) > capacity) { return rejected(diagnostic); }
    *length = exi_bitstream_get_length(&stream); (void)memcpy(output, guarded, *length);
    return OPENV2G_RESULT_OK;
}
static OpenV2g_Result decode(void *context, const uint8_t *input, size_t length,
    void *object, size_t size, uint8_t *workspace, size_t capacity, uint16_t *id, OpenV2g_ExiDiagnostic *diagnostic)
{
    struct iso2_exiDocument generated;
    OpenV2g_ExiDcDocument *value = object;
    exi_bitstream_t stream;
    uint32_t padding;
    (void)context;
    if (input == NULL || length == 0u || length > OPENV2G_EXI_DC_PAYLOAD_MAX || value == NULL || size != sizeof(*value) ||
        workspace == NULL || capacity < OPENV2G_EXI_DC_WORKSPACE_SIZE || id == NULL) { return rejected(diagnostic); }
    (void)memset(workspace, 0, capacity); (void)memcpy(workspace, input, length);
    (void)memset(&generated, 0, sizeof(generated)); init_iso2_exiDocument(&generated);
    exi_bitstream_init(&stream, workspace, length, 0u, NULL);
    if (decode_iso2_exiDocument(&stream, &generated) != 0 || exi_bitstream_get_length(&stream) != length ||
        stream.byte_pos * 8u + stream.bit_count > length * 8u) { return rejected(diagnostic); }
    padding = stream.bit_count < 8u ? (1u << (8u - stream.bit_count)) - 1u : 0u;
    if ((workspace[length - 1u] & padding) != 0u) { return rejected(diagnostic); }
    (void)memset(value, 0, sizeof(*value));
    if (!from_generated(&generated, value)) { return rejected(diagnostic); }
    *id = (uint16_t)((unsigned)value->kind + (value->response ? OPENV2G_EXI_DC_RESPONSE_OFFSET : 0u));
    /* PowerDeliveryRes has no START/STOP discriminator. Caller validates it
     * against its pending request and uses the shared response DTO id below. */
    return OPENV2G_RESULT_OK;
}
static void reset(void *context, uint8_t *workspace, size_t capacity)
{
    (void)context; if (workspace != NULL) { (void)memset(workspace, 0, capacity); }
}
OpenV2g_Result OpenV2g_Cbv2gIso2_Bind(OpenV2g_ExiCodec *codec)
{
    static const OpenV2g_SchemaDescriptor descriptor = {
        OPENV2G_SCHEMA_ISO_15118_2, OPENV2G_PROTOCOL_ISO_15118_2, OPENV2G_MODE_DC,
        OPENV2G_EXI_DC_PAYLOAD_MAX, OPENV2G_EXI_DC_WORKSPACE_SIZE, sizeof(OpenV2g_ExiDcDocument), 1u,
        "libcbv2g-iso2-dc-host/" OPENV2G_CBV2G_COMMIT,
        "upstream generated model; exact XSD provenance OPEN", OPENV2G_CBV2G_DIGEST
    };
    const OpenV2g_ExiBackend backend = {NULL, encode, decode, reset};
    return OpenV2g_ExiCodec_Bind(codec, &descriptor, &backend);
}
