#include <stdio.h>
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "interrupt_example";
const int BTN_PIN = 4;
static volatile int button_pressed = 0;
static volatile bool button_pressed_flag = false;

static void IRAM_ATTR button_isr_handler(void *arg)
{
    (void)arg;
    button_pressed++;
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

void app_main(void)
{
    setup_button_interrupt();

    while (1)
    {
        if (button_pressed_flag)
        {
            ESP_LOGI(TAG, "Button pressed %d times", button_pressed);
            button_pressed_flag = false;
        }
        vTaskDelay(1);
    }
}