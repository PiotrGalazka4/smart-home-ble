#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/conn.h>
#include <dk_buttons_and_leds.h>
#include <zephyr/settings/settings.h>

#include "gateway_service.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

#define PAIRING_ACC_BUTTON DK_BTN1_MSK
#define PAIRING_REJ_BUTTON DK_BTN2_MSK
#define PAIRING_BUTTON DK_BTN3_MSK

#define PAIRING_TIMEOUT K_SECONDS(30)

#define BT_LE_ADV_CONN_ACCEPT_LIST                                                                 \
	BT_LE_ADV_PARAM(BT_LE_ADV_OPT_CONN | BT_LE_ADV_OPT_FILTER_CONN,                \
			BT_GAP_ADV_FAST_INT_MIN_2, BT_GAP_ADV_FAST_INT_MAX_2, NULL)

/* ---------------------------------------------------------------------------
 * State
 * ------------------------------------------------------------------------- */

static struct k_work_delayable pairing_timeout_work;
static struct bt_conn *auth_conn;
static struct k_work adv_work;
static bool pairing_enabled = false;


/* ---------------------------------------------------------------------------
 * Accept list management
 * ------------------------------------------------------------------------- */

static void add_to_accept_list(const struct bt_bond_info *info, void *user_data)
{
        char addr[BT_ADDR_LE_STR_LEN];
        bt_addr_le_to_str(&info->addr, addr, sizeof(addr));
	int err = bt_le_filter_accept_list_add(&info->addr);
	if (err) {
		LOG_ERR("Cannot add peer to Filter Accept List (err: %d)", err);
	} else {
		LOG_INF("Added following peer to Filter Accept List: %s", addr);
	}
}

static void setup_accept_list(void)
{
        int err = bt_le_filter_accept_list_clear();
        if (err) {
                LOG_ERR("Cannot clear Filter Accept List (err: %d)", err);
                return;
        }

        bt_foreach_bond(BT_ID_DEFAULT, add_to_accept_list, NULL);

}

/* ---------------------------------------------------------------------------
 * Advertising 
 * ------------------------------------------------------------------------- */

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static const struct bt_data sd[] = {
        BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_GATEWAY_ID_VAL),
};

