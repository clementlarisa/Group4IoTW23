#include <zephyr/drivers/gpio.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/sys/byteorder.h>

#include "hts221_reader.h"
#include "ble_hts.h"

// Used across multiple modules, defined in main.cpp
extern struct gpio_dt_spec led_r;
extern struct gpio_dt_spec led_g;
extern struct gpio_dt_spec *led_to_blink;
extern struct k_mutex led_mutex;

// Flags that are set when a client subscribes to notifications/inidcations
bool do_indicate_tm_char = false;
bool do_notify_it_char = false;

bool do_indicate_hum_char = false;

// Define on connection open/close callbacks
BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = on_conn_open,
	.disconnected = on_conn_close,
};

// Advertisment data with flats general dicoverable + no BREDR (=only ble) and Health Thermometer
// Service
static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(BT_UUID_HTS_VAL))};

// BLE characteristic read callback
static ssize_t read_temp_request(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
				 uint16_t len, uint16_t offset)
{
	double temp, humidity;

	int err;
	if ((err = hts221_get_sample(&temp)) != 0) {
		printk("hts221_get_sample failed (err %d)\n", err);
		return err;
	}
	static uint8_t data[5];
	encode_temp_char(&temp, data);

	return bt_gatt_attr_read(conn, attr, buf, len, offset, &data, sizeof(data));
}

// BLE characteristic read callback
static ssize_t read_humidity_request(struct bt_conn *conn, const struct bt_gatt_attr *attr,
				     void *buf, uint16_t len, uint16_t offset)
{
	double temp, humidity;

	int err;
	if ((err = hts221_get_sample(&temp)) != 0) {
		printk("hts221_get_sample failed (err %d)\n", err);
		return err;
	}
	const uint8_t hum = (uint8_t)humidity;
	uint8_t data[2] = {hum, 0};
	// encode_temp_char(&temp, data);

	return bt_gatt_attr_read(conn, attr, buf, len, offset, &data, sizeof(data));
}

// UUID for optional Intemediate Teperature characteristic
#define BT_UUID_INTERMEDIATE_TEMP_VAL 0x2a1e
#define BT_UUID_INTERMEDIATE_TEMP     BT_UUID_DECLARE_16(BT_UUID_INTERMEDIATE_TEMP_VAL)

/* Health Thermometer Service Declaration */
BT_GATT_SERVICE_DEFINE(
	hts_service, BT_GATT_PRIMARY_SERVICE(BT_UUID_HTS),

	// Mandatory: Temperature Measurement characteristic with possibility to read it
	BT_GATT_CHARACTERISTIC(BT_UUID_HTS_MEASUREMENT, BT_GATT_CHRC_INDICATE | BT_GATT_CHRC_READ,
			       BT_GATT_PERM_READ, read_temp_request, NULL, NULL),
	BT_GATT_CCC(temp_measurement_ccc_changed_callback, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

	// // Mandatory: Humidity Measurement characteristic with possibility to read it
	// BT_GATT_CHARACTERISTIC(BT_UUID_HUMIDITY, BT_GATT_CHRC_INDICATE | BT_GATT_CHRC_READ,
	// 		       BT_GATT_PERM_READ, read_humidity_request, NULL, NULL),
	// BT_GATT_CCC(humidity_measurement_ccc_changed_callback,
	// 	    BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

	// // Optional: Intermediate Temperature
	BT_GATT_CHARACTERISTIC(BT_UUID_INTERMEDIATE_TEMP, BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_NONE,
			       NULL, NULL, NULL),
	BT_GATT_CCC(intermediate_temp_ccc_changed_callback,
		    BT_GATT_PERM_READ | BT_GATT_PERM_WRITE));

int init_ble_hts()
{
	int err;

	err = bt_enable(ble_ready);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return err;
	}

	return 0;
}

void ble_ready()
{
	// Begin advertising
	int err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
		return;
	} else {
		printk("Started advertising.\n");
	}
}

void on_conn_open(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		printk("Connection failed (err 0x%02x)\n", err);
	} else {
		printk("Connected\n");
	}

	// Lock the led mutex, change blinking led to green, unlock mutex
	k_mutex_lock(&led_mutex, K_FOREVER);
	gpio_pin_set(led_to_blink->port, led_to_blink->pin, 0);
	led_to_blink = &led_g;
	k_mutex_unlock(&led_mutex);
}

void on_conn_close(struct bt_conn *conn, uint8_t reason)
{
	printk("Disconnected (reason 0x%02x)\n", reason);

	do_indicate_tm_char = false;
	do_notify_it_char = false;

	// Lock the led mutex, change blinking led to red, unlock mutex
	k_mutex_lock(&led_mutex, K_FOREVER);
	gpio_pin_set(led_to_blink->port, led_to_blink->pin, 0);
	led_to_blink = &led_r;
	k_mutex_unlock(&led_mutex);
}

void temp_measurement_ccc_changed_callback(const struct bt_gatt_attr *attr, uint16_t value)
{
	do_indicate_tm_char = value == BT_GATT_CCC_INDICATE;
}

void humidity_measurement_ccc_changed_callback(const struct bt_gatt_attr *attr, uint16_t value)
{
	do_indicate_hum_char = value == BT_GATT_CCC_INDICATE;
}

void intermediate_temp_ccc_changed_callback(const struct bt_gatt_attr *attr, uint16_t value)
{
	do_notify_it_char = value == BT_GATT_CCC_NOTIFY;
}

void encode_temp_char(double *temp, char *data)
{
	uint32_t mantissa;
	uint8_t exponent;
	mantissa = (uint32_t)(*temp * 100);
	exponent = (uint8_t)-2;

	// Whole flags byte set to 0:
	// bit 0 - celsius, bit 1:7 - no time stamp, no temp type, reserverd bits set to 0
	data[0] = 0;

	// med float: 1 byte exponent, 4 bytes mantissa
	sys_put_le24(mantissa, (uint8_t *)&data[1]);
	data[4] = exponent;
}

void do_indicate_temperture_measurement(double *temp)
{
	static uint8_t data[5];
	encode_temp_char(temp, data);

	static struct bt_gatt_indicate_params ind_params;
	// CCC attribute pointer (see line 36)
	ind_params.attr = &hts_service.attrs[2];
	// data to indicate
	ind_params.data = &data;
	ind_params.len = sizeof(data);
	// we don't use the Indicate Value callback / Indicate operation complete callback
	// so they remain unitialzied

	bt_gatt_indicate(NULL, &ind_params);
}

void do_notify_intermediate_temperature(double *temp)
{
	static uint8_t data[5];
	encode_temp_char(temp, data);

	// second parameter is out CCC attribute pointer (see line 36)
	bt_gatt_notify(NULL, &hts_service.attrs[4], &data, sizeof(data));
}

void do_indicate_humidity_measurement(double *humidity)
{
	uint8_t data[2] = {(uint8_t)humidity, 0};

	// encode_temp_char(temp, data);

	static struct bt_gatt_indicate_params ind_params;
	// CCC attribute pointer (see line 36)
	ind_params.attr = &hts_service.attrs[2];
	// data to indicate
	ind_params.data = &data;
	ind_params.len = sizeof(data);
	// we don't use the Indicate Value callback / Indicate operation complete callback
	// so they remain unitialzied

	bt_gatt_indicate(NULL, &ind_params);
}