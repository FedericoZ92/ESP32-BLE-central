
// SPDX-FileCopyrightText: 2017-2024 Espressif Systems (Shanghai) CO LTD
// SPDX-License-Identifier: Apache-2.0

#include "esp_log.h"
#include "driver/gpio.h"
//led
#include "led.h"
//nimle
#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "console/console.h"
#include "services/gap/ble_svc_gap.h"
#include "ble_cts_cent.h"
#include "services/cts/ble_svc_cts.h"

static const char *tag = "NimBLE_CTS_CENT";
//nimble
#define TARGET_DEVICE_NAME "nimble-bleprph" //"111181111-faina"
#define MAX_PING_LENGTH 16

static int ble_cts_cent_gap_event(struct ble_gap_event *event, void *arg);
static uint8_t peer_addr[6];

typedef enum eDeviceType
{
    VACUUM_DEV_TYPE = 1,
    TEMPERATURE_DEV_TYPE = 2,
    INVALID_DEV_TYPE = 0xFF
}eDeviceType;

typedef enum eConnStatusType
{
    UNBONDED_STATUS = 0,
    BONDED_STATUS = 1,
    CONNECTED_STATUS = 2,
    CONN_INVALID_STATUS = 0xFF
}eConnStatusType;

typedef struct sDiscDevice
{
    eConnStatusType isConnect;
    eDeviceType devType;
    ble_addr_t addr;
    uint8_t nameASCII[20]; //coincide con il nome del device
    uint16_t conn_handle;
    uint32_t lastAdv;
    bool peer_ready;
    uint8_t pingData[MAX_PING_LENGTH];
    // uint8_t cfgData[MAX_CFG_LENGTH];
}sDiscDevice;
static sDiscDevice gDevice;

void ble_store_config_init(void);
static void ble_cts_cent_scan(void);

static struct ble_gap_conn_params connecting_params = { //FEDE: se parametri custom, inserire tutti i campi
    .scan_itvl = 10,
    .scan_window = 10,
    .itvl_min = 24, // 30 ms
    .itvl_max = 40, // 50 ms
    .latency = 0,
    .supervision_timeout = 512, 
    .min_ce_len = 0,
    .max_ce_len = 0,
};

// Initiates the GAP general discovery procedure.
static void ble_cts_cent_scan(void)
{
    uint8_t own_addr_type;
    struct ble_gap_disc_params disc_params;
    int rc;

    // Figure out address to use while advertising (no privacy for now) 
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(tag, "error determining address type; rc=%d", rc);
        return;
    }

    // Tell the controller to filter duplicates; we don't want to process repeated advertisements from the same device.    
    disc_params.filter_duplicates = 1;
    // Perform a passive scan.  I.e., don't send follow-up scan requests to each advertiser.
    disc_params.passive = 1;

    // Use defaults for the rest of the parameters. 
    disc_params.itvl = 0;
    disc_params.window = 0;
    disc_params.filter_policy = 0;
    disc_params.limited = 0;

    rc = ble_gap_disc(own_addr_type, BLE_HS_FOREVER, &disc_params,
                      ble_cts_cent_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(tag, "Error initiating GAP discovery procedure; rc=%d", rc);
    }
}

