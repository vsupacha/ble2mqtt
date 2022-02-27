#ifndef _BLE_GATT_H_
#define _BLE_GATT_H_

/* PUBLIC PROTOTYPES */
void ble_gatt_init(void);

/* TYPE DEFINITIONS */
typedef struct env_sensor_msg_t {
    float temp_val;
    uint8_t humid_val;
} env_sensor_msg_t;

#endif