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
 * @brief       NDN-BLE-Demo: firmware for gateway nodes
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 *
 * @}
 */

#include <stdio.h>
#include <stdint.h>

#include "fmt.h"
#include "shell.h"
#include "assert.h"
#include "event/timeout.h"
#include "nimble_riot.h"
#include "net/bluetil/ad.h"
#include "nimble_netif.h"
#include "nimble_autoconn.h"
#include "nimble_autoconn_params.h"

#include "host/ble_hs.h"
#include "host/ble_gatt.h"
#include "nimble/nimble_port.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include "app.h"

#define HRS_FLAGS_DEFAULT       (0x01)      /* 16-bit BPM value */
#define SENSOR_LOCATION         (0x02)      /* wrist sensor */
#define UPDATE_INTERVAL         (500U)      /* in ms */
#define BPM_MIN                 (80U)
#define BPM_MAX                 (210U)
#define BPM_STEP                (2)
#define BAT_LEVEL               (42U)
#define NSTATE_HRS              (0x0001)
#define NSTATE_NDN              (0x0002)

#define HRS_NAME_BUFSIZE        (32U)
#define HRS_NAME_BASE           "/demo/hrs/"
#define HRS_NAME_ID_POS         (10U)


static const ble_uuid128_t _uuid_ndn_svc = BLE_UUID128_INIT(
                                0x94, 0xc0, 0x8e, 0x7a, 0x9c, 0xa0, 0x45, 0x38,
                                0xae, 0x55, 0xf6, 0x18, 0x36, 0xeb, 0x52, 0xad);

static const ble_uuid128_t _uuid_ndn_char = BLE_UUID128_INIT(
                                0x94, 0xc0, 0x8e, 0x7b, 0x9c, 0xa0, 0x45, 0x38,
                                0xae, 0x55, 0xf6, 0x18, 0x36, 0xeb, 0x52, 0xad);

static const char *_device_name = "GATT-NDN-Gateway";

static const char *_manufacturer_name = "Super NDN Inc.";
static const char *_model_number = "NDN-BLE-HRS";
static const char *_serial_number = "a8b302c7f3-29183-x8";
static const char *_fw_ver = "13.7.12";
static const char *_hw_ver = "V3B";

static struct ble_npl_callout _hrs_update_evt;
static ble_npl_time_t _hrs_updt_itvl;

static uint16_t _conn_handle;
static uint16_t _ndn_val_handle;
static uint16_t _hrs_val_handle;
static uint16_t _noti_state;

static char _hrs_name[HRS_NAME_BUFSIZE];
static uint32_t _hrs_chunk_id = 0;

static int _ndn_handler(uint16_t conn_handle, uint16_t attr_handle,
                        struct ble_gatt_access_ctxt *ctxt, void *arg);

static int _hrs_handler(uint16_t conn_handle, uint16_t attr_handle,
                        struct ble_gatt_access_ctxt *ctxt, void *arg);

static int _devinfo_handler(uint16_t conn_handle, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt *ctxt, void *arg);

static int _bas_handler(uint16_t conn_handle, uint16_t attr_handle,
                        struct ble_gatt_access_ctxt *ctxt, void *arg);

static void _start_advertising(void);
static void _hrs_conn(uint8_t state);
static void _ndn_conn(uint8_t state);

/* GATT service definitions */
static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        /* NDN Service */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = (ble_uuid_t*) &_uuid_ndn_svc.u,
        .characteristics = (struct ble_gatt_chr_def[]) { {
            .uuid = (ble_uuid_t*) &_uuid_ndn_char.u,
            .access_cb = _ndn_handler,
            .val_handle = &_ndn_val_handle,
            .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
        }, {
            0, /* no more characteristics in this service */
        }, }
    },
    {
        /* Heart Rate Service */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(BLE_GATT_SVC_HRS),
        .characteristics = (struct ble_gatt_chr_def[]) { {
            .uuid = BLE_UUID16_DECLARE(BLE_GATT_CHAR_HEART_RATE_MEASURE),
            .access_cb = _hrs_handler,
            .val_handle = &_hrs_val_handle,
            .flags = BLE_GATT_CHR_F_NOTIFY,
        }, {
            .uuid = BLE_UUID16_DECLARE(BLE_GATT_CHAR_BODY_SENSE_LOC),
            .access_cb = _hrs_handler,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
            0, /* no more characteristics in this service */
        }, }
    },
    {
        /* Device Information Service */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(BLE_GATT_SVC_DEVINFO),
        .characteristics = (struct ble_gatt_chr_def[]) { {
            .uuid = BLE_UUID16_DECLARE(BLE_GATT_CHAR_MANUFACTURER_NAME),
            .access_cb = _devinfo_handler,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
            .uuid = BLE_UUID16_DECLARE(BLE_GATT_CHAR_MODEL_NUMBER_STR),
            .access_cb = _devinfo_handler,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
            .uuid = BLE_UUID16_DECLARE(BLE_GATT_CHAR_SERIAL_NUMBER_STR),
            .access_cb = _devinfo_handler,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
            .uuid = BLE_UUID16_DECLARE(BLE_GATT_CHAR_FW_REV_STR),
            .access_cb = _devinfo_handler,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
            .uuid = BLE_UUID16_DECLARE(BLE_GATT_CHAR_HW_REV_STR),
            .access_cb = _devinfo_handler,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
            0, /* no more characteristics in this service */
        }, }
    },
    {
        /* Battery Level Service */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(BLE_GATT_SVC_BAS),
        .characteristics = (struct ble_gatt_chr_def[]) { {
            .uuid = BLE_UUID16_DECLARE(BLE_GATT_CHAR_BATTERY_LEVEL),
            .access_cb = _bas_handler,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
            0, /* no more characteristics in this service */
        }, }
    },
    {
        0, /* no more services */
    },
};

