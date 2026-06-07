/*
 * @version: 1.0
 * @LastEditors: qingmeijiupiao
 * @Description: GPIO3 电池电压采集与 GPIO2 低功耗门控实现
 * @author: qingmeijiupiao
 * @LastEditTime: 2026-06-07 13:33:53
 */
#include "battery_voltage.h"

#include "adc.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

namespace BatteryVoltage {
namespace {

constexpr gpio_num_t SAMPLE_ENABLE_GPIO = GPIO_NUM_2;
constexpr adc_channel_t BATTERY_ADC_CHANNEL = ADC_CHANNEL_3;
constexpr int VOLTAGE_DIVIDER_RATIO = 2;
constexpr uint32_t STABILITY_POLL_MS = 1;
constexpr uint32_t STABILITY_TIMEOUT_MS = 30;
constexpr int STABILITY_THRESHOLD_MV = 25;
constexpr uint32_t REQUIRED_STABLE_READINGS = 4;
constexpr uint32_t AVERAGE_SAMPLE_COUNT = 16;
constexpr uint32_t AVERAGE_SAMPLE_INTERVAL_MS = 1;

adc_t battery_adc(BATTERY_ADC_CHANNEL);
bool initialized;
bool sampling;
SemaphoreHandle_t state_mutex;
SemaphoreHandle_t completion;
esp_err_t last_result = ESP_ERR_INVALID_STATE;
int last_voltage_mv;
CompletionCallback completion_callback;
void* completion_context;

esp_err_t read_battery_mv_once(int& voltage_mv) {
    int divided_voltage_mv = 0;
    const esp_err_t result = battery_adc.read_voltage_mV(divided_voltage_mv);
    if (result == ESP_OK) {
        voltage_mv = divided_voltage_mv * VOLTAGE_DIVIDER_RATIO;
    }
    return result;
}

esp_err_t wait_until_stable() {
    int previous_mv = 0;
    esp_err_t result = read_battery_mv_once(previous_mv);
    if (result != ESP_OK) {
        return result;
    }

    uint32_t stable_readings = 0;
    for (uint32_t elapsed_ms = STABILITY_POLL_MS;
         elapsed_ms <= STABILITY_TIMEOUT_MS;
         elapsed_ms += STABILITY_POLL_MS) {
        vTaskDelay(pdMS_TO_TICKS(STABILITY_POLL_MS));

        int current_mv = 0;
        result = read_battery_mv_once(current_mv);
        if (result != ESP_OK) {
            return result;
        }

        const int difference_mv =
            current_mv >= previous_mv ? current_mv - previous_mv : previous_mv - current_mv;
        stable_readings =
            difference_mv <= STABILITY_THRESHOLD_MV ? stable_readings + 1 : 0;
        if (stable_readings >= REQUIRED_STABLE_READINGS) {
            return ESP_OK;
        }
        previous_mv = current_mv;
    }
    // The timeout is a maximum settling delay, not a sampling failure.
    // ADC noise may prevent four consecutive readings from meeting the
    // threshold, so continue with the averaged acquisition after 30 ms.
    return ESP_OK;
}

esp_err_t read_average_mv(int& voltage_mv) {
    int64_t voltage_sum_mv = 0;
    for (uint32_t sample = 0; sample < AVERAGE_SAMPLE_COUNT; ++sample) {
        int sample_mv = 0;
        const esp_err_t result = read_battery_mv_once(sample_mv);
        if (result != ESP_OK) {
            return result;
        }
        voltage_sum_mv += sample_mv;
        if (sample + 1 < AVERAGE_SAMPLE_COUNT) {
            vTaskDelay(pdMS_TO_TICKS(AVERAGE_SAMPLE_INTERVAL_MS));
        }
    }

    voltage_mv = static_cast<int>(
        (voltage_sum_mv + AVERAGE_SAMPLE_COUNT / 2) / AVERAGE_SAMPLE_COUNT);
    return ESP_OK;
}

void sampling_task(void*) {
    int voltage_mv = 0;
    esp_err_t result = enable_sample_path();
    if (result == ESP_OK) {
        result = wait_until_stable();
    }
    if (result == ESP_OK) {
        result = read_average_mv(voltage_mv);
    }

    const esp_err_t disable_result = disable_sample_path();
    if (result == ESP_OK) {
        result = disable_result;
    }

    xSemaphoreTake(state_mutex, portMAX_DELAY);
    last_result = result;
    last_voltage_mv = voltage_mv;
    const CompletionCallback callback = completion_callback;
    void* const context = completion_context;
    completion_callback = nullptr;
    completion_context = nullptr;
    xSemaphoreGive(state_mutex);

    if (callback != nullptr) {
        callback(result, voltage_mv, context);
    }

    xSemaphoreTake(state_mutex, portMAX_DELAY);
    sampling = false;
    xSemaphoreGive(state_mutex);
    xSemaphoreGive(completion);
    vTaskDelete(nullptr);
}

} // namespace

esp_err_t disable_sample_path() {
    gpio_config_t config = {};
    config.pin_bit_mask = 1ULL << SAMPLE_ENABLE_GPIO;
    config.mode = GPIO_MODE_INPUT;
    config.pull_up_en = GPIO_PULLUP_DISABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    return gpio_config(&config);
}

esp_err_t enable_sample_path() {
    gpio_config_t config = {};
    config.pin_bit_mask = 1ULL << SAMPLE_ENABLE_GPIO;
    config.mode = GPIO_MODE_OUTPUT_OD;
    config.pull_up_en = GPIO_PULLUP_DISABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;

    esp_err_t ret = gpio_config(&config);
    if (ret != ESP_OK) {
        return ret;
    }
    return gpio_set_level(SAMPLE_ENABLE_GPIO, 0);
}


esp_err_t init() {
    if (initialized) {
        return ESP_OK;
    }

    esp_err_t ret = disable_sample_path();
    if (ret != ESP_OK) {
        return ret;
    }
    ret = battery_adc.init();
    if (ret != ESP_OK) {
        return ret;
    }

    state_mutex = xSemaphoreCreateMutex();
    completion = xSemaphoreCreateBinary();
    if (state_mutex == nullptr || completion == nullptr) {
        if (state_mutex != nullptr) {
            vSemaphoreDelete(state_mutex);
            state_mutex = nullptr;
        }
        if (completion != nullptr) {
            vSemaphoreDelete(completion);
            completion = nullptr;
        }
        return ESP_ERR_NO_MEM;
    }

    initialized = true;
    return ESP_OK;
}

esp_err_t start_async(CompletionCallback callback, void* context) {
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(state_mutex, portMAX_DELAY);
    if (sampling) {
        xSemaphoreGive(state_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    sampling = true;
    last_result = ESP_ERR_INVALID_STATE;
    completion_callback = callback;
    completion_context = context;
    xSemaphoreTake(completion, 0);
    xSemaphoreGive(state_mutex);

    if (xTaskCreate(sampling_task, "battery_sample", 3072, nullptr, 3, nullptr) != pdPASS) {
        xSemaphoreTake(state_mutex, portMAX_DELAY);
        sampling = false;
        completion_callback = nullptr;
        completion_context = nullptr;
        xSemaphoreGive(state_mutex);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t wait_mv(int& voltage_mv, TickType_t ticks_to_wait) {
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(state_mutex, portMAX_DELAY);
    const bool currently_sampling = sampling;
    if (!currently_sampling) {
        voltage_mv = last_voltage_mv;
        const esp_err_t result = last_result;
        xSemaphoreGive(state_mutex);
        return result;
    }
    xSemaphoreGive(state_mutex);

    if (xSemaphoreTake(completion, ticks_to_wait) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    xSemaphoreGive(completion);

    xSemaphoreTake(state_mutex, portMAX_DELAY);
    voltage_mv = last_voltage_mv;
    const esp_err_t result = last_result;
    xSemaphoreGive(state_mutex);
    return result;
}

bool is_busy() {
    if (!initialized) {
        return false;
    }
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    const bool result = sampling;
    xSemaphoreGive(state_mutex);
    return result;
}

esp_err_t read_mv(int& voltage_mv) {
    const esp_err_t start_result = start_async();
    if (start_result != ESP_OK) {
        return start_result;
    }
    return wait_mv(voltage_mv);
}

} // namespace BatteryVoltage
