#
# CMakeLists.txt
#
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Zeepunt

set(lvgl_port_incs
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

set(lvgl_port_srcs
    "${CMAKE_CURRENT_SOURCE_DIR}/source/lv_port_disp.c"
)

component_register(
    SRCS
        ${lvgl_port_srcs}
    INCLUDE_DIRS
        ${lvgl_port_incs}
    REQUIRES
        "board"
        "lvgl"
)

target_include_directories(${COMPONENT_LIB}
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/../config"
)
