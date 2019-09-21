/*
 * Copyright (C) 2019 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     examples
 * @{
 *
 * @file
 * @brief       NDN-BLE-Demo: firmware for sensor nodes
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 *
 * @}
 */

#include <stdio.h>
#include <string.h>

#include "ccnl-producer.h"

#include "msg.h"
#include "shell.h"
#include "assert.h"
#include "random.h"
#include "ccn-lite-riot.h"
#include "net/gnrc/netif.h"
#include "net/gnrc/pktdump.h"

#ifdef BOARD_CK12
#include "board.h"
#define VIB_DURATION        (50U)
#endif

#define NAME_HRS            { "icn19", "watch", "hrs" }
#define NAME_HRS_COMPCNT    (4U)

/* main thread's message queue */
#define MAIN_QUEUE_SIZE     (8)
static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];

/* some local buffers */
static const char *_name_hrs[] = NAME_HRS;
static unsigned char _csbuf[CCNL_MAX_PACKET_SIZE];


static void _cs_insert(struct ccnl_relay_s *relay, struct ccnl_prefix_s *prefix,
                       void *payload, size_t payload_len)
{
    int res;
    (void)res;  /* in case we build without develhelp */

    /* generate a NDN-TLV item */
    size_t offs = sizeof(_csbuf);
    size_t reslen = 0;
    res = ccnl_ndntlv_prependContent(prefix, payload, payload_len,
                                     NULL, NULL, &offs, _csbuf, &reslen);
    assert(res == 0);

    /* do strange CCN-lite things to add the content into the content store */
    size_t len;
    uint64_t type;
    unsigned char *olddata = _csbuf + offs;
    unsigned char *data = olddata;
    res = ccnl_ndntlv_dehead(&data, &reslen, &type, &len);
    assert((res == 0) && (type == NDN_TLV_Data));

    struct ccnl_pkt_s *pkt = ccnl_ndntlv_bytes2pkt(type, olddata, &data, &reslen);
    assert(pkt != NULL);
    struct ccnl_content_s *c = ccnl_content_new(&pkt);
    assert(c != NULL);
    // c->flags |= CCNL_CONTENT_FLAGS_STATIC;
    if (ccnl_content_add2cache(relay, c) == NULL){
        ccnl_content_free(c);
    }
}

static void _heartbeat_into_cs(struct ccnl_relay_s *relay,
                               struct ccnl_prefix_s *prefix)
{
    uint16_t bpm = (uint16_t)random_uint32_range(80, 120);
    _cs_insert(relay, prefix, &bpm, 2);
}

static int _on_interest(struct ccnl_relay_s *relay,
                        struct ccnl_face_s *from,
                        struct ccnl_pkt_s *pkt)
{
    (void)relay;
    (void)from;
    struct ccnl_prefix_s *p = pkt->pfx;

    if (p->compcnt == NAME_HRS_COMPCNT &&
        memcmp(p->comp[0], _name_hrs[0], p->complen[0]) == 0 &&
        memcmp(p->comp[1], _name_hrs[1], p->complen[1]) == 0 &&
        memcmp(p->comp[2], _name_hrs[2], p->complen[2]) == 0) {
        puts("[sensor] got interest for /icn19/watch/hrs/x");
#ifdef BOARD_CK12
        board_vibrate(VIB_DURATION);
#endif
        _heartbeat_into_cs(relay, p);
    }

    return 0;
}

static void _insert_static_content(char *name, const char *data)
{
    struct ccnl_prefix_s *prefix = ccnl_URItoPrefix(name,
                                                    CCNL_SUITE_NDNTLV, NULL);
    _cs_insert(&ccnl_relay, prefix, (char *)data, strlen(data));
    ccnl_prefix_free(prefix);
}

int main(void)
{
    int res;
    (void)res;
    puts("NDN-BLE-Demo: Sensor Node");

    msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);
    ccnl_core_init();
    ccnl_start();
    char hello[32] = "/hello";
    _insert_static_content(hello, "World!");
    char foo[32] = "/foo";
    _insert_static_content(foo, "Bar!");

    /* initialize the first network interface for CCN-lite */
    gnrc_netif_t *netif = gnrc_netif_iter(NULL);
    assert(netif);
    res = ccnl_open_netif(netif->pid, GNRC_NETTYPE_CCN);
    assert(res >= 0);

    /* we produce new heart-rate data on-the-fly */
    ccnl_set_local_producer(_on_interest);

    /* run the shell (for debugging purposes) */
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(NULL, line_buf, SHELL_DEFAULT_BUFSIZE);
    return 0;
}
