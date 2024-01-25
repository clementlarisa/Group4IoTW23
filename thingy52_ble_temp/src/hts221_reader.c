#include <zephyr/drivers/sensor.h>

#include "hts221_reader.h"

extern const struct device *hts221_dev;

int hts221_get_sample(double *temperature)
{
	int err;

	if ((err = sensor_sample_fetch(hts221_dev)) != 0) {
		return err;
	}

	struct sensor_value temp_sensor_value;
	if ((err = sensor_channel_get(hts221_dev, SENSOR_CHAN_AMBIENT_TEMP, &temp_sensor_value)) !=
	    0) {
		return err;
	}

	*temperature = sensor_value_to_double(&temp_sensor_value);
	// *humidity = sensor_value_to_double(&humidity_sensor_value);
	return 0;
}