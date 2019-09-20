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

#include "ccnl-producer.h"

#include "msg.h"
#include "shell.h"
#include "ccn-lite-riot.h"
#include "net/gnrc/netif.h"
#include "net/gnrc/pktdump.h"


#define NAME_BUF_LEN        (128U)

/* main thread's message queue */
#define MAIN_QUEUE_SIZE     (8)
static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];

/* some local buffers */
static char _namebuf[NAME_BUF_LEN];

static int _name_cpflat(char *buf, const struct ccnl_prefix_s *pfx)
{
    (void)buf;
    (void)pfx;
    return 0;
}

static int _on_interest(struct ccnl_relay_s *relay,
                        struct ccnl_face_s *from,
                        struct ccnl_pkt_s *pkt)
{
    (void)relay;
    (void)from;

    _name_cpflat(_namebuf, pkt->pfx);

    struct ccnl_prefix_s *p = pkt->pfx;
    printf("incoming INTEREST: ");
    for (uint32_t pos = 0; pos < p->compcnt; pos++) {
        _namebuf[0] = '/';
        memcpy(_namebuf + 1, p->comp[pos], p->complen[pos]);
        _namebuf[p->complen[pos] + 1] = '\0';
        printf("%s", _namebuf);
    }
    puts("");



    return 0;
}

int main(void)
{
    int res;
    (void)res;
    puts("NDN-BLE-Demo: Sensor Node");

    msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);
    ccnl_core_init();
    ccnl_start();

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