// Add device name filtering for legacy advertising
static int ble_cts_cent_should_connect(const struct ble_gap_disc_desc *disc)
{
    struct ble_hs_adv_fields fields;
    int rc;
    int i;

    // The device has to be advertising connectability.
    if (disc->event_type != BLE_HCI_ADV_RPT_EVTYPE_ADV_IND &&
            disc->event_type != BLE_HCI_ADV_RPT_EVTYPE_DIR_IND) {
        return 0;
    }

    ESP_LOGD(tag, "Parsing advertisement fields (addr_type=%d, addr=%02x:%02x:%02x:%02x:%02x:%02x, len=%d)",
        disc->addr.type,
        disc->addr.val[0], disc->addr.val[1], disc->addr.val[2],
        disc->addr.val[3], disc->addr.val[4], disc->addr.val[5],
        disc->length_data);
    rc = ble_hs_adv_parse_fields(&fields, disc->data, disc->length_data);
    if (rc != 0) {
        ESP_LOGW(tag, "Failed to parse advertisement fields (rc=%d)", rc);
        return 0;
    }

    // Device name filtering
    if (fields.name != NULL && fields.name_len > 0) {
        char adv_name[32] = {0};
        int copy_len = fields.name_len < (int)sizeof(adv_name)-1 ? fields.name_len : (int)sizeof(adv_name)-1;
        memcpy(adv_name, fields.name, copy_len);
        adv_name[copy_len] = '\0';
        ESP_LOGI(tag, "Checking device name: '%s' (target: '%s')", adv_name, TARGET_DEVICE_NAME);
        if (fields.name_len == strlen(TARGET_DEVICE_NAME) &&
            strncmp((const char *)fields.name, TARGET_DEVICE_NAME, fields.name_len) == 0) {
            ESP_LOGI(tag, "Device name matches target. Will attempt to connect.");
            // Name matches, continue
        } else {
            ESP_LOGI(tag, "Device name does not match target. Skipping.");
            return 0;
        }
    } else {
        ESP_LOGI(tag, "No name in advertisement, skipping device");
        return 0;
    }

    if (strlen(CONFIG_EXAMPLE_PEER_ADDR) && (strncmp(CONFIG_EXAMPLE_PEER_ADDR, "ADDR_ANY", strlen("ADDR_ANY")) != 0)) {
        ESP_LOGD(tag, "Peer address from menuconfig: %s", CONFIG_EXAMPLE_PEER_ADDR);
        // Convert string to address
        sscanf(CONFIG_EXAMPLE_PEER_ADDR, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
            &peer_addr[5], &peer_addr[4], &peer_addr[3],
            &peer_addr[2], &peer_addr[1], &peer_addr[0]);
        if (memcmp(peer_addr, disc->addr.val, sizeof(disc->addr.val)) != 0) {
            ESP_LOGD(tag, "Advertisement address does not match peer address filter. Skipping.");
            return 0;
        }
    }

    // Relaxed: connect to any device with the correct name, regardless of advertised services.
    ESP_LOGI(tag, "Service UUID check skipped: connecting to device with matching name.");
    return 1;
}

// Connects to the sender of the specified advertisement if it looks interesting.  
// A device is "interesting" if it advertises connectability and support for the Current Time service.
static void ble_cts_cent_connect_if_interesting(void *disc)
{
    uint8_t own_addr_type;
    int rc;
    ble_addr_t *addr;
    // Don't do anything if we don't care about this advertiser. 
    #if CONFIG_EXAMPLE_EXTENDED_ADV
        if (!ext_ble_cts_cent_should_connect((struct ble_gap_ext_disc_desc *)disc)) {
            ESP_LOGD(tag, "Advertisement did not pass filter (extended adv)");
            return;
        }
    #else
        if (!ble_cts_cent_should_connect((struct ble_gap_disc_desc *)disc)) {
            ESP_LOGD(tag, "Advertisement did not pass filter (legacy adv)");
            return;
        }
    #endif
    #if !(MYNEWT_VAL(BLE_HOST_ALLOW_CONNECT_WITH_SCAN))
        // If BLE_HOST_ALLOW_CONNECT_WITH_SCAN is enabled (set to 1/true), your application can call ble_gap_connect without first stopping an ongoing scan (ble_gap_disc).
        // If it is disabled (set to 0/false), you must stop scanning (using ble_gap_disc_cancel) before calling ble_gap_connect, or the connection attempt will fail.
        // This option is useful for applications that want to connect to devices as soon as they are discovered, without interrupting the scan process.
        rc = ble_gap_disc_cancel();
        if (rc != 0) {
            ESP_LOGD(tag, "Failed to cancel scan; rc=%d", rc);
            return;
        }
    #endif
    // Figure out address to use for connect (no privacy for now) 
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(tag, "error determining address type; rc=%d", rc);
        return;
    }
    // Try to connect the the advertiser.  Allow 30 seconds (30000 ms) for timeout.
    #if CONFIG_EXAMPLE_EXTENDED_ADV
        addr = &((struct ble_gap_ext_disc_desc *)disc)->addr;
    #else
        addr = &((struct ble_gap_disc_desc *)disc)->addr;
    #endif

    memset(&gDevice, 0, sizeof(gDevice));
    gDevice.addr = *addr; // addr is the BLE address you are connecting to
    gDevice.isConnect = UNBONDED_STATUS;
    gDevice.conn_handle = 0;
    gDevice.peer_ready = false;
    ESP_LOGI(tag, "Attempting to connect to device (addr_type=%d, addr=%02x:%02x:%02x:%02x:%02x:%02x)",
                    addr->type, addr->val[0], addr->val[1], addr->val[2], addr->val[3], addr->val[4], addr->val[5]);
    //rc = ble_gap_connect(own_addr_type, addr, 30000, NULL, ble_cts_cent_gap_event, &gDevice);
    rc = ble_gap_connect(own_addr_type, addr, 30000, &connecting_params, ble_cts_cent_gap_event, &gDevice);
    if (rc != 0) {
        ESP_LOGE(tag, "Error: Failed to connect to device; addr_type=%d addr=%02x:%02x:%02x:%02x:%02x:%02x; rc=%d",
                        addr->type, addr->val[0], addr->val[1], addr->val[2], addr->val[3], addr->val[4], addr->val[5], rc);
        return;
    }
    ESP_LOGI(tag, "Connection initiated. Waiting for link establishment event.");
}

