idf_component_register(
    SRCS "app_main.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        blackbox
        blackbox_service
        battery_voltage
        button_input
        espnow_link
        espnow_remote
        HXC_NVS
        power_manager
        shell_command
        status_led
        esp_system
        esp_timer
        log
)

set(SCRIPT_DIR "${CMAKE_SOURCE_DIR}/scripts")
add_custom_target(run_post_build ALL
    COMMAND python "${SCRIPT_DIR}/post_build.py"
    COMMENT "Generating merged firmware"
    VERBATIM
)
add_dependencies(run_post_build app bootloader)
