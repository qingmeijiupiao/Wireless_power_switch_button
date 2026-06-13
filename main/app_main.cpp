#include "HXC_NVS.h"
#include "battery_level.h"
#include "battery_voltage.h"
#include "blackbox.h"
#include "blackbox_service.h"
#include "button_input.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
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

const char* reset_reason_name(esp_reset_reason_t reason) {
    switch (reason) {
        case ESP_RST_POWERON: return "power_on";
        case ESP_RST_EXT: return "external";
        case ESP_RST_SW: return "software";
        case ESP_RST_PANIC: return "panic";
        case ESP_RST_INT_WDT: return "interrupt_wdt";
        case ESP_RST_TASK_WDT: return "task_wdt";
        case ESP_RST_WDT: return "other_wdt";
        case ESP_RST_DEEPSLEEP: return "deep_sleep";
        case ESP_RST_BROWNOUT: return "brownout";
        case ESP_RST_SDIO: return "sdio";
        case ESP_RST_USB: return "usb";
        case ESP_RST_JTAG: return "jtag";
        case ESP_RST_EFUSE: return "efuse";
        case ESP_RST_PWR_GLITCH: return "power_glitch";
        case ESP_RST_CPU_LOCKUP: return "cpu_lockup";
        case ESP_RST_UNKNOWN:
        default:
            return "unknown";
    }
}

void log_wakeup(esp_err_t result, int battery_voltage_mv, void* context) {
    const auto* wake = static_cast<const WakeLogContext*>(context);
    if (result != ESP_OK) {
        (void)BlackboxService::append_text_event(
            "battery: result=%s", esp_err_to_name(result));
        return;
    }

    const BatteryLevel::Status level =
        BatteryLevel::update(battery_voltage_mv, wake->usb_mode);
    (void)BlackboxService::append_text_event(
        "battery: voltage_mv=%d estimated=%u displayed=%u charging=%u rtc=%u",
        battery_voltage_mv,
        static_cast<unsigned>(level.estimated_percent),
        static_cast<unsigned>(level.displayed_percent),
        level.charging ? 1U : 0U,
        level.restored_from_rtc ? 1U : 0U);
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
    const esp_err_t battery_result = BatteryVoltage::wait_mv(battery_voltage_mv);
    BatteryLevel::Status level = {};
    const bool level_valid = BatteryLevel::get_status(level);
    BlackboxService::Statistics statistics = {};
    BlackboxService::get_statistics(&statistics);
    (void)BlackboxService::append_text_event(
        "sleep: uptime_ms=%llu battery_mv=%d battery_pct=%u battery_result=%s "
        "heap_free=%lu heap_min=%lu records=%lu/%lu log_drop=%lu persist_fail=%lu",
        static_cast<unsigned long long>(esp_timer_get_time() / 1000),
        battery_voltage_mv,
        level_valid ? static_cast<unsigned>(level.displayed_percent) : 0U,
        esp_err_to_name(battery_result),
        static_cast<unsigned long>(esp_get_free_heap_size()),
        static_cast<unsigned long>(esp_get_minimum_free_heap_size()),
        static_cast<unsigned long>(Blackbox::count()),
        static_cast<unsigned long>(Blackbox::capacity()),
        static_cast<unsigned long>(statistics.dropped_logs),
        static_cast<unsigned long>(statistics.persist_failures));
    (void)BlackboxService::sync();
    ESP_ERROR_CHECK(PowerManager::enter_deep_sleep());
}

} // namespace

extern "C" void app_main(void) {
    ESP_ERROR_CHECK(PowerManager::init());

    const PowerManager::WakeSource source = PowerManager::wake_source();
    const bool usb_mode = PowerManager::usb_connected();
    const bool process_button =
        button_wakeup(source) || PowerManager::button_pressed();

    ESP_ERROR_CHECK(StatusLed::init());
    if (process_button) {
        ESP_ERROR_CHECK(ButtonInput::start_wakeup_tracking());
    }
    ESP_ERROR_CHECK(Blackbox::init());
    ESP_ERROR_CHECK(HXC::NVS_Base::setup());
    ESP_ERROR_CHECK(BlackboxService::init());
    ESP_ERROR_CHECK(BatteryVoltage::init());

    wake_log_context = {
        .source = source,
        .usb_mode = usb_mode,
        .button_pressed = PowerManager::button_pressed(),
    };
    (void)BlackboxService::append_text_event(
        "boot: reset=%s wake=%s usb=%u button=%u heap_free=%lu heap_min=%lu",
        reset_reason_name(esp_reset_reason()),
        wake_source_name(source),
        usb_mode ? 1U : 0U,
        wake_log_context.button_pressed ? 1U : 0U,
        static_cast<unsigned long>(esp_get_free_heap_size()),
        static_cast<unsigned long>(esp_get_minimum_free_heap_size()));
    (void)BlackboxService::append_text_event(
        "firmware: version=%d.%d.%d build=%s",
        VERSION_MAJOR,
        VERSION_MINOR,
        VERSION_PATCH,
        BUILD_TIME);
    ESP_ERROR_CHECK(BatteryVoltage::start_async(log_wakeup, &wake_log_context));

    if (!usb_mode && !process_button) {
        sleep_when_inputs_idle();
        return;
    }

    ESP_ERROR_CHECK(EspNowRemote::init());
    ButtonInput::notify_radio_ready();
    ESP_LOGI(TAG, "radio ready usb=%u button_transaction=%u",
             usb_mode ? 1U : 0U, process_button ? 1U : 0U);

    if (!usb_mode) {
        sleep_when_inputs_idle();
        return;
    }

    ESP_ERROR_CHECK(
        BatteryVoltage::start_calibration_monitor(PowerManager::usb_connected));
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
