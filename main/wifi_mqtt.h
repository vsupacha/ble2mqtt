#ifndef _WIFI_MQTT_H
#define _WIFI_MQTT_H

/**********************
 *  PUBLIC PROTOTYPES
 **********************/
void wifi_init_sta(void);
void mqtt_publish_val(float temp_val, uint8_t humid_val);

#endif