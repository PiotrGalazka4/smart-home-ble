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

uint16_t current_temp = 0;
uint16_t target_temp = 0;
bool light_state = false;
uint16_t light_delay_time = 10;

static ssize_t current_temp_read(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset)
{
    LOG_INF("Current temperature read: %d", current_temp);
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &current_temp, sizeof(current_temp));
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

    uint16_t new_target_temp = sys_get_le16(buf);

    if (new_target_temp < TEMP_MIN_VAL || new_target_temp > TEMP_MAX_VAL) {
        return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
    }

    target_temp = new_target_temp;

    LOG_INF("Target temperature updated to: %d", target_temp);

    return len;
}

static ssize_t light_state_read(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset)
{
    LOG_INF("Light state read: %d", light_state);
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &light_state, sizeof(light_state));
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

    light_delay_time = sys_get_le16(buf);

    LOG_INF("Light delay time updated to: %d", light_delay_time);

    return len;
}


BT_GATT_SERVICE_DEFINE(heating_svc, BT_GATT_PRIMARY_SERVICE(BT_UUID_HEATING_SERVICE),
    BT_GATT_CHARACTERISTIC(BT_UUID_HEATING_CURRENT_TEMP, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_READ_AUTHEN, current_temp_read, NULL, &current_temp),
    BT_GATT_CCC(NULL, BT_GATT_PERM_READ_AUTHEN | BT_GATT_PERM_WRITE_AUTHEN),
    BT_GATT_CHARACTERISTIC(BT_UUID_HEATING_TARGET_TEMP, BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE, BT_GATT_PERM_READ_AUTHEN | BT_GATT_PERM_WRITE_AUTHEN, target_temp_read, target_temp_upd, &target_temp)
);

BT_GATT_SERVICE_DEFINE(light_svc, BT_GATT_PRIMARY_SERVICE(BT_UUID_LIGHT_SERVICE),
    BT_GATT_CHARACTERISTIC(BT_UUID_LIGHT_STATE, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_READ_AUTHEN, light_state_read, NULL, &light_state),
    BT_GATT_CCC(NULL, BT_GATT_PERM_READ_AUTHEN | BT_GATT_PERM_WRITE_AUTHEN),
    BT_GATT_CHARACTERISTIC(BT_UUID_LIGHT_DELAY_TIME, BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE, BT_GATT_PERM_READ_AUTHEN | BT_GATT_PERM_WRITE_AUTHEN, light_delay_time_read, light_delay_time_write, &light_delay_time) 
);