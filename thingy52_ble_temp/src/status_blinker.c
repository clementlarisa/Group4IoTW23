#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include "status_blinker.h"

#define STATUS_BLINKER_STACKSIZE 1024   // Thread stack size
#define STATUS_BLINKER_PRIORITY 7       // Thread priority
#define STATUS_BLINKER_SLEEPTIME 500    // Blinker thread sleep time (=period)

// Used across multiple modules, defined in main.cpp
extern struct gpio_dt_spec  led_r;
extern struct gpio_dt_spec  led_g;
extern struct gpio_dt_spec* led_to_blink;
extern struct k_mutex       led_mutex;

// Define blinker thread stack and thread data struct
K_THREAD_STACK_DEFINE(status_blinker_stack, STATUS_BLINKER_STACKSIZE);
static struct k_thread status_blinker_thr_data;

int init_status_blinker()
{
    int err;

    // Configure red and green leds as output+inactive
    if((err = gpio_pin_configure_dt(&led_r, GPIO_OUTPUT_INACTIVE)) != 0 ||
       (err = gpio_pin_configure_dt(&led_g, GPIO_OUTPUT_INACTIVE)) != 0)
    {
        return err;
    }

    // Set the led to blink to red -> we're not connected yet
    led_to_blink = &led_r;

    // Create and start the blinker thread with blink() acting as entry point
    k_thread_create(&status_blinker_thr_data, status_blinker_stack,
            K_THREAD_STACK_SIZEOF(status_blinker_stack),
            blink, NULL, NULL, NULL,
            STATUS_BLINKER_PRIORITY, 0, K_FOREVER);
    k_thread_start(&status_blinker_thr_data);
    return 0;
}

void blink(void)
{
    while(true)
    {
        // Lock the led mutex, toggle led, unlock mutex and go to sleep
        k_mutex_lock(&led_mutex, K_FOREVER);
        gpio_pin_toggle(led_to_blink->port, led_to_blink->pin);
        k_mutex_unlock(&led_mutex);
        k_msleep(STATUS_BLINKER_SLEEPTIME);
    }
}