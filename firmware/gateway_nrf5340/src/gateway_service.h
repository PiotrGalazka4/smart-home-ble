#ifndef GATEWAY_SERVICE_H_
#define GATEWAY_SERVICE_H_

#include <zephyr/bluetooth/uuid.h>

#define BT_UUID_GATEWAY_ID_VAL \
        BT_UUID_128_ENCODE(0x27a40001, 0x488f, 0x4854, 0x9bf9, 0xf8f9b70d6ddc)

#define BT_UUID_HEATING_SERVICE_VAL \
        BT_UUID_128_ENCODE(0x27a40002, 0x488f, 0x4854, 0x9bf9, 0xf8f9b70d6ddc)

#define BT_UUID_HEATING_TARGET_TEMP_VAL \
        BT_UUID_128_ENCODE(0x27a40003, 0x488f, 0x4854, 0x9bf9, 0xf8f9b70d6ddc)

#define BT_UUID_HEATING_CURRENT_TEMP_VAL \
        BT_UUID_128_ENCODE(0x27a40004, 0x488f, 0x4854, 0x9bf9, 0xf8f9b70d6ddc)

#define BT_UUID_LIGHT_SERVICE_VAL \
        BT_UUID_128_ENCODE(0x27a40005, 0x488f, 0x4854, 0x9bf9, 0xf8f9b70d6ddc)

#define BT_UUID_LIGHT_DELAY_TIME_VAL \
        BT_UUID_128_ENCODE(0x27a40006, 0x488f, 0x4854, 0x9bf9, 0xf8f9b70d6ddc)

#define BT_UUID_LIGHT_STATE_VAL \
        BT_UUID_128_ENCODE(0x27a40007, 0x488f, 0x4854, 0x9bf9, 0xf8f9b70d6ddc)

#define BT_UUID_HEATING_SERVICE BT_UUID_DECLARE_128(BT_UUID_HEATING_SERVICE_VAL)
#define BT_UUID_HEATING_TARGET_TEMP BT_UUID_DECLARE_128(BT_UUID_HEATING_TARGET_TEMP_VAL)
#define BT_UUID_HEATING_CURRENT_TEMP BT_UUID_DECLARE_128(BT_UUID_HEATING_CURRENT_TEMP_VAL)

#define BT_UUID_LIGHT_SERVICE BT_UUID_DECLARE_128(BT_UUID_LIGHT_SERVICE_VAL)
#define BT_UUID_LIGHT_DELAY_TIME BT_UUID_DECLARE_128(BT_UUID_LIGHT_DELAY_TIME_VAL)
#define BT_UUID_LIGHT_STATE BT_UUID_DECLARE_128(BT_UUID_LIGHT_STATE_VAL)

typedef int (*heating_target_temp_write_cb_t)(const uint16_t target_temp);
typedef int16_t (*heating_current_temp_read_cb_t)(void);
typedef bool (*light_state_read_cb_t)(void);
typedef int (*light_state_write_cb_t)(const bool state);
typedef int (*light_delay_time_write_cb_t)(const uint16_t delay_time);
        

struct bt_gateway_service_cb{
    heating_target_temp_write_cb_t heating_target_temp_write;
    heating_current_temp_read_cb_t heating_current_temp_read;
    light_state_read_cb_t light_state_read;
    light_state_write_cb_t light_state_write;
    light_delay_time_write_cb_t light_delay_time_write;
};

int bt_gateway_service_init(struct bt_gateway_service_cb *callbacks);

#endif