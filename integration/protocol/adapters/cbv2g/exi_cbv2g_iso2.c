/* Project mapper for the pinned iso2 generated model; bounded DC and single-phase AC EIM host profiles. */
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

static void ac_status_out(struct iso2_AC_EVSEStatusType *target, const OpenV2g_ExiDcDocument *value)
{
    target->RCD = value->rcd_fault; target->EVSENotification = iso2_EVSENotificationType_None;
}
static bool ac_status_in(const struct iso2_AC_EVSEStatusType *source, OpenV2g_ExiDcDocument *value)
{
    REQUIRE(source->EVSENotification == iso2_EVSENotificationType_None);
    value->ac_mode = true; value->rcd_fault = source->RCD != 0; value->ready = !value->rcd_fault;
    return true;
}
static bool meter_out(const OpenV2g_ExiMeterInfo *source, struct iso2_MeterInfoType *target)
{
    REQUIRE(source->id_length > 0u && source->id_length <= sizeof(source->id) && source->id_length < sizeof(target->MeterID.characters));
    REQUIRE(source->signature_length <= sizeof(source->signature) && source->signature_length <= sizeof(target->SigMeterReading.bytes));
    target->MeterID.charactersLen = source->id_length;
    (void)memcpy(target->MeterID.characters, source->id, source->id_length);
    target->MeterReading_isUsed = source->has_reading; target->MeterReading = source->reading_wh;
    target->MeterStatus_isUsed = source->has_status; target->MeterStatus = source->status;
    target->TMeter_isUsed = source->has_time; target->TMeter = source->time_s;
    target->SigMeterReading_isUsed = source->signature_length != 0u;
    target->SigMeterReading.bytesLen = source->signature_length;
    (void)memcpy(target->SigMeterReading.bytes, source->signature, source->signature_length);
    return true;
}
static bool meter_in(const struct iso2_MeterInfoType *source, OpenV2g_ExiMeterInfo *target)
{
    REQUIRE(source->MeterID.charactersLen > 0u && source->MeterID.charactersLen <= sizeof(target->id));
    target->id_length = source->MeterID.charactersLen;
    (void)memcpy(target->id, source->MeterID.characters, target->id_length);
    target->has_reading = source->MeterReading_isUsed; target->reading_wh = source->MeterReading;
    target->has_status = source->MeterStatus_isUsed; target->status = source->MeterStatus;
    target->has_time = source->TMeter_isUsed; target->time_s = source->TMeter;
    if (source->SigMeterReading_isUsed) {
        REQUIRE(source->SigMeterReading.bytesLen > 0u && source->SigMeterReading.bytesLen <= sizeof(target->signature));
        target->signature_length = source->SigMeterReading.bytesLen;
        (void)memcpy(target->signature, source->SigMeterReading.bytes, target->signature_length);
    }
    return true;
}
static bool to_generated(const OpenV2g_ExiDcDocument *value, struct iso2_exiDocument *generated)
{
    struct iso2_BodyType *body = &generated->V2G_Message.Body;
    iso2_responseCodeType code = value->accepted ?
        (value->new_session ? iso2_responseCodeType_OK_NewSessionEstablished : (value->resumed_session ? iso2_responseCodeType_OK_OldSessionJoined : iso2_responseCodeType_OK)) : iso2_responseCodeType_FAILED;
    if (value->soc_permille > 1000u || value->voltage_mv > INT32_MAX || value->max_voltage_mv > INT32_MAX ||
        value->min_voltage_mv > INT32_MAX || value->min_current_ma < 0 ||
        value->current_ma < 0 || value->max_current_ma < 0 || value->max_power_w < 0 || value->schedule_power_w < 0) { return false; }
    generated->V2G_Message.Header.SessionID.bytesLen = 8u;
    (void)memcpy(generated->V2G_Message.Header.SessionID.bytes, value->session_id, 8u);
    if (!value->response) {
        switch (value->kind) {
        case OPENV2G_PROTOCOL_MESSAGE_SESSION_SETUP:
            body->SessionSetupReq_isUsed = 1u;
            body->SessionSetupReq.EVCCID.bytesLen = 6u;
            (void)memcpy(body->SessionSetupReq.EVCCID.bytes, value->evcc_id, 6u); break;
        case OPENV2G_PROTOCOL_MESSAGE_SERVICE_DETAIL:
            body->ServiceDetailReq_isUsed = 1u; body->ServiceDetailReq.ServiceID = value->service_id; break;
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
            if (value->ac_mode) {
                struct iso2_AC_EVChargeParameterType *ac = &body->ChargeParameterDiscoveryReq.AC_EVChargeParameter;
                body->ChargeParameterDiscoveryReq.AC_EVChargeParameter_isUsed = 1u;
                body->ChargeParameterDiscoveryReq.RequestedEnergyTransferMode = iso2_EnergyTransferModeType_AC_single_phase_core;
                OUT_PHYS(ac->EAmount, (int64_t)value->ac_energy_wh * 1000, Wh);
                OUT_PHYS(ac->EVMaxVoltage, value->max_voltage_mv, V);
                OUT_PHYS(ac->EVMaxCurrent, value->max_current_ma, A);
                OUT_PHYS(ac->EVMinCurrent, value->min_current_ma, A);
            } else {
            body->ChargeParameterDiscoveryReq.DC_EVChargeParameter_isUsed = 1u;
            body->ChargeParameterDiscoveryReq.RequestedEnergyTransferMode = iso2_EnergyTransferModeType_DC_core;
            ev_status(&parameters->DC_EVStatus, value);
            OUT_PHYS(parameters->EVMaximumCurrentLimit, value->max_current_ma, A);
            OUT_PHYS(parameters->EVMaximumVoltageLimit, value->max_voltage_mv, V);
            parameters->EVMaximumPowerLimit_isUsed = 1u;
            OUT_PHYS(parameters->EVMaximumPowerLimit, (int64_t)value->max_power_w * 1000, W);
            }
            break;
        }
        case OPENV2G_PROTOCOL_MESSAGE_CABLE_CHECK:
            REQUIRE(!value->ac_mode);
            body->CableCheckReq_isUsed = 1u; ev_status(&body->CableCheckReq.DC_EVStatus, value); break;
        case OPENV2G_PROTOCOL_MESSAGE_PRECHARGE:
            REQUIRE(!value->ac_mode);
            body->PreChargeReq_isUsed = 1u; ev_status(&body->PreChargeReq.DC_EVStatus, value);
            OUT_PHYS(body->PreChargeReq.EVTargetVoltage, value->voltage_mv, V);
            OUT_PHYS(body->PreChargeReq.EVTargetCurrent, value->current_ma, A); break;
        case OPENV2G_PROTOCOL_MESSAGE_RENEGOTIATE:
        case OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_START:
        case OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_STOP:
            body->PowerDeliveryReq_isUsed = 1u;
            body->PowerDeliveryReq.ChargeProgress = value->kind == OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_START ? iso2_chargeProgressType_Start : (value->kind == OPENV2G_PROTOCOL_MESSAGE_RENEGOTIATE ? iso2_chargeProgressType_Renegotiate : iso2_chargeProgressType_Stop);
            body->PowerDeliveryReq.SAScheduleTupleID = value->schedule_id;
            body->PowerDeliveryReq.DC_EVPowerDeliveryParameter_isUsed = !value->ac_mode;
            ev_status(&body->PowerDeliveryReq.DC_EVPowerDeliveryParameter.DC_EVStatus, value);
            body->PowerDeliveryReq.DC_EVPowerDeliveryParameter.ChargingComplete = value->kind == OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_STOP; break;
        case OPENV2G_PROTOCOL_MESSAGE_CHARGING_STATUS:
            REQUIRE(value->ac_mode); body->ChargingStatusReq_isUsed = 1u; break;
        case OPENV2G_PROTOCOL_MESSAGE_METERING_RECEIPT:
            REQUIRE(value->has_meter_info);
            body->MeteringReceiptReq_isUsed = 1u;
            body->MeteringReceiptReq.SessionID.bytesLen = 8u;
            (void)memcpy(body->MeteringReceiptReq.SessionID.bytes, value->session_id, 8u);
            body->MeteringReceiptReq.SAScheduleTupleID_isUsed = 1u;
            body->MeteringReceiptReq.SAScheduleTupleID = value->schedule_id;
            REQUIRE(meter_out(&value->meter_info, &body->MeteringReceiptReq.MeterInfo)); break;
        case OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND:
            REQUIRE(!value->ac_mode);
            body->CurrentDemandReq_isUsed = 1u; body->CurrentDemandReq.ChargingComplete = value->charging_complete; ev_status(&body->CurrentDemandReq.DC_EVStatus, value);
            OUT_PHYS(body->CurrentDemandReq.EVTargetVoltage, value->voltage_mv, V);
            OUT_PHYS(body->CurrentDemandReq.EVTargetCurrent, value->current_ma, A); break;
        case OPENV2G_PROTOCOL_MESSAGE_WELDING_DETECTION:
            REQUIRE(!value->ac_mode);
            body->WeldingDetectionReq_isUsed = 1u; ev_status(&body->WeldingDetectionReq.DC_EVStatus, value); break;
        case OPENV2G_PROTOCOL_MESSAGE_SESSION_STOP:
            body->SessionStopReq_isUsed = 1u;
            body->SessionStopReq.ChargingSession = value->pause_session ? iso2_chargingSessionType_Pause : iso2_chargingSessionType_Terminate;
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
        case OPENV2G_PROTOCOL_MESSAGE_SERVICE_DETAIL: {
            unsigned i;
            body->ServiceDetailRes_isUsed = 1u; body->ServiceDetailRes.ResponseCode = code;
            body->ServiceDetailRes.ServiceID = value->service_id;
            REQUIRE(value->parameter_count <= OPENV2G_EXI_PARAMETER_MAX);
            body->ServiceDetailRes.ServiceParameterList_isUsed = value->parameter_count != 0u;
            body->ServiceDetailRes.ServiceParameterList.ParameterSet.arrayLen = value->parameter_count;
            for (i = 0u; i < value->parameter_count; ++i) {
                const OpenV2g_ExiParameter *source = &value->parameters[i];
                struct iso2_ParameterSetType *set = &body->ServiceDetailRes.ServiceParameterList.ParameterSet.array[i];
                struct iso2_ParameterType *parameter = &set->Parameter.array[0];
                set->ParameterSetID = source->set_id; set->Parameter.arrayLen = 1u;
                REQUIRE(source->name_length > 0u && source->name_length <= sizeof(source->name) && source->name_length < sizeof(parameter->Name.characters));
                parameter->Name.charactersLen = source->name_length;
                (void)memcpy(parameter->Name.characters, source->name, source->name_length);
                if (source->is_string) {
                    REQUIRE(source->text_length <= sizeof(source->text) && source->text_length < sizeof(parameter->stringValue.characters));
                    parameter->stringValue_isUsed = 1u; parameter->stringValue.charactersLen = source->text_length;
                    (void)memcpy(parameter->stringValue.characters, source->text, source->text_length);
                } else { parameter->intValue_isUsed = 1u; parameter->intValue = source->integer_value; }
            }
            break;
        }
        case OPENV2G_PROTOCOL_MESSAGE_SERVICE_DISCOVERY:
            body->ServiceDiscoveryRes_isUsed = 1u; body->ServiceDiscoveryRes.ResponseCode = code;
            body->ServiceDiscoveryRes.PaymentOptionList.PaymentOption.arrayLen = 1u;
            body->ServiceDiscoveryRes.PaymentOptionList.PaymentOption.array[0] = iso2_paymentOptionType_ExternalPayment;
            body->ServiceDiscoveryRes.ChargeService.ServiceID = value->service_id;
            body->ServiceDiscoveryRes.ChargeService.ServiceCategory = iso2_serviceCategoryType_EVCharging;
            body->ServiceDiscoveryRes.ChargeService.SupportedEnergyTransferMode.EnergyTransferMode.arrayLen = 1u;
            body->ServiceDiscoveryRes.ChargeService.SupportedEnergyTransferMode.EnergyTransferMode.array[0] = (value->ac_mode ? iso2_EnergyTransferModeType_AC_single_phase_core : iso2_EnergyTransferModeType_DC_core);
            body->ServiceDiscoveryRes.ChargeService.FreeService = 1; break;
        case OPENV2G_PROTOCOL_MESSAGE_SERVICE_SELECTION:
            body->PaymentServiceSelectionRes_isUsed = 1u; body->PaymentServiceSelectionRes.ResponseCode = code; break;
        case OPENV2G_PROTOCOL_MESSAGE_AUTHORIZATION:
            body->AuthorizationRes_isUsed = 1u; body->AuthorizationRes.ResponseCode = code;
            body->AuthorizationRes.EVSEProcessing = value->waiting_for_customer ? iso2_EVSEProcessingType_Ongoing_WaitingForCustomerInteraction : (value->ongoing ? iso2_EVSEProcessingType_Ongoing : iso2_EVSEProcessingType_Finished); break;
        case OPENV2G_PROTOCOL_MESSAGE_CHARGE_PARAMETER: {
            struct iso2_DC_EVSEChargeParameterType *parameters = &body->ChargeParameterDiscoveryRes.DC_EVSEChargeParameter;
            body->ChargeParameterDiscoveryRes_isUsed = 1u; body->ChargeParameterDiscoveryRes.ResponseCode = code;
            body->ChargeParameterDiscoveryRes.EVSEProcessing = value->ongoing ? iso2_EVSEProcessingType_Ongoing : iso2_EVSEProcessingType_Finished;
            REQUIRE(value->ongoing || (!value->omit_pending_parameters && !value->omit_pending_schedules));
            if (value->ac_mode) {
                struct iso2_AC_EVSEChargeParameterType *ac = &body->ChargeParameterDiscoveryRes.AC_EVSEChargeParameter;
                body->ChargeParameterDiscoveryRes.AC_EVSEChargeParameter_isUsed = 1u;
                ac_status_out(&ac->AC_EVSEStatus, value);
                OUT_PHYS(ac->EVSENominalVoltage, value->voltage_mv, V);
                OUT_PHYS(ac->EVSEMaxCurrent, value->max_current_ma, A);
            } else {
            body->ChargeParameterDiscoveryRes.DC_EVSEChargeParameter_isUsed = 1u;
            evse_status(&parameters->DC_EVSEStatus, value);
            OUT_PHYS(parameters->EVSEMaximumVoltageLimit, value->max_voltage_mv, V);
            OUT_PHYS(parameters->EVSEMaximumCurrentLimit, value->max_current_ma, A);
            OUT_PHYS(parameters->EVSEMaximumPowerLimit, (int64_t)value->max_power_w * 1000, W);
            OUT_PHYS(parameters->EVSEMinimumVoltageLimit, value->min_voltage_mv, V);
            OUT_PHYS(parameters->EVSEMinimumCurrentLimit, value->min_current_ma, A);
            OUT_PHYS(parameters->EVSEPeakCurrentRipple, 0, A);
            }
            body->ChargeParameterDiscoveryRes.SAScheduleList_isUsed = 1u;
            {
                unsigned i, j, count = value->schedule_count != 0u ? value->schedule_count : 1u;
                REQUIRE(count <= OPENV2G_EXI_SCHEDULE_MAX);
                body->ChargeParameterDiscoveryRes.SAScheduleList.SAScheduleTuple.arrayLen = (uint16_t)count;
                for (i = 0u; i < count; ++i) {
                    OpenV2g_ExiSchedule fallback = {0};
                    const OpenV2g_ExiSchedule *schedule = &value->schedules[i];
                    struct iso2_SAScheduleTupleType *tuple = &body->ChargeParameterDiscoveryRes.SAScheduleList.SAScheduleTuple.array[i];
                    uint32_t end = 0u;
                    if (value->schedule_count == 0u) { fallback.id = value->schedule_id; fallback.count = 1u; fallback.points[0].duration_s = value->schedule_duration_s; fallback.points[0].power_w = value->schedule_power_w; schedule = &fallback; }
                    REQUIRE(schedule->id != 0u && schedule->count > 0u && schedule->count <= OPENV2G_EXI_SCHEDULE_POINTS_MAX);
                    for (j = 0u; j < i; ++j) { REQUIRE(schedule->id != value->schedules[j].id); }
                    tuple->SAScheduleTupleID = schedule->id;
                    tuple->PMaxSchedule.PMaxScheduleEntry.arrayLen = schedule->count;
                    for (j = 0u; j < schedule->count; ++j) {
                        const OpenV2g_ExiSchedulePoint *point = &schedule->points[j];
                        struct iso2_PMaxScheduleEntryType *entry = &tuple->PMaxSchedule.PMaxScheduleEntry.array[j];
                        REQUIRE(point->start_s == end && point->duration_s > 0u && point->duration_s <= UINT32_MAX - end && point->power_w >= 0);
                        end += point->duration_s;
                        entry->RelativeTimeInterval_isUsed = 1u; entry->RelativeTimeInterval.start = point->start_s;
                        entry->RelativeTimeInterval.duration_isUsed = 1u; entry->RelativeTimeInterval.duration = point->duration_s;
                        OUT_PHYS(entry->PMax, (int64_t)point->power_w * 1000, W);
                    }
                }
            }
            if (value->omit_pending_parameters) { body->ChargeParameterDiscoveryRes.DC_EVSEChargeParameter_isUsed = 0u; body->ChargeParameterDiscoveryRes.AC_EVSEChargeParameter_isUsed = 0u; }
            if (value->omit_pending_schedules) { body->ChargeParameterDiscoveryRes.SAScheduleList_isUsed = 0u; }
            break;
        }
        case OPENV2G_PROTOCOL_MESSAGE_CABLE_CHECK:
            REQUIRE(!value->ac_mode);
            body->CableCheckRes_isUsed = 1u; body->CableCheckRes.ResponseCode = code;
            evse_status(&body->CableCheckRes.DC_EVSEStatus, value);
            body->CableCheckRes.EVSEProcessing = value->ongoing ? iso2_EVSEProcessingType_Ongoing : iso2_EVSEProcessingType_Finished; break;
        case OPENV2G_PROTOCOL_MESSAGE_PRECHARGE:
            REQUIRE(!value->ac_mode);
            body->PreChargeRes_isUsed = 1u; body->PreChargeRes.ResponseCode = code;
            evse_status(&body->PreChargeRes.DC_EVSEStatus, value);
            OUT_PHYS(body->PreChargeRes.EVSEPresentVoltage, value->voltage_mv, V); break;
        case OPENV2G_PROTOCOL_MESSAGE_RENEGOTIATE:
        case OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_START:
        case OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_STOP:
            body->PowerDeliveryRes_isUsed = 1u; body->PowerDeliveryRes.ResponseCode = code;
            body->PowerDeliveryRes.DC_EVSEStatus_isUsed = !value->ac_mode;
            body->PowerDeliveryRes.AC_EVSEStatus_isUsed = value->ac_mode;
            if (value->ac_mode) { ac_status_out(&body->PowerDeliveryRes.AC_EVSEStatus, value); }
            else { evse_status(&body->PowerDeliveryRes.DC_EVSEStatus, value); } break;
        case OPENV2G_PROTOCOL_MESSAGE_CHARGING_STATUS:
            REQUIRE(value->ac_mode && (!value->receipt_required || value->has_meter_info));
            body->ChargingStatusRes_isUsed = 1u; body->ChargingStatusRes.ResponseCode = code;
            REQUIRE(value->evse_id_length > 0u && value->evse_id_length <= sizeof(value->evse_id) && value->evse_id_length < sizeof(body->ChargingStatusRes.EVSEID.characters));
            body->ChargingStatusRes.EVSEID.charactersLen = value->evse_id_length;
            (void)memcpy(body->ChargingStatusRes.EVSEID.characters, value->evse_id, value->evse_id_length);
            body->ChargingStatusRes.SAScheduleTupleID = value->schedule_id;
            body->ChargingStatusRes.EVSEMaxCurrent_isUsed = value->has_max_current;
            if (value->has_max_current) { OUT_PHYS(body->ChargingStatusRes.EVSEMaxCurrent, value->max_current_ma, A); }
            body->ChargingStatusRes.MeterInfo_isUsed = value->has_meter_info;
            if (value->has_meter_info) { REQUIRE(meter_out(&value->meter_info, &body->ChargingStatusRes.MeterInfo)); }
            body->ChargingStatusRes.ReceiptRequired_isUsed = value->receipt_required;
            body->ChargingStatusRes.ReceiptRequired = value->receipt_required;
            ac_status_out(&body->ChargingStatusRes.AC_EVSEStatus, value); break;
        case OPENV2G_PROTOCOL_MESSAGE_METERING_RECEIPT:
            body->MeteringReceiptRes_isUsed = 1u; body->MeteringReceiptRes.ResponseCode = code;
            body->MeteringReceiptRes.DC_EVSEStatus_isUsed = !value->ac_mode;
            body->MeteringReceiptRes.AC_EVSEStatus_isUsed = value->ac_mode;
            if (value->ac_mode) { ac_status_out(&body->MeteringReceiptRes.AC_EVSEStatus, value); }
            else { evse_status(&body->MeteringReceiptRes.DC_EVSEStatus, value); } break;
        case OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND:
            REQUIRE(!value->ac_mode);
            body->CurrentDemandRes_isUsed = 1u; body->CurrentDemandRes.ResponseCode = code;
            evse_status(&body->CurrentDemandRes.DC_EVSEStatus, value);
            OUT_PHYS(body->CurrentDemandRes.EVSEPresentVoltage, value->voltage_mv, V);
            OUT_PHYS(body->CurrentDemandRes.EVSEPresentCurrent, value->current_ma, A);
            REQUIRE(value->evse_id_length > 0u && value->evse_id_length <= sizeof(value->evse_id) && value->evse_id_length < sizeof(body->CurrentDemandRes.EVSEID.characters));
            body->CurrentDemandRes.EVSEID.charactersLen = value->evse_id_length;
            (void)memcpy(body->CurrentDemandRes.EVSEID.characters, value->evse_id, value->evse_id_length);
            body->CurrentDemandRes.SAScheduleTupleID = value->schedule_id;
            REQUIRE(!value->receipt_required || value->has_meter_info);
            body->CurrentDemandRes.MeterInfo_isUsed = value->has_meter_info;
            if (value->has_meter_info) { REQUIRE(meter_out(&value->meter_info, &body->CurrentDemandRes.MeterInfo)); }
            body->CurrentDemandRes.ReceiptRequired_isUsed = value->receipt_required;
            body->CurrentDemandRes.ReceiptRequired = value->receipt_required;
            body->CurrentDemandRes.EVSEMaximumVoltageLimit_isUsed = value->has_max_voltage;
            body->CurrentDemandRes.EVSEMaximumCurrentLimit_isUsed = value->has_max_current;
            body->CurrentDemandRes.EVSEMaximumPowerLimit_isUsed = value->has_max_power;
            if (value->has_max_voltage) { OUT_PHYS(body->CurrentDemandRes.EVSEMaximumVoltageLimit, value->max_voltage_mv, V); }
            if (value->has_max_current) { OUT_PHYS(body->CurrentDemandRes.EVSEMaximumCurrentLimit, value->max_current_ma, A); }
            if (value->has_max_power) { OUT_PHYS(body->CurrentDemandRes.EVSEMaximumPowerLimit, (int64_t)value->max_power_w * 1000, W); }
            body->CurrentDemandRes.EVSECurrentLimitAchieved = value->current_limit_achieved;
            body->CurrentDemandRes.EVSEVoltageLimitAchieved = value->voltage_limit_achieved;
            body->CurrentDemandRes.EVSEPowerLimitAchieved = value->power_limit_achieved;
            break;
        case OPENV2G_PROTOCOL_MESSAGE_WELDING_DETECTION:
            REQUIRE(!value->ac_mode);
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
        value->resumed_session = body->SessionSetupRes.ResponseCode == iso2_responseCodeType_OK_OldSessionJoined;
        value->accepted = value->accepted || value->resumed_session;
        value->raw_response_code = (uint16_t)body->SessionSetupRes.ResponseCode;
        value->new_session = body->SessionSetupRes.ResponseCode == iso2_responseCodeType_OK_NewSessionEstablished;
        REQUIRE(body->SessionSetupRes.EVSEID.charactersLen > 0u && body->SessionSetupRes.EVSEID.charactersLen <= sizeof(value->evse_id));
        value->evse_id_length = body->SessionSetupRes.EVSEID.charactersLen;
        (void)memcpy(value->evse_id, body->SessionSetupRes.EVSEID.characters, value->evse_id_length);
    }
    if (body->ServiceDetailReq_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_SERVICE_DETAIL; value->response = false;
        value->service_id = body->ServiceDetailReq.ServiceID;
    }
    if (body->ServiceDetailRes_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_SERVICE_DETAIL; value->response = true;
        unsigned i;
        value->accepted = body->ServiceDetailRes.ResponseCode == iso2_responseCodeType_OK;
        value->service_id = body->ServiceDetailRes.ServiceID;
        if (body->ServiceDetailRes.ServiceParameterList_isUsed) {
            REQUIRE(body->ServiceDetailRes.ServiceParameterList.ParameterSet.arrayLen > 0u && body->ServiceDetailRes.ServiceParameterList.ParameterSet.arrayLen <= OPENV2G_EXI_PARAMETER_MAX);
            value->parameter_count = (uint8_t)body->ServiceDetailRes.ServiceParameterList.ParameterSet.arrayLen;
            for (i = 0u; i < value->parameter_count; ++i) {
                const struct iso2_ParameterSetType *set = &body->ServiceDetailRes.ServiceParameterList.ParameterSet.array[i];
                const struct iso2_ParameterType *parameter = &set->Parameter.array[0];
                OpenV2g_ExiParameter *source = &value->parameters[i];
                REQUIRE(set->Parameter.arrayLen == 1u);
                REQUIRE(!parameter->boolValue_isUsed && !parameter->byteValue_isUsed && !parameter->shortValue_isUsed && !parameter->physicalValue_isUsed);
                REQUIRE((unsigned)parameter->intValue_isUsed + (unsigned)parameter->stringValue_isUsed == 1u);
                source->set_id = set->ParameterSetID; source->is_string = parameter->stringValue_isUsed;

                REQUIRE(parameter->Name.charactersLen > 0u && parameter->Name.charactersLen <= sizeof(source->name));
                source->name_length = parameter->Name.charactersLen; (void)memcpy(source->name, parameter->Name.characters, source->name_length);
                if (source->is_string) {
                    REQUIRE(parameter->stringValue.charactersLen <= sizeof(source->text));
                    source->text_length = parameter->stringValue.charactersLen; (void)memcpy(source->text, parameter->stringValue.characters, source->text_length);
                } else { source->integer_value = parameter->intValue; }
            }
        }
    }
    if (body->ServiceDiscoveryReq_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_SERVICE_DISCOVERY; value->response = false;
        REQUIRE(!body->ServiceDiscoveryReq.ServiceScope_isUsed && !body->ServiceDiscoveryReq.ServiceCategory_isUsed);
    }
    if (body->ServiceDiscoveryRes_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_SERVICE_DISCOVERY; value->response = true;
        value->accepted = body->ServiceDiscoveryRes.ResponseCode == iso2_responseCodeType_OK;
        REQUIRE(body->ServiceDiscoveryRes.PaymentOptionList.PaymentOption.arrayLen == 1u && body->ServiceDiscoveryRes.PaymentOptionList.PaymentOption.array[0] == iso2_paymentOptionType_ExternalPayment);
        REQUIRE(body->ServiceDiscoveryRes.ChargeService.SupportedEnergyTransferMode.EnergyTransferMode.arrayLen == 1u);
        value->ac_mode = body->ServiceDiscoveryRes.ChargeService.SupportedEnergyTransferMode.EnergyTransferMode.array[0] == iso2_EnergyTransferModeType_AC_single_phase_core;
        REQUIRE(value->ac_mode || body->ServiceDiscoveryRes.ChargeService.SupportedEnergyTransferMode.EnergyTransferMode.array[0] == iso2_EnergyTransferModeType_DC_core);
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
        REQUIRE(body->AuthorizationRes.EVSEProcessing == iso2_EVSEProcessingType_Finished || body->AuthorizationRes.EVSEProcessing == iso2_EVSEProcessingType_Ongoing || body->AuthorizationRes.EVSEProcessing == iso2_EVSEProcessingType_Ongoing_WaitingForCustomerInteraction);
        value->waiting_for_customer = body->AuthorizationRes.EVSEProcessing == iso2_EVSEProcessingType_Ongoing_WaitingForCustomerInteraction;
        value->ongoing = body->AuthorizationRes.EVSEProcessing != iso2_EVSEProcessingType_Finished;
    }
    if (body->ChargeParameterDiscoveryReq_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_CHARGE_PARAMETER; value->response = false;
        REQUIRE(!body->ChargeParameterDiscoveryReq.EVChargeParameter_isUsed);
        value->ac_mode = body->ChargeParameterDiscoveryReq.AC_EVChargeParameter_isUsed;
        REQUIRE((unsigned)body->ChargeParameterDiscoveryReq.DC_EVChargeParameter_isUsed + (unsigned)value->ac_mode == 1u);
        if (value->ac_mode) {
            const struct iso2_AC_EVChargeParameterType *ac = &body->ChargeParameterDiscoveryReq.AC_EVChargeParameter;
            REQUIRE(body->ChargeParameterDiscoveryReq.RequestedEnergyTransferMode == iso2_EnergyTransferModeType_AC_single_phase_core);
            IN_PHYS(ac->EAmount, Wh, physical); REQUIRE(physical % 1000 == 0); value->ac_energy_wh = (uint32_t)(physical / 1000);
            IN_PHYS(ac->EVMaxVoltage, V, physical); value->max_voltage_mv = (uint32_t)physical;
            IN_PHYS(ac->EVMaxCurrent, A, value->max_current_ma); IN_PHYS(ac->EVMinCurrent, A, value->min_current_ma);
        } else {
        REQUIRE(body->ChargeParameterDiscoveryReq.RequestedEnergyTransferMode == iso2_EnergyTransferModeType_DC_core);
        REQUIRE(request_status_in(&body->ChargeParameterDiscoveryReq.DC_EVChargeParameter.DC_EVStatus, value));
        IN_PHYS(body->ChargeParameterDiscoveryReq.DC_EVChargeParameter.EVMaximumVoltageLimit, V, physical); value->max_voltage_mv = (uint32_t)physical;
        IN_PHYS(body->ChargeParameterDiscoveryReq.DC_EVChargeParameter.EVMaximumCurrentLimit, A, value->max_current_ma);
        REQUIRE(body->ChargeParameterDiscoveryReq.DC_EVChargeParameter.EVMaximumPowerLimit_isUsed);
        IN_PHYS(body->ChargeParameterDiscoveryReq.DC_EVChargeParameter.EVMaximumPowerLimit, W, physical); REQUIRE(physical % 1000 == 0); value->max_power_w = physical / 1000;        }

    }
    if (body->ChargeParameterDiscoveryRes_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_CHARGE_PARAMETER; value->response = true;
        value->accepted = body->ChargeParameterDiscoveryRes.ResponseCode == iso2_responseCodeType_OK;
        REQUIRE(body->ChargeParameterDiscoveryRes.EVSEProcessing == iso2_EVSEProcessingType_Finished || body->ChargeParameterDiscoveryRes.EVSEProcessing == iso2_EVSEProcessingType_Ongoing);
        value->ongoing = body->ChargeParameterDiscoveryRes.EVSEProcessing != iso2_EVSEProcessingType_Finished;
        REQUIRE((unsigned)body->ChargeParameterDiscoveryRes.AC_EVSEChargeParameter_isUsed + (unsigned)body->ChargeParameterDiscoveryRes.DC_EVSEChargeParameter_isUsed <= 1u);
        REQUIRE(value->ongoing || (!body->ChargeParameterDiscoveryRes.EVSEChargeParameter_isUsed && !body->ChargeParameterDiscoveryRes.SASchedules_isUsed));
        value->ac_mode = body->ChargeParameterDiscoveryRes.AC_EVSEChargeParameter_isUsed;
        value->omit_pending_parameters = !body->ChargeParameterDiscoveryRes.DC_EVSEChargeParameter_isUsed && !value->ac_mode;
        value->omit_pending_schedules = !body->ChargeParameterDiscoveryRes.SAScheduleList_isUsed;
        REQUIRE(value->ongoing || (!value->omit_pending_parameters && !value->omit_pending_schedules));
        if (!value->omit_pending_parameters) {
        if (value->ac_mode) {
            const struct iso2_AC_EVSEChargeParameterType *ac = &body->ChargeParameterDiscoveryRes.AC_EVSEChargeParameter;
            int64_t power;
            REQUIRE(ac_status_in(&ac->AC_EVSEStatus, value));
            IN_PHYS(ac->EVSENominalVoltage, V, physical); value->voltage_mv = (uint32_t)physical;
            value->max_voltage_mv = value->voltage_mv; /* Host single-phase limit projection. */
            IN_PHYS(ac->EVSEMaxCurrent, A, value->max_current_ma);
            power = (int64_t)value->voltage_mv * value->max_current_ma / 1000000;
            REQUIRE(power <= INT32_MAX); value->max_power_w = (int32_t)power;
        } else {
        REQUIRE(status_in(&body->ChargeParameterDiscoveryRes.DC_EVSEChargeParameter.DC_EVSEStatus, value));
        IN_PHYS(body->ChargeParameterDiscoveryRes.DC_EVSEChargeParameter.EVSEMinimumVoltageLimit, V, physical); value->min_voltage_mv = (uint32_t)physical;
        IN_PHYS(body->ChargeParameterDiscoveryRes.DC_EVSEChargeParameter.EVSEMinimumCurrentLimit, A, value->min_current_ma);
        REQUIRE(!body->ChargeParameterDiscoveryRes.DC_EVSEChargeParameter.EVSECurrentRegulationTolerance_isUsed);
        REQUIRE(!body->ChargeParameterDiscoveryRes.DC_EVSEChargeParameter.EVSEEnergyToBeDelivered_isUsed);
        IN_PHYS(body->ChargeParameterDiscoveryRes.DC_EVSEChargeParameter.EVSEMaximumVoltageLimit, V, physical); value->max_voltage_mv = (uint32_t)physical;
        IN_PHYS(body->ChargeParameterDiscoveryRes.DC_EVSEChargeParameter.EVSEMaximumCurrentLimit, A, value->max_current_ma);
        IN_PHYS(body->ChargeParameterDiscoveryRes.DC_EVSEChargeParameter.EVSEMaximumPowerLimit, W, physical); REQUIRE(physical % 1000 == 0); value->max_power_w = physical / 1000;
        }
        }
        if (!value->omit_pending_schedules) {
        unsigned i, j;
        unsigned count = body->ChargeParameterDiscoveryRes.SAScheduleList.SAScheduleTuple.arrayLen;
        REQUIRE(count > 0u && count <= OPENV2G_EXI_SCHEDULE_MAX);
        value->schedule_count = (uint8_t)count;
        for (i = 0u; i < count; ++i) {
            const struct iso2_SAScheduleTupleType *tuple = &body->ChargeParameterDiscoveryRes.SAScheduleList.SAScheduleTuple.array[i];
            OpenV2g_ExiSchedule *schedule = &value->schedules[i];
            uint32_t end = 0u;
            REQUIRE(!tuple->SalesTariff_isUsed && tuple->SAScheduleTupleID > 0 && tuple->SAScheduleTupleID <= UINT8_MAX);
            schedule->id = (uint8_t)tuple->SAScheduleTupleID;
            for (j = 0u; j < i; ++j) { REQUIRE(schedule->id != value->schedules[j].id); }
            REQUIRE(tuple->PMaxSchedule.PMaxScheduleEntry.arrayLen > 0u && tuple->PMaxSchedule.PMaxScheduleEntry.arrayLen <= OPENV2G_EXI_SCHEDULE_POINTS_MAX);
            schedule->count = (uint8_t)tuple->PMaxSchedule.PMaxScheduleEntry.arrayLen;
            for (j = 0u; j < schedule->count; ++j) {
                const struct iso2_PMaxScheduleEntryType *entry = &tuple->PMaxSchedule.PMaxScheduleEntry.array[j];
                OpenV2g_ExiSchedulePoint *point = &schedule->points[j];
                REQUIRE(entry->RelativeTimeInterval_isUsed && !entry->TimeInterval_isUsed && entry->RelativeTimeInterval.duration_isUsed);
                point->start_s = entry->RelativeTimeInterval.start; point->duration_s = entry->RelativeTimeInterval.duration;
                REQUIRE(point->start_s == end && point->duration_s > 0u && point->duration_s <= UINT32_MAX - end);
                end += point->duration_s;
                IN_PHYS(entry->PMax, W, physical); REQUIRE(physical % 1000 == 0); point->power_w = physical / 1000;
            }
        }
        /* Host selection policy: first offered tuple, conservative minimum power
         * throughout its full contiguous duration. All offered tuples retained. */
        value->schedule_id = value->schedules[0].id;
        value->schedule_power_w = value->schedules[0].points[0].power_w;
        value->schedule_duration_s = 0u;
        for (j = 0u; j < value->schedules[0].count; ++j) {
            const OpenV2g_ExiSchedulePoint *point = &value->schedules[0].points[j];
            value->schedule_duration_s += point->duration_s;
            if (point->power_w < value->schedule_power_w) { value->schedule_power_w = point->power_w; }
        }
        }
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
        value->ac_mode = !body->PowerDeliveryReq.DC_EVPowerDeliveryParameter_isUsed;
        if (!value->ac_mode) { REQUIRE(request_status_in(&body->PowerDeliveryReq.DC_EVPowerDeliveryParameter.DC_EVStatus, value)); }
        REQUIRE(body->PowerDeliveryReq.ChargeProgress == iso2_chargeProgressType_Start || body->PowerDeliveryReq.ChargeProgress == iso2_chargeProgressType_Stop || body->PowerDeliveryReq.ChargeProgress == iso2_chargeProgressType_Renegotiate);
        value->kind = body->PowerDeliveryReq.ChargeProgress == iso2_chargeProgressType_Start ? OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_START : (body->PowerDeliveryReq.ChargeProgress == iso2_chargeProgressType_Renegotiate ? OPENV2G_PROTOCOL_MESSAGE_RENEGOTIATE : OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_STOP);
        value->schedule_id = body->PowerDeliveryReq.SAScheduleTupleID;
    }
    if (body->PowerDeliveryRes_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_START; value->response = true;
        value->accepted = body->PowerDeliveryRes.ResponseCode == iso2_responseCodeType_OK;
        REQUIRE((unsigned)body->PowerDeliveryRes.DC_EVSEStatus_isUsed + (unsigned)body->PowerDeliveryRes.AC_EVSEStatus_isUsed == 1u && !body->PowerDeliveryRes.EVSEStatus_isUsed);
        if (body->PowerDeliveryRes.AC_EVSEStatus_isUsed) { REQUIRE(ac_status_in(&body->PowerDeliveryRes.AC_EVSEStatus, value)); }
        else { REQUIRE(status_in(&body->PowerDeliveryRes.DC_EVSEStatus, value)); }
    }
    if (body->ChargingStatusReq_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_CHARGING_STATUS; value->ac_mode = true; }
    if (body->ChargingStatusRes_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_CHARGING_STATUS; value->response = true;
        value->accepted = body->ChargingStatusRes.ResponseCode == iso2_responseCodeType_OK;
        REQUIRE(ac_status_in(&body->ChargingStatusRes.AC_EVSEStatus, value));
        REQUIRE(body->ChargingStatusRes.EVSEID.charactersLen > 0u && body->ChargingStatusRes.EVSEID.charactersLen <= sizeof(value->evse_id));
        value->evse_id_length = body->ChargingStatusRes.EVSEID.charactersLen;
        (void)memcpy(value->evse_id, body->ChargingStatusRes.EVSEID.characters, value->evse_id_length);
        value->schedule_id = body->ChargingStatusRes.SAScheduleTupleID;
        value->has_max_current = body->ChargingStatusRes.EVSEMaxCurrent_isUsed;
        if (value->has_max_current) { IN_PHYS(body->ChargingStatusRes.EVSEMaxCurrent, A, value->max_current_ma); }
        value->has_meter_info = body->ChargingStatusRes.MeterInfo_isUsed;
        if (value->has_meter_info) { REQUIRE(meter_in(&body->ChargingStatusRes.MeterInfo, &value->meter_info)); }
        value->receipt_required = body->ChargingStatusRes.ReceiptRequired_isUsed && body->ChargingStatusRes.ReceiptRequired;
        REQUIRE(!value->receipt_required || value->has_meter_info);
    }
    if (body->MeteringReceiptReq_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_METERING_RECEIPT;
        REQUIRE(!body->MeteringReceiptReq.Id_isUsed && body->MeteringReceiptReq.SessionID.bytesLen == 8u);
        REQUIRE(memcmp(body->MeteringReceiptReq.SessionID.bytes, value->session_id, 8u) == 0);
        REQUIRE(body->MeteringReceiptReq.SAScheduleTupleID_isUsed && body->MeteringReceiptReq.SAScheduleTupleID != 0u);
        value->schedule_id = body->MeteringReceiptReq.SAScheduleTupleID; value->has_meter_info = true;
        REQUIRE(meter_in(&body->MeteringReceiptReq.MeterInfo, &value->meter_info));
    }
    if (body->MeteringReceiptRes_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_METERING_RECEIPT; value->response = true;
        value->accepted = body->MeteringReceiptRes.ResponseCode == iso2_responseCodeType_OK;
        REQUIRE((unsigned)body->MeteringReceiptRes.DC_EVSEStatus_isUsed + (unsigned)body->MeteringReceiptRes.AC_EVSEStatus_isUsed == 1u && !body->MeteringReceiptRes.EVSEStatus_isUsed);
        if (body->MeteringReceiptRes.AC_EVSEStatus_isUsed) { REQUIRE(ac_status_in(&body->MeteringReceiptRes.AC_EVSEStatus, value)); }
        else { REQUIRE(status_in(&body->MeteringReceiptRes.DC_EVSEStatus, value)); }
    }
    if (body->CurrentDemandReq_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND; value->response = false;
        REQUIRE(request_status_in(&body->CurrentDemandReq.DC_EVStatus, value));
        value->charging_complete = body->CurrentDemandReq.ChargingComplete;
        IN_PHYS(body->CurrentDemandReq.EVTargetVoltage, V, physical); value->voltage_mv = (uint32_t)physical;
        IN_PHYS(body->CurrentDemandReq.EVTargetCurrent, A, value->current_ma);
    }
    if (body->CurrentDemandRes_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND; value->response = true;
        value->accepted = body->CurrentDemandRes.ResponseCode == iso2_responseCodeType_OK;
        REQUIRE(status_in(&body->CurrentDemandRes.DC_EVSEStatus, value));
        IN_PHYS(body->CurrentDemandRes.EVSEPresentVoltage, V, physical); value->voltage_mv = (uint32_t)physical;
        IN_PHYS(body->CurrentDemandRes.EVSEPresentCurrent, A, value->current_ma);
        value->has_max_voltage = body->CurrentDemandRes.EVSEMaximumVoltageLimit_isUsed;
        value->has_max_current = body->CurrentDemandRes.EVSEMaximumCurrentLimit_isUsed;
        value->has_max_power = body->CurrentDemandRes.EVSEMaximumPowerLimit_isUsed;
        if (value->has_max_voltage) { IN_PHYS(body->CurrentDemandRes.EVSEMaximumVoltageLimit, V, physical); value->max_voltage_mv = (uint32_t)physical; }
        if (value->has_max_current) { IN_PHYS(body->CurrentDemandRes.EVSEMaximumCurrentLimit, A, value->max_current_ma); }
        if (value->has_max_power) { IN_PHYS(body->CurrentDemandRes.EVSEMaximumPowerLimit, W, physical); REQUIRE(physical % 1000 == 0); value->max_power_w = physical / 1000; }
        value->current_limit_achieved = body->CurrentDemandRes.EVSECurrentLimitAchieved;
        value->voltage_limit_achieved = body->CurrentDemandRes.EVSEVoltageLimitAchieved;
        value->power_limit_achieved = body->CurrentDemandRes.EVSEPowerLimitAchieved;
        value->has_meter_info = body->CurrentDemandRes.MeterInfo_isUsed;
        value->receipt_required = body->CurrentDemandRes.ReceiptRequired_isUsed && body->CurrentDemandRes.ReceiptRequired;
        REQUIRE(!value->receipt_required || value->has_meter_info);
        if (value->has_meter_info) { REQUIRE(meter_in(&body->CurrentDemandRes.MeterInfo, &value->meter_info)); }
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
        value->pause_session = body->SessionStopReq.ChargingSession == iso2_chargingSessionType_Pause;
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
