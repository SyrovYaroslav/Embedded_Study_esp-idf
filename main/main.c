#include <stdio.h>
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "interrupt_example";
const int BTN_PIN = 4;
int button_pressed = 0;
const unsigned long DEBOUNCE_TIME_MS = 50;
const unsigned long POLL_INTERVAL_MS = 10;
static unsigned long state_enter_time = 0;

typedef enum
{
    BTN_IDLE,
    BTN_DEBOUNCE_PRESS,
    BTN_PRESSED,
    BTN_DEBOUNCE_RELEASE
} button_state_t;

static button_state_t state = BTN_IDLE;

static void setup_pin(void)
{
    gpio_reset_pin(BTN_PIN);
    gpio_set_direction(BTN_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BTN_PIN, GPIO_PULLUP_ONLY);
}

static inline uint32_t millis(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static void update_button_state(void)
{
    int level = gpio_get_level(BTN_PIN);
    uint32_t now = millis();

    switch (state)
    {
    case BTN_IDLE:
        if (level == 0)
        {
            state = BTN_DEBOUNCE_PRESS;
            state_enter_time = now;
        }
        break;

    case BTN_DEBOUNCE_PRESS:
        if (level != 0)
        {
            state = BTN_IDLE;
        }
        else if (now - state_enter_time >= DEBOUNCE_TIME_MS)
        {
            button_pressed++;
            ESP_LOGI(TAG, "Button pressed %d times", button_pressed);
            state = BTN_PRESSED;
        }
        break;

    case BTN_PRESSED:
        if (level != 0)
        {
            state = BTN_DEBOUNCE_RELEASE;
            state_enter_time = now;
        }
        break;

    case BTN_DEBOUNCE_RELEASE:
        if (level == 0)
        {
            state = BTN_PRESSED;
        }
        else if (now - state_enter_time >= DEBOUNCE_TIME_MS)
        {
            state = BTN_IDLE;
        }
        break;
    }
}

void app_main(void)
{
    setup_pin();

    while (1)
    {
        update_button_state();
        vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL_MS));
    }
}