#include "openv2g/linux/bsw.h"
#include <stdio.h>
#include <string.h>

static bool check_result(OpenV2g_Result result, int line)
{
    if ((result != OPENV2G_RESULT_OK) && (result != OPENV2G_RESULT_ACCEPTED)) {
        (void)fprintf(stderr, "Linux stub operation failed at line %d: %d\n", line, (int)result);
        return false;
    }
    return true;
}
#define ok(result) check_result((result), __LINE__)

bool Ov2g_EnvironmentInit(Ov2g_LinuxEnvironment *environment)
{
    if (environment == NULL) { return false; }
    (void)memset(environment, 0, sizeof(*environment));
    Ov2g_ServicesInit(&environment->services);
    Ov2g_OsInit(&environment->os, 1u);
    return OpenV2g_HostSimulation_Init(&environment->rte, NULL) == OPENV2G_RESULT_OK;
}
static void deliver(void *context, uint32_t generation);
typedef struct {
    Ov2g_LinuxEnvironment *environment;
    OpenV2g_ProtocolMessage response;
    bool success;
    OpenV2g_TimeUs dispatch_time; /* Owner-selected synthetic OS dispatch clock. */
} Delivery;
static void deliver(void *context, uint32_t generation)
{
    Delivery *delivery = context;
    OpenV2g_HlcSessionStatus status;
    OpenV2g_HostSimulation *sim = &delivery->environment->rte;
    if (generation != sim->coordinator.generation) { return; }
    delivery->success = ok(OpenV2g_Iso15118Chrg_OnProtocolMessage(
        &sim->hlc, delivery->dispatch_time, &delivery->response));
    if (delivery->success) {
        delivery->success = ok(sim->hlc_status_port.read(sim->hlc_status_port.context, &status)) &&
            ok(OpenV2g_ChargingCoordinator_OnHlcStatus(&sim->coordinator, &status, delivery->dispatch_time));
    }
}
bool Ov2g_EnvironmentRun(Ov2g_LinuxEnvironment *environment, int station_fd)
{
    OpenV2g_HostSimulation *sim;
    OpenV2g_TimeUs now;
    OpenV2g_QualifiedCpPp cp;
    OpenV2g_EvEnergySnapshot energy;
    OpenV2g_ProtocolTxRequest request;
    Ov2g_StubMessage message;
    uint8_t frame[OV2G_STUB_FRAME_SIZE];
    OpenV2g_TcpTxCsPort tcp_port;
    OpenV2g_TransportTxRequest tx;
    OpenV2g_TransportTxResult completion;
    Delivery delivery;
    Ov2g_OsEvent event;
    bool success = false;
    if ((environment == NULL) || environment->running) { return false; }
    environment->running = true;
    sim = &environment->rte;
    tcp_port = Ov2g_TcpTxPort(&station_fd);
    if (!ok(OpenV2g_TcpTlsSession_BindTcpTxPort(&sim->hlc.tcp_tls_session, &tcp_port))) { goto stop; }
    now = Ov2g_OsNow();
    cp = OpenV2g_HostSimulation_DefaultCpPp(sim, now);
    if (!ok(OpenV2g_HostSimulation_InjectCpPp(sim, &cp, now)) ||
        !ok(OpenV2g_HostSimulation_AdvancePlcToReady(sim, now + 10u)) ||
        !ok(OpenV2g_HostSimulation_EstablishHlc(sim, now + 20u)) ||
        !ok(OpenV2g_Iso15118Chrg_TakeProtocolRequest(
            &sim->hlc, sim->coordinator.generation, &request))) { goto stop; }
    Ov2g_OsCancel(&environment->os, request.generation);
    message.generation = request.generation;
    message.correlation = request.correlation_id;
    message.kind = (uint32_t)request.kind;
    message.accepted = 0u;
    Ov2g_Encode(&message, frame);
    (void)memset(&tx, 0, sizeof(tx));
    tx.generation = request.generation;
    tx.connection_id = sim->hlc.tcp_tls_session.status.connection_id;
    tx.request_id = request.correlation_id;
    tx.timestamp = now + 30u;
    tx.deadline = now + 500000u;
    tx.data = frame; tx.length = sizeof(frame);
    if (!ok(OpenV2g_TcpTlsSession_Send(&sim->hlc.tcp_tls_session, &tx))) { goto stop; }
    (void)memset(&completion, 0, sizeof(completion));
    completion.generation = tx.generation; completion.connection_id = tx.connection_id;
    completion.request_id = tx.request_id; completion.timestamp = Ov2g_OsNow();
    completion.result = OPENV2G_RESULT_OK; completion.transferred_length = sizeof(frame);
    if (!ok(OpenV2g_TcpTlsSession_OnTxResult(&sim->hlc.tcp_tls_session, &completion)) ||
        !Ov2g_TcpExchange(station_fd, frame, sizeof(frame), false, now + 500000u) ||
        !Ov2g_Decode(frame, &message) ||
        (message.generation != request.generation) ||
        (message.correlation != request.correlation_id) ||
        (message.kind != (uint32_t)request.kind)) { goto stop; }
    (void)memset(&delivery, 0, sizeof(delivery));
    delivery.environment = environment;
    delivery.response.generation = message.generation;
    delivery.response.correlation_id = message.correlation;
    delivery.response.kind = (OpenV2g_ProtocolMessageKind)message.kind;
    delivery.response.accepted = message.accepted != 0u;
    now = Ov2g_OsNow() + 100u;
    delivery.response.now = now; delivery.dispatch_time = now;
    event.run = deliver; event.context = &delivery;
    event.generation = message.generation; event.due = now;
    if (!Ov2g_OsPost(&environment->os, &event) ||
        (Ov2g_OsDispatch(&environment->os, now) != 1u) || !delivery.success) { goto stop; }
    /* Refresh required measured/provider evidence after socket latency. */
    cp = OpenV2g_HostSimulation_DefaultCpPp(sim, now);
    energy = OpenV2g_HostSimulation_DefaultEnergy(sim, now);
    if (!ok(OpenV2g_HostSimulation_InjectCpPp(sim, &cp, now)) ||
        !ok(OpenV2g_HostSimulation_InjectEnergy(sim, &energy, now)) ||
        !ok(OpenV2g_HostSimulation_InitializeHardwareSafe(sim, now + 1u)) ||
        !ok(OpenV2g_HostSimulation_PrepareEnergyTransfer(sim, now + 10u)) ||
        !ok(OpenV2g_HostSimulation_StartEnergyTransfer(sim, now + 20u))) { goto stop; }
    success = sim->coordinator.state_machine.state == OPENV2G_COORDINATOR_ENERGY_TRANSFER;
    (void)printf("Linux stub: socket SessionSetup complete; host transfer=%s\n", success ? "yes" : "no");
stop:
    now = Ov2g_OsNow() + 100u;
    if (now < sim->coordinator.owner_time) { now = sim->coordinator.owner_time; } /* This smoke fixture advances a synthetic owner clock. */
    if (!ok(OpenV2g_HostSimulation_Stop(sim, now, OPENV2G_REASON_CANCELLED))) { success = false; }
    Ov2g_OsCancel(&environment->os, 0u);
    sim->hlc.tcp_tls_session.tcp_tx_required_port.context = NULL;
    sim->hlc.tcp_tls_session.tcp_tx_required_port.send = NULL;
    environment->running = false;
    if (!success) {
        OpenV2g_DiagnosticPort dem = Ov2g_DemPort(&environment->services);
        dem.report_event(dem.context, 1u, OPENV2G_REASON_PRECONDITION);
    }
    return success;
}
