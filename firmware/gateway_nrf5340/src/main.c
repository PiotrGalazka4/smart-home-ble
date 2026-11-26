#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/conn.h>
#include <dk_buttons_and_leds.h>
#include <zephyr/settings/settings.h>

#include "gateway_service.h"
#include "client_link.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#define PAIRING_ACC_BUTTON DK_BTN1_MSK
#define PAIRING_REJ_BUTTON DK_BTN2_MSK
#define PAIRING_BUTTON DK_BTN3_MSK

/* ---------------------------------------------------------------------------
 * button callback
 * ------------------------------------------------------------------------- */

static void button_change(uint32_t button_state, uint32_t has_changed)
{
        if (has_changed & button_state & PAIRING_ACC_BUTTON) {
                client_link_pairing_confirm();
        }

        else if (has_changed & button_state & PAIRING_REJ_BUTTON) {
                client_link_pairing_reject();
        }

        if (has_changed & button_state & PAIRING_BUTTON) {
                client_link_set_pairing_start();
        }
}

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

        err = client_link_init();
        if (err) {
                LOG_ERR("Failed to initialize client link (err: %d)", err);
                return -1;
        }

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

        err = client_link_start();
        if (err) {
                LOG_ERR("Failed to start advertising (err: %d)", err);
                return -1;
        }

}