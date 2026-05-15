set(srcs src/stm32.cc src/stm32_flash.cc src/stm32_bootloader.cc src/stm32_bl.cc
)
set(platform_srcs
   esp32/mutex.cc esp32/stm32_uart.cc esp32/stm32_impl.cc esp32/stm32_flash.cc
)
include("${CMAKE_CURRENT_LIST_DIR}/../../source_filter.cmake")


if(COMMAND idf_component_register)
idf_component_register(
    SRCS ${srcs} 
    INCLUDE_DIRS include 
    PRIV_INCLUDE_DIRS priv_include 
    PRIV_REQUIRES net utils_debug utils_misc
                  esp_driver_uart esp_driver_gpio hal 
)

target_compile_options(${COMPONENT_LIB} PRIVATE "SHELL:-include sdkconfig.h")


else()
add_library(stm32 STATIC ${srcs})
target_include_directories(stm32 PUBLIC include PRIVATE priv_include)
target_link_libraries(stm32 PRIVATE net utils_debug utils_misc)
endif()

