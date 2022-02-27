#include "main.h"

#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"

#include "ble_gatt.h"

/* DEFINES */
#define TAG                 "BLE-MQTT"
#define PROFILE_NUM         1
#define PROFILE_A_APP_ID    0
#define INVALID_HANDLE      0

/* parameters for Xiaomi Mijia temperature and humidity sensor */
const char remote_device_name[] = "LYWSD03MMC";
const uint8_t REMOTE_SERVICE_UUID[ESP_UUID_LEN_128]     = {0xA6, 0xA3, 0x7D, 0x99, 0xF2, 0x6F, 0x1A, 0x8A, 0x0C, 0x4B, 0x0A, 0x7A, 0xB0, 0xCC, 0xE0, 0xEB};
const uint8_t REMOTE_NOTIFY_CHAR_UUID[ESP_UUID_LEN_128] = {0xA6, 0xA3, 0x7D, 0x99, 0xF2, 0x6F, 0x1A, 0x8A, 0x0C, 0x4B, 0x0A, 0x7A, 0xC1, 0xCC, 0xE0, 0xEB};

/* STATIC VARIABLES */
static bool connect_flag = false;
static bool service_flag = false;

static esp_ble_scan_params_t ble_scan_params = {
    .scan_type              = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval          = 0x50,
    .scan_window            = 0x30,
    .scan_duplicate         = BLE_SCAN_DUPLICATE_DISABLE
};

static esp_bt_uuid_t remote_device_service_uuid = {
    .len = ESP_UUID_LEN_128,
    .uuid = {.uuid128 = {0},},
};
static esp_bt_uuid_t remote_device_char_uuid = {
    .len = ESP_UUID_LEN_128,
    .uuid = {.uuid128 = {0},},
};
static esp_gattc_char_elem_t *char_elem_result = NULL;

env_sensor_msg_t sensor_msg = {0.0, 0};

/* STATIC PROTOTYPES */
static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
static void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
static void gattc_profile_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);

/* GATT-based profile for one app_id and one gattc_if */
struct gattc_profile_inst {
    esp_gattc_cb_t gattc_cb;
    uint16_t gattc_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_start_handle;
    uint16_t service_end_handle;
    uint16_t char_handle;
    esp_bd_addr_t remote_bda;
};

/* array to store the gattc_if returned by ESP_GATTS_REG_EVT */
static struct gattc_profile_inst gatt_profile_tab[PROFILE_NUM] = {
    [PROFILE_A_APP_ID] = {
        .gattc_cb = gattc_profile_event_handler,
        .gattc_if = ESP_GATT_IF_NONE,       /* initial is ESP_GATT_IF_NONE */ 
    },
};

/**
 * @brief Callback function to handle GATT profile events
 * 
 * @param event 
 * @param gattc_if 
 * @param param 
 */
