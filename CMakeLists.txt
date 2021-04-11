cmake_minimum_required(VERSION 3.13)

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include(pico_sdk_import.cmake)

project(my_project C CXX)

set(CMAKE_C_STANDARD 11) 
set(CMAKE_CXX_STANDARD 17) 

set(CMAKE_BUILD_TYPE MinSizeRel)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

# --------------------------------------------------
add_executable(pio_sample)

target_compile_options (pio_sample PUBLIC "-Wno-psabi" )

# generate the header file into the source tree as it is included in the RP2040 datasheet
pico_generate_pio_header(pio_sample ${CMAKE_CURRENT_LIST_DIR}/pio_sample.pio 
                                    OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/generated)

target_sources(pio_sample PRIVATE pio_sample.cpp)

pico_enable_stdio_usb (pio_sample 1)
pico_enable_stdio_uart(pio_sample 1)

target_link_libraries(pio_sample 
                        PRIVATE 
                           pico_stdlib 
                           hardware_pio 
                           pico_multicore
                     )
pico_add_extra_outputs(pio_sample)

