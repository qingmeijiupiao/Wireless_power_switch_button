#ifndef BLACKBOX_SERVICE_H
#define BLACKBOX_SERVICE_H

#include <cstddef>
#include <cstdint>

#include "esp_err.h"

namespace BlackboxService {

struct Statistics {
    uint32_t captured_logs;
    uint32_t dropped_logs;
    uint32_t persist_failures;
    size_t pending_logs;
};

/** @brief Start ESP_LOG capture and the asynchronous capture worker. */
esp_err_t init();

/**
 * @brief Wait until captured logs and queued Flash writes are persisted.
 *
 * Call this before deep sleep and before exporting logs.
 */
esp_err_t sync();

esp_err_t append_event(const char* fmt, ...);
esp_err_t append_text_event(const char* fmt, ...);

/** @brief Return a lock-safe snapshot of the log capture pipeline counters. */
void get_statistics(Statistics* statistics);

} // namespace BlackboxService

#endif
