#include "openv2g/linux/bsw.h"
#include <string.h>

void Ov2g_ServicesInit(Ov2g_Services *services)
{
    if (services != NULL) { (void)memset(services, 0, sizeof(*services)); }
}
static void report(void *context, uint16_t id, OpenV2g_ReasonCode reason)
{
    Ov2g_Services *services = context;
    if (services == NULL) { return; }
    if (services->count == 16u) { services->overflow = true; return; }
    services->events[services->count].id = id;
    services->events[services->count++].reason = reason;
}
OpenV2g_DiagnosticPort Ov2g_DemPort(Ov2g_Services *services)
{
    OpenV2g_DiagnosticPort port = {services, report};
    return port;
}
static OpenV2g_Result crypto(void *context, uint32_t job, uint32_t key,
                            const uint8_t *input, size_t length)
{
    (void)context; (void)job; (void)key; (void)input; (void)length;
    return OPENV2G_RESULT_NOT_IMPLEMENTED;
}
OpenV2g_CryptoPort Ov2g_CryptoPort(void)
{
    OpenV2g_CryptoPort port = {NULL, crypto};
    return port;
}
bool Ov2g_NvWrite(Ov2g_Services *services, size_t block, const void *data, size_t length)
{
    if ((services == NULL) || (block >= OV2G_NVM_BLOCKS) || (data == NULL) ||
        (length == 0u) || (length > OV2G_NVM_BLOCK_SIZE)) { return false; }
    (void)memcpy(services->nvm[block], data, length);
    services->nvm_length[block] = length;
    return true;
}
bool Ov2g_NvRead(const Ov2g_Services *services, size_t block, void *data, size_t length)
{
    if ((services == NULL) || (block >= OV2G_NVM_BLOCKS) || (data == NULL) ||
        (length == 0u) || (length != services->nvm_length[block])) { return false; }
    (void)memcpy(data, services->nvm[block], length);
    return true;
}
