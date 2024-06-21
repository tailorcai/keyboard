set(PROJECTNAME keyboard)

cmake_minimum_required(VERSION 3.14)
include(pico_sdk_import.cmake)

project(${PROJECTNAME} VERSION 1.0.0)

add_executable(${PROJECTNAME}
               main.c
               usb_descriptors.c
               led.c
               save.c
               queue.c
               persist.c
               src/pio_usb.c
               src/pio_usb_device.c
               src/pio_usb_host.c
               src/usb_crc.c
               )

pico_sdk_init()

set(dir ${CMAKE_CURRENT_LIST_DIR}/src)

pico_generate_pio_header(${PROJECTNAME} ${dir}/usb_tx.pio)
pico_generate_pio_header(${PROJECTNAME} ${dir}/usb_rx.pio)

#pico_enable_stdio_usb(${PROJECTNAME} 0)
#pico_enable_stdio_uart(${PROJECTNAME} 1)
#pico_add_extra_outputs(${PROJECTNAME})

target_link_libraries(${PROJECTNAME}
                      pico_stdlib
                      tinyusb_device
                      tinyusb_board
                      hardware_gpio
                      hardware_flash
                      pico_multicore
                      hardware_dma    
                      hardware_pio       
                    #   tinyusb_host                                 
                      )

#TinyUSB stuff so it can pick up tinyusb_config.h
target_include_directories(${PROJECTNAME} PRIVATE 
                           ${CMAKE_CURRENT_LIST_DIR}
                           ${dir}
                           )

pico_add_extra_outputs(${PROJECTNAME})