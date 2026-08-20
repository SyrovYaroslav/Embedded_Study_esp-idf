#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

static const char *TAG = "ADC_POT";

#define ADC_UNIT_USED ADC_UNIT_1
#define ADC_CHANNEL_USED ADC_CHANNEL_3
#define ADC_ATTEN_USED ADC_ATTEN_DB_12
#define ADC_BITWIDTH_USED ADC_BITWIDTH_DEFAULT
#define ADC_BITWIDTH_MANUAL ADC_BITWIDTH_12

#define V_REF_MV 3300.0f
#define ADC_MAX_RAW ((1 << ADC_BITWIDTH_MANUAL) - 1)

#define SAMPLE_COUNT 100
#define SAMPLE_PERIOD_MS 100

static adc_oneshot_unit_handle_t adc_handle;
static adc_cali_handle_t cali_handle = NULL;
static bool cali_enabled = false;

static bool adc_calibration_init(adc_unit_t unit, adc_channel_t channel,
                                 adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

    if (!calibrated)
    {
        ESP_LOGI(TAG, "Калібрування схемою Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .chan = channel,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_USED,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK)
        {
            calibrated = true;
        }
    }

    *out_handle = handle;
    if (calibrated)
    {
        ESP_LOGI(TAG, "Калібрування АЦП успішне");
    }
    else
    {
        ESP_LOGW(TAG, "Калібрування АЦП недоступне (eFuse не запрограмовано)");
    }
    return calibrated;
}

void app_main(void)
{
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_USED,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

    adc_oneshot_chan_cfg_t chan_config = {
        .atten = ADC_ATTEN_USED,
        .bitwidth = ADC_BITWIDTH_USED,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_USED, &chan_config));

    cali_enabled = adc_calibration_init(ADC_UNIT_USED, ADC_CHANNEL_USED,
                                        ADC_ATTEN_USED, &cali_handle);

    int max_raw = ADC_MAX_RAW;

    ESP_LOGI(TAG, "Vref(manual)=%.0f mV, Atten=%d, MaxRaw=%d",
             V_REF_MV, ADC_ATTEN_USED, max_raw);

    printf("\n RAW   U_manual(mV)   U_cali(mV)   Error(%%)\n");
    printf("------------------------------------------\n");

    for (int i = 0; i < SAMPLE_COUNT; i++)
    {
        int raw = 0;
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL_USED, &raw));
        float u_manual = (float)raw * V_REF_MV / (float)max_raw;

        int u_cali_mv = 0;
        float error_pct = 0.0f;

        if (cali_enabled)
        {
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(cali_handle, raw, &u_cali_mv));
            if (u_cali_mv != 0)
            {
                error_pct = fabsf(u_manual - (float)u_cali_mv) / (float)u_cali_mv * 100.0f;
            }
            printf("%4d      %7.1f        %4d        %5.2f\n",
                   raw, u_manual, u_cali_mv, error_pct);
        }
        else
        {
            printf("%4d      %7.1f         --           --\n", raw, u_manual);
        }

        vTaskDelay(pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
    }
}