#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

static const char *TAG = "LDR_LED";

#define LDR_ADC_UNIT ADC_UNIT_1
#define LDR_ADC_CHANNEL ADC_CHANNEL_3
#define LED_GPIO GPIO_NUM_5

#define SMA_WINDOW_SIZE 10
#define DARK_THRESHOLD 3000
#define LIGHT_THRESHOLD 3700

static int sma_buffer[SMA_WINDOW_SIZE] = {0};
static int sma_index = 0;
static int sma_sum = 0;
static int sma_count = 0;

static int sma_update(int new_value)
{
    sma_sum -= sma_buffer[sma_index];

    sma_buffer[sma_index] = new_value;
    sma_sum += new_value;

    sma_index = (sma_index + 1) % SMA_WINDOW_SIZE;

    if (sma_count < SMA_WINDOW_SIZE)
    {
        sma_count++;
    }

    return sma_sum / sma_count;
}

static adc_oneshot_unit_handle_t adc1_init(void)
{
    adc_oneshot_unit_handle_t adc1_handle;

    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = LDR_ADC_UNIT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc1_handle));

    adc_oneshot_chan_cfg_t chan_config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, LDR_ADC_CHANNEL, &chan_config));

    return adc1_handle;
}

static void led_init(void)
{
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_GPIO, 0);
}

void app_main(void)
{
    adc_oneshot_unit_handle_t adc1_handle = adc1_init();
    led_init();

    bool led_state = false;

    ESP_LOGI(TAG, "Старт: DARK_THRESHOLD=%d, LIGHT_THRESHOLD=%d, SMA_WINDOW=%d",
             DARK_THRESHOLD, LIGHT_THRESHOLD, SMA_WINDOW_SIZE);

    while (1)
    {
        int raw_value = 0;

        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, LDR_ADC_CHANNEL, &raw_value));

        int filtered_value = sma_update(raw_value);

        if (!led_state && filtered_value < DARK_THRESHOLD)
        {
            led_state = true;
            gpio_set_level(LED_GPIO, 1);
            ESP_LOGI(TAG, "Стало темно -> LED увімкнено (SMA=%d)", filtered_value);
        }
        else if (led_state && filtered_value > LIGHT_THRESHOLD)
        {
            led_state = false;
            gpio_set_level(LED_GPIO, 0);
            ESP_LOGI(TAG, "Стало світло -> LED вимкнено (SMA=%d)", filtered_value);
        }

        ESP_LOGI(TAG, "raw=%4d  sma=%4d  led=%s", raw_value, filtered_value,
                 led_state ? "ON" : "OFF");

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}