static void ble_cent_on_disc_complete(const struct peer *peer, int status, void *arg)
{
    if(!arg){
        ESP_LOGE(tag, "Error: Argument is NULL in discovery complete callback; status=%d conn_handle=%d\n", status, peer->conn_handle);
        // Terminate the connection. 
        ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        return;
    }
    sDiscDevice* pDev = arg;
    ESP_LOGI(tag, "Service discovery complete; status=%d conn_handle=%d\n", status, peer->conn_handle);
    if (status != 0) {
        // Service discovery failed.  Terminate the connection. 
        ESP_LOGE(tag, "Error: Service discovery failed; status=%d conn_handle=%d\n", status, peer->conn_handle);
        ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        pDev->isConnect = BONDED_STATUS;
        pDev->conn_handle = 0;
        pDev->peer_ready = false;
        return;
    }
    //TODO enable when it connects
    /*struct ble_gap_upd_params upd_params = {
        // intervallo connessione 1 secondo
        .itvl_min = 500, // 1.25ms * 500 = 625ms
        .itvl_max = 500,
        .latency = 0,
        .supervision_timeout = 1000,
        .min_ce_len = 0,
        .max_ce_len = 0,
    };

    int rc = ble_gap_update_params(peer->conn_handle, &upd_params);
    ESP_LOGI(tag, "Update param request; rc=%d conn_handle=%d itvl_min=%d itvl_max=%d timeout=%d\n", 
                rc, peer->conn_handle, upd_params.itvl_min, upd_params.itvl_max, upd_params.supervision_timeout);
    if (rc != 0) {
        // Update param failed.  Terminate the connection. 
        ESP_LOGE(tag, "Error: Update param failed; rc=%d conn_handle=%d\n", rc, peer->conn_handle);
        ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        pDev->isConnect = BONDED_STATUS;
        pDev->conn_handle = 0;
        pDev->peer_ready = false;
        return;
    }*/

    pDev->isConnect = CONNECTED_STATUS;
    pDev->conn_handle = peer->conn_handle;
    pDev->peer_ready = true;
}

