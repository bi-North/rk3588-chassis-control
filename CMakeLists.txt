cmake_minimum_required(VERSION 3.16)

project(rk3588_chassis_control C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

add_library(chassis_core
    src/socketcan.c
    src/motor_3508.c
    src/pid.c
)

target_include_directories(chassis_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(chassis_core PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

add_executable(monitor_3508
    tools/monitor_3508.c
)
target_link_libraries(monitor_3508 PRIVATE chassis_core)
target_compile_options(monitor_3508 PRIVATE -Wall -Wextra -Wpedantic)

add_executable(test_single_motor
    tools/test_single_motor.c
)
target_link_libraries(test_single_motor PRIVATE chassis_core)
target_compile_options(test_single_motor PRIVATE -Wall -Wextra -Wpedantic)

add_executable(test_speed_pid
    tools/test_speed_pid.c
)
target_link_libraries(test_speed_pid PRIVATE chassis_core)
target_compile_options(test_speed_pid PRIVATE -Wall -Wextra -Wpedantic)
