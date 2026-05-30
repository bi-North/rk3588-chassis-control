cmake_minimum_required(VERSION 3.16)

project(rk3588_chassis_control C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

add_library(chassis_core
    src/socketcan.c
    src/motor_3508.c
    src/pid.c
    src/chassis_control.c
)

target_include_directories(chassis_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(UNIX)
    target_link_libraries(chassis_core PUBLIC m)
endif()

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

add_executable(test_chassis_keyboard
    tools/test_chassis_keyboard.c
)
target_link_libraries(test_chassis_keyboard PRIVATE chassis_core)
target_compile_options(test_chassis_keyboard PRIVATE -Wall -Wextra -Wpedantic)

add_executable(test_chassis_command
    tools/test_chassis_command.c
)
target_link_libraries(test_chassis_command PRIVATE chassis_core)
target_compile_options(test_chassis_command PRIVATE -Wall -Wextra -Wpedantic)

add_executable(chassis_velocity
    tools/chassis_velocity.c
)
target_link_libraries(chassis_velocity PRIVATE chassis_core)
target_compile_options(chassis_velocity PRIVATE -Wall -Wextra -Wpedantic)
