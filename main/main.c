#include <stdio.h>
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "LED";

typedef struct
{
    gpio_num_t pin;
    uint32_t period;
    uint32_t lastTime;
    bool state;
} Led;

static inline uint32_t millis(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

void app_main(void)
{
    Led leds[] =
        {
            {GPIO_NUM_4, 200, 0, false},
            {GPIO_NUM_5, 500, 0, false},
            {GPIO_NUM_6, 1000, 0, false},
        };

    const int ledCount = sizeof(leds) / sizeof(leds[0]);

    for (int i = 0; i < ledCount; i++)
    {
        gpio_reset_pin(leds[i].pin);
        gpio_set_direction(leds[i].pin, GPIO_MODE_OUTPUT);
    }

    while (1)
    {
        uint32_t now = millis();

        for (int i = 0; i < ledCount; i++)
        {
            if (now - leds[i].lastTime >= leds[i].period)
            {
                leds[i].lastTime = now;
                leds[i].state = !leds[i].state;
                gpio_set_level(leds[i].pin, leds[i].state);
            }
        }

        vTaskDelay(1);
    }
}