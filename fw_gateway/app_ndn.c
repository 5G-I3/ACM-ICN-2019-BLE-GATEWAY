
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "app.h"
#include "fmt.h"
#include "assert.h"
#include "net/gnrc/netif.h"
#include "ccn-lite-riot.h"

#define BUF_SIZE                (64U)
#define PRIO                    (THREAD_PRIORITY_MAIN -1)
#define STACKSIZE               (THREAD_STACKSIZE_DEFAULT)
#define MQSIZE                  (16U)

static char _stack[STACKSIZE];
static uint8_t _scratchpad[BUF_SIZE];
static gnrc_netreg_entry_t _reg;
static msg_t _mq[MQSIZE];

static void *_on_data(void *arg)
{
    (void)arg;
    msg_t msg;

    msg_init_queue(_mq, MQSIZE);

    while (1) {
        msg_receive(&msg);

        if (msg.type == GNRC_NETAPI_MSG_TYPE_RCV) {
            gnrc_pktsnip_t *snip = msg.content.ptr;
            assert(snip);
            if (snip->type != GNRC_NETTYPE_CCN_CHUNK) {
                puts("[NDN] received invalid snip");
            }
            else {
                printf("[NDN] received Content (size %i)\n", (int)snip->size);
                uint8_t *data = (uint8_t *)snip->data;
                for (size_t i = 0; i < snip->size; i++) {
                    printf("%i ", (int)data[i]);
                }
                puts(" END");
                if (snip->size == 2) {
                    app_hrs_update((uint16_t)data[0]);
                }
                else {
                    /* this content belongs to some other interest... */
                    app_ndn_update((const char *)snip->data, snip->size);
                }
            }
        }
        /* we ignore everything else */
        else {
            assert(0);  /* DEBUGGING... REMOVE */
        }
    }

    /* never reached */
    return NULL;
}

int app_ndn_send_interest(const char *name)
{
    struct ccnl_prefix_s *prefix;
    int res;
    char tmp[64];
    memcpy(tmp, name, strlen(name));

    memset(_scratchpad, 0, sizeof(_scratchpad));
    prefix = ccnl_URItoPrefix(tmp, CCNL_SUITE_NDNTLV, NULL);
    res = ccnl_send_interest(prefix, _scratchpad, sizeof(_scratchpad), NULL);
    ccnl_prefix_free(prefix);

    return (res >= 0) ? 0 : -1;
}

void app_ndn_init(void)
{
    ccnl_core_init();
    ccnl_start();

    /* get the BLE netif interface and register it with CCNlite */
    gnrc_netif_t *netif = gnrc_netif_iter(NULL);
    assert(netif);
    assert(netif->device_type == NETDEV_TYPE_BLE);

    int res = ccnl_open_netif(netif->pid, GNRC_NETTYPE_CCN);
    assert(res >= 0);

    /* open a thread to handle incoming NDN traffic */
    kernel_pid_t pid = thread_create(_stack, sizeof(_stack), PRIO, 0,
                                     _on_data, NULL, "ndn-data-handler");
    assert(pid > 0);

    /* register data handler to receive all NDN traffic */
    gnrc_netreg_entry_init_pid(&_reg, GNRC_NETREG_DEMUX_CTX_ALL, pid);
    res = gnrc_netreg_register(GNRC_NETTYPE_CCN_CHUNK, &_reg);
    assert(res == 0);
}
