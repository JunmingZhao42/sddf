#include <stdbool.h>
#include <stdint.h>
#include <microkit.h>
#include <sddf/sound/queue.h>

#define DRIVER_CH 3
#define CLIENT_CH_BEGIN 1
#define NUM_CLIENTS 2

#define NO_OWNER -1
#define MAX_STREAMS 16

uintptr_t drv_cmd_req;
uintptr_t drv_cmd_res;
uintptr_t drv_pcm_req;
uintptr_t drv_pcm_res;

uintptr_t c0_cmd_req;
uintptr_t c0_cmd_res;
uintptr_t c0_pcm_req;
uintptr_t c0_pcm_res;

uintptr_t c1_cmd_req;
uintptr_t c1_cmd_res;
uintptr_t c1_pcm_req;
uintptr_t c1_pcm_res;

uintptr_t sound_shared_state;

static sound_queues_t clients[NUM_CLIENTS];
static sound_queues_t driver_rings;

static int owners[MAX_STREAMS];
static bool started;

static void respond_to_cmd(sound_queues_t *client_rings,
                           sound_cmd_t *cmd,
                           sound_status_t status)
{
    cmd->status = status;
    if (sound_enqueue_cmd(client_rings->cmd_res, cmd) != 0) {
        microkit_dbg_puts("SND VIRT|ERR: failed to respond to command\n");
    }
}

static void respond_to_pcm(sound_queues_t *client_rings,
                           sound_pcm_t *pcm,
                           sound_status_t status)
{
    pcm->status = status;
    pcm->latency_bytes = 0;
    if (sound_enqueue_pcm(client_rings->pcm_res, pcm) != 0) {
        microkit_dbg_puts("SND VIRT|ERR: failed to respond to pcm\n");
    }
}

static int notified_by_client(int client)
{
    if (client < 0 || client > NUM_CLIENTS) {
        microkit_dbg_puts("SND VIRT|ERR: invalid client id\n");
        return -1;
    }

    bool notify_client = false;
    bool notify_driver = false;
    sound_queues_t *client_rings = &clients[client];
    sound_cmd_t cmd;

    while (sound_dequeue_cmd(client_rings->cmd_req, &cmd) == 0) {

        if (cmd.stream_id > MAX_STREAMS) {
            microkit_dbg_puts("SND VIRT|ERR: stream id too large\n");
            respond_to_cmd(client_rings, &cmd, SOUND_S_BAD_MSG);
            continue;
        }

        if (owners[cmd.stream_id] == -1) {
            if (cmd.code == SOUND_CMD_TAKE) {
                owners[cmd.stream_id] = client;
            } else {
                microkit_dbg_puts("SND VIRT|ERR: client must take first\n");
                respond_to_cmd(client_rings, &cmd, SOUND_S_BAD_MSG);
                notify_client = true;
                continue;
            }
        }

        int owner = owners[cmd.stream_id];
        if (owner != client) {
            microkit_dbg_puts("SND VIRT|ERR: stream busy\n");
            respond_to_cmd(client_rings, &cmd, SOUND_S_BUSY);
            notify_client = true;
            continue;
        }

        if (sound_enqueue_cmd(driver_rings.cmd_req, &cmd) != 0) {
            microkit_dbg_puts("SND VIRT|ERR: Failed to enqueue command\n");
            return -1;
        }
        notify_driver = true;
    }

    sound_pcm_t pcm;
    while (sound_dequeue_pcm(client_rings->pcm_req, &pcm) == 0) {

        if (pcm.stream_id > MAX_STREAMS) {
            microkit_dbg_puts("SND VIRT|ERR: stream id too large\n");
            respond_to_pcm(client_rings, &pcm, SOUND_S_BAD_MSG);
            notify_client = true;
            continue;
        }

        int owner = owners[pcm.stream_id];
        if (owner != client) {
            microkit_dbg_puts("SND VIRT|ERR: driver replied to busy stream\n");
            respond_to_pcm(client_rings, &pcm, SOUND_S_BAD_MSG);
            notify_client = true;
            continue;
        }

        // Write PCM data to RAM
        microkit_arm_vspace_data_clean(pcm.addr, pcm.addr + pcm.len);

        if (sound_enqueue_pcm(driver_rings.pcm_req, &pcm) != 0) {
            microkit_dbg_puts("SND VIRT|ERR: Failed to enqueue PCM data\n");
            return -1;
        }
        notify_driver = true;
    }

    if (notify_client) {
        microkit_notify(CLIENT_CH_BEGIN + client);
    }
    if (notify_driver) {
        microkit_notify(DRIVER_CH);
    }

    return 0;
}