static void adv_work_handler(struct k_work *work)
{
        int err;
        if (!pairing_enabled) {
                err = bt_le_adv_start(BT_LE_ADV_CONN_ACCEPT_LIST, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
        } else{
	        err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_2, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
        }

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

static void advertising_set_pairing(bool enable)
{
        pairing_enabled = enable;

        if (enable) {
                k_work_reschedule(&pairing_timeout_work, PAIRING_TIMEOUT);
        } else {
                k_work_cancel_delayable(&pairing_timeout_work);
        }

        int err = bt_le_adv_stop();

	if (err== -EALREADY) {
                LOG_INF("Advertising already stopped");
        } else if (err) {
		LOG_INF("Cannot stop advertising err= %d \n", err);
		return;
	}

        LOG_INF("Pairing mode %s", enable ? "enabled" : "disabled");

        setup_accept_list();
        advertising_start();
}

static void pairing_timeout_handler(struct k_work *work)
{
        LOG_INF("Pairing mode timed out");
        advertising_set_pairing(false);
}
/* ---------------------------------------------------------------------------
 * Connection information callbacks
 * ------------------------------------------------------------------------- */

static void on_pairing_complete(struct bt_conn *conn, bool bonded){

        char addr[BT_ADDR_LE_STR_LEN];
        bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
        LOG_INF("Pairing completed with %s, bonded: %d", addr, bonded);

        advertising_set_pairing(false);
}

static void on_pairing_failed(struct bt_conn *conn, enum bt_security_err reason){

        char addr[BT_ADDR_LE_STR_LEN];
        bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
        LOG_INF("Pairing failed with %s, reason: %d", addr, reason);

        advertising_set_pairing(false);
}

static struct bt_conn_auth_info_cb conn_info_callbacks = {
        .pairing_complete = on_pairing_complete,
        .pairing_failed = on_pairing_failed,
};

/* ---------------------------------------------------------------------------
 * Connection callbacks
 * ------------------------------------------------------------------------- */

static void on_connected(struct bt_conn *conn, uint8_t err)
{
        char addr[BT_ADDR_LE_STR_LEN];

        bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

        if (err) {
                LOG_ERR("Connection failed (err 0x%02x)", err);
                return;
        }
        LOG_INF("Connected: %s", addr);

        advertising_start();
}

static void on_disconnected(struct bt_conn *conn, uint8_t reason)
{
        LOG_INF("Disconnected (reason 0x%02x)", reason);

        if (auth_conn == conn) {
                bt_conn_unref(auth_conn);
                auth_conn = NULL;
        }
}

static void on_recycled(void)
{
        advertising_start();
}

static struct bt_conn_cb conn_callbacks = {
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
                if (has_changed & button_state & PAIRING_ACC_BUTTON) {
                        bt_conn_auth_passkey_confirm(auth_conn);
                        bt_conn_unref(auth_conn);
                        auth_conn = NULL;
                }

                else if (has_changed & button_state & PAIRING_REJ_BUTTON) {
                       bt_conn_auth_cancel(auth_conn);
                       
                }
        }

        if (has_changed & button_state & PAIRING_BUTTON) {
                advertising_set_pairing(true);
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

/* ---------------------------------------------------------------------------
 * gateway service callbacks
 * ------------------------------------------------------------------------- */
static int heating_target_temp_write_cb(const uint16_t target_temp)
{
        LOG_INF("Heating target temperature write callback: %d.%dC", target_temp/10, target_temp%10);
        return 0;
}

static int16_t heating_current_temp_read_cb(void)
{
        LOG_INF("Heating current temperature read callback");
        return 250; // 25.0°C
}

static bool light_state_read_cb(void)
{
        LOG_INF("Light state read callback");
        return true;
}

static int light_state_write_cb(const bool state)
{
        LOG_INF("Light state write callback: %d", state);
        return 0;
}

static int light_delay_time_write_cb(const uint16_t delay_time)
{
        LOG_INF("Light delay time write callback: %d", delay_time);
        return 0;
}

static struct bt_gateway_service_cb gateway_service_cb = {
        .heating_target_temp_write = heating_target_temp_write_cb,
        .heating_current_temp_read = heating_current_temp_read_cb,
        .light_state_read = light_state_read_cb,
        .light_state_write = light_state_write_cb,
        .light_delay_time_write = light_delay_time_write_cb,
};

int main(void)
{
        int err;

        LOG_INF("Starting Bluetooth initialization");

        k_work_init(&adv_work, adv_work_handler);
        k_work_init_delayable(&pairing_timeout_work, pairing_timeout_handler);

        err = dk_buttons_init(button_change);
        if (err) {
                LOG_ERR("Cannot init buttons (err: %d)", err);
                return -1;
        }

        err = bt_gateway_service_init(&gateway_service_cb);
        if (err) {
                LOG_ERR("Failed to initialize gateway service (err: %d)", err);
                return -1;
        }

        err = bt_conn_auth_cb_register(&conn_auth_callbacks);
	if (err) {
		LOG_INF("Failed to register authorization callbacks.\n");
		return -1;
	}

        err = bt_conn_auth_info_cb_register(&conn_info_callbacks);
	if (err) {
		LOG_INF("Failed to register authorization info callbacks.\n");
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

        err = settings_load();
        if (err) {
                LOG_ERR("Failed to load settings (err %d)", err);
                return 0;
        }
        // bt_unpair(BT_ID_DEFAULT, BT_ADDR_LE_ANY); // Unpair all devices on startup for testing purposes
        setup_accept_list();
        advertising_start();
}
