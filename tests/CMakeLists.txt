find_package(GTest CONFIG REQUIRED)

include(GoogleTest)

set(Python3_FIND_UNVERSIONED_NAMES FIRST)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(DATA_TEST_FILE_DIR ${CMAKE_CURRENT_BINARY_DIR}/data_test_files)
set(DATA_TEST_FILE_HEADER ${DATA_TEST_FILE_DIR}/test-data-paths.h)
file(MAKE_DIRECTORY ${DATA_TEST_FILE_DIR})
add_custom_command(
    OUTPUT ${DATA_TEST_FILE_HEADER}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/generate_test_npy.py
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/generate_test_npy.py
            --header ${DATA_TEST_FILE_HEADER}
            ${DATA_TEST_FILE_DIR}
    COMMENT "Generating test Numpy data files"
)

add_executable(
    test-data test-data.cpp ../src/models/data.cpp ${DATA_TEST_FILE_HEADER}
)
target_include_directories(test-data PRIVATE ../src ${DATA_TEST_FILE_DIR})
target_link_libraries(test-data PRIVATE GTest::gtest_main cnpy::cnpy)
gtest_discover_tests(test-data)

add_executable(
    test-tasks-parsing
    test-tasks-parsing.cpp
    ../src/models/task-info.cpp
)
target_include_directories(test-tasks-parsing PRIVATE ../src)
target_link_libraries(
    test-tasks-parsing
    PRIVATE GTest::gtest_main nlohmann_json::nlohmann_json
)
set(TEST_JSON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tasks)
target_compile_definitions(
    test-tasks-parsing
    PRIVATE
        JSON_PATH="${TEST_JSON_DIR}/swallow-tasks.json"
        INVALID_JSON_PATH="${TEST_JSON_DIR}/invalid.json"
        MISSING_FIELD_JSON_PATH="${TEST_JSON_DIR}/swallow-missing-field.json"
        ANNOTATIONS_JSON_PATH="${CMAKE_CURRENT_SOURCE_DIR}/annotations.json"
)
gtest_discover_tests(test-tasks-parsing)

add_executable(test-observable test-observable.cpp)
target_link_libraries(test-observable PRIVATE GTest::gtest_main)
target_include_directories(test-observable PRIVATE ../src)
gtest_discover_tests(test-observable)

add_executable(
    test-annotation-json
    test-annotation-json.cpp
    ../src/models/annotation-json.cpp
)
target_link_libraries(
    test-annotation-json
    PRIVATE
    GTest::gtest_main nlohmann_json::nlohmann_json
)
target_include_directories(test-annotation-json PRIVATE ../src)
gtest_discover_tests(test-annotation-json)

add_executable(test-view-list test-view-list.cpp)
target_include_directories(test-view-list PRIVATE ../src/app)
target_link_libraries(test-view-list PRIVATE GTest::gtest_main)
gtest_discover_tests(test-view-list)
