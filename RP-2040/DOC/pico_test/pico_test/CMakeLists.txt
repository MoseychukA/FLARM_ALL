cmake_minimum_required(VERSION 3.13)

# Include the SDK using the PICO_SDK_PATH environment variable:
# (before project())
include(pico_sdk_import.cmake)

project(hello)

# Initialize the Raspberry Pi Pico SDK: (after project())
pico_sdk_init()

add_executable(hello src/hello.c)

# pull in common dependencies
target_link_libraries(hello pico_stdlib)

# enable usb output, disable uart output
pico_enable_stdio_usb(hello 1)
pico_enable_stdio_uart(hello 0)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(hello)


