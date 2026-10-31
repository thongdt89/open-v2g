/* Project mapper for the pinned din generated model; host DC EIM only. */
#include "openv2g/exi_cbv2g_dc.h"
#include <limits.h>
#include <string.h>
#include "cbv2g/din/din_msgDefDatatypes.h"
#include "cbv2g/din/din_msgDefEncoder.h"
#include "cbv2g/din/din_msgDefDecoder.h"
#include "cbv2g/common/exi_bitstream.h"
#include "cbv2g_pin.h"

static bool physical_out(struct din_PhysicalValueType *target, int64_t milli, din_unitSymbolType unit)
{
    int multiplier = -3;
    while ((milli > INT16_MAX || milli < INT16_MIN) && multiplier < 3 && milli % 10 == 0) {
        milli /= 10; ++multiplier;
    }
    if (milli > INT16_MAX || milli < INT16_MIN) { return false; }
    target->Value = (int16_t)milli; target->Multiplier = (int8_t)multiplier; target->Unit = unit;
    target->Unit_isUsed = 1u;
    return true;
}
static bool physical_in(const struct din_PhysicalValueType *source, din_unitSymbolType unit, int32_t *milli)
{
    int exponent = (int)source->Multiplier + 3;
    int64_t value = source->Value;
    if (source->Unit != unit || exponent < -6 || exponent > 6) { return false; }
    if (!source->Unit_isUsed) { return false; }
    while (exponent > 0) { value *= 10; --exponent; }
    while (exponent < 0) { if (value % 10 != 0) { return false; } value /= 10; ++exponent; }
    if (value < 0 || value > INT32_MAX) { return false; }
    *milli = (int32_t)value; return true;
}
static void ev_status(struct din_DC_EVStatusType *status, const OpenV2g_ExiDcDocument *value)
{
    status->EVReady = value->ready; status->EVErrorCode = din_DC_EVErrorCodeType_NO_ERROR;
    status->EVRESSSOC = (int8_t)(value->soc_permille / 10u);
}
static void evse_status(struct din_DC_EVSEStatusType *status, const OpenV2g_ExiDcDocument *value)
{
    status->EVSEStatusCode = value->ready ? din_DC_EVSEStatusCodeType_EVSE_Ready : din_DC_EVSEStatusCodeType_EVSE_NotReady;
    status->EVSEIsolationStatus_isUsed = 1u;
    status->EVSEIsolationStatus = value->isolation_valid ? din_isolationLevelType_Valid : din_isolationLevelType_Fault;
    status->EVSENotification = din_EVSENotificationType_None;
}
static bool status_in(const struct din_DC_EVSEStatusType *status, OpenV2g_ExiDcDocument *value)
{
    if (status->EVSENotification != din_EVSENotificationType_None) { return false; }
    value->ready = status->EVSEStatusCode == din_DC_EVSEStatusCodeType_EVSE_Ready;
    value->isolation_valid = status->EVSEIsolationStatus_isUsed && status->EVSEIsolationStatus == din_isolationLevelType_Valid;
    return true;
}
static bool request_status_in(const struct din_DC_EVStatusType *status, OpenV2g_ExiDcDocument *value)
{
    if (status->EVErrorCode != din_DC_EVErrorCodeType_NO_ERROR || status->EVRESSSOC < 0 || status->EVRESSSOC > 100) { return false; }
    value->ready = status->EVReady != 0; value->soc_permille = (uint16_t)status->EVRESSSOC * 10u; return true;
}
#define REQUIRE(expression) do { if (!(expression)) { return false; } } while (0)
#define OUT_PHYS(target, amount, unit) REQUIRE(physical_out(&(target), (amount), din_unitSymbolType_##unit))
#define IN_PHYS(source, unit, destination) REQUIRE(physical_in(&(source), din_unitSymbolType_##unit, &(destination)))

