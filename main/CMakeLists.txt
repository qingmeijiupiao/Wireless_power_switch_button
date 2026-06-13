idf_component_register(
    SRCS "app_main.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        app_runtime
        battery_voltage
        blackbox
        blackbox_service
        button_input
        espnow_remote
        HXC_NVS
        power_manager
        shell_command
        status_led
)

set(SCRIPT_DIR "${CMAKE_SOURCE_DIR}/scripts")
add_custom_target(run_post_build ALL
    COMMAND python "${SCRIPT_DIR}/post_build.py"
    COMMENT "Generating merged firmware"
    VERBATIM
)
add_dependencies(run_post_build app bootloader)