static int _ndn_handler(uint16_t conn_handle, uint16_t attr_handle,
                        struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    printf("NDN handler triggered\n");
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        puts("WRITE CHR");
    }
    else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_DSC) {
        puts("WRITE desc");
    }

    return 0;
}

static int _hrs_handler(uint16_t conn_handle, uint16_t attr_handle,
                        struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    if (ble_uuid_u16(ctxt->chr->uuid) != BLE_GATT_CHAR_BODY_SENSE_LOC) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    puts("[READ] heart rate service: body sensor location value");

    uint8_t loc = SENSOR_LOCATION;
    int res = os_mbuf_append(ctxt->om, &loc, sizeof(loc));
    return (res == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static int _devinfo_handler(uint16_t conn_handle, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;
    const char *str;

    switch (ble_uuid_u16(ctxt->chr->uuid)) {
        case BLE_GATT_CHAR_MANUFACTURER_NAME:
            puts("[READ] device information service: manufacturer name value");
            str = _manufacturer_name;
            break;
        case BLE_GATT_CHAR_MODEL_NUMBER_STR:
            puts("[READ] device information service: model number value");
            str = _model_number;
            break;
        case BLE_GATT_CHAR_SERIAL_NUMBER_STR:
            puts("[READ] device information service: serial number value");
            str = _serial_number;
            break;
        case BLE_GATT_CHAR_FW_REV_STR:
            puts("[READ] device information service: firmware revision value");
            str = _fw_ver;
            break;
        case BLE_GATT_CHAR_HW_REV_STR:
            puts("[READ] device information service: hardware revision value");
            str = _hw_ver;
            break;
        default:
            return BLE_ATT_ERR_UNLIKELY;
    }

    int res = os_mbuf_append(ctxt->om, str, strlen(str));
    return (res == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static int _bas_handler(uint16_t conn_handle, uint16_t attr_handle,
                        struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    puts("[READ] battery level service: battery level value");

    uint8_t level = BAT_LEVEL;  /* this battery will never drain :-) */
    int res = os_mbuf_append(ctxt->om, &level, sizeof(level));
    return (res == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static int _gap_event_cb(struct ble_gap_event *event, void *arg)
{
    (void)arg;

    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            puts("main: GAP_CONNECT");
            if (event->connect.status) {
                _hrs_conn(0);
                _start_advertising();
                return 0;
            }
            nimble_autoconn_enable();
            _conn_handle = event->connect.conn_handle;
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            puts("main: GAP_DISCONNECT");
            _hrs_conn(0);
            nimble_autoconn_disable();
            _start_advertising();
            break;

        case BLE_GAP_EVENT_SUBSCRIBE:
            puts("main: GAP_SUBSCRIBE");
            if (event->subscribe.attr_handle == _hrs_val_handle) {
                _hrs_conn(event->subscribe.cur_notify);
            }
            else if (event->subscribe.attr_handle == _ndn_val_handle) {
                _ndn_conn(event->subscribe.cur_notify);
            }
            break;

        case BLE_GAP_EVENT_CONN_UPDATE:
            puts("main: GAP_CONN_UPDAETE");
            break;
    }

    return 0;
}

// static void _on_gap_passthrough(struct ble_gap_event *event)
// {
//     if (event->type == BLE_GAP_EVENT_SUBSCRIBE) {
//         if (event->subscribe.attr_handle == _hrs_val_handle) {
//             _hrs_conn(event->subscribe.cur_notify);
//         }
//         else if (event->subscribe.attr_handle == _ndn_val_handle) {
//             _ndn_conn(event->subscribe.cur_notify);
//         }
//     }
//     else {
//         printf("[GAP_PASSTHROUGH] unknown event %i\n", (int)event->type);
//     }
// }

static void _start_advertising(void)
{
    struct ble_gap_adv_params advp;
    int res;

    memset(&advp, 0, sizeof advp);
    advp.conn_mode = BLE_GAP_CONN_MODE_UND;
    advp.disc_mode = BLE_GAP_DISC_MODE_GEN;
    advp.itvl_min  = BLE_GAP_ADV_FAST_INTERVAL1_MIN;
    advp.itvl_max  = BLE_GAP_ADV_FAST_INTERVAL1_MAX;
    res = ble_gap_adv_start(nimble_riot_own_addr_type, NULL, BLE_HS_FOREVER,
                            &advp, _gap_event_cb, NULL);
    assert(res == 0);
    (void)res;
}

static void _ndn_conn(uint8_t state)
{
    if (state != 1) {
        _noti_state &= ~NSTATE_NDN;
        puts("[NOTIFY_NDN] disabled");
    }
    else {
        _noti_state |= NSTATE_NDN;
        puts("[NOTIFY_NDN] enabled");
    }
}

static void _hrs_conn(uint8_t state)
{
    if (state != 1) {
        _noti_state &= ~NSTATE_HRS;
        ble_npl_callout_stop(&_hrs_update_evt);
        puts("[NOTIFY_HRS] disabled");
    }
    else {
        _noti_state |= NSTATE_HRS;
        ble_npl_callout_reset(&_hrs_update_evt, _hrs_updt_itvl);
        puts("[NOTIFY_HRS] enabled");
    }
}

static void _hrs_update_trigger(struct ble_npl_event *ev)
{
    (void)ev;
    size_t pos = HRS_NAME_ID_POS;

    printf("[NOTIFY_HRS] sending interest\n");
    _hrs_chunk_id++;
    pos += fmt_u32_dec((_hrs_name + pos), _hrs_chunk_id);
    _hrs_name[pos] = '\0';
    app_ndn_send_interest(_hrs_name);

    /* schedule next update event */
    ble_npl_callout_reset(&_hrs_update_evt, _hrs_updt_itvl);
}

void app_hrs_update(uint16_t bpm)
{
    printf("[NOTIFY_HRS] send new datum: %i\n", (int)bpm);

    struct os_mbuf *om;
    int res;
    (void)res;

    /* make sure HRS notifications are active */
    if (!(_noti_state & NSTATE_HRS)) {
        return;
    }

    uint8_t flags = HRS_FLAGS_DEFAULT;

    /* put HRS data into a mbuf and trigger a notification */
    om = ble_hs_mbuf_from_flat(&flags, sizeof(flags));
    assert(om);
    res = os_mbuf_append(om, &bpm, sizeof(bpm));
    assert(res == 0);
    res = ble_gattc_notify_custom(_conn_handle, _hrs_val_handle, om);
    assert(res == 0);
}

int main(void)
{
    puts("Demo: NDN-BLE-Gateway");

    int res = 0;
    (void)res;

    /* setup hrs update event, we simply run it on the host's event loop */
    ble_npl_callout_init(&_hrs_update_evt, nimble_port_get_dflt_eventq(),
                         _hrs_update_trigger, NULL);
    ble_npl_time_ms_to_ticks(UPDATE_INTERVAL, &_hrs_updt_itvl);

    /* verify and add our custom services */
    res = ble_gatts_count_cfg(gatt_svr_svcs);
    assert(res == 0);
    res = ble_gatts_add_svcs(gatt_svr_svcs);
    assert(res == 0);

    /* set the device name */
    ble_svc_gap_device_name_set(_device_name);
    /* reload the GATT server to link our added services */
    ble_gatts_start();

    /* setup NDN (CCN-lite) */
    app_ndn_init();

    /* tell nimble_netif that we want to know about subcribe events */
    // nimble_netif_gappassthrough(_on_gap_passthrough);

    /* configure and set the advertising data */
    // uint8_t buf[BLE_HS_ADV_MAX_SZ];
    // bluetil_ad_t ad;
    // bluetil_ad_init_with_flags(&ad, buf, sizeof(buf), BLUETIL_AD_FLAGS_DEFAULT);
    // uint16_t uuids[2] = { BLE_GATT_SVC_HRS, BLE_GATT_SVC_NDNSS };
    // bluetil_ad_add(&ad, BLE_GAP_AD_UUID16_INCOMP, &uuids, sizeof(uuids));
    // bluetil_ad_add_name(&ad, _device_name);
    /* start to advertise this node */
    nimble_autoconn_init(&nimble_autoconn_params, NULL, 0);

    /* configure and set the advertising data */
    uint8_t buf[BLE_HS_ADV_MAX_SZ];
    bluetil_ad_t ad;
    bluetil_ad_init_with_flags(&ad, buf, sizeof(buf), BLUETIL_AD_FLAGS_DEFAULT);
    uint16_t hrs_uuid = BLE_GATT_SVC_HRS;
    bluetil_ad_add(&ad, BLE_GAP_AD_UUID16_INCOMP, &hrs_uuid, sizeof(hrs_uuid));
    bluetil_ad_add_name(&ad, _device_name);
    ble_gap_adv_set_data(ad.buf, ad.pos);
    _start_advertising();

    /* run the shell (for debugging purposes) */
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(NULL, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}