static bool parameter_out(const OpenV2g_ExiParameter *source, struct din_ParameterType *target)
{
    OpenV2g_ParameterKind kind = source->kind == OPENV2G_PARAM_LEGACY ?
        (source->is_string ? OPENV2G_PARAM_STRING : OPENV2G_PARAM_INT) : source->kind;
    if (source->name_length == 0u || source->name_length > sizeof(source->name) || source->name_length > sizeof(target->Name.characters)) { return false; }
    target->Name.charactersLen = source->name_length; (void)memcpy(target->Name.characters,source->name,source->name_length);
    switch (kind) {
    case OPENV2G_PARAM_BOOL:
        if (source->integer_value != 0 && source->integer_value != 1) { return false; }
        target->boolValue_isUsed = 1u; target->boolValue = source->integer_value; target->ValueType = din_valueType_bool; break;
    case OPENV2G_PARAM_BYTE:
        if (source->integer_value < INT8_MIN || source->integer_value > INT8_MAX) { return false; }
        target->byteValue_isUsed = 1u; target->byteValue = (int8_t)source->integer_value; target->ValueType = din_valueType_byte; break;
    case OPENV2G_PARAM_SHORT:
        if (source->integer_value < INT16_MIN || source->integer_value > INT16_MAX) { return false; }
        target->shortValue_isUsed = 1u; target->shortValue = (int16_t)source->integer_value; target->ValueType = din_valueType_short; break;
    case OPENV2G_PARAM_INT:
        target->intValue_isUsed = 1u; target->intValue = source->integer_value; target->ValueType = din_valueType_int; break;
    case OPENV2G_PARAM_PHYSICAL:
        if (source->physical_multiplier < -3 || source->physical_multiplier > 3) { return false; }
        target->physicalValue_isUsed = 1u; target->physicalValue.Value = source->physical_value;
        target->physicalValue.Multiplier = source->physical_multiplier; target->ValueType = din_valueType_physicalValue; target->physicalValue.Unit_isUsed = 1u;
        switch (source->physical_unit) {
        case OPENV2G_PARAM_HOUR: target->physicalValue.Unit = din_unitSymbolType_h; break;
        case OPENV2G_PARAM_METER: target->physicalValue.Unit = din_unitSymbolType_m; break;
        case OPENV2G_PARAM_SECOND: target->physicalValue.Unit = din_unitSymbolType_s; break;
        case OPENV2G_PARAM_AMPERE: target->physicalValue.Unit = din_unitSymbolType_A; break;
        case OPENV2G_PARAM_VOLT: target->physicalValue.Unit = din_unitSymbolType_V; break;
        case OPENV2G_PARAM_WATT: target->physicalValue.Unit = din_unitSymbolType_W; break;
        case OPENV2G_PARAM_WATT_HOUR: target->physicalValue.Unit = din_unitSymbolType_Wh; break;
        case OPENV2G_PARAM_AMPERE_HOUR: target->physicalValue.Unit = din_unitSymbolType_Ah; break;
        case OPENV2G_PARAM_VOLT_AMPERE: target->physicalValue.Unit = din_unitSymbolType_VA; break;
        case OPENV2G_PARAM_WATT_SECOND: target->physicalValue.Unit = din_unitSymbolType_W_s; break;
        default: return false;
        }
        break;
    case OPENV2G_PARAM_STRING:
        if (source->text_length > sizeof(source->text) || source->text_length > sizeof(target->stringValue.characters)) { return false; }
        target->stringValue_isUsed = 1u; target->stringValue.charactersLen = source->text_length;
        (void)memcpy(target->stringValue.characters,source->text,source->text_length); target->ValueType = din_valueType_string; break;
    default: return false;
    }
    return true;
}
static bool parameter_in(const struct din_ParameterType *source, int16_t set_id, OpenV2g_ExiParameter *target)
{
    if ((unsigned)source->boolValue_isUsed+(unsigned)source->byteValue_isUsed+(unsigned)source->shortValue_isUsed+
        (unsigned)source->intValue_isUsed+(unsigned)source->physicalValue_isUsed+(unsigned)source->stringValue_isUsed != 1u ||
        source->Name.charactersLen == 0u || source->Name.charactersLen > sizeof(target->name)) { return false; }
    target->set_id = set_id; target->name_length = source->Name.charactersLen; (void)memcpy(target->name,source->Name.characters,target->name_length);
    if (source->boolValue_isUsed) { if (source->boolValue != 0 && source->boolValue != 1) { return false; } target->kind = OPENV2G_PARAM_BOOL; target->integer_value = source->boolValue; if (source->ValueType != din_valueType_bool) { return false; } }
    else if (source->byteValue_isUsed) { target->kind = OPENV2G_PARAM_BYTE; target->integer_value = source->byteValue; if (source->ValueType != din_valueType_byte) { return false; } }
    else if (source->shortValue_isUsed) { target->kind = OPENV2G_PARAM_SHORT; target->integer_value = source->shortValue; if (source->ValueType != din_valueType_short) { return false; } }
    else if (source->intValue_isUsed) { target->kind = OPENV2G_PARAM_INT; target->integer_value = source->intValue; if (source->ValueType != din_valueType_int) { return false; } }
    else if (source->stringValue_isUsed) {
        if (source->ValueType != din_valueType_string) { return false; }
        if (source->stringValue.charactersLen > sizeof(target->text)) { return false; }
        target->kind = OPENV2G_PARAM_STRING; target->is_string = true; target->text_length = source->stringValue.charactersLen;
        (void)memcpy(target->text,source->stringValue.characters,target->text_length);
    } else {
        if (source->ValueType != din_valueType_physicalValue || !source->physicalValue.Unit_isUsed) { return false; }
        if (source->physicalValue.Multiplier < -3 || source->physicalValue.Multiplier > 3) { return false; }
        target->kind = OPENV2G_PARAM_PHYSICAL; target->physical_value = source->physicalValue.Value; target->physical_multiplier = source->physicalValue.Multiplier;
        switch (source->physicalValue.Unit) {
        case din_unitSymbolType_h: target->physical_unit = OPENV2G_PARAM_HOUR; break;
        case din_unitSymbolType_m: target->physical_unit = OPENV2G_PARAM_METER; break;
        case din_unitSymbolType_s: target->physical_unit = OPENV2G_PARAM_SECOND; break;
        case din_unitSymbolType_A: target->physical_unit = OPENV2G_PARAM_AMPERE; break;
        case din_unitSymbolType_V: target->physical_unit = OPENV2G_PARAM_VOLT; break;
        case din_unitSymbolType_W: target->physical_unit = OPENV2G_PARAM_WATT; break;
        case din_unitSymbolType_Wh: target->physical_unit = OPENV2G_PARAM_WATT_HOUR; break;
        case din_unitSymbolType_Ah: target->physical_unit = OPENV2G_PARAM_AMPERE_HOUR; break;
        case din_unitSymbolType_VA: target->physical_unit = OPENV2G_PARAM_VOLT_AMPERE; break;
        case din_unitSymbolType_W_s: target->physical_unit = OPENV2G_PARAM_WATT_SECOND; break;
        default: return false;
        }
    }
    return true;
}
static bool to_generated(const OpenV2g_ExiDcDocument *value, struct din_exiDocument *generated)
{
    struct din_BodyType *body = &generated->V2G_Message.Body;
    din_responseCodeType code = value->accepted ?
        (value->new_session ? din_responseCodeType_OK_NewSessionEstablished : din_responseCodeType_OK) : din_responseCodeType_FAILED;
    if (value->has_signature || value->signature.body_id_length != 0u || value->ac_mode || value->has_meter_info || value->receipt_required) { return false; }
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
            body->ServicePaymentSelectionReq_isUsed = 1u;
            body->ServicePaymentSelectionReq.SelectedPaymentOption = din_paymentOptionType_ExternalPayment;
            body->ServicePaymentSelectionReq.SelectedServiceList.SelectedService.arrayLen = 1u;
            body->ServicePaymentSelectionReq.SelectedServiceList.SelectedService.array[0].ServiceID = value->service_id; break;
        case OPENV2G_PROTOCOL_MESSAGE_AUTHORIZATION:
            body->ContractAuthenticationReq_isUsed = 1u; break;
        case OPENV2G_PROTOCOL_MESSAGE_CHARGE_PARAMETER: {
            struct din_DC_EVChargeParameterType *parameters = &body->ChargeParameterDiscoveryReq.DC_EVChargeParameter;
            body->ChargeParameterDiscoveryReq_isUsed = 1u;
            body->ChargeParameterDiscoveryReq.DC_EVChargeParameter_isUsed = 1u;
            body->ChargeParameterDiscoveryReq.EVRequestedEnergyTransferType = din_EVRequestedEnergyTransferType_DC_core;
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
            body->PowerDeliveryReq.ReadyToChargeState = value->kind == OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_START;
            body->PowerDeliveryReq.DC_EVPowerDeliveryParameter_isUsed = 1u;
            ev_status(&body->PowerDeliveryReq.DC_EVPowerDeliveryParameter.DC_EVStatus, value);
            body->PowerDeliveryReq.DC_EVPowerDeliveryParameter.ChargingComplete = value->kind == OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_STOP; break;
        case OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND:
            body->CurrentDemandReq_isUsed = 1u; body->CurrentDemandReq.ChargingComplete = value->charging_complete; ev_status(&body->CurrentDemandReq.DC_EVStatus, value);
            OUT_PHYS(body->CurrentDemandReq.EVTargetVoltage, value->voltage_mv, V);
            OUT_PHYS(body->CurrentDemandReq.EVTargetCurrent, value->current_ma, A); break;
        case OPENV2G_PROTOCOL_MESSAGE_WELDING_DETECTION:
            body->WeldingDetectionReq_isUsed = 1u; ev_status(&body->WeldingDetectionReq.DC_EVStatus, value); break;
        case OPENV2G_PROTOCOL_MESSAGE_SESSION_STOP:
            body->SessionStopReq_isUsed = 1u;
            break;
        default: return false;
        }
    } else {
        switch (value->kind) {
        case OPENV2G_PROTOCOL_MESSAGE_SESSION_SETUP:
            body->SessionSetupRes_isUsed = 1u; body->SessionSetupRes.ResponseCode = code;
            REQUIRE(value->evse_id_length > 0u && value->evse_id_length <= sizeof(body->SessionSetupRes.EVSEID.bytes));
            body->SessionSetupRes.EVSEID.bytesLen = value->evse_id_length;
            (void)memcpy(body->SessionSetupRes.EVSEID.bytes, value->evse_id, value->evse_id_length); break;
        case OPENV2G_PROTOCOL_MESSAGE_SERVICE_DETAIL: {
            unsigned i;
            body->ServiceDetailRes_isUsed = 1u; body->ServiceDetailRes.ResponseCode = code;
            body->ServiceDetailRes.ServiceID = value->service_id;
            REQUIRE(value->parameter_count <= OPENV2G_EXI_PARAMETER_MAX);
            body->ServiceDetailRes.ServiceParameterList_isUsed = value->parameter_count != 0u;
            body->ServiceDetailRes.ServiceParameterList.ParameterSet.arrayLen = value->parameter_count;
            for (i = 0u; i < value->parameter_count; ++i) {
                unsigned j;
                for (j = 0u; j < i; ++j) { REQUIRE(value->parameters[j].set_id != value->parameters[i].set_id); }
                body->ServiceDetailRes.ServiceParameterList.ParameterSet.array[i].ParameterSetID = value->parameters[i].set_id;
                REQUIRE(parameter_out(&value->parameters[i],&body->ServiceDetailRes.ServiceParameterList.ParameterSet.array[i].Parameter));
            }
            break;
        }
        case OPENV2G_PROTOCOL_MESSAGE_SERVICE_DISCOVERY:
            body->ServiceDiscoveryRes_isUsed = 1u; body->ServiceDiscoveryRes.ResponseCode = code;
            body->ServiceDiscoveryRes.PaymentOptions.PaymentOption.arrayLen = 1u;
            body->ServiceDiscoveryRes.PaymentOptions.PaymentOption.array[0] = din_paymentOptionType_ExternalPayment;
            body->ServiceDiscoveryRes.ChargeService.ServiceTag.ServiceID = value->service_id;
            body->ServiceDiscoveryRes.ChargeService.ServiceTag.ServiceCategory = din_serviceCategoryType_EVCharging;
            body->ServiceDiscoveryRes.ChargeService.EnergyTransferType = din_EVSESupportedEnergyTransferType_DC_core;
            body->ServiceDiscoveryRes.ChargeService.FreeService = 1; break;
        case OPENV2G_PROTOCOL_MESSAGE_SERVICE_SELECTION:
            body->ServicePaymentSelectionRes_isUsed = 1u; body->ServicePaymentSelectionRes.ResponseCode = code; break;
        case OPENV2G_PROTOCOL_MESSAGE_AUTHORIZATION:
            body->ContractAuthenticationRes_isUsed = 1u; body->ContractAuthenticationRes.ResponseCode = code;
            body->ContractAuthenticationRes.EVSEProcessing = value->ongoing ? din_EVSEProcessingType_Ongoing : din_EVSEProcessingType_Finished; break;
        case OPENV2G_PROTOCOL_MESSAGE_CHARGE_PARAMETER: {
            struct din_DC_EVSEChargeParameterType *parameters = &body->ChargeParameterDiscoveryRes.DC_EVSEChargeParameter;
            body->ChargeParameterDiscoveryRes_isUsed = 1u; body->ChargeParameterDiscoveryRes.ResponseCode = code;
            body->ChargeParameterDiscoveryRes.EVSEProcessing = value->ongoing ? din_EVSEProcessingType_Ongoing : din_EVSEProcessingType_Finished;
            REQUIRE(value->ongoing || (!value->omit_pending_parameters && !value->omit_pending_schedules));
            body->ChargeParameterDiscoveryRes.DC_EVSEChargeParameter_isUsed = 1u;
            evse_status(&parameters->DC_EVSEStatus, value);
            OUT_PHYS(parameters->EVSEMaximumVoltageLimit, value->max_voltage_mv, V);
            OUT_PHYS(parameters->EVSEMaximumCurrentLimit, value->max_current_ma, A);
            OUT_PHYS(parameters->EVSEMaximumPowerLimit, (int64_t)value->max_power_w * 1000, W);
            parameters->EVSEMaximumPowerLimit_isUsed = 1u;
            OUT_PHYS(parameters->EVSEMinimumVoltageLimit, value->min_voltage_mv, V);
            OUT_PHYS(parameters->EVSEMinimumCurrentLimit, value->min_current_ma, A);
            OUT_PHYS(parameters->EVSEPeakCurrentRipple, 0, A);
            body->ChargeParameterDiscoveryRes.SAScheduleList_isUsed = 1u;
            {
                unsigned i, j, count = value->schedule_count != 0u ? value->schedule_count : 1u;
                REQUIRE(count <= OPENV2G_EXI_SCHEDULE_MAX);
                body->ChargeParameterDiscoveryRes.SAScheduleList.SAScheduleTuple.arrayLen = (uint16_t)count;
                for (i = 0u; i < count; ++i) {
                    OpenV2g_ExiSchedule fallback = {0};
                    const OpenV2g_ExiSchedule *schedule = &value->schedules[i];
                    struct din_SAScheduleTupleType *tuple = &body->ChargeParameterDiscoveryRes.SAScheduleList.SAScheduleTuple.array[i];
                    uint32_t end = 0u;
                    if (value->schedule_count == 0u) { fallback.id = value->schedule_id; fallback.count = 1u; fallback.points[0].duration_s = value->schedule_duration_s; fallback.points[0].power_w = value->schedule_power_w; schedule = &fallback; }
                    REQUIRE(schedule->id != 0u && schedule->count > 0u && schedule->count <= OPENV2G_EXI_SCHEDULE_POINTS_MAX);
                    for (j = 0u; j < i; ++j) { REQUIRE(schedule->id != value->schedules[j].id); }
                    tuple->SAScheduleTupleID = schedule->id; tuple->PMaxSchedule.PMaxScheduleID = 1;
                    tuple->PMaxSchedule.PMaxScheduleEntry.arrayLen = schedule->count;
                    for (j = 0u; j < schedule->count; ++j) {
                        const OpenV2g_ExiSchedulePoint *point = &schedule->points[j];
                        struct din_PMaxScheduleEntryType *entry = &tuple->PMaxSchedule.PMaxScheduleEntry.array[j];
                        REQUIRE(point->start_s == end && point->duration_s > 0u && point->duration_s <= UINT32_MAX - end && point->power_w >= 0);
                        end += point->duration_s;
                        entry->RelativeTimeInterval_isUsed = 1u; entry->RelativeTimeInterval.start = point->start_s;
                        entry->RelativeTimeInterval.duration_isUsed = 1u; entry->RelativeTimeInterval.duration = point->duration_s;
                        REQUIRE(point->power_w <= INT16_MAX); entry->PMax = (int16_t)point->power_w;
                    }
                }
            }
            if (value->omit_pending_parameters) { body->ChargeParameterDiscoveryRes.DC_EVSEChargeParameter_isUsed = 0u; }
            if (value->omit_pending_schedules) { body->ChargeParameterDiscoveryRes.SAScheduleList_isUsed = 0u; }
            break;
        }
        case OPENV2G_PROTOCOL_MESSAGE_CABLE_CHECK:
            body->CableCheckRes_isUsed = 1u; body->CableCheckRes.ResponseCode = code;
            evse_status(&body->CableCheckRes.DC_EVSEStatus, value);
            body->CableCheckRes.EVSEProcessing = value->ongoing ? din_EVSEProcessingType_Ongoing : din_EVSEProcessingType_Finished; break;
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
static bool from_generated(const struct din_exiDocument *generated, OpenV2g_ExiDcDocument *value)
{
    const struct din_BodyType *body = &generated->V2G_Message.Body;
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
        value->accepted = body->SessionSetupRes.ResponseCode == din_responseCodeType_OK || body->SessionSetupRes.ResponseCode == din_responseCodeType_OK_NewSessionEstablished;
        value->new_session = body->SessionSetupRes.ResponseCode == din_responseCodeType_OK_NewSessionEstablished;
        REQUIRE(body->SessionSetupRes.EVSEID.bytesLen > 0u && body->SessionSetupRes.EVSEID.bytesLen <= sizeof(value->evse_id));
        value->evse_id_length = body->SessionSetupRes.EVSEID.bytesLen;
        (void)memcpy(value->evse_id, body->SessionSetupRes.EVSEID.bytes, value->evse_id_length);
    }
    if (body->ServiceDetailReq_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_SERVICE_DETAIL; value->response = false;
        value->service_id = body->ServiceDetailReq.ServiceID;
    }
    if (body->ServiceDetailRes_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_SERVICE_DETAIL; value->response = true;
        unsigned i, j;
        value->accepted = body->ServiceDetailRes.ResponseCode == din_responseCodeType_OK; value->service_id = body->ServiceDetailRes.ServiceID;
        if (body->ServiceDetailRes.ServiceParameterList_isUsed) {
            unsigned count = body->ServiceDetailRes.ServiceParameterList.ParameterSet.arrayLen;
            REQUIRE(count > 0u && count <= OPENV2G_EXI_PARAMETER_MAX);
            for (i = 0u; i < count; ++i) {
                const struct din_ParameterSetType *set = &body->ServiceDetailRes.ServiceParameterList.ParameterSet.array[i];
                for (j = 0u; j < i; ++j) { REQUIRE(body->ServiceDetailRes.ServiceParameterList.ParameterSet.array[j].ParameterSetID != set->ParameterSetID); }
                REQUIRE(parameter_in(&set->Parameter,set->ParameterSetID,&value->parameters[value->parameter_count])); ++value->parameter_count;
            }
        }
    }
    if (body->ServiceDiscoveryReq_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_SERVICE_DISCOVERY; value->response = false;
        REQUIRE(!body->ServiceDiscoveryReq.ServiceScope_isUsed && !body->ServiceDiscoveryReq.ServiceCategory_isUsed);
    }
    if (body->ServiceDiscoveryRes_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_SERVICE_DISCOVERY; value->response = true;
        value->accepted = body->ServiceDiscoveryRes.ResponseCode == din_responseCodeType_OK;
        REQUIRE(body->ServiceDiscoveryRes.PaymentOptions.PaymentOption.arrayLen == 1u && body->ServiceDiscoveryRes.PaymentOptions.PaymentOption.array[0] == din_paymentOptionType_ExternalPayment);
        REQUIRE(body->ServiceDiscoveryRes.ChargeService.EnergyTransferType == din_EVSESupportedEnergyTransferType_DC_core);
        REQUIRE(body->ServiceDiscoveryRes.ChargeService.ServiceTag.ServiceCategory == din_serviceCategoryType_EVCharging && !body->ServiceDiscoveryRes.ServiceList_isUsed);
        value->service_id = body->ServiceDiscoveryRes.ChargeService.ServiceTag.ServiceID;
    }
    if (body->ServicePaymentSelectionReq_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_SERVICE_SELECTION; value->response = false;
        REQUIRE(body->ServicePaymentSelectionReq.SelectedPaymentOption == din_paymentOptionType_ExternalPayment && body->ServicePaymentSelectionReq.SelectedServiceList.SelectedService.arrayLen == 1u);
        REQUIRE(!body->ServicePaymentSelectionReq.SelectedServiceList.SelectedService.array[0].ParameterSetID_isUsed);
        value->service_id = body->ServicePaymentSelectionReq.SelectedServiceList.SelectedService.array[0].ServiceID;
    }
    if (body->ServicePaymentSelectionRes_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_SERVICE_SELECTION; value->response = true;
        value->accepted = body->ServicePaymentSelectionRes.ResponseCode == din_responseCodeType_OK;
    }
    if (body->ContractAuthenticationReq_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_AUTHORIZATION; value->response = false;
        REQUIRE(!body->ContractAuthenticationReq.Id_isUsed && !body->ContractAuthenticationReq.GenChallenge_isUsed);
    }
    if (body->ContractAuthenticationRes_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_AUTHORIZATION; value->response = true;
        value->accepted = body->ContractAuthenticationRes.ResponseCode == din_responseCodeType_OK;
        REQUIRE(body->ContractAuthenticationRes.EVSEProcessing == din_EVSEProcessingType_Finished || body->ContractAuthenticationRes.EVSEProcessing == din_EVSEProcessingType_Ongoing);
        value->ongoing = body->ContractAuthenticationRes.EVSEProcessing != din_EVSEProcessingType_Finished;
    }
    if (body->ChargeParameterDiscoveryReq_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_CHARGE_PARAMETER; value->response = false;
        REQUIRE(body->ChargeParameterDiscoveryReq.DC_EVChargeParameter_isUsed && !body->ChargeParameterDiscoveryReq.AC_EVChargeParameter_isUsed && !body->ChargeParameterDiscoveryReq.EVChargeParameter_isUsed);
        REQUIRE(body->ChargeParameterDiscoveryReq.EVRequestedEnergyTransferType == din_EVRequestedEnergyTransferType_DC_core);
        REQUIRE(request_status_in(&body->ChargeParameterDiscoveryReq.DC_EVChargeParameter.DC_EVStatus, value));
        IN_PHYS(body->ChargeParameterDiscoveryReq.DC_EVChargeParameter.EVMaximumVoltageLimit, V, physical); value->max_voltage_mv = (uint32_t)physical;
        IN_PHYS(body->ChargeParameterDiscoveryReq.DC_EVChargeParameter.EVMaximumCurrentLimit, A, value->max_current_ma);
        REQUIRE(body->ChargeParameterDiscoveryReq.DC_EVChargeParameter.EVMaximumPowerLimit_isUsed);
        IN_PHYS(body->ChargeParameterDiscoveryReq.DC_EVChargeParameter.EVMaximumPowerLimit, W, physical); REQUIRE(physical % 1000 == 0); value->max_power_w = physical / 1000;
    }
    if (body->ChargeParameterDiscoveryRes_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_CHARGE_PARAMETER; value->response = true;
        value->accepted = body->ChargeParameterDiscoveryRes.ResponseCode == din_responseCodeType_OK;
        REQUIRE(body->ChargeParameterDiscoveryRes.EVSEProcessing == din_EVSEProcessingType_Finished || body->ChargeParameterDiscoveryRes.EVSEProcessing == din_EVSEProcessingType_Ongoing);
        value->ongoing = body->ChargeParameterDiscoveryRes.EVSEProcessing != din_EVSEProcessingType_Finished;
        REQUIRE(!body->ChargeParameterDiscoveryRes.AC_EVSEChargeParameter_isUsed && (value->ongoing || (!body->ChargeParameterDiscoveryRes.EVSEChargeParameter_isUsed && !body->ChargeParameterDiscoveryRes.SASchedules_isUsed)));
        value->omit_pending_parameters = !body->ChargeParameterDiscoveryRes.DC_EVSEChargeParameter_isUsed;
        value->omit_pending_schedules = !body->ChargeParameterDiscoveryRes.SAScheduleList_isUsed;
        REQUIRE(value->ongoing || (!value->omit_pending_parameters && !value->omit_pending_schedules));
        if (!value->omit_pending_parameters) {
        REQUIRE(status_in(&body->ChargeParameterDiscoveryRes.DC_EVSEChargeParameter.DC_EVSEStatus, value));
        IN_PHYS(body->ChargeParameterDiscoveryRes.DC_EVSEChargeParameter.EVSEMinimumVoltageLimit, V, physical); value->min_voltage_mv = (uint32_t)physical;
        IN_PHYS(body->ChargeParameterDiscoveryRes.DC_EVSEChargeParameter.EVSEMinimumCurrentLimit, A, value->min_current_ma);
        REQUIRE(!body->ChargeParameterDiscoveryRes.DC_EVSEChargeParameter.EVSECurrentRegulationTolerance_isUsed);
        REQUIRE(!body->ChargeParameterDiscoveryRes.DC_EVSEChargeParameter.EVSEEnergyToBeDelivered_isUsed);
        IN_PHYS(body->ChargeParameterDiscoveryRes.DC_EVSEChargeParameter.EVSEMaximumVoltageLimit, V, physical); value->max_voltage_mv = (uint32_t)physical;
        IN_PHYS(body->ChargeParameterDiscoveryRes.DC_EVSEChargeParameter.EVSEMaximumCurrentLimit, A, value->max_current_ma);
        REQUIRE(body->ChargeParameterDiscoveryRes.DC_EVSEChargeParameter.EVSEMaximumPowerLimit_isUsed);
        IN_PHYS(body->ChargeParameterDiscoveryRes.DC_EVSEChargeParameter.EVSEMaximumPowerLimit, W, physical); REQUIRE(physical % 1000 == 0); value->max_power_w = physical / 1000;
        }
        if (!value->omit_pending_schedules) {
        unsigned i, j;
        unsigned count = body->ChargeParameterDiscoveryRes.SAScheduleList.SAScheduleTuple.arrayLen;
        REQUIRE(count > 0u && count <= OPENV2G_EXI_SCHEDULE_MAX);
        value->schedule_count = (uint8_t)count;
        for (i = 0u; i < count; ++i) {
            const struct din_SAScheduleTupleType *tuple = &body->ChargeParameterDiscoveryRes.SAScheduleList.SAScheduleTuple.array[i];
            OpenV2g_ExiSchedule *schedule = &value->schedules[i];
            uint32_t end = 0u;
            REQUIRE(!tuple->SalesTariff_isUsed && tuple->SAScheduleTupleID > 0 && tuple->SAScheduleTupleID <= UINT8_MAX);
            schedule->id = (uint8_t)tuple->SAScheduleTupleID;
            for (j = 0u; j < i; ++j) { REQUIRE(schedule->id != value->schedules[j].id); }
            REQUIRE(tuple->PMaxSchedule.PMaxScheduleEntry.arrayLen > 0u && tuple->PMaxSchedule.PMaxScheduleEntry.arrayLen <= OPENV2G_EXI_SCHEDULE_POINTS_MAX);
            schedule->count = (uint8_t)tuple->PMaxSchedule.PMaxScheduleEntry.arrayLen;
            for (j = 0u; j < schedule->count; ++j) {
                const struct din_PMaxScheduleEntryType *entry = &tuple->PMaxSchedule.PMaxScheduleEntry.array[j];
                OpenV2g_ExiSchedulePoint *point = &schedule->points[j];
                REQUIRE(entry->RelativeTimeInterval_isUsed && !entry->TimeInterval_isUsed && entry->RelativeTimeInterval.duration_isUsed);
                point->start_s = entry->RelativeTimeInterval.start; point->duration_s = entry->RelativeTimeInterval.duration;
                REQUIRE(point->start_s == end && point->duration_s > 0u && point->duration_s <= UINT32_MAX - end);
                end += point->duration_s;
                point->power_w = entry->PMax; REQUIRE(point->power_w >= 0);
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
        value->accepted = body->CableCheckRes.ResponseCode == din_responseCodeType_OK;
        REQUIRE(body->CableCheckRes.EVSEProcessing == din_EVSEProcessingType_Finished || body->CableCheckRes.EVSEProcessing == din_EVSEProcessingType_Ongoing);
        value->ongoing = body->CableCheckRes.EVSEProcessing != din_EVSEProcessingType_Finished;
        REQUIRE(status_in(&body->CableCheckRes.DC_EVSEStatus, value));
    }
    if (body->PreChargeReq_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_PRECHARGE; value->response = false;
        REQUIRE(request_status_in(&body->PreChargeReq.DC_EVStatus, value));
        IN_PHYS(body->PreChargeReq.EVTargetVoltage, V, physical); value->voltage_mv = (uint32_t)physical;
        IN_PHYS(body->PreChargeReq.EVTargetCurrent, A, value->current_ma);
    }
    if (body->PreChargeRes_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_PRECHARGE; value->response = true;
        value->accepted = body->PreChargeRes.ResponseCode == din_responseCodeType_OK;
        REQUIRE(status_in(&body->PreChargeRes.DC_EVSEStatus, value));
        IN_PHYS(body->PreChargeRes.EVSEPresentVoltage, V, physical); value->voltage_mv = (uint32_t)physical;
    }
    if (body->PowerDeliveryReq_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_START; value->response = false;
        REQUIRE(!body->PowerDeliveryReq.ChargingProfile_isUsed && !body->PowerDeliveryReq.EVPowerDeliveryParameter_isUsed);
        REQUIRE(body->PowerDeliveryReq.DC_EVPowerDeliveryParameter_isUsed);
        REQUIRE(request_status_in(&body->PowerDeliveryReq.DC_EVPowerDeliveryParameter.DC_EVStatus, value));
        value->kind = body->PowerDeliveryReq.ReadyToChargeState ? OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_START : OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_STOP;
    }
    if (body->PowerDeliveryRes_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_START; value->response = true;
        value->accepted = body->PowerDeliveryRes.ResponseCode == din_responseCodeType_OK;
        REQUIRE(body->PowerDeliveryRes.DC_EVSEStatus_isUsed && !body->PowerDeliveryRes.AC_EVSEStatus_isUsed && !body->PowerDeliveryRes.EVSEStatus_isUsed);
        REQUIRE(status_in(&body->PowerDeliveryRes.DC_EVSEStatus, value));
    }
    if (body->CurrentDemandReq_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND; value->response = false;
        REQUIRE(request_status_in(&body->CurrentDemandReq.DC_EVStatus, value));
        value->charging_complete = body->CurrentDemandReq.ChargingComplete;
        IN_PHYS(body->CurrentDemandReq.EVTargetVoltage, V, physical); value->voltage_mv = (uint32_t)physical;
        IN_PHYS(body->CurrentDemandReq.EVTargetCurrent, A, value->current_ma);
    }
    if (body->CurrentDemandRes_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND; value->response = true;
        value->accepted = body->CurrentDemandRes.ResponseCode == din_responseCodeType_OK;
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
    }
    if (body->WeldingDetectionReq_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_WELDING_DETECTION; value->response = false;
        REQUIRE(request_status_in(&body->WeldingDetectionReq.DC_EVStatus, value));
    }
    if (body->WeldingDetectionRes_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_WELDING_DETECTION; value->response = true;
        value->accepted = body->WeldingDetectionRes.ResponseCode == din_responseCodeType_OK;
        REQUIRE(status_in(&body->WeldingDetectionRes.DC_EVSEStatus, value));
        IN_PHYS(body->WeldingDetectionRes.EVSEPresentVoltage, V, physical); value->voltage_mv = (uint32_t)physical;
    }
    if (body->SessionStopReq_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_SESSION_STOP; value->response = false;
    }
    if (body->SessionStopRes_isUsed) { ++selected; value->kind = OPENV2G_PROTOCOL_MESSAGE_SESSION_STOP; value->response = true;
        value->accepted = body->SessionStopRes.ResponseCode == din_responseCodeType_OK;
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
    struct din_exiDocument generated;
    uint8_t guarded[OPENV2G_EXI_DC_PAYLOAD_MAX + 1u] = {0};
    exi_bitstream_t stream;
    int error;
    (void)context;
    if (length != NULL) { *length = 0u; }
    if (value == NULL || size != sizeof(*value) || output == NULL || capacity == 0u || length == NULL ||
        id != (uint16_t)((unsigned)value->kind + (value->response ? OPENV2G_EXI_DC_RESPONSE_OFFSET : 0u))) { return rejected(diagnostic); }
    (void)memset(&generated, 0, sizeof(generated));
    init_din_exiDocument(&generated);
    if (!to_generated(value, &generated)) { return rejected(diagnostic); }
    if (capacity > OPENV2G_EXI_DC_PAYLOAD_MAX) { capacity = OPENV2G_EXI_DC_PAYLOAD_MAX; }
    exi_bitstream_init(&stream, guarded, capacity, 0u, NULL);
    error = encode_din_exiDocument(&stream, &generated);
    if (error != 0 || exi_bitstream_get_length(&stream) > capacity) { return rejected(diagnostic); }
    *length = exi_bitstream_get_length(&stream); (void)memcpy(output, guarded, *length);
    return OPENV2G_RESULT_OK;
}
static OpenV2g_Result decode(void *context, const uint8_t *input, size_t length,
    void *object, size_t size, uint8_t *workspace, size_t capacity, uint16_t *id, OpenV2g_ExiDiagnostic *diagnostic)
{
    struct din_exiDocument generated;
    OpenV2g_ExiDcDocument *value = object;
    exi_bitstream_t stream;
    uint32_t padding;
    (void)context;
    if (input == NULL || length == 0u || length > OPENV2G_EXI_DC_PAYLOAD_MAX || value == NULL || size != sizeof(*value) ||
        workspace == NULL || capacity < OPENV2G_EXI_DC_WORKSPACE_SIZE || id == NULL) { return rejected(diagnostic); }
    (void)memset(workspace, 0, capacity); (void)memcpy(workspace, input, length);
    (void)memset(&generated, 0, sizeof(generated)); init_din_exiDocument(&generated);
    exi_bitstream_init(&stream, workspace, length, 0u, NULL);
    if (decode_din_exiDocument(&stream, &generated) != 0 || exi_bitstream_get_length(&stream) != length ||
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
OpenV2g_Result OpenV2g_Cbv2gDin_Bind(OpenV2g_ExiCodec *codec)
{
    static const OpenV2g_SchemaDescriptor descriptor = {
        OPENV2G_SCHEMA_DIN_70121, OPENV2G_PROTOCOL_DIN_70121, OPENV2G_MODE_DC,
        OPENV2G_EXI_DC_PAYLOAD_MAX, OPENV2G_EXI_DC_WORKSPACE_SIZE, sizeof(OpenV2g_ExiDcDocument), 1u,
        "libcbv2g-din-dc-host/" OPENV2G_CBV2G_COMMIT,
        "upstream generated model; exact XSD provenance OPEN", OPENV2G_CBV2G_DIGEST
    };
    const OpenV2g_ExiBackend backend = {NULL, encode, decode, reset};
    return OpenV2g_ExiCodec_Bind(codec, &descriptor, &backend);
}
