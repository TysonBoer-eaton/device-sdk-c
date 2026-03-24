
include(../cmake/FetchGTest.cmake)

add_executable(unit_tests 
    ${PROJECT_SOURCE_DIR}/
    ${PROJECT_SOURCE_DIR}/
)

target_include_directories(unit_tests PUBLIC
    .
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/
    ${IOT_INCLUDE_DIR}
    ${EDGEX_CSDK_INCLUDE_DIR}
)

target_link_libraries(unit_tests PRIVATE
    gtest_main
    ${IOT_LIBRARY}
    ${EDGEX_CSDK_LIBRARIES}
)

target_compile_options(unit_tests PRIVATE -std=c++20 -Wall -Wextra -Wpedantic -Wno-unused-parameter)