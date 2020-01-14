project(Perf)

find_package(Threads REQUIRED)

file(GLOB SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.cc ${CMAKE_CURRENT_SOURCE_DIR}/*.h ${CMAKE_SOURCE_DIR}/runtime/testHelper/GeneratedCode.cc)
add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/runtime)
target_link_libraries(${PROJECT_NAME} NanoLog ${CMAKE_THREAD_LIBS_INIT})