// The nimble host executes this callback when a GAP event occurs.  The
// application associates a GAP event callback with each connection that is
// established.  ble_cts_cent uses the same callback for all connections.
// @param event  The event being signalled.
// @param arg    Application-specified argument; unused by ble_cts_cent.
// @return       0 if the application successfully handled the event; nonzero on failure.  
// The semantics of the return code is specific to the particular GAP event being signalled.
static int ble_cts_cent_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    struct ble_hs_adv_fields fields;
    int rc;
    ESP_LOGW(tag, "GAP event: %d", event->type);
    switch (event->type) {
    /*case BLE_GAP_EVENT_CONNECT:
        // A new connection was established or a connection attempt failed. 
        if (event->connect.status == 0) {
            // Connection successfully established. 
            ESP_LOGI(tag, "Connection established ");
            rc = ble_gap_security_initiate(event->connect.conn_handle);

            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            assert(rc == 0);

            ble_att_set_preferred_mtu(512);
            ble_gattc_exchange_mtu(event->connect.conn_handle, NULL, NULL);

            // Remember peer. 
            rc = peer_add(event->connect.conn_handle);
            if ((rc != 0) && (rc != BLE_HS_EALREADY)){
                ESP_LOGE(tag, "Failed to add peer; rc=%d", rc);
                return 0;
            }

            // Perform service discovery 
            rc = peer_disc_all(event->connect.conn_handle, ble_cent_on_disc_complete, arg);
            if(rc != 0) {
                ESP_LOGE(tag, "Failed to discover services; rc=%d", rc);
                return 0;
            }
        } else {
            // Connection attempt failed; resume scanning.
            ESP_LOGE(tag, "Error: Connection failed; status=%d", event->connect.status);
            ble_cts_cent_scan();
        }
        return 0;
        */
    case BLE_GAP_EVENT_CONN_UPDATE:
        // Connection parameters updated (after ble_gap_update_params)
        rc = ble_gap_conn_find(event->conn_update.conn_handle, &desc);
        if (rc == 0) {
            ESP_LOGI(tag, "Connection interval updated: %.2f ms", desc.conn_itvl * 1.25);
        } else {
            ESP_LOGW(tag, "Failed to get connection descriptor after update");
        }
        return 0;
        
    case BLE_GAP_EVENT_DISC:
        rc = ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data);
        if (rc != 0) {
            return 0;
        }
        // An advertisement report was received during GAP discovery. 
        print_adv_fields(&fields);
        // Try to connect to the advertiser if it looks interesting. 
        ble_cts_cent_connect_if_interesting(&event->disc);
        return 0;

    case BLE_GAP_EVENT_LINK_ESTAB:
        // A new connection was established or a connection attempt failed. 
        if (event->connect.status == 0) {
            // Connection successfully established. 
            ESP_LOGI(tag, "Connection established ");
            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            assert(rc == 0);
            print_conn_desc(&desc);
            led_blue(); // Turn LED on
            // Remember peer. 
            rc = peer_add(event->connect.conn_handle);
            if (rc != 0) {
                ESP_LOGE(tag, "Failed to add peer; rc=%d", rc);
                return 0;
            }

            // Set connection interval to 625 ms (500 units)
            /*struct ble_gap_upd_params params = {0};
            params.itvl_min = 500; // 1.25ms * 500 = 625ms
            params.itvl_max = 500; // 1.25ms * 500 = 625ms
            params.latency = desc.conn_latency;
            params.supervision_timeout = desc.supervision_timeout;
            rc = ble_gap_update_params(event->connect.conn_handle, &params);
            if (rc != 0) {
                ESP_LOGW(tag, "Failed to update connection interval; rc=%d", rc);
            } else {
                ESP_LOGI(tag, "Requested connection interval: 1 second");
            }*/

            ESP_LOGI(tag, "Connection interval established: %.2f ms", desc.conn_itvl * 1.25);

            #if CONFIG_EXAMPLE_ENCRYPTION
                // Initiate security - It will perform
                // Pairing (Exchange keys)
                // Bonding (Store keys)
                // Encryption (Enable encryption)
                // Will invoke event BLE_GAP_EVENT_ENC_CHANGE
                rc = ble_gap_security_initiate(event->connect.conn_handle);
                if (rc != 0) {
                    ESP_LOGI(tag, "Security could not be initiated, rc = %d", rc);
                    return ble_gap_terminate(event->connect.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
                } else {
                    ESP_LOGI(tag, "Connection secured");
                }
            #else
                // Perform service discovery 
                rc = peer_disc_all(event->connect.conn_handle, ble_cent_on_disc_complete, arg);
                if (rc != 0) {
                    ESP_LOGE(tag, "Failed to discover services; rc=%d", rc);
                    return 0;
                }
            #endif
        } else {
            // Connection attempt failed; resume scanning. 
            ESP_LOGE(tag, "Error: Connection failed; status=%d", event->connect.status);
            ble_cts_cent_scan();
        }
        return 0;


    case BLE_GAP_EVENT_DISCONNECT:
        // Connection terminated. 
        ESP_LOGI(tag, "disconnect; reason=%d ", event->disconnect.reason);
        print_conn_desc(&event->disconnect.conn);
        ESP_LOGI(tag, "");
        led_red(); // Turn LED off
        peer_delete(event->disconnect.conn.conn_handle); // Forget about peer. 
        ble_cts_cent_scan(); // Resume scanning. 
        return 0;

    case BLE_GAP_EVENT_DISC_COMPLETE:
        ESP_LOGI(tag, "discovery complete; reason=%d", event->disc_complete.reason);
        return 0;

    case BLE_GAP_EVENT_ENC_CHANGE:
        // Encryption has been enabled or disabled for this connection. 
        ESP_LOGI(tag, "encryption change event; status=%d ", event->enc_change.status);
        rc = ble_gap_conn_find(event->enc_change.conn_handle, &desc);
        assert(rc == 0);
        print_conn_desc(&desc);
        #if CONFIG_EXAMPLE_ENCRYPTION
            // Go for service discovery after encryption has been successfully enabled 
            rc = peer_disc_all(event->connect.conn_handle, ble_cent_on_disc_complete, arg);
            if (rc != 0) {
                ESP_LOGE(tag, "Failed to discover services; rc=%d", rc);
                return 0;
            }
        #endif
        return 0;

    case BLE_GAP_EVENT_NOTIFY_RX:
        // Peer sent us a notification or indication. 
        ESP_LOGI(tag, "received %s; conn_handle=%d attr_handle=%d attr_len=%d", event->notify_rx.indication ? "indication" : "notification", event->notify_rx.conn_handle, event->notify_rx.attr_handle, OS_MBUF_PKTLEN(event->notify_rx.om));

        // Attribute data is contained in event->notify_rx.om. Use
        // `os_mbuf_copydata` to copy the data received in notification mbuf 
        return 0;

    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(tag, "mtu update event; conn_handle=%d cid=%d mtu=%d", event->mtu.conn_handle, event->mtu.channel_id, event->mtu.value);
        return 0;

    case BLE_GAP_EVENT_DATA_LEN_CHG:
        return 0;

    #if CONFIG_EXAMPLE_EXTENDED_ADV
        case BLE_GAP_EVENT_EXT_DISC:
            // An advertisement report was received during GAP discovery. 
            ext_print_adv_report(&event->disc);
            ble_cts_cent_connect_if_interesting(&event->disc);
            return 0;
    #endif

    default:
        return 0;
    }
}

