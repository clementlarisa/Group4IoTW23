#ifndef __BLE_HTS_HPP__
#define __BLE_HTS_HPP__

// forward declare structs to supress warning messages
struct bt_conn;
struct bt_gatt_attr;

int init_ble_hts();
void ble_ready();
void on_conn_open(struct bt_conn *conn, uint8_t err);
void on_conn_close(struct bt_conn *conn, uint8_t reason);

void temp_measurement_ccc_changed_callback(const struct bt_gatt_attr *attr, uint16_t value);
void intermediate_temp_ccc_changed_callback(const struct bt_gatt_attr *attr, uint16_t value);
void do_indicate_temperture_measurement(double *temp);
void do_notify_intermediate_temperature(double *temp);

void humidity_measurement_ccc_changed_callback(const struct bt_gatt_attr *attr, uint16_t value);
void do_indicate_humidity_measurement(double *temp);
// void do_notify_intermediate_humidity(double *temp);

#endif
