#include "main.h"

#include "nvs_flash.h"

#include "esp_wifi.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#include "mqtt_client.h"

/* DEFINES */
#define TAG            "WIFI_MQTT"

#define WIFI_SSID      "YOUR SSID"
#define WIFI_PASS      "YOUR PASSWORD"
#define WIFI_MAXIMUM_RETRY  10
#define MQTT_BROKER    "mqtt://broker.hivemq.com"
#define MQTT_TOPIC     "tesa_tech_update/sensor"

/* STATIC VARIABLES */
static int retry_num = 0;
static esp_mqtt_client_handle_t mqtt_client;

/* STATIC PROTOTYPES */
static void esp_wifi_cb(void* args, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void esp_mqtt_cb(void *args, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void mqtt_init(void);

/**
 * @brief WiFi event callback: connect to AP, then notify status via EventGroup
 * 
 * @param arg 
 * @param event_base 
 * @param event_id 
 * @param event_data 
 */
static void esp_wifi_cb(void* args, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if ((event_base == WIFI_EVENT) && (event_id == WIFI_EVENT_STA_START)) {
        esp_wifi_connect();
    } else if ((event_base == WIFI_EVENT) && (event_id == WIFI_EVENT_STA_DISCONNECTED)) {
        if (retry_num < WIFI_MAXIMUM_RETRY) {
            esp_wifi_connect();
            retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(net_evt_group, WIFI_FAIL_BIT);
            ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s", WIFI_SSID, WIFI_PASS);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(net_evt_group, WIFI_CONNECTED_BIT);
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s", WIFI_SSID, WIFI_PASS);
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        retry_num = 0;
        mqtt_init();
    }
}

/**
 * @brief Initialize WiFi in stationary mode
 * 
 */
void wifi_init_sta(void) {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    /* register WiFi event */
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    esp_event_handler_instance_register(WIFI_EVENT,
                                        ESP_EVENT_ANY_ID,
                                        &esp_wifi_cb,
                                        NULL,
                                        &instance_any_id);
    esp_event_handler_instance_register(IP_EVENT,
                                        IP_EVENT_STA_GOT_IP,
                                        &esp_wifi_cb,
                                        NULL,
                                        &instance_got_ip);
    /* connect to AP */
    wifi_config_t wifi_cfg = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
	        .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    esp_wifi_start();
}

/**
 * @brief process MQTT status
 * 
 * @param args 
 * @param event_base 
 * @param event_id 
 * @param event_data 
 */
static void esp_mqtt_cb(void *args, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        xEventGroupSetBits(net_evt_group, MQTT_CONNECTED_BIT);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

/**
 * @brief create MQTT client, then connect to TCP-based broker
 * 
 */
static void mqtt_init(void) {
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = MQTT_BROKER,
        .port = 1883,
    };
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);    
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, esp_mqtt_cb, NULL);
    esp_mqtt_client_start(mqtt_client);    
}

/**
 * @brief publish JSON data to broker
 * 
 * @param temp_val 
 * @param humid_val 
 */
void mqtt_publish_val(float temp_val, uint8_t humid_val) {
    char buf[50];
    sprintf(buf, "{\"temp\":%.1f, \"humid\":%d}", temp_val, humid_val);
    esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC, buf, strlen(buf), 0, 0);
}