static void ble_cts_cent_on_reset(int reason)
{
    ESP_LOGE(tag, "Resetting state; reason=%d", reason);
}

static void ble_cts_cent_on_sync(void)
{
    int rc;

    // Make sure we have proper identity address set (public preferred) 
    rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);

    // Begin scanning for a peripheral to connect to. 
    ble_cts_cent_scan();
}

void ble_cts_cent_host_task(void *param)
{
    ESP_LOGI(tag, "BLE Host Task Started");
    // This function will return only when nimble_port_stop() is executed 
    nimble_port_run();

    nimble_port_freertos_deinit();
}

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);

    int rc;
    // Initialize NVS — it is used to store PHY calibration data 
    esp_err_t ret = nvs_flash_init();
    if  (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(tag, "Failed to init nimble %d ", ret);
        return;
    }

    // Initialize LED
    led_init();
    led_green();

    // Configure the host. 
    ble_hs_cfg.reset_cb = ble_cts_cent_on_reset;
    ble_hs_cfg.sync_cb = ble_cts_cent_on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    // Initialize data structures to track connected peers. 
    rc = peer_init(MYNEWT_VAL(BLE_MAX_CONNECTIONS), 64, 64, 64);
    assert(rc == 0);

    // Set the default device name. 
    rc = ble_svc_gap_device_name_set("nimble-cts-cent");
    assert(rc == 0);

    // XXX Need to have template for store 
    ble_store_config_init();

    nimble_port_freertos_init(ble_cts_cent_host_task);
}
