project(Tests)

find_package(GTest REQUIRED)

file(GLOB SOURCES *.cc)
add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME} GTest::GTest GTest::Main NanoLog ${CMAKE_THREAD_LIBS_INIT} ${RT_LIBRARIES})
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/runtime)

gtest_discover_tests(${PROJECT_NAME})
