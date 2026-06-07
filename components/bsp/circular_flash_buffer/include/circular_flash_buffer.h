/*
 * @LastEditors: qingmeijiupiao
 * @Description: 基于ESP32 SPI Flash分区的循环缓冲区驱动，与具体数据结构无关
 * @Author: qingmeijiupiao
 */
#ifndef CIRCULAR_FLASH_BUFFER_H
#define CIRCULAR_FLASH_BUFFER_H

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

namespace CircularFlashBuffer {

    constexpr uint32_t PAGE_SIZE = 256;
    constexpr uint32_t SECTOR_SIZE = 4096;
    constexpr uint32_t PAGES_PER_SECTOR = SECTOR_SIZE / PAGE_SIZE;

    constexpr uint8_t BLOCK_SOF = 0xAA;

    esp_err_t init(const char* partition_name, size_t block_size);

    esp_err_t write_block(const uint8_t* data);

    esp_err_t read_block(uint32_t index, uint8_t* data);

    /**
     * @brief 物理擦除整个分区并重置内部指针
     */
    esp_err_t erase_all();

    uint32_t get_count();

    /** @brief 返回环形缓冲最多可保留的数据块数量。 */
    uint32_t get_capacity();

    void set_enable(bool enable);
}

#endif
