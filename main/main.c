#include <stdio.h>
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gptimer.h"
#include "esp_task_wdt.h"

static const char *TAG = "Timer Example";

#define RELE_PIN GPIO_NUM_4
#define HOUR_US (60ULL * 60 * 1000000)
#define RELE_WORKING_US (15ULL * 60 * 1000000)

static gptimer_handle_t hour_timer;
static gptimer_handle_t off_timer;

static volatile bool rele_on_flag = false;
static volatile bool rele_off_flag = false;

static bool IRAM_ATTR off_timer_callback(
    gptimer_handle_t timer,
    const gptimer_alarm_event_data_t *edata,
    void *user_ctx)
{
    gpio_set_level(RELE_PIN, 0);
    rele_off_flag = true;
    return false;
}

static bool IRAM_ATTR hour_timer_callback(
    gptimer_handle_t timer,
    const gptimer_alarm_event_data_t *edata,
    void *user_ctx)
{
    gpio_set_level(RELE_PIN, 1);
    rele_on_flag = true;

    gptimer_stop(off_timer);
    gptimer_set_raw_count(off_timer, 0);

    gptimer_alarm_config_t alarm_cfg = {
        .alarm_count = RELE_WORKING_US,
        .reload_count = 0,
        .flags.auto_reload_on_alarm = false,
    };
    gptimer_set_alarm_action(off_timer, &alarm_cfg);

    gptimer_start(off_timer);

    return false;
}

void setup_pins(void)
{
    gpio_reset_pin(RELE_PIN);
    gpio_set_direction(RELE_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(RELE_PIN, 0);
}

void wotchdog_setup(void)
{
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = 2000,
        .idle_core_mask = 0,
        .trigger_panic = true,
    };
    esp_task_wdt_init(&wdt_config);
    esp_task_wdt_add(NULL);
}

static gptimer_handle_t create_timer(
    gptimer_alarm_cb_t callback,
    uint64_t alarm_us,
    bool auto_reload)
{
    gptimer_handle_t timer = NULL;

    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000,
    };

    gptimer_new_timer(&timer_config, &timer);

    gptimer_event_callbacks_t callbacks = {
        .on_alarm = callback,
    };

    gptimer_register_event_callbacks(timer, &callbacks, NULL);

    gptimer_alarm_config_t alarm_config = {
        .alarm_count = alarm_us,
        .reload_count = 0,
        .flags.auto_reload_on_alarm = auto_reload,
    };

    gptimer_set_alarm_action(timer, &alarm_config);

    gptimer_enable(timer);

    return timer;
}

void app_main(void)
{
    setup_pins();
    wotchdog_setup();

    hour_timer = create_timer(
        hour_timer_callback,
        HOUR_US,
        true);

    off_timer = create_timer(
        off_timer_callback,
        RELE_WORKING_US,
        false);

    gptimer_start(hour_timer);

    while (1)
    {
        if (rele_on_flag)
        {
            rele_on_flag = false;
            ESP_LOGI(TAG, "Rele turned on");
        }
        if (rele_off_flag)
        {
            rele_off_flag = false;
            ESP_LOGI(TAG, "Rele turned off");
        }
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}