#include "HXC_NVS.h"
#include "battery_voltage.h"
#include "blackbox.h"
#include "blackbox_service.h"
#include "button_input.h"
#include "esp_log.h"
#include "espnow_remote.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "power_manager.h"
#include "shell_command.h"
#include "status_led.h"

namespace {

constexpr char TAG[] = "app_main";

struct WakeLogContext {
    PowerManager::WakeSource source;
    bool usb_mode;
    bool button_pressed;
};

WakeLogContext wake_log_context = {};

const char* wake_source_name(PowerManager::WakeSource source) {
    switch (source) {
        case PowerManager::WakeSource::POWER_ON:
            return "power_on";
        case PowerManager::WakeSource::BUTTON:
            return "button";
        case PowerManager::WakeSource::USB:
            return "usb";
        case PowerManager::WakeSource::BUTTON_AND_USB:
            return "button_and_usb";
        case PowerManager::WakeSource::OTHER:
        default:
            return "other";
    }
}

void log_wakeup(esp_err_t result, int battery_voltage_mv, void* context) {
    const auto& wake = *static_cast<const WakeLogContext*>(context);
    (void)BlackboxService::append_text_event(
        "wake: source=%s battery_mv=%d battery_result=%s usb=%u button=%u "
        "firmware=%d.%d.%d build=%s",
        wake_source_name(wake.source),
        battery_voltage_mv,
        esp_err_to_name(result),
        wake.usb_mode ? 1U : 0U,
        wake.button_pressed ? 1U : 0U,
        VERSION_MAJOR,
        VERSION_MINOR,
        VERSION_PATCH,
        BUILD_TIME);
}

bool button_wakeup(PowerManager::WakeSource source) {
    return source == PowerManager::WakeSource::BUTTON ||
           source == PowerManager::WakeSource::BUTTON_AND_USB;
}

void sleep_when_inputs_idle() {
    while (PowerManager::usb_connected() ||
           PowerManager::button_pressed() ||
           ButtonInput::is_busy()) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    ESP_LOGI(TAG, "entering deep sleep");
    int battery_voltage_mv = 0;
    (void)BatteryVoltage::wait_mv(battery_voltage_mv);
    (void)BlackboxService::sync();
    ESP_ERROR_CHECK(PowerManager::enter_deep_sleep());
}

} // namespace

extern "C" void app_main(void) {
    ESP_ERROR_CHECK(PowerManager::init());

    const PowerManager::WakeSource source = PowerManager::wake_source();
    const bool usb_mode = PowerManager::usb_connected();
    const bool process_button = button_wakeup(source) || PowerManager::button_pressed();

    ESP_ERROR_CHECK(StatusLed::init());
    if (process_button) {
        ESP_ERROR_CHECK(ButtonInput::start_wakeup_tracking());
    }
    ESP_ERROR_CHECK(Blackbox::init());
    HXC::NVS_Base::setup();
    ESP_ERROR_CHECK(BlackboxService::init());
    ESP_ERROR_CHECK(BatteryVoltage::init());

    wake_log_context = {
        .source = source,
        .usb_mode = usb_mode,
        .button_pressed = PowerManager::button_pressed(),
    };
    ESP_ERROR_CHECK(BatteryVoltage::start_async(log_wakeup, &wake_log_context));

    if (!usb_mode && !process_button) {
        sleep_when_inputs_idle();
        return;
    }

    ESP_ERROR_CHECK(EspNowRemote::init());
    ButtonInput::notify_radio_ready();

    if (!usb_mode) {
        sleep_when_inputs_idle();
        return;
    }

    ESP_ERROR_CHECK(ShellCommand::init());
    ESP_ERROR_CHECK(ButtonInput::init_usb_mode());
    ESP_LOGI(TAG, "USB mode ready; shell and button input enabled");

    while (PowerManager::usb_connected()) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    ESP_LOGI(TAG, "USB removed");
    EspNowRemote::stop_pairing();
    sleep_when_inputs_idle();
}
