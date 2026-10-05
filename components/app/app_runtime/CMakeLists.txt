idf_component_register(
    SRCS "src/app_runtime.cpp"
    INCLUDE_DIRS "include"
    REQUIRES espnow_link
        blackbox
        blackbox_service
        battery_level
        battery_voltage
        button_input
        power_manager
        esp_system
        esp_timer
        freertos
        log
)
