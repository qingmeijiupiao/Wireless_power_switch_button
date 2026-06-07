idf_component_register(
    SRCS "src/shell_command.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        battery_voltage
        blackbox
        blackbox_service
        espnow_remote
        espnow_service
        shell
        status_led
)
