idf_component_register(
    SRC_DIRS "src/aes" "src/hal" "src/lmic" "src/aes/ideetron"
    INCLUDE_DIRS "project_config" "src"
    # TODO: remove when the component "arduino-esp32" be
    # officially updated to ESP-IDF v5.1.
    REQUIRES "arduino-esp32"
)
