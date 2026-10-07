cmake_minimum_required(VERSION 3.16)
project(coroutine_tutorial LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

foreach(example IN ITEMS manual_task generator task_chain event_loop exceptions)
    add_executable(${example} examples/${example}.cpp)
    target_compile_options(${example} PRIVATE -Wall -Wextra -Wpedantic)
endforeach()

add_test(NAME manual_task_runs COMMAND manual_task)
add_test(NAME generator_runs COMMAND generator)
add_test(NAME task_chain_runs COMMAND task_chain)
add_test(NAME event_loop_runs COMMAND event_loop)
add_test(NAME exceptions_run COMMAND exceptions)

# The workshop includes intentionally invalid programs. Compile them only via
# the checker, which requires failure and checks the relevant diagnostic.
add_executable(workshop_report labs/12/01_report.cpp)
target_compile_options(workshop_report PRIVATE -Wall -Wextra -Wpedantic)

if(BUILD_TESTING)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    foreach(lesson IN ITEMS 01 02 03 04 05 06 07 08 09 10 11 12)
        add_test(NAME workshop_${lesson}
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_labs.py
                ${lesson} --cxx ${CMAKE_CXX_COMPILER})
    endforeach()
endif()
