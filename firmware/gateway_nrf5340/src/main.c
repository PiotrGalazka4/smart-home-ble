#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/conn.h>
#include <dk_buttons_and_leds.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

#define BT_UUID_GATEWAY_SERVICE_VAL \
        BT_UUID_128_ENCODE(0x00000001, 0x1212, 0xefde, 0x1523, 0x785feabcd123)

/* ---------------------------------------------------------------------------
 * State
 * ------------------------------------------------------------------------- */

static struct bt_conn *auth_conn;
static struct k_work adv_work;

/* ---------------------------------------------------------------------------
 * Advertising 
 * ------------------------------------------------------------------------- */

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static const struct bt_data sd[] = {
        BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_GATEWAY_SERVICE_VAL),
};

static void adv_work_handler(struct k_work *work)
{
	int err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_2, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
        
	if (err == -ENOMEM) {
                LOG_ERR("No free connections slots");
	} else if (err == -EALREADY) {
                LOG_INF("Advertising already started");
        } else if (err) {
                LOG_ERR("Advertising failed to start (err %d)", err);
                return;
        } else {
                LOG_INF("Advertising successfully started");
        }
}

static void advertising_start(void)
{
	k_work_submit(&adv_work);
}

/* ---------------------------------------------------------------------------
 * Connection callbacks
 * ------------------------------------------------------------------------- */

void on_connected(struct bt_conn *conn, uint8_t err)
{
        advertising_start();
}

void on_disconnected(struct bt_conn *conn, uint8_t reason)
{
        if (auth_conn == conn) {
                bt_conn_unref(auth_conn);
                auth_conn = NULL;
        }
}

void on_recycled(void)
{
        advertising_start();
}

struct bt_conn_cb conn_callbacks = {
        .connected = on_connected,
        .disconnected = on_disconnected,
        .recycled = on_recycled,
};

/* ---------------------------------------------------------------------------
 * Authentication callbacks
 * ------------------------------------------------------------------------- */


static void auth_passkey_display(struct bt_conn *conn, unsigned int passkey)
{
        char addr[BT_ADDR_LE_STR_LEN];

        bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
        LOG_INF("Passkey for %s: %06u", addr, passkey);
}

static void auth_passkey_confirm(struct bt_conn *conn, unsigned int passkey)
{
        if (auth_conn) {
                LOG_WRN("Already waiting for confirmation on another connection");
                return;
        }
        char addr[BT_ADDR_LE_STR_LEN];

        bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
        LOG_INF("Confirm passkey for %s: %06u", addr, passkey);

        auth_conn = bt_conn_ref(conn);
}

static void button_change(uint32_t button_state, uint32_t has_changed)
{
        if (auth_conn){
                if (has_changed & button_state & DK_BTN1_MSK) {
                        bt_conn_auth_passkey_confirm(auth_conn);
                        bt_conn_unref(auth_conn);
                        auth_conn = NULL;
                }

                else if (has_changed & button_state & DK_BTN2_MSK) {
                       bt_conn_auth_cancel(auth_conn);
                }
        }
}

static void auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	LOG_INF("Pairing cancelled: %s\n", addr);
        if (auth_conn) {
                bt_conn_unref(auth_conn);
                auth_conn = NULL;
        }
}

static struct bt_conn_auth_cb conn_auth_callbacks = {
        .passkey_confirm = auth_passkey_confirm,
        .passkey_display = auth_passkey_display,
	.cancel = auth_cancel,
};

int main(void)
{
        int err;

        LOG_INF("Starting Bluetooth initialization");

        err = dk_buttons_init(button_change);
        if (err) {
                LOG_ERR("Cannot init buttons (err: %d)", err);
                return -1;
        }

        err = bt_conn_auth_cb_register(&conn_auth_callbacks);
	if (err) {
		LOG_INF("Failed to register authorization callbacks.\n");
		return -1;
	}

        err = bt_conn_cb_register(&conn_callbacks);
	if (err) {
		LOG_INF("Failed to register connection callbacks.\n");
		return -1;
	}

        err = bt_enable(NULL);
        if (err) {
                LOG_ERR("Bluetooth init failed (err %d)", err);
                return -1;
        }
        LOG_INF("Bluetooth initialized");

        k_work_init(&adv_work, adv_work_handler);
        advertising_start();
}
