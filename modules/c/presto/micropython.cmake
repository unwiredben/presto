add_library(usermod_presto INTERFACE)

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}../../../../" ABSOLUTE)

target_sources(usermod_presto INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/presto.c
    ${CMAKE_CURRENT_LIST_DIR}/presto.cpp
    ${REPO_ROOT}/drivers/st7701/st7701.cpp
    ${PIMORONI_PICO_PATH}/libraries/pico_graphics/pico_graphics_pen_rgb565.cpp
)
pico_generate_pio_header(usermod_presto ${REPO_ROOT}/drivers/st7701/st7701_parallel.pio)
pico_generate_pio_header(usermod_presto ${REPO_ROOT}/drivers/st7701/st7701_timing.pio)

target_include_directories(usermod_presto INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
    ${PIMORONI_PICO_PATH}/libraries/pico_graphics/
    ${REPO_ROOT}/drivers/st7701/
    ${REPO_ROOT}/micropython/modules
)

target_compile_definitions(usermod_presto INTERFACE
    MODULE_PRESTO_ENABLED=1
)

target_link_libraries(usermod INTERFACE usermod_presto)