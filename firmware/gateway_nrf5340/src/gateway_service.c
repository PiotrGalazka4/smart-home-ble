#include <zephyr/types.h>
#include <stddef.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/kernel.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>

#include "gateway_service.h"

#define TEMP_MIN_VAL 150 // 15.0°C
#define TEMP_MAX_VAL 300 // 30.0°C
#define LIGHT_DELAY_MIN_VAL_S 5 // 5 seconds
#define LIGHT_DELAY_MAX_VAL_S 180 // 180 seconds
LOG_MODULE_REGISTER(gateway_service, LOG_LEVEL_INF);

static int16_t current_temp = 0; // 0.0°C
static int16_t target_temp = 230; // 23.0°C
static bool light_state = false;
static uint16_t light_delay_time_s = 10; // 10 seconds

static struct bt_gateway_service_cb gateway_service_cb;

static ssize_t current_temp_read(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset)
{
    if(gateway_service_cb.heating_current_temp_read) {
        current_temp = gateway_service_cb.heating_current_temp_read();
        LOG_INF("Current temperature read: %d.%dC", current_temp/10, current_temp%10);
        int16_t le_current_temp = (int16_t)sys_cpu_to_le16(current_temp);
        return bt_gatt_attr_read(conn, attr, buf, len, offset, &le_current_temp, sizeof(le_current_temp));
    }

    LOG_WRN("Current temperature read callback not set");
    return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
    
}

static ssize_t target_temp_read(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset)
{
    LOG_INF("Target temperature read: %d.%dC", target_temp/10, target_temp%10);
    int16_t le_target_temp = (int16_t)sys_cpu_to_le16(target_temp);
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &le_target_temp, sizeof(le_target_temp));
}

static ssize_t target_temp_write(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
    if (len != sizeof(target_temp)) {
        LOG_WRN("Invalid target temperature write length: %d", len);
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }

    if (offset != 0) {
        LOG_WRN("Invalid target temperature write offset: %d", offset);
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

    int16_t new_target_temp = (int16_t)sys_get_le16(buf);

    if (new_target_temp < TEMP_MIN_VAL || new_target_temp > TEMP_MAX_VAL) {
        LOG_WRN("Target temperature value out of range: %d.%dC", new_target_temp/10, new_target_temp%10);
        return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
    }

    if (!gateway_service_cb.heating_target_temp_write) {
        LOG_WRN("Target temperature write callback not set");
        return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
    }

    int err = gateway_service_cb.heating_target_temp_write(new_target_temp);
    if (err) {
        LOG_ERR("Error in target temperature write callback: %d", err);
        return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
    }
    target_temp = new_target_temp;

    LOG_INF("Target temperature updated to: %d.%dC", target_temp/10, target_temp%10);
    return len;
}

static ssize_t light_state_read(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset)
{

    if(gateway_service_cb.light_state_read) {
        light_state = gateway_service_cb.light_state_read();
        LOG_INF("Light state read: %d", light_state);
        return bt_gatt_attr_read(conn, attr, buf, len, offset, &light_state, sizeof(light_state));
    } 

    LOG_WRN("Light state read callback not set");
    return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
}

static ssize_t light_state_write(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
    if (len != sizeof(uint8_t)) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }

    if (offset != 0) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

    uint8_t new_light_state = *(const uint8_t *)buf;

    if (new_light_state != 0x00 && new_light_state != 0x01) {
        LOG_WRN("Invalid light state value: %d", new_light_state);
        return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
    }

    if(!gateway_service_cb.light_state_write) {
        LOG_INF("Light state write callback not set");
        return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
    }

    bool state_bool = new_light_state;
            
    int err = gateway_service_cb.light_state_write(state_bool);
    if (err) {
        LOG_ERR("Error in light state write callback: %d", err);
        return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
    }
    light_state = state_bool;

    LOG_INF("Light state write: %d", new_light_state);
    return len;
}

static ssize_t light_delay_time_read(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset)
{
    LOG_INF("Light delay time read: %d", light_delay_time_s);
    uint16_t le_light_delay_time = sys_cpu_to_le16(light_delay_time_s);
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &le_light_delay_time, sizeof(le_light_delay_time));
}

static ssize_t light_delay_time_write(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
    if (len != sizeof(light_delay_time_s)) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }

    if (offset != 0) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

    if(!gateway_service_cb.light_delay_time_write) {
        LOG_INF("Light delay time write callback not set");
        return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
    }

    uint16_t new_light_delay_time = sys_get_le16(buf);

    if (new_light_delay_time < LIGHT_DELAY_MIN_VAL_S || new_light_delay_time > LIGHT_DELAY_MAX_VAL_S) {
        LOG_WRN("Light delay time value out of range: %d", new_light_delay_time);
        return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
    }

    int err = gateway_service_cb.light_delay_time_write(new_light_delay_time);
    if (err) {
        LOG_ERR("Error in light delay time write callback: %d", err);
        return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
    }
    light_delay_time_s = new_light_delay_time;
    
    LOG_INF("Light delay time updated to: %d", light_delay_time_s);
    return len;
}


BT_GATT_SERVICE_DEFINE(heating_svc, BT_GATT_PRIMARY_SERVICE(BT_UUID_HEATING_SERVICE),
    BT_GATT_CHARACTERISTIC(BT_UUID_HEATING_CURRENT_TEMP, BT_GATT_CHRC_READ, BT_GATT_PERM_READ_AUTHEN, current_temp_read, NULL, &current_temp),
    BT_GATT_CHARACTERISTIC(BT_UUID_HEATING_TARGET_TEMP, BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE, BT_GATT_PERM_READ_AUTHEN | BT_GATT_PERM_WRITE_AUTHEN, target_temp_read, target_temp_write, &target_temp)
);

BT_GATT_SERVICE_DEFINE(light_svc, BT_GATT_PRIMARY_SERVICE(BT_UUID_LIGHT_SERVICE),
    BT_GATT_CHARACTERISTIC(BT_UUID_LIGHT_STATE, BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE, BT_GATT_PERM_READ_AUTHEN | BT_GATT_PERM_WRITE_AUTHEN, light_state_read, light_state_write, &light_state),
    BT_GATT_CHARACTERISTIC(BT_UUID_LIGHT_DELAY_TIME, BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE, BT_GATT_PERM_READ_AUTHEN | BT_GATT_PERM_WRITE_AUTHEN, light_delay_time_read, light_delay_time_write, &light_delay_time_s) 
);

int bt_gateway_service_init(struct bt_gateway_service_cb *callbacks)
{
    if (callbacks){
        gateway_service_cb.heating_target_temp_write = callbacks->heating_target_temp_write;
        gateway_service_cb.heating_current_temp_read = callbacks->heating_current_temp_read;
        gateway_service_cb.light_delay_time_write = callbacks->light_delay_time_write;
        gateway_service_cb.light_state_read = callbacks->light_state_read;
        gateway_service_cb.light_state_write = callbacks->light_state_write;
    }

    return 0;
}