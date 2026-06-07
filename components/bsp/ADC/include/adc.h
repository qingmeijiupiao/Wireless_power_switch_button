/*
 * @version: no version
 * @LastEditors: qingmeijiupiao
 * @Description: ADC驱动类，用于读取ADC通道的电压值
 * @author: qingmeijiupiao
 * @LastEditTime: 2026-06-01 18:36:16
 */

#ifndef ADC_H
#define ADC_H
#include "esp_err.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"

class adc_t{
public:
    adc_t(adc_channel_t _channel):adc_channel(_channel), cali_handle(nullptr){};
    ~adc_t(){};
    esp_err_t init();
    esp_err_t read_raw(int& raw);
    esp_err_t read_voltage_mV(int& voltage);
private:
    adc_channel_t adc_channel;
    adc_cali_handle_t cali_handle;
    static adc_oneshot_unit_handle_t adc1_unit_handle;
};

#endif
