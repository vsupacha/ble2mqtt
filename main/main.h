#ifndef _MAIN_H
#define _MAIN_H

/* Common include files */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_freertos_hooks.h"
#include "esp_event.h"
#include "esp_system.h"
#include "esp_log.h"

/* FreeRTOS synchronization objects */
extern EventGroupHandle_t net_evt_group;
extern QueueHandle_t env_sensor_q;
extern SemaphoreHandle_t label_txt_sem;
extern char label_txt[];

/* Event group definition */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define MQTT_CONNECTED_BIT BIT2

#endif