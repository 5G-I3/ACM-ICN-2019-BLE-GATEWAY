APPLICATION = fw_relay
BOARD ?= nrf52dk
RIOTBASE ?= $(CURDIR)/../RIOT

# Basic RIOT modules needed
USEMODULE += ps
USEMODULE += shell
USEMODULE += shell_commands
USEMODULE += prng_xorshift

USEMODULE += gnrc_pktdump

# Include packages that pull up and auto-init the link layer
USEMODULE += gnrc_netdev_default
USEMODULE += auto_init_gnrc_netif

# Include NimBLE
USEMODULE += nimble_autoconn_ndnsp
USEMODULE += nimble_autoconn_autostart

# Include and configure CCN-lite
USEPKG += ccn-lite
CFLAGS += -DUSE_LINKLAYER
CFLAGS += -DUSE_RONR
CFLAGS += -DCCNL_UAPI_H_
CFLAGS += -DUSE_SUITE_NDNTLV
CFLAGS += -DNEEDS_PREFIX_MATCHING
CFLAGS += -DNEEDS_PACKET_CRAFTING

# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
DEVELHELP ?= 1

include $(RIOTBASE)/Makefile.include
