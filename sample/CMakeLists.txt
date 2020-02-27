project(sample)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/modules")

find_package(Threads REQUIRED)
find_package(RT REQUIRED)

file(GLOB SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.cc)
add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/runtime)
target_link_libraries(${PROJECT_NAME} NanoLog ${CMAKE_THREAD_LIBS_INIT} ${RT_LIBRARIES})
