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
 * @brief       NDN-BLE-Demo: firmware for relay nodes
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 *
 * @}
 */

#include <stdio.h>

#include "msg.h"
#include "shell.h"
#include "ccn-lite-riot.h"
#include "net/gnrc/netif.h"

// REMOVE
#include "net/gnrc/pktdump.h"

/* main thread's message queue */
#define MAIN_QUEUE_SIZE     (8)
static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];

int main(void)
{
    msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);

    puts("NDN-BLE-Demo: Relay Node");

    ccnl_core_init();
    ccnl_start();

    /* get the default interface */
    gnrc_netif_t *netif;

    /* set the relay's PID, configure the interface to use CCN nettype */
    if (((netif = gnrc_netif_iter(NULL)) == NULL) ||
        (ccnl_open_netif(netif->pid, GNRC_NETTYPE_CCN) < 0)) {
        puts("Error registering at network interface!");
        return -1;
    }

    gnrc_netreg_entry_t dump = GNRC_NETREG_ENTRY_INIT_PID(GNRC_NETREG_DEMUX_CTX_ALL,
                                                          gnrc_pktdump_pid);
    gnrc_netreg_register(GNRC_NETTYPE_CCN_CHUNK, &dump);

    /* run the shell */
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(NULL, line_buf, SHELL_DEFAULT_BUFSIZE);
    return 0;
}
