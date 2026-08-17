#include <stdio.h>
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

static const char *TAG = "LED";

#define GO_TIME_MS (10000)
#define CHANGE_TIME_MS (3000)
#define STOP_TIME_MS (10000)

#define GREEN_BLINK_TOTAL_MS (3000)
#define GREEN_BLINK_INTERVAL_MS (500)
#define GREEN_BLINK_TICKS (GREEN_BLINK_TOTAL_MS / GREEN_BLINK_INTERVAL_MS)

#define GREEN_SOLID_MS (GO_TIME_MS - GREEN_BLINK_TOTAL_MS)

#define BUZZER_INTERVAL_SOLID_MS (500)
#define BUZZER_INTERVAL_BLINK_MS (250)

#define EMERGENCY_BLINK_INTERVAL_MS (500)
#define BUTTON_DEBOUNCE_MS (50)

enum
{
    GREEN_LED = GPIO_NUM_4,
    YELLOW_LED = GPIO_NUM_5,
    RED_LED = GPIO_NUM_6,
    TOUCH_SENSOR_PIN = GPIO_NUM_15,
    BUZZER_PIN = GPIO_NUM_7,
    BUTTON_PIN = GPIO_NUM_16,
};

typedef enum
{
    TRAFFIC_IDLE,
    TRAFFIC_GO,
    TRAFFIC_GO_BLINK,
    TRAFFIC_CHANGE_TO_STOP,
    TRAFFIC_STOP,
    TRAFFIC_CHANGE_TO_GO,
    TRAFFIC_EMERGENCY,
} trafficlight_state_t;

static trafficlight_state_t current_state = TRAFFIC_IDLE;
static esp_timer_handle_t traffic_light_timer = NULL;
static esp_timer_handle_t blink_timer = NULL;
static esp_timer_handle_t buzzer_timer = NULL;
static esp_timer_handle_t emergency_blink_timer = NULL;

static int blink_ticks_left = 0;
static bool blink_led_on = false;
static bool buzzer_on = false;
static bool emergency_led_on = false;

static volatile bool button_event_flag = false;

static void update_led(int green, int yellow, int red)
{
    gpio_set_level(GREEN_LED, green);
    gpio_set_level(YELLOW_LED, yellow);
    gpio_set_level(RED_LED, red);
}

static void set_state(trafficlight_state_t new_state);
static void start_green_blink(void);

static void buzzer_timer_callback(void *arg)
{
    buzzer_on = !buzzer_on;
    gpio_set_level(BUZZER_PIN, buzzer_on ? 1 : 0);
}

static void start_buzzer(uint32_t interval_ms)
{
    esp_timer_stop(buzzer_timer);
    buzzer_on = false;
    gpio_set_level(BUZZER_PIN, 0);
    esp_timer_start_periodic(buzzer_timer, (uint64_t)interval_ms * 1000);
}

static void stop_buzzer(void)
{
    esp_timer_stop(buzzer_timer);
    gpio_set_level(BUZZER_PIN, 0);
}

static void blink_timer_callback(void *arg)
{
    blink_led_on = !blink_led_on;
    update_led(blink_led_on ? 1 : 0, 0, 0);

    blink_ticks_left--;
    if (blink_ticks_left <= 0)
    {
        esp_timer_stop(blink_timer);
        stop_buzzer();
        set_state(TRAFFIC_CHANGE_TO_STOP);
    }
}

static void start_green_blink(void)
{
    current_state = TRAFFIC_GO_BLINK;
    blink_ticks_left = GREEN_BLINK_TICKS;
    blink_led_on = true;
    update_led(1, 0, 0);

    ESP_LOGI(TAG, "State -> GO_BLINK, %d ticks x %d ms", blink_ticks_left, GREEN_BLINK_INTERVAL_MS);
    esp_timer_start_periodic(blink_timer, GREEN_BLINK_INTERVAL_MS * 1000);
    start_buzzer(BUZZER_INTERVAL_BLINK_MS);
}

static void traffic_light_timer_callback(void *arg)
{
    switch (current_state)
    {
    case TRAFFIC_GO:
        start_green_blink();
        break;
    case TRAFFIC_CHANGE_TO_STOP:
        set_state(TRAFFIC_STOP);
        break;
    case TRAFFIC_STOP:
        set_state(TRAFFIC_IDLE);
        break;
    case TRAFFIC_CHANGE_TO_GO:
        set_state(TRAFFIC_GO);
        break;
    default:
        break;
    }
}

