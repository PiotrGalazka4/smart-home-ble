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

#define TEMP_MIN_VAL 15
#define TEMP_MAX_VAL 30
LOG_MODULE_REGISTER(gateway_service, LOG_LEVEL_INF);

static uint16_t current_temp = 0;
static uint16_t target_temp = 0;
static bool light_state = false;
static uint16_t light_delay_time = 10;

static struct bt_gateway_service_cb gateway_service_cb;

static ssize_t current_temp_read(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset)
{
    if(gateway_service_cb.heating_current_temp_read) {
        current_temp = gateway_service_cb.heating_current_temp_read();
        LOG_INF("Current temperature read: %d", current_temp);
        return bt_gatt_attr_read(conn, attr, buf, len, offset, &current_temp, sizeof(current_temp));
    }

    LOG_INF("Current temperature read callback not set");
    return 0;
    
}

static ssize_t target_temp_read(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset)
{
    LOG_INF("Target temperature read: %d", target_temp);
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &target_temp, sizeof(target_temp));
}

static ssize_t target_temp_upd(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
    if (len != sizeof(target_temp)) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }

    if (offset != 0) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

    if (gateway_service_cb.heating_target_temp_write) {
        uint16_t new_target_temp = sys_get_le16(buf);

        if (new_target_temp < TEMP_MIN_VAL || new_target_temp > TEMP_MAX_VAL) {
            return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
        }
        gateway_service_cb.heating_target_temp_write(new_target_temp);

        target_temp = new_target_temp;

        LOG_INF("Target temperature updated to: %d", target_temp);
        return len;     

    }

    LOG_INF("Target temperature write callback not set");
    return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);

}

static ssize_t light_state_read(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset)
{

    if(gateway_service_cb.light_state_read) {
        light_state = gateway_service_cb.light_state_read();
        LOG_INF("Light state read: %d", light_state);
        return bt_gatt_attr_read(conn, attr, buf, len, offset, &light_state, sizeof(light_state));
    } 

    LOG_INF("Light state read callback not set");
    return 0;
    
}

static ssize_t light_state_write(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
    if (len != sizeof(uint8_t)) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }

    if (offset != 0) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

    if(gateway_service_cb.light_state_write) {
        uint8_t new_light_state = *(const uint8_t *)buf;

        if (new_light_state == 0x00 || new_light_state == 0x01) {

            bool state_bool = new_light_state;
            
            gateway_service_cb.light_state_write(state_bool);
            light_state = state_bool;
            LOG_INF("Light state write: %d", new_light_state);
            return len;

        } else {
            LOG_WRN("Invalid light state value: %d", new_light_state);
            return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
        }

    }

    LOG_INF("Light state write callback not set");
    return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);

}

static ssize_t light_delay_time_read(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset)
{
    LOG_INF("Light delay time read: %d", light_delay_time);
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &light_delay_time, sizeof(light_delay_time));
}

static ssize_t light_delay_time_write(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
    if (len != sizeof(light_delay_time)) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }

    if (offset != 0) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }
    if(gateway_service_cb.light_delay_time_write) {
        uint16_t new_light_delay_time = sys_get_le16(buf);

        gateway_service_cb.light_delay_time_write(new_light_delay_time);
        light_delay_time = new_light_delay_time;
        LOG_INF("Light delay time updated to: %d", light_delay_time);

        return len;
    }

    LOG_INF("Light delay time write callback not set");
    return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);

}


BT_GATT_SERVICE_DEFINE(heating_svc, BT_GATT_PRIMARY_SERVICE(BT_UUID_HEATING_SERVICE),
    BT_GATT_CHARACTERISTIC(BT_UUID_HEATING_CURRENT_TEMP, BT_GATT_CHRC_READ, BT_GATT_PERM_READ_AUTHEN, current_temp_read, NULL, &current_temp),
    BT_GATT_CHARACTERISTIC(BT_UUID_HEATING_TARGET_TEMP, BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE, BT_GATT_PERM_READ_AUTHEN | BT_GATT_PERM_WRITE_AUTHEN, target_temp_read, target_temp_upd, &target_temp)
);

BT_GATT_SERVICE_DEFINE(light_svc, BT_GATT_PRIMARY_SERVICE(BT_UUID_LIGHT_SERVICE),
    BT_GATT_CHARACTERISTIC(BT_UUID_LIGHT_STATE, BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE, BT_GATT_PERM_READ_AUTHEN | BT_GATT_PERM_WRITE_AUTHEN, light_state_read, light_state_write, &light_state),
    BT_GATT_CHARACTERISTIC(BT_UUID_LIGHT_DELAY_TIME, BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE, BT_GATT_PERM_READ_AUTHEN | BT_GATT_PERM_WRITE_AUTHEN, light_delay_time_read, light_delay_time_write, &light_delay_time) 
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