#ifndef OPENV2G_LINUX_PKI_H
#define OPENV2G_LINUX_PKI_H
#include "openv2g/linux/crypto.h"
#include "openv2g/iso15118_chrg/certificate_manager.h"
/* Optional host client-signing profile. Not the selected OEM/PnC PKI policy. */
typedef struct {
    Ov2g_Crypto *crypto;
    void *store;
    char identity[OPENV2G_CERT_IDENTITY_MAX];
    OpenV2g_TimeUs time_max_age;
    OpenV2g_TimeUs proof_ttl;
} Ov2g_Pki;
OpenV2g_Result Ov2g_PkiInit(Ov2g_Pki *, Ov2g_Crypto *, const char *ca_file,
    const char *crl_file, const char *dns_identity, OpenV2g_TimeUs time_max_age, OpenV2g_TimeUs proof_ttl);
void Ov2g_PkiClose(Ov2g_Pki *);
/* Populate only security and monotonic-clock ports; caller supplies storage ports. */
void Ov2g_PkiBind(Ov2g_Pki *, OpenV2g_CertificatePorts *);
#endif