int notified_by_driver(void)
{
    bool notify[NUM_CLIENTS] = {0};

    sound_cmd_t cmd;
    while (sound_dequeue_cmd(driver_rings.cmd_res, &cmd) == 0) {

        if (cmd.stream_id > MAX_STREAMS) {
            microkit_dbg_puts("SND VIRT|ERR: stream id too large\n");
            continue;
        }

        int owner = owners[cmd.stream_id];
        if (owner < 0 || owner > NUM_CLIENTS) {
            microkit_dbg_puts("SND VIRT|ERR: invalid owner id\n");
            continue;
        }

        if (cmd.code == SOUND_CMD_RELEASE ||
            (cmd.code == SOUND_CMD_TAKE && cmd.status != SOUND_S_OK)) {
            owners[cmd.stream_id] = -1;
        }

        if (sound_enqueue_cmd(clients[owner].cmd_res, &cmd) != 0) {
            microkit_dbg_puts("SND VIRT|ERR: Failed to enqueue command response\n");
            return -1;
        }
        notify[owner] = true;
    }

    sound_pcm_t pcm;
    while (sound_dequeue_pcm(driver_rings.pcm_res, &pcm) == 0) {

        if (pcm.stream_id > MAX_STREAMS) {
            microkit_dbg_puts("SND VIRT|ERR: stream id too large\n");
            continue;
        }

        int owner = owners[pcm.stream_id];
        if (owner < 0 || owner > NUM_CLIENTS) {
            microkit_dbg_puts("SND VIRT|ERR: invalid owner id\n");
            continue;
        }

        // Cache is dirty as device may have written to buffer
        microkit_arm_vspace_data_invalidate(pcm.addr, pcm.addr + pcm.len);

        if (sound_enqueue_pcm(clients[owner].pcm_res, &pcm) != 0) {
            microkit_dbg_puts("SND VIRT|ERR: Failed to enqueue PCM data\n");
            return -1;
        }
        notify[owner] = true;
    }

    for (int client = 0; client < NUM_CLIENTS; client++) {
        if (notify[client]) {
            microkit_notify(CLIENT_CH_BEGIN + client);
        }
    }

    if (!started) {
        for (int client = 0; client < NUM_CLIENTS; client++) {
            microkit_notify(CLIENT_CH_BEGIN + client);
        }
        started = true;
    }

    return 0;
}

void init(void)
{
    clients[0].cmd_req = (void *)c0_cmd_req;
    clients[0].cmd_res = (void *)c0_cmd_res;
    clients[0].pcm_req = (void *)c0_pcm_req;
    clients[0].pcm_res = (void *)c0_pcm_res;

    clients[1].cmd_req = (void *)c1_cmd_req;
    clients[1].cmd_res = (void *)c1_cmd_res;
    clients[1].pcm_req = (void *)c1_pcm_req;
    clients[1].pcm_res = (void *)c1_pcm_res;

    driver_rings.cmd_req = (void *)drv_cmd_req;
    driver_rings.cmd_res = (void *)drv_cmd_res;
    driver_rings.pcm_req = (void *)drv_pcm_req;
    driver_rings.pcm_res = (void *)drv_pcm_res;
    sound_queues_init_default(&driver_rings);

    for (int i = 0; i < MAX_STREAMS; i++) {
        owners[i] = NO_OWNER;
    }
    started = false;
}

void notified(microkit_channel ch)
{
    if (ch == DRIVER_CH) {
        if (notified_by_driver() != 0) {
            microkit_dbg_puts("SND VIRT|ERR: Failed to handle driver notification\n");
        }
    } else if (ch >= CLIENT_CH_BEGIN && ch < CLIENT_CH_BEGIN + NUM_CLIENTS) {
        if (notified_by_client(ch - CLIENT_CH_BEGIN) != 0) {
            microkit_dbg_puts("SND VIRT|ERR: Failed to handle client notification\n");
        }
    } else {
        microkit_dbg_puts("SND VIRT|ERR: Received a bad client channel\n");
    }
}
