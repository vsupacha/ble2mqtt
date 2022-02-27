#include "main.h"

#include "driver/gpio.h"
#include "nvs_flash.h"

#include "gui.h"
#include "ble_gatt.h"
#include "wifi_mqtt.h"

/* DEFINES */
#define TAG "BLE2MQTT"

const char LABEL_TXT_TMPL[] = "Demo app for TESA Tech Update: RTOS\n"
                              "Feb 25, 2022 by Supachai Vorapojpisut\n"
                              "WiFi: %d, MQTT: %d\n"
                              "Temperature: %.1f degC\n"
                              "Humidity: %d %%RH\n";

/* PUBLIC VARIABLES */
EventGroupHandle_t net_evt_group;
QueueHandle_t env_sensor_q;
SemaphoreHandle_t label_txt_sem;
char label_txt[200] = {0};

/* STATIC VARIABLES */
static bool wifi_status_flag = false;
static bool mqtt_status_flag = false;
static float temp_val = 0.0;
static uint8_t humid_val = 0;

/**
 * @brief Main application, create GUI task, forward BLE to MQTT
 * 
 * @return none
 */
void app_main() {
    /* initialize NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    /* create FreeRTOS objects */
    net_evt_group = xEventGroupCreate();
    env_sensor_q = xQueueCreate(1, sizeof(env_sensor_msg_t*));
    label_txt_sem = xSemaphoreCreateMutex();

    /* create GUI task on core 1 */
    xTaskCreatePinnedToCore(gui_task_fcn, "gui", 4096*2, NULL, 1, NULL, 1);

    /* init Bluetooth and WiFi */
    ble_gatt_init();
    vTaskDelay(pdMS_TO_TICKS(100));
    wifi_init_sta();
    vTaskDelay(pdMS_TO_TICKS(100));

    for (;;) {
        /* wait for network status upto 100 ms */
        EventBits_t bits = xEventGroupWaitBits(net_evt_group,
                                               WIFI_CONNECTED_BIT | WIFI_FAIL_BIT | MQTT_CONNECTED_BIT,
                                               pdFALSE,
                                               pdFALSE,
                                               pdMS_TO_TICKS(100));
        if (bits & WIFI_CONNECTED_BIT) {
            wifi_status_flag = true;
        } 
        if (bits & WIFI_FAIL_BIT) {
            wifi_status_flag = false;
        } 
        if (bits & MQTT_CONNECTED_BIT) {
            mqtt_status_flag = true;
        } 
        // non-blocking check for new sensor value
        env_sensor_msg_t *p_msg;
        if (xQueueReceive(env_sensor_q, &(p_msg), (TickType_t)0) == pdPASS) {
            temp_val = p_msg->temp_val;
            humid_val = p_msg->humid_val;
            mqtt_publish_val(temp_val, humid_val);
        } 
        // non-blocking update label text
        if (xSemaphoreTake(label_txt_sem, (TickType_t)0) == pdPASS) {
            sprintf(label_txt, LABEL_TXT_TMPL, wifi_status_flag, mqtt_status_flag, temp_val, humid_val);
            xSemaphoreGive(label_txt_sem);
        }
    }

    /* should not reach here */
    esp_restart();
}
