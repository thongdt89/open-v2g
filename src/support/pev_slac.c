#include "openv2g/support/pev_slac.h"
#include "openv2g/common/validation.h"
#include <string.h>
static bool nonzero(const uint8_t *value, unsigned size)
{
    unsigned i; uint8_t combined = 0u; for (i = 0u; i < size; ++i) { combined |= value[i]; } return combined != 0u;
}
static bool mac(const uint8_t value[6]) { return nonzero(value,6u) && (value[0] & 1u) == 0u; }
static void publish(OpenV2g_PevSlac *slac, OpenV2g_SlacState state, OpenV2g_TimeUs now, OpenV2g_ReasonCode reason)
{
    slac->status.state = state; ++slac->status.env.sequence_counter; slac->status.env.timestamp = now;
    slac->status.env.quality = OPENV2G_QUALITY_VALID; slac->status.env.reason = reason;
}
static void clear_evidence(OpenV2g_PevSlac *slac)
{
    slac->rx_count = 0u; slac->rx_head = 0u; slac->overflow = false;
    (void)memset(slac->rx,0,sizeof(slac->rx)); (void)memset(slac->status.network_id,0,sizeof(slac->status.network_id));
    slac->status.key_handle = slac->status.key_version = 0u;
}
static void cleanup(OpenV2g_PevSlac *slac)
{
    if (!slac->cancel_notified) { slac->ports.cancel(slac->ports.context,slac->status.env.session_generation); slac->cancel_notified = true; }
    clear_evidence(slac);
}
static OpenV2g_Result fail(OpenV2g_PevSlac *slac, OpenV2g_TimeUs now, OpenV2g_ReasonCode reason)
{
    /* An established link loses its proof immediately; coordinated Stop owns
     * provider teardown after permission revocation. Matching has no energy authority. */
    if (slac->status.state == OPENV2G_SLAC_MATCHED) { clear_evidence(slac); }
    else { cleanup(slac); }
    publish(slac,OPENV2G_SLAC_FAILED,now,reason); return OPENV2G_RESULT_REJECTED;
}
static void stage(OpenV2g_PevSlac *slac, OpenV2g_SlacState state, OpenV2g_TimeUs now)
{
    slac->stage_started = now;
    slac->stage_deadline = slac->config.stage_budget < slac->total_deadline-now ? now+slac->config.stage_budget : slac->total_deadline;
    slac->transmissions = 0u; publish(slac,state,now,OPENV2G_REASON_NONE);
}
static bool send_frame(OpenV2g_PevSlac *slac, OpenV2g_SlacMessageKind kind, OpenV2g_TimeUs now)
{
    OpenV2g_SlacFrame frame = {0}; OpenV2g_Result result;
    frame.env = slac->status.env; frame.env.timestamp = now; frame.kind = kind;
    (void)memcpy(frame.run_id,slac->status.run_id,8u); (void)memcpy(frame.source,slac->pev,6u);
    if (kind == OPENV2G_SLAC_PARAM_REQ) { (void)memset(frame.destination,0xff,6u); }
    else { (void)memcpy(frame.destination,slac->status.peer,6u); }
    frame.sound_count = slac->config.sound_count; frame.group_count = slac->config.group_count;
    frame.sounds_remaining = kind == OPENV2G_SLAC_SOUND ? (uint8_t)(slac->config.sound_count-slac->sounds_sent-1u) : 0u;
    frame.accepted = true;
    result = slac->ports.send(slac->ports.context,&frame);
    return result == OPENV2G_RESULT_OK || result == OPENV2G_RESULT_ACCEPTED;
}
OpenV2g_Result OpenV2g_PevSlac_Init(OpenV2g_PevSlac *slac, const OpenV2g_SlacConfig *config, const OpenV2g_SlacPorts *ports)
{
    if (slac == NULL || config == NULL || ports == NULL || ports->send == NULL || ports->cancel == NULL || ports->key_is_current == NULL ||
        config->start_indications == 0u || config->start_indications > 8u || config->sound_count == 0u || config->sound_count > 16u ||
        config->group_count == 0u || config->group_count > OPENV2G_SLAC_GROUPS_MAX || config->max_transmissions == 0u || config->max_transmissions > 4u ||
        config->stage_budget == 0u || config->total_budget < config->stage_budget || config->retry_interval == 0u || config->retry_interval >= config->stage_budget) { return OPENV2G_RESULT_REJECTED; }
    (void)memset(slac,0,sizeof(*slac)); slac->config = *config; slac->ports = *ports; slac->bound = true; return OPENV2G_RESULT_OK;
}
OpenV2g_Result OpenV2g_PevSlac_Start(OpenV2g_PevSlac *slac, uint32_t generation, OpenV2g_TimeUs now, const uint8_t run_id[8], const uint8_t pev[6])
{
    uint32_t old;
    if (slac == NULL || !slac->bound || generation == 0u || now == 0u || now < slac->owner_time || run_id == NULL || pev == NULL ||
        !nonzero(run_id,8u) || !mac(pev) || now > UINT64_MAX-slac->config.total_budget ||
        (slac->status.state != OPENV2G_SLAC_IDLE && slac->status.state != OPENV2G_SLAC_CANCELLED && slac->status.state != OPENV2G_SLAC_FAILED) ||
        (slac->status.state == OPENV2G_SLAC_FAILED && !slac->cancel_notified) || memcmp(run_id,slac->last_run_id,8u) == 0) { return OPENV2G_RESULT_REJECTED; }
    old = slac->status.env.session_generation;
    if (old != 0u && (generation == old || (uint32_t)(generation-old) >= 0x80000000u)) { return OPENV2G_RESULT_REJECTED; }
    slac->status.env.session_generation = generation; slac->status.env.sequence_counter = 0u; slac->owner_time = now; slac->cancel_notified = false;
    slac->total_deadline = now+slac->config.total_budget; slac->rx_count = slac->rx_head = 0u; slac->overflow = false;
    (void)memcpy(slac->status.run_id,run_id,8u); (void)memcpy(slac->last_run_id,run_id,8u); (void)memcpy(slac->pev,pev,6u);
    (void)memset(slac->status.peer,0,6u); slac->starts_sent = slac->sounds_sent = 0u;
    stage(slac,OPENV2G_SLAC_WAIT_PARAM,now);
    if (!send_frame(slac,OPENV2G_SLAC_PARAM_REQ,now)) { return fail(slac,now,OPENV2G_REASON_PRECONDITION); }
    slac->transmissions = 1u; slac->retry_at = now+slac->config.retry_interval; return OPENV2G_RESULT_ACCEPTED;
}
OpenV2g_Result OpenV2g_PevSlac_Enqueue(OpenV2g_PevSlac *slac, const OpenV2g_SlacFrame *frame)
{
    unsigned slot;
    if (slac == NULL || !slac->bound || frame == NULL || frame->env.session_generation == 0u || frame->env.session_generation != slac->status.env.session_generation ||
        slac->status.state == OPENV2G_SLAC_IDLE || slac->status.state == OPENV2G_SLAC_MATCHED ||
        slac->status.state == OPENV2G_SLAC_FAILED || slac->status.state == OPENV2G_SLAC_CANCELLED ||
        frame->kind < OPENV2G_SLAC_PARAM_REQ || frame->kind > OPENV2G_SLAC_MATCH_CNF || frame->group_count > OPENV2G_SLAC_GROUPS_MAX || frame->sound_count > 16u) { return OPENV2G_RESULT_REJECTED; }
    if (slac->rx_count >= OPENV2G_SLAC_RX_CAPACITY) { slac->overflow = true; return OPENV2G_RESULT_OVERFLOW; }
    slot = ((unsigned)slac->rx_head+slac->rx_count)%OPENV2G_SLAC_RX_CAPACITY; slac->rx[slot] = *frame; ++slac->rx_count;
    return OPENV2G_RESULT_ACCEPTED;
}
OpenV2g_Result OpenV2g_PevSlac_Pump(OpenV2g_PevSlac *slac, uint32_t generation, OpenV2g_TimeUs now)
{
    if (slac == NULL || !slac->bound || generation == 0u || generation != slac->status.env.session_generation) { return OPENV2G_RESULT_REJECTED; }
    if (now == 0u || now < slac->owner_time) { return fail(slac,slac->owner_time,OPENV2G_REASON_STALE_DATA); }
    slac->owner_time = now;
    if (slac->status.state == OPENV2G_SLAC_MATCHED) {
        if (!slac->ports.key_is_current(slac->ports.context,generation,slac->status.network_id,slac->status.key_handle,slac->status.key_version)) { return fail(slac,now,OPENV2G_REASON_PRECONDITION); }
        return OPENV2G_RESULT_OK;
    }
    if (slac->status.state == OPENV2G_SLAC_IDLE || slac->status.state == OPENV2G_SLAC_FAILED || slac->status.state == OPENV2G_SLAC_CANCELLED) { return OPENV2G_RESULT_REJECTED; }
    if (slac->overflow) { return fail(slac,now,OPENV2G_REASON_PRECONDITION); }
    if (now >= slac->total_deadline || now >= slac->stage_deadline) { return fail(slac,now,OPENV2G_REASON_TIMEOUT); }
    if (slac->rx_count != 0u) {
        OpenV2g_SlacFrame frame = slac->rx[slac->rx_head]; unsigned i,sum = 0u;
        (void)memset(&slac->rx[slac->rx_head],0,sizeof(frame)); slac->rx_head = (uint8_t)((slac->rx_head+1u)%OPENV2G_SLAC_RX_CAPACITY); --slac->rx_count;
        if (!OpenV2g_EnvelopeIsCurrent(&frame.env,generation,now,slac->config.stage_budget) || frame.env.timestamp < slac->stage_started ||
            memcmp(frame.run_id,slac->status.run_id,8u) != 0 || memcmp(frame.destination,slac->pev,6u) != 0 || !mac(frame.source) ||
            memcmp(frame.source,slac->pev,6u) == 0 || (slac->status.state != OPENV2G_SLAC_WAIT_PARAM && memcmp(frame.source,slac->status.peer,6u) != 0)) { return OPENV2G_RESULT_BUSY; }
        if (slac->status.state == OPENV2G_SLAC_WAIT_PARAM && frame.kind == OPENV2G_SLAC_PARAM_CNF) {
            if (!frame.accepted || frame.sound_count != slac->config.sound_count || frame.group_count != slac->config.group_count) { return fail(slac,now,OPENV2G_REASON_PRECONDITION); }
            (void)memcpy(slac->status.peer,frame.source,6u); stage(slac,OPENV2G_SLAC_SEND_START,now);
        } else if (slac->status.state == OPENV2G_SLAC_WAIT_ATTEN && frame.kind == OPENV2G_SLAC_ATTEN_IND) {
            if (!frame.accepted || frame.sound_count != slac->config.sound_count || frame.group_count != slac->config.group_count) { return fail(slac,now,OPENV2G_REASON_PRECONDITION); }
            for (i = 0u; i < frame.group_count; ++i) { sum += frame.attenuation[i]; }
            if (sum > (unsigned)slac->config.max_attenuation*frame.group_count) { return fail(slac,now,OPENV2G_REASON_PRECONDITION); }
            stage(slac,OPENV2G_SLAC_SEND_ATTEN_RSP,now);
        } else if (slac->status.state == OPENV2G_SLAC_WAIT_MATCH && frame.kind == OPENV2G_SLAC_MATCH_CNF) {
            if (!frame.accepted || !nonzero(frame.network_id,7u) || frame.key_handle == 0u || frame.key_version == 0u ||
                !slac->ports.key_is_current(slac->ports.context,generation,frame.network_id,frame.key_handle,frame.key_version)) { return fail(slac,now,OPENV2G_REASON_PRECONDITION); }
            slac->rx_count = slac->rx_head = 0u; (void)memset(slac->rx,0,sizeof(slac->rx));
            (void)memcpy(slac->status.network_id,frame.network_id,7u); slac->status.key_handle = frame.key_handle; slac->status.key_version = frame.key_version;
            publish(slac,OPENV2G_SLAC_MATCHED,now,OPENV2G_REASON_NONE); return OPENV2G_RESULT_OK;
        }
        return OPENV2G_RESULT_BUSY;
    }
    switch (slac->status.state) {
    case OPENV2G_SLAC_SEND_START:
        if (!send_frame(slac,OPENV2G_SLAC_START_ATTEN,now)) { return fail(slac,now,OPENV2G_REASON_PRECONDITION); }
        if (++slac->starts_sent == slac->config.start_indications) { stage(slac,OPENV2G_SLAC_SEND_SOUND,now); } break;
    case OPENV2G_SLAC_SEND_SOUND:
        if (!send_frame(slac,OPENV2G_SLAC_SOUND,now)) { return fail(slac,now,OPENV2G_REASON_PRECONDITION); }
        if (++slac->sounds_sent == slac->config.sound_count) { stage(slac,OPENV2G_SLAC_WAIT_ATTEN,now); } break;
    case OPENV2G_SLAC_SEND_ATTEN_RSP:
        if (!send_frame(slac,OPENV2G_SLAC_ATTEN_RSP,now)) { return fail(slac,now,OPENV2G_REASON_PRECONDITION); }
        stage(slac,OPENV2G_SLAC_SEND_MATCH,now); break;
    case OPENV2G_SLAC_SEND_MATCH:
        stage(slac,OPENV2G_SLAC_WAIT_MATCH,now);
        if (!send_frame(slac,OPENV2G_SLAC_MATCH_REQ,now)) { return fail(slac,now,OPENV2G_REASON_PRECONDITION); }
        slac->transmissions = 1u; slac->retry_at = slac->config.retry_interval < slac->stage_deadline-now ? now+slac->config.retry_interval : slac->stage_deadline; break;
    case OPENV2G_SLAC_WAIT_PARAM:
    case OPENV2G_SLAC_WAIT_MATCH:
        if (now >= slac->retry_at && slac->transmissions < slac->config.max_transmissions) {
            if (!send_frame(slac,slac->status.state == OPENV2G_SLAC_WAIT_PARAM ? OPENV2G_SLAC_PARAM_REQ : OPENV2G_SLAC_MATCH_REQ,now)) { return fail(slac,now,OPENV2G_REASON_PRECONDITION); }
            ++slac->transmissions; slac->retry_at = slac->config.retry_interval < slac->stage_deadline-now ? now+slac->config.retry_interval : slac->stage_deadline;
        }
        break;
    default: break;
    }
    return OPENV2G_RESULT_BUSY;
}
OpenV2g_Result OpenV2g_PevSlac_Cancel(OpenV2g_PevSlac *slac, uint32_t generation, OpenV2g_TimeUs now)
{
    if (slac == NULL || !slac->bound || generation == 0u || generation != slac->status.env.session_generation) { return OPENV2G_RESULT_REJECTED; }
    if (slac->status.state == OPENV2G_SLAC_CANCELLED) { return OPENV2G_RESULT_OK; }
    if (now < slac->owner_time) { now = slac->owner_time; }
    slac->owner_time = now; cleanup(slac); publish(slac,OPENV2G_SLAC_CANCELLED,now,OPENV2G_REASON_CANCELLED); return OPENV2G_RESULT_OK;
}