static void set_state(trafficlight_state_t new_state)
{
    current_state = new_state;
    uint64_t duration_ms = 0;

    switch (new_state)
    {
    case TRAFFIC_GO:
        update_led(1, 0, 0);
        duration_ms = GREEN_SOLID_MS;
        start_buzzer(BUZZER_INTERVAL_SOLID_MS);
        break;
    case TRAFFIC_CHANGE_TO_STOP:
        update_led(0, 1, 0);
        duration_ms = CHANGE_TIME_MS;
        break;
    case TRAFFIC_STOP:
        update_led(0, 0, 1);
        duration_ms = STOP_TIME_MS;
        break;
    case TRAFFIC_CHANGE_TO_GO:
        update_led(0, 1, 1);
        duration_ms = CHANGE_TIME_MS;
        break;
    default:
        return;
    }

    ESP_LOGI(TAG, "State -> %d, next in %llu ms", (int)new_state, duration_ms);
    esp_timer_start_once(traffic_light_timer, duration_ms * 1000);
}

static void emergency_blink_timer_callback(void *arg)
{
    emergency_led_on = !emergency_led_on;
    update_led(0, emergency_led_on ? 1 : 0, 0);
}

static void handle_button_press(void)
{
    if (current_state != TRAFFIC_EMERGENCY)
    {
        ESP_LOGI(TAG, "EMERGENCY: button pressed -> stopping timers, entering EMERGENCY");

        esp_timer_stop(traffic_light_timer);
        esp_timer_stop(blink_timer);
        stop_buzzer();

        current_state = TRAFFIC_EMERGENCY;
        emergency_led_on = true;
        update_led(0, 1, 0);
        esp_timer_start_periodic(emergency_blink_timer, EMERGENCY_BLINK_INTERVAL_MS * 1000);
    }
    else
    {
        ESP_LOGI(TAG, "EMERGENCY: button pressed again -> returning to IDLE");

        esp_timer_stop(emergency_blink_timer);
        update_led(0, 0, 1);
        current_state = TRAFFIC_IDLE;
    }
}

static void IRAM_ATTR button_isr_handler(void *arg)
{
    button_event_flag = true;
}

static void check_button_event(void)
{
    if (!button_event_flag)
    {
        return;
    }
    button_event_flag = false;

    vTaskDelay(pdMS_TO_TICKS(BUTTON_DEBOUNCE_MS));

    if (gpio_get_level(BUTTON_PIN) == 0)
    {
        handle_button_press();
    }
}

static void setup_pins(void)
{
    gpio_reset_pin(GREEN_LED);
    gpio_set_direction(GREEN_LED, GPIO_MODE_OUTPUT);

    gpio_reset_pin(YELLOW_LED);
    gpio_set_direction(YELLOW_LED, GPIO_MODE_OUTPUT);

    gpio_reset_pin(RED_LED);
    gpio_set_direction(RED_LED, GPIO_MODE_OUTPUT);

    update_led(0, 0, 1);

    gpio_reset_pin(BUZZER_PIN);
    gpio_set_direction(BUZZER_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(BUZZER_PIN, 0);

    gpio_reset_pin(TOUCH_SENSOR_PIN);
    gpio_set_direction(TOUCH_SENSOR_PIN, GPIO_MODE_INPUT);

    gpio_reset_pin(BUTTON_PIN);
    gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BUTTON_PIN, GPIO_PULLUP_ONLY);
    gpio_set_intr_type(BUTTON_PIN, GPIO_INTR_NEGEDGE);
}

static void setup_button_interrupt(void)
{
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_PIN, button_isr_handler, NULL);
}

void app_main(void)
{
    setup_pins();

    const esp_timer_create_args_t timer_args = {
        .callback = &traffic_light_timer_callback,
        .name = "traffic_light_timer",
    };
    esp_timer_create(&timer_args, &traffic_light_timer);

    const esp_timer_create_args_t blink_args = {
        .callback = &blink_timer_callback,
        .name = "green_blink_timer",
    };
    esp_timer_create(&blink_args, &blink_timer);

    const esp_timer_create_args_t buzzer_args = {
        .callback = &buzzer_timer_callback,
        .name = "buzzer_timer",
    };
    esp_timer_create(&buzzer_args, &buzzer_timer);

    const esp_timer_create_args_t emergency_args = {
        .callback = &emergency_blink_timer_callback,
        .name = "emergency_blink_timer",
    };
    esp_timer_create(&emergency_args, &emergency_blink_timer);

    setup_button_interrupt();

    ESP_LOGI(TAG, "Waiting for sensor input...");

    while (1)
    {
        check_button_event();

        if (current_state == TRAFFIC_IDLE)
        {
            int sensor_check = gpio_get_level(TOUCH_SENSOR_PIN);
            if (sensor_check == 1)
            {
                ESP_LOGI(TAG, "Sensor is active, starting cycle");
                set_state(TRAFFIC_CHANGE_TO_GO);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}