#ifndef BLACKBOX_SERVICE_H
#define BLACKBOX_SERVICE_H

#include "esp_err.h"

namespace BlackboxService {

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

} // namespace BlackboxService

#endif
