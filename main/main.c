#include <stdio.h>
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "interrupt_example";
const int BTN_PIN = 4;
int button_pressed = 0;
static volatile bool button_pressed_flag = false;
unsigned long last_press_time = 0;
const unsigned long DEBOUNCE_TIME_MS = 50;

static void IRAM_ATTR button_isr_handler(void *arg)
{
    (void)arg;
    button_pressed_flag = true;
}

static void setup_button_interrupt(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BTN_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&io_conf);
    gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    gpio_isr_handler_add(BTN_PIN, button_isr_handler, NULL);
}

static inline uint32_t millis(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

void app_main(void)
{
    setup_button_interrupt();

    while (1)
    {
        if (button_pressed_flag)
        {
            button_pressed_flag = false;

            vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_TIME_MS));

            if (gpio_get_level(BTN_PIN) == 0)
            {
                button_pressed++;
                ESP_LOGI(TAG, "Button pressed %d times", button_pressed);
            }
        }

        vTaskDelay(1);
    }
}