static void gattc_profile_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param) {
    esp_ble_gattc_cb_param_t *p_data = (esp_ble_gattc_cb_param_t *)param;

    ESP_LOGI(TAG, "GATT event handler: core %d\n", xPortGetCoreID());
    switch (event) {
    case ESP_GATTC_REG_EVT:
        /* start scan when GATT callback is registered */
        ESP_LOGI(TAG, "ESP_GATTC_REG_EVT");
        esp_ble_gap_set_scan_params(&ble_scan_params);
        break;
    case ESP_GATTC_CONNECT_EVT:
        /* update connection status to GATT profile, then send MTU request */
        ESP_LOGI(TAG, "ESP_GATTC_CONNECT_EVT conn_id %d, if %d", p_data->connect.conn_id, gattc_if);
        gatt_profile_tab[PROFILE_A_APP_ID].conn_id = p_data->connect.conn_id;
        memcpy(gatt_profile_tab[PROFILE_A_APP_ID].remote_bda, p_data->connect.remote_bda, sizeof(esp_bd_addr_t));
        ESP_LOGI(TAG, "REMOTE BDA:");
        esp_log_buffer_hex(TAG, gatt_profile_tab[PROFILE_A_APP_ID].remote_bda, sizeof(esp_bd_addr_t));
        esp_ble_gattc_send_mtu_req (gattc_if, p_data->connect.conn_id);
        break;
    case ESP_GATTC_OPEN_EVT:
        ESP_LOGI(TAG, "ESP_GATTC_OPEN_EVT");
        break;
    case ESP_GATTC_DIS_SRVC_CMPL_EVT:
        /* search for matched service UUID */
        ESP_LOGI(TAG, "ESP_GATTC_DIS_SRVC_CMPL_EVT");
        memcpy(remote_device_service_uuid.uuid.uuid128, REMOTE_SERVICE_UUID, ESP_UUID_LEN_128);        
        esp_ble_gattc_search_service(gattc_if, param->cfg_mtu.conn_id, &remote_device_service_uuid);
        break;
    case ESP_GATTC_CFG_MTU_EVT:
        ESP_LOGI(TAG, "ESP_GATTC_CFG_MTU_EVT, Status %d, MTU %d, conn_id %d", param->cfg_mtu.status, param->cfg_mtu.mtu, param->cfg_mtu.conn_id);
        break;        
    case ESP_GATTC_SEARCH_RES_EVT:
        /* for each found service, check for matched UUID */
        ESP_LOGI(TAG, "ESP_GATTC_SEARCH_RES_EVT");
        if (p_data->search_res.srvc_id.uuid.len == ESP_UUID_LEN_128) {
            ESP_LOGI(TAG, "Service ID:");
            esp_log_buffer_hex(TAG, p_data->search_res.srvc_id.uuid.uuid.uuid128, ESP_UUID_LEN_128);
            if (memcmp(p_data->search_res.srvc_id.uuid.uuid.uuid128, REMOTE_SERVICE_UUID, ESP_UUID_LEN_128) == 0){
                ESP_LOGI(TAG, "service UUID128 found");
                service_flag = true;
                gatt_profile_tab[PROFILE_A_APP_ID].service_start_handle = p_data->search_res.start_handle;
                gatt_profile_tab[PROFILE_A_APP_ID].service_end_handle = p_data->search_res.end_handle;
            } else {
                ESP_LOGE(TAG, "service not found");
            }
        }
        break;        
    case ESP_GATTC_SEARCH_CMPL_EVT:
        /* if service UUID is matched, check for matched characteristics */
        ESP_LOGI(TAG, "ESP_GATTC_SEARCH_CMPL_EVT");
        if (service_flag) {
            uint16_t count = 0;
            esp_ble_gattc_get_attr_count(gattc_if,
                                         p_data->search_cmpl.conn_id,
                                         ESP_GATT_DB_CHARACTERISTIC,
                                         gatt_profile_tab[PROFILE_A_APP_ID].service_start_handle,
                                         gatt_profile_tab[PROFILE_A_APP_ID].service_end_handle,
                                         INVALID_HANDLE,
                                         &count);
            ESP_LOGI(TAG, "ATTR count: %d\n", count);
            if (count > 0) {
                /* register for notification */
                memcpy(remote_device_char_uuid.uuid.uuid128, REMOTE_NOTIFY_CHAR_UUID, ESP_UUID_LEN_128);
                char_elem_result = (esp_gattc_char_elem_t *)malloc(sizeof(esp_gattc_char_elem_t) *count);
                if (char_elem_result) {
                    esp_ble_gattc_get_char_by_uuid(gattc_if,
                                                   p_data->search_cmpl.conn_id,
                                                   gatt_profile_tab[PROFILE_A_APP_ID].service_start_handle,
                                                   gatt_profile_tab[PROFILE_A_APP_ID].service_end_handle,
                                                   remote_device_char_uuid,
                                                   char_elem_result,
                                                   &count);
                    /* use only first 'char_elem_result' */
                    if (count > 0 && (char_elem_result[0].properties & ESP_GATT_CHAR_PROP_BIT_NOTIFY)) {
                        gatt_profile_tab[PROFILE_A_APP_ID].char_handle = char_elem_result[0].char_handle;
                        esp_ble_gattc_register_for_notify(gattc_if, gatt_profile_tab[PROFILE_A_APP_ID].remote_bda, char_elem_result[0].char_handle);
                    }
                } else {
                    ESP_LOGE(TAG, "gattc no mem");
                } 
                /* free char_elem_result */
                free(char_elem_result);
            } else {
                ESP_LOGE(TAG, "no char found");
            }
        }
        break;
    case ESP_GATTC_REG_FOR_NOTIFY_EVT:
        ESP_LOGI(TAG, "ESP_GATTC_REG_FOR_NOTIFY_EVT");
        break;
    case ESP_GATTC_NOTIFY_EVT:
        /* send notified data via queue */
        if (p_data->notify.is_notify){
            ESP_LOGI(TAG, "ESP_GATTC_NOTIFY_EVT, receive notify value:");
            esp_log_buffer_hex(TAG, p_data->notify.value, p_data->notify.value_len);
            sensor_msg.temp_val = (p_data->notify.value[0] | (p_data->notify.value[1] << 8))/100.0;
            sensor_msg.humid_val = p_data->notify.value[2];
            env_sensor_msg_t *p_msg;
            p_msg = &sensor_msg;
            xQueueSend(env_sensor_q, (void *)&p_msg, (TickType_t)0); // non-blocking
        } else {
            ESP_LOGI(TAG, "ESP_GATTC_NOTIFY_EVT, receive indicate value:");
        }
        break;        
    case ESP_GATTC_DISCONNECT_EVT:
        ESP_LOGI(TAG, "ESP_GATTC_DISCONNECT_EVT");
        connect_flag = false;
        service_flag = false;
        break;        
    default:
        break;
    }  
}

