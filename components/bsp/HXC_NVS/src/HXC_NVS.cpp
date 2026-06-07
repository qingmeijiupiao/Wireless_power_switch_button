#include "HXC_NVS.h"

namespace HXC {

static const char* TAG = "HXC_NVS";

// 初始化 NVS_Base 静态成员
bool NVS_Base::is_setup = false;
nvs_handle_t NVS_Base::_handle = 0;

void NVS_Base::setup() {
    if (is_setup) return;
    if(nvs_flash_init() != ESP_OK) {
        ESP_LOGE(TAG, "nvs_flash_init failed");
        return;
    }
    // 打开 NVS
    esp_err_t err = nvs_open(NVS_NAME, NVS_READWRITE, &_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "nvs_open %s success", NVS_NAME);
    is_setup = true;
}

// =========================================================
// NVS_DATA<char*> 特化类的具体实现
// =========================================================

NVS_DATA<char*>::NVS_DATA(const char* _key, const char* default_value) : value(nullptr), is_read(false) {
    strncpy(this->key, _key, 15);
    this->key[15] = '\0';
    if (strlen(_key) > 15) {
        ESP_LOGE(TAG, "nvs key too long, truncated: %s", this->key);
    }
    
    // 深拷贝默认值
    if (default_value) {
        this->value = new char[strlen(default_value) + 1];
        strcpy(this->value, default_value);
    }
}

NVS_DATA<char*>::~NVS_DATA() {
    if (value) {
        delete[] value;
        value = nullptr;
    }
}

esp_err_t NVS_DATA<char*>::save() {
    setup();
    if (!value) return ESP_FAIL;

    // ESP-IDF 存储字符串推荐直接使用 nvs_set_str
    esp_err_t err = nvs_set_str(_handle, key, value);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_set_str fail: %s %s", key, esp_err_to_name(err));
    }
    err = nvs_commit(_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_commit fail: %s %s", key, esp_err_to_name(err));
    }
    return err;
}

char* NVS_DATA<char*>::read() {
    setup();
    if (is_read) return value;

    size_t datalen = 0;
    // 获取所需长度（包含 \0）
    esp_err_t err = nvs_get_str(_handle, key, NULL, &datalen);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "KEY=%s NVS无数据, 使用默认值", key);
        is_read = true;
        return value;
    }

    char* arr = new char[datalen];
    err = nvs_get_str(_handle, key, arr, &datalen);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "KEY=%s NVS读取失败, 使用默认值", key);
        delete[] arr;
        return value;
    }

    // 释放旧值内存并指向从 NVS 读出的新值
    if (value) {
        delete[] value;
    }
    value = arr;
    is_read = true;
    return value;
}

NVS_DATA<char*>& NVS_DATA<char*>::operator=(const char* newValue) {
    // 释放原有内存，重新分配
    if (value) {
        delete[] value;
        value = nullptr;
    }
    if (newValue) {
        value = new char[strlen(newValue) + 1];
        strcpy(value, newValue);
    }
    save();
    return *this;
}

NVS_DATA<char*>::operator char*() {
    return read();
}

} // namespace HXC