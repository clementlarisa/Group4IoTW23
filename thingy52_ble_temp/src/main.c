#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>

#include <zephyr/logging/log.h>

#include "status_blinker.h"
#include "ble_hts.h"
#include "hts221_reader.h"

#include "stdio.h"

#define IND_NOTY_PERIOD 5000 // 5 seconds

//  Get device tree data for status leds and hts221 sensor
#define LED_R_NODE  DT_NODELABEL(led0)
#define LED_G_NODE  DT_NODELABEL(led1)
#define HTS221_NODE DT_NODELABEL(hts221)
#if (DT_NODE_HAS_STATUS(LED_R_NODE, okay) && DT_NODE_HAS_STATUS(LED_G_NODE, okay) &&               \
     DT_NODE_HAS_STATUS(HTS221_NODE, okay))

struct gpio_dt_spec led_r = GPIO_DT_SPEC_GET(LED_R_NODE, gpios);
struct gpio_dt_spec led_g = GPIO_DT_SPEC_GET(LED_G_NODE, gpios);
const struct device *hts221_dev = DEVICE_DT_GET(HTS221_NODE);

#else
#error "Failed to get the node ids of the devices from the device tree, check their statuses"
#endif

struct gpio_dt_spec *led_to_blink = NULL;
K_MUTEX_DEFINE(led_mutex);

// boolean flag that is set when a remote enddpoint is subscribed
// to Temperature Measurement characteristic indication
// or Intermediate Temperature chracteristic notificateion
// of HT service or Intermeditace
extern bool do_indicate_tm_char;
extern bool do_notify_it_char;
extern bool do_indicate_hum_char;

int main()
{
	int err;

	// Initialize status led, start status update thread
	err = init_status_blinker();
	if (err) {
		printk("Status blinker initialization failed (err %d)\n", err);
		return err;
	}

	if (!device_is_ready(hts221_dev)) {
		printk("HTS 221 initialization failed (err %d)\n", err);
		return err;
	}

	// Initialize ble module
	err = init_ble_hts();
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return err;
	}

	while (true) {

		printk("Bluetooth initialized\n");

		if (do_indicate_tm_char || do_notify_it_char || do_indicate_hum_char) {
			double temp, humidity;
			if ((err = hts221_get_sample(&temp)) != 0) {
				printk("hts221_get_sample failed (err %d)\n", err);
				return err;
			}

			printk("Temperature: %f\n", temp);
			printk("Humidity: %f\n", humidity);

			if (do_indicate_tm_char) {
				do_indicate_temperture_measurement(&temp);
			}
			if (do_notify_it_char) {
				do_notify_intermediate_temperature(&temp);
			}
			if (do_indicate_hum_char) {
				do_indicate_humidity_measurement(&humidity);
			}
		}

		k_msleep(IND_NOTY_PERIOD);
	}

	return 0;
}