/**
 * @brief GAP callback, search for matched device name
 * 
 * @param event 
 * @param param 
 */
static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    uint8_t *adv_name = NULL;
    uint8_t adv_name_len = 0;

    ESP_LOGI(TAG, "GAP event handler: core %d\n", xPortGetCoreID());
    switch (event) {    
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
        /* start GAP scanning */
        ESP_LOGI(TAG, "ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT");        
        esp_ble_gap_start_scanning(30); // duration
        break;
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:        
        ESP_LOGI(TAG, "ESP_GAP_BLE_SCAN_START_COMPLETE_EVT");
        break;
    case ESP_GAP_BLE_SCAN_RESULT_EVT: 
        /* for each scan result, compare device name */
        ESP_LOGI(TAG, "ESP_GAP_BLE_SCAN_RESULT_EVT");
        esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
        switch (scan_result->scan_rst.search_evt) {
        case ESP_GAP_SEARCH_INQ_RES_EVT:
            adv_name = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv,
                                                ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len);
            if (adv_name != NULL) {
                if (strlen(remote_device_name) == adv_name_len && strncmp((char *)adv_name, remote_device_name, adv_name_len) == 0) {
                    ESP_LOGI(TAG, "Found %s\n", remote_device_name);
                    /* stop scan and open connection */
                    if (connect_flag == false) {
                        connect_flag = true;
                        esp_ble_gap_stop_scanning();
                        esp_ble_gattc_open(gatt_profile_tab[PROFILE_A_APP_ID].gattc_if, scan_result->scan_rst.bda, scan_result->scan_rst.ble_addr_type, true);
                    }
                }
            }            
            break;  
        case ESP_GAP_SEARCH_INQ_CMPL_EVT:
            break;
        default:
            break;         
        }
        break;
    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
        ESP_LOGI(TAG, "ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT");
        break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        ESP_LOGI(TAG, "ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT");
        break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
         ESP_LOGI(TAG, "update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
                  param->update_conn_params.status,
                  param->update_conn_params.min_int,
                  param->update_conn_params.max_int,
                  param->update_conn_params.conn_int,
                  param->update_conn_params.latency,
                  param->update_conn_params.timeout);
        break;        
    default:
        break;
    }
}

/**
 * @brief GATT callback, forward event to be handled by gattc_profile_event_handler
 * 
 * @param event 
 * @param gattc_if 
 * @param param 
 */
static void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param) {
    if (event == ESP_GATTC_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            gatt_profile_tab[param->reg.app_id].gattc_if = gattc_if;
        } else {
            ESP_LOGI(TAG, "reg app failed, app_id %04x, status %d",
                    param->reg.app_id,
                    param->reg.status);
            return;
        }
    }
    /* process event by corresponding profile handler */
    for (int idx = 0; idx < PROFILE_NUM; idx++) {
        /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
        if ((gattc_if == ESP_GATT_IF_NONE) || (gattc_if == gatt_profile_tab[idx].gattc_if)) {
            if (gatt_profile_tab[idx].gattc_cb) {
                gatt_profile_tab[idx].gattc_cb(event, gattc_if, param);
            }
        }
    }
}

/**
 * @brief initialize BLE driver
 * 
 */
void ble_gatt_init() {
    /* initialize Bluetooth module */
    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BLE);
    esp_bluedroid_init();
    esp_bluedroid_enable();

    /* register callback functions for GAP/GATT */
    esp_ble_gap_register_callback(esp_gap_cb);
    esp_ble_gattc_register_callback(esp_gattc_cb);
    esp_ble_gattc_app_register(PROFILE_A_APP_ID);
    esp_ble_gatt_set_local_mtu(500);
}