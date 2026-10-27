#define _POSIX_C_SOURCE 200809L
#include "openv2g/linux/tls.h"
#include "openv2g/linux/bsw.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <sys/socket.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/x509v3.h>

/* Socket BIO uses MSG_NOSIGNAL: never change the application's SIGPIPE policy. */
static int bio_create(BIO *bio) { BIO_set_init(bio, 1); return 1; }
static int bio_destroy(BIO *bio) { if (bio != NULL) { BIO_set_data(bio, NULL); BIO_set_init(bio, 0); } return 1; }
static long bio_ctrl(BIO *bio, int command, long number, void *pointer)
{
    (void)bio; (void)number; (void)pointer;
    return command == BIO_CTRL_FLUSH ? 1 : 0;
}
static int bio_read(BIO *bio, char *output, int length)
{
    Ov2g_Tls *tls = BIO_get_data(bio);
    ssize_t result;
    BIO_clear_retry_flags(bio);
    if (tls == NULL || output == NULL || length <= 0) { return 0; }
    result = recv(tls->fd, output, (size_t)length, 0);
    if (result < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) { BIO_set_retry_read(bio); }
    return (int)result;
}
static int bio_write(BIO *bio, const char *input, int length)
{
    Ov2g_Tls *tls = BIO_get_data(bio);
    ssize_t result;
    BIO_clear_retry_flags(bio);
    if (tls == NULL || input == NULL || length <= 0) { return 0; }
    result = send(tls->fd, input, (size_t)length, MSG_NOSIGNAL);
    if (result < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) { BIO_set_retry_write(bio); }
    return (int)result;
}
static void release(Ov2g_Tls *tls)
{
    SSL_free(tls->ssl); tls->ssl = NULL;
    SSL_CTX_free(tls->context); tls->context = NULL;
    BIO_meth_free(tls->bio_method); tls->bio_method = NULL;
    OPENSSL_cleanse(tls->read_copy, sizeof(tls->read_copy)); tls->pending_read = 0u;
    OPENSSL_cleanse(tls->write_copy, sizeof(tls->write_copy)); tls->pending_write = 0u;
    tls->peer_verified = false;
}
static OpenV2g_Result fail(Ov2g_Tls *tls, OpenV2g_ReasonCode reason)
{
    if (tls->ssl != NULL) { tls->certificate_result = SSL_get_verify_result(tls->ssl); }
    release(tls); tls->state = OV2G_TLS_FAILED; tls->reason = reason;
    return OPENV2G_RESULT_REJECTED;
}
static bool current(const Ov2g_Tls *tls, uint32_t generation, uint32_t connection)
{
    return tls != NULL && generation != 0u && connection != 0u && tls->generation == generation && tls->connection_id == connection;
}
static int no_password(char *buffer, int size, int writing, void *context)
{
    (void)buffer; (void)size; (void)writing; (void)context; return 0;
}
void Ov2g_TlsInit(Ov2g_Tls *tls)
{
    if (tls != NULL) { (void)memset(tls, 0, sizeof(*tls)); tls->fd = -1; }
}
OpenV2g_Result Ov2g_TlsStart(Ov2g_Tls *tls, int fd, uint32_t generation, uint32_t connection,
    OpenV2g_TimeUs now, OpenV2g_TimeUs deadline, const Ov2g_TlsConfig *config)
{
    SSL_CTX *context;
    SSL *ssl;
    BIO_METHOD *method;
    BIO *bio;
    int version, flags;
    bool verify;
    if (tls == NULL || config == NULL || fd < 0 || generation == 0u || connection == 0u || now == 0u || deadline <= now ||
        (tls->state != OV2G_TLS_CLOSED && tls->state != OV2G_TLS_FAILED) ||
        (config->version != OPENV2G_SECURITY_TLS12 && config->version != OPENV2G_SECURITY_TLS13)) { return OPENV2G_RESULT_REJECTED; }
    verify = !config->server || config->require_peer_certificate;
    if ((verify && (config->trust_file == NULL || config->peer_name == NULL || config->peer_name[0] == '\0' || strnlen(config->peer_name, 256u) >= 256u)) ||
        (config->server && (config->certificate_file == NULL || config->private_key_file == NULL)) ||
        ((config->certificate_file == NULL) != (config->private_key_file == NULL)) ||
        (flags = fcntl(fd, F_GETFL, 0)) < 0 || (flags & O_NONBLOCK) == 0) { return OPENV2G_RESULT_REJECTED; }
    release(tls); tls->fd = fd; tls->generation = generation; tls->connection_id = connection; tls->deadline = deadline;
    tls->reason = OPENV2G_REASON_NONE; tls->certificate_result = X509_V_OK; tls->negotiated_version = 0u;
    tls->verify_peer = verify; tls->read_events = POLLIN; tls->write_events = POLLOUT;
    tls->handshake_events = config->server ? POLLIN : POLLOUT;
    ERR_clear_error();
    context = SSL_CTX_new(TLS_method()); tls->context = context;
    version = config->version == OPENV2G_SECURITY_TLS12 ? TLS1_2_VERSION : TLS1_3_VERSION;
    if (context == NULL || SSL_CTX_set_min_proto_version(context, version) != 1 || SSL_CTX_set_max_proto_version(context, version) != 1 ||
        SSL_CTX_set_cipher_list(context, "ECDHE-ECDSA-AES128-GCM-SHA256") != 1 ||
        SSL_CTX_set_ciphersuites(context, "TLS_AES_128_GCM_SHA256") != 1 || SSL_CTX_set1_groups_list(context, "P-256") != 1) { return fail(tls, OPENV2G_REASON_SECURITY); }
    (void)SSL_CTX_set_options(context, SSL_OP_NO_COMPRESSION | SSL_OP_NO_RENEGOTIATION | SSL_OP_NO_TICKET);
    SSL_CTX_set_max_cert_list(context, 16384u);
    (void)SSL_CTX_set_session_cache_mode(context, SSL_SESS_CACHE_OFF);
    if (SSL_CTX_set_num_tickets(context, 0u) != 1 || SSL_CTX_set_max_early_data(context, 0u) != 1) { return fail(tls, OPENV2G_REASON_SECURITY); }
    SSL_CTX_set_default_passwd_cb(context, no_password);
    if (config->certificate_file != NULL && (SSL_CTX_use_certificate_chain_file(context, config->certificate_file) != 1 ||
        SSL_CTX_use_PrivateKey_file(context, config->private_key_file, SSL_FILETYPE_PEM) != 1 || SSL_CTX_check_private_key(context) != 1)) { return fail(tls, OPENV2G_REASON_SECURITY); }
    if (verify) {
        if (SSL_CTX_load_verify_locations(context, config->trust_file, NULL) != 1) { return fail(tls, OPENV2G_REASON_SECURITY); }
        SSL_CTX_set_verify(context, SSL_VERIFY_PEER | (config->server ? SSL_VERIFY_FAIL_IF_NO_PEER_CERT : 0), NULL);
        SSL_CTX_set_verify_depth(context, 4);
        if (config->crl_file != NULL) {
            X509_STORE *store = SSL_CTX_get_cert_store(context);
            X509_LOOKUP *lookup = X509_STORE_add_lookup(store, X509_LOOKUP_file());
            if (lookup == NULL || X509_load_crl_file(lookup, config->crl_file, X509_FILETYPE_PEM) <= 0 ||
                X509_STORE_set_flags(store, X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL) != 1) { return fail(tls, OPENV2G_REASON_SECURITY); }
        }
    }
    ssl = SSL_new(context); tls->ssl = ssl;
    method = BIO_meth_new(BIO_get_new_index() | BIO_TYPE_SOURCE_SINK, "openv2g bounded nonblocking socket"); tls->bio_method = method;
    if (ssl == NULL || method == NULL || BIO_meth_set_create(method, bio_create) != 1 || BIO_meth_set_destroy(method, bio_destroy) != 1 ||
        BIO_meth_set_read(method, bio_read) != 1 || BIO_meth_set_write(method, bio_write) != 1 || BIO_meth_set_ctrl(method, bio_ctrl) != 1) { return fail(tls, OPENV2G_REASON_RESOURCE); }
    bio = BIO_new(method);
    if (bio == NULL) { return fail(tls, OPENV2G_REASON_RESOURCE); }
    BIO_set_data(bio, tls); SSL_set_bio(ssl, bio, bio);
    if (verify) {
        uint8_t address[16];
        X509_VERIFY_PARAM *parameters = SSL_get0_param(ssl);
        SSL_set_hostflags(ssl, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS | X509_CHECK_FLAG_NEVER_CHECK_SUBJECT);
        if (inet_pton(AF_INET, config->peer_name, address) == 1 || inet_pton(AF_INET6, config->peer_name, address) == 1) {
            if (X509_VERIFY_PARAM_set1_ip_asc(parameters, config->peer_name) != 1) { return fail(tls, OPENV2G_REASON_SECURITY); }
        } else if (SSL_set1_host(ssl, config->peer_name) != 1 || (!config->server && SSL_set_tlsext_host_name(ssl, config->peer_name) != 1)) { return fail(tls, OPENV2G_REASON_SECURITY); }
    }
    if (config->server) { SSL_set_accept_state(ssl); } else { SSL_set_connect_state(ssl); }
    tls->state = OV2G_TLS_HANDSHAKE; return OPENV2G_RESULT_ACCEPTED;
}
static OpenV2g_Result retry(Ov2g_Tls *tls, int result, int *events)
{
    int error = SSL_get_error(tls->ssl, result); /* Must immediately follow the same SSL call. */
    if (error == SSL_ERROR_WANT_READ) { *events = POLLIN; return OPENV2G_RESULT_BUSY; }
    if (error == SSL_ERROR_WANT_WRITE) { *events = POLLOUT; return OPENV2G_RESULT_BUSY; }
    return fail(tls, OPENV2G_REASON_SECURITY);
}
OpenV2g_Result Ov2g_TlsPump(Ov2g_Tls *tls, uint32_t generation, uint32_t connection, OpenV2g_TimeUs now)
{
    int result;
    if (!current(tls, generation, connection) || tls->state != OV2G_TLS_HANDSHAKE) { return OPENV2G_RESULT_REJECTED; }
    if (now == 0u || now >= tls->deadline) { return fail(tls, OPENV2G_REASON_TIMEOUT); }
    ERR_clear_error(); result = SSL_do_handshake(tls->ssl);
    if (result != 1) { return retry(tls, result, &tls->handshake_events); }
    if (tls->verify_peer) {
        X509 *peer = SSL_get1_peer_certificate(tls->ssl);
        tls->certificate_result = SSL_get_verify_result(tls->ssl);
        if (peer == NULL || tls->certificate_result != X509_V_OK) { X509_free(peer); return fail(tls, OPENV2G_REASON_SECURITY); }
        X509_free(peer); tls->peer_verified = true;
    }
    if (Ov2g_OsNow() >= tls->deadline) { return fail(tls, OPENV2G_REASON_TIMEOUT); }
    tls->negotiated_version = (uint16_t)SSL_version(tls->ssl); tls->state = OV2G_TLS_READY;
    return OPENV2G_RESULT_OK;
}
OpenV2g_Result Ov2g_TlsWrite(Ov2g_Tls *tls, uint32_t generation, uint32_t connection,
    const void *input, size_t length, size_t *written)
{
    int result;
    if (written != NULL) { *written = 0u; }
    if (!current(tls, generation, connection) || tls->state != OV2G_TLS_READY || input == NULL || written == NULL ||
        length == 0u || length > sizeof(tls->write_copy)) { return OPENV2G_RESULT_REJECTED; }
    if (tls->pending_read != 0u && tls->read_events == POLLOUT) { return OPENV2G_RESULT_BUSY; }
    if (tls->pending_write != 0u && (length != tls->pending_write || memcmp(tls->write_copy, input, length) != 0)) { return OPENV2G_RESULT_REJECTED; }
    if (tls->pending_write == 0u) { (void)memcpy(tls->write_copy, input, length); tls->pending_write = length; }
    ERR_clear_error(); result = SSL_write_ex(tls->ssl, tls->write_copy, tls->pending_write, written);
    if (result != 1) { return retry(tls, result, &tls->write_events); }
    OPENSSL_cleanse(tls->write_copy, tls->pending_write); tls->pending_write = 0u; tls->write_events = POLLOUT;
    return OPENV2G_RESULT_OK;
}
OpenV2g_Result Ov2g_TlsRead(Ov2g_Tls *tls, uint32_t generation, uint32_t connection,
    void *output, size_t capacity, size_t *length)
{
    int result;
    if (length != NULL) { *length = 0u; }
    if (!current(tls, generation, connection) || tls->state != OV2G_TLS_READY || output == NULL || length == NULL || capacity == 0u ||
        capacity > OPENV2G_CFG_TRANSPORT_TX_FRAME_MAX) { return OPENV2G_RESULT_REJECTED; }
    if (tls->pending_write != 0u) { return OPENV2G_RESULT_BUSY; }
    if (tls->pending_read != 0u && capacity < tls->pending_read) { return OPENV2G_RESULT_REJECTED; }
    if (tls->pending_read == 0u) { tls->pending_read = capacity; }
    ERR_clear_error(); result = SSL_read_ex(tls->ssl, tls->read_copy, tls->pending_read, length);
    if (result != 1) { return retry(tls, result, &tls->read_events); }
    (void)memcpy(output, tls->read_copy, *length);
    OPENSSL_cleanse(tls->read_copy, tls->pending_read); tls->pending_read = 0u;
    tls->read_events = POLLIN; return OPENV2G_RESULT_OK;
}
bool Ov2g_TlsHasData(const Ov2g_Tls *tls)
{
    return tls != NULL && tls->state == OV2G_TLS_READY && SSL_pending(tls->ssl) > 0;
}
OpenV2g_Result Ov2g_TlsCancel(Ov2g_Tls *tls, uint32_t generation, uint32_t connection)
{
    if (!current(tls, generation, connection)) { return OPENV2G_RESULT_REJECTED; }
    if (tls->state == OV2G_TLS_READY && tls->pending_write == 0u) { ERR_clear_error(); (void)SSL_shutdown(tls->ssl); }
    release(tls); tls->state = OV2G_TLS_CLOSED; tls->reason = OPENV2G_REASON_CANCELLED;
    return OPENV2G_RESULT_OK;
}
