#include "esp_log.h"
#include "esp_err.h"

#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"


static const char *TAG = "ADC SINGLE";

// ADC1_CHANNEL_2 = GPIO2 on STM32-C3 according to
// https://github.com/espressif/esp-idf/blob/master/components/driver/deprecated/driver/adc_types_legacy.h#L68C1-L68C56
// Newer headers don't know about ADC1_CHANNEL2_2, ADC_CHANNEL_2 seems to work
#define ADC_CHANNEL ADC_CHANNEL_2

// Attenuation 3.55x according to
// https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32/api-reference/peripherals/adc.html#_CPPv411adc_atten_t
// Added to the voltage divider by half on the PCB, the max voltage is 800 mV * 3.55 * 2 = 5.68 V
// Next smaller max voltage would be 800 mV * 2 * 2 = 3.20 V, which is too low
#define ADC_ATTEN ADC_ATTEN_DB_11

//static esp_adc_cal_characteristics_t adc_chars;

adc_oneshot_unit_handle_t adc1_handle;

adc_cali_handle_t adc_cali_handle;

esp_err_t battery_init(void)
{
    //ADC1 config
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    adc_oneshot_chan_cfg_t adc_config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL, &adc_config));

    ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");

    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_config, &adc_cali_handle));


    /*
     * This is a legacy implementation I thought I had committed before
     * I'll leave it here for now just in case

    //esp_adc_cal_characteristics_t *adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN,
            ADC_WIDTH_BIT_DEFAULT, ESP_ADC_CAL_VAL_EFUSE_VREF, &adc_chars);
    //Check type of calibration value used to characterize ADC
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        ESP_LOGI(TAG, "eFuse Vref");
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        ESP_LOGI(TAG, "Two Point");
    } else {
        ESP_LOGI(TAG, "Default");
    }
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN, ADC_WIDTH_BIT_DEFAULT, 0, &adc_chars);
    */
    return 0;
}

int battery_voltage_mv(void)
{
    // Average some values
    /*
    uint32_t sum = 0;
    static const int ROUNDS = 16;
    for (int i = 0; i < ROUNDS; i++)
    {
        sum += adc1_get_raw(ADC_CHANNEL);
    }
    sum = sum / ROUNDS;
    // undo the attenuation and voltage divider
    sum = (sum * 355 * 2) / 100;
    return sum;
    */

    /*
    uint32_t reading =  adc1_get_raw(ADC_CHANNEL);
    uint32_t voltage = esp_adc_cal_raw_to_voltage(reading, &adc_chars);
    */

    int adc_raw, voltage;
    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL, &adc_raw));
    ESP_LOGI(TAG, "ADC%d Channel[%d] Raw Data: %d", ADC_UNIT_1 + 1, ADC_CHANNEL, adc_raw);

    ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc_cali_handle, adc_raw, &voltage));
    ESP_LOGI(TAG, "ADC%d Channel[%d] Cali Voltage: %d mV", ADC_UNIT_1 + 1, ADC_CHANNEL, voltage);

    // Voltage divired on pcb splits it in half.
    voltage *= 2.0;

    // Voltage is not perfect, multimeter measured 5.28, this function reported 5574 mV
    return voltage;
}
