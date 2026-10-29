#
# CMakeLists.txt
#
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Zeepunt

cmake_minimum_required(VERSION 3.22)

if(NOT DEFINED ENV{SDK_STM32L4XX_PATH})
    message(FATAL_ERROR "SDK directory not found")
endif()

if(NOT IS_DIRECTORY "$ENV{SDK_STM32L4XX_PATH}")
    message(FATAL_ERROR "SDK directory invalid: $ENV{SDK_STM32L4XX_PATH}")
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

project(hello_world C ASM)

# 生成 compile_commands.json 文件
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(STM32_HAL_CONFIG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/config")
set(FREERTOS_CONFIG_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/config")
set(RTT_CONFIG_DIR       "${CMAKE_CURRENT_SOURCE_DIR}/config")

add_subdirectory(
    "$ENV{SDK_STM32L4XX_PATH}"
    "${CMAKE_BINARY_DIR}/sdk"
)

# 编译目标
add_executable("${PROJECT_NAME}"
    stm32l4xx_hal_msp.c
    retarget_syscall.c
    freertos_port.c
    main.c
)

# 统一输出后缀为 .elf
set_target_properties("${PROJECT_NAME}" PROPERTIES
    SUFFIX ".elf"
)

target_include_directories("${PROJECT_NAME}" PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/config"
)

target_link_libraries("${PROJECT_NAME}" PRIVATE
    CMSIS
    cmsis-device-l4
    stm32l4xx-hal-driver
    FreeRTOS
    board
)

if(CONFIG_SDK_LPTIM_TICK)
    target_link_libraries("${PROJECT_NAME}" PRIVATE "LPTIM-Tick")
endif()


if(CONFIG_SDK_SEGGER_RTT)
    target_link_libraries("${PROJECT_NAME}" PRIVATE "RTT")
endif()

# 链接脚本处理
if("${TOOLCHAIN}" STREQUAL "gcc")
    set(LD_SCRIPT_IN  "${CMAKE_CURRENT_SOURCE_DIR}/link.ld.in")
    set(LD_SCRIPT     "${CMAKE_CURRENT_BINARY_DIR}/link.ld")

    add_custom_command(
        OUTPUT  "${LD_SCRIPT}"
        COMMAND "${CMAKE_C_COMPILER}" -E -P -x c "-I${CMAKE_CURRENT_SOURCE_DIR}/config" "${LD_SCRIPT_IN}" -o "${LD_SCRIPT}"
        DEPENDS "${LD_SCRIPT_IN}"
        COMMENT "Generating ${LD_SCRIPT}"
        VERBATIM
    )
elseif("${TOOLCHAIN}" STREQUAL "armclang")
    set(LD_SCRIPT_IN  "${CMAKE_CURRENT_SOURCE_DIR}/link.sct.in")
    set(LD_SCRIPT     "${CMAKE_CURRENT_BINARY_DIR}/link.sct")

    add_custom_command(
        OUTPUT  "${LD_SCRIPT}"
        COMMAND "${CMAKE_C_COMPILER}" --target=arm-arm-none-eabi -mcpu=cortex-m4 -E -P -x c "-I${CMAKE_CURRENT_SOURCE_DIR}/config" "${LD_SCRIPT_IN}" -o "${LD_SCRIPT}"
        DEPENDS "${LD_SCRIPT_IN}"
        COMMENT "Generating ${LD_SCRIPT}"
        VERBATIM
    )
else()
    message(FATAL_ERROR "Unknown TOOLCHAIN='${TOOLCHAIN}'")
endif()

add_custom_target(link_script_gen DEPENDS "${LD_SCRIPT}")

add_dependencies("${PROJECT_NAME}" link_script_gen)

if("${TOOLCHAIN}" STREQUAL "gcc")
    # 链接选项
    target_link_options("${PROJECT_NAME}" PRIVATE
        --specs=nano.specs
        --specs=nosys.specs
        -T "${LD_SCRIPT}"
        "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.map"
        -Wl,--gc-sections
        -Wl,--wrap=malloc
        -Wl,--wrap=_malloc_r
        -Wl,--wrap=calloc
        -Wl,--wrap=_calloc_r
        -Wl,--wrap=free
        -Wl,--wrap=_free_r
    )

    # 生成 bin 文件
    add_custom_command(
        TARGET "${PROJECT_NAME}" POST_BUILD
        COMMAND "${CMAKE_OBJCOPY}" -O binary "$<TARGET_FILE:${PROJECT_NAME}>" "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.bin"
        COMMENT "Generating ${PROJECT_NAME}.bin"
    )

    # 生成 hex 文件
    add_custom_command(
        TARGET "${PROJECT_NAME}" POST_BUILD
        COMMAND "${CMAKE_OBJCOPY}" -O ihex "$<TARGET_FILE:${PROJECT_NAME}>" "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.hex"
        COMMENT "Generating ${PROJECT_NAME}.hex"
    )

    # 显示固件大小
    add_custom_command(
        TARGET "${PROJECT_NAME}" POST_BUILD
        COMMAND "${CMAKE_SIZE}" "$<TARGET_FILE:${PROJECT_NAME}>"
        COMMENT "Firmware size:"
    )
elseif("${TOOLCHAIN}" STREQUAL "armclang")
    target_link_options("${PROJECT_NAME}" PRIVATE
        "--scatter=${LD_SCRIPT}"
        --map
        "--list=${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.map"
        --info=sizes
        --info=totals
        --info=unused
        --info=veneers
        --strict
    )

    # 生成 bin 文件
    add_custom_command(
        TARGET "${PROJECT_NAME}" POST_BUILD
        COMMAND "${CMAKE_OBJCOPY}" --bin "$<TARGET_FILE:${PROJECT_NAME}>" --output "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.bin"
        COMMENT "Generating ${PROJECT_NAME}.bin"
    )

    # 生成 hex 文件
    add_custom_command(
        TARGET "${PROJECT_NAME}" POST_BUILD
        COMMAND "${CMAKE_OBJCOPY}" --i32combined "$<TARGET_FILE:${PROJECT_NAME}>" --output "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.hex"
        COMMENT "Generating ${PROJECT_NAME}.hex"
    )

    # 显示固件大小
    add_custom_command(
        TARGET "${PROJECT_NAME}" POST_BUILD
        COMMAND "${CMAKE_SIZE}" --info=totals "$<TARGET_FILE:${PROJECT_NAME}>"
        COMMENT "Firmware size:"
    )
else()
    message(FATAL_ERROR "Unknown TOOLCHAIN='${TOOLCHAIN}'")
endif()
