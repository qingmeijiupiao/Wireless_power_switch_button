#include "blackbox_service.h"

#include <cstdarg>
#include <cstdio>

#include "blackbox.h"
#include "blackbox_service_internal.h"

namespace BlackboxService {
namespace {

bool initialized;

esp_err_t append_v(const char* fmt, va_list args) {
    if (fmt == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    char text[Blackbox::TEXT_BUFFER_SIZE] = {};
    vsnprintf(text, sizeof(text), fmt, args);
    return Blackbox::append_text("%s", text);
}

} // namespace

esp_err_t init() {
    if (initialized) {
        return ESP_OK;
    }

    const esp_err_t worker_result = Internal::start_worker();
    if (worker_result != ESP_OK) {
        return worker_result;
    }
    Internal::install_log_capture();
    initialized = true;
    return Blackbox::append_text("blackbox_service: initialized");
}

esp_err_t sync() {
    return Internal::sync_pending_logs();
}

esp_err_t append_event(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    const esp_err_t ret = append_v(fmt, args);
    va_end(args);
    return ret;
}

esp_err_t append_text_event(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    const esp_err_t ret = append_v(fmt, args);
    va_end(args);
    return ret;
}

} // namespace BlackboxService
