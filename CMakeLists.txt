cmake_minimum_required(VERSION 3.10)

project(NanoLog VERSION 0.0.1 DESCRIPTION "Nanolog is an extremely performant nanosecond scale logging system for C++ that exposes a simple printf-like API.")

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/modules")

set(CMAKE_CXX_STANDARD 17)

# pre-requisites
find_package(Threads REQUIRED)
find_package(RT REQUIRED)

file(GLOB HEADERS ${PROJECT_SOURCE_DIR}/runtime/*.h)
file(GLOB SOURCES ${PROJECT_SOURCE_DIR}/runtime/*.cc ${PROJECT_SOURCE_DIR}/runtime/testHelper/GeneratedCode.cc)
add_library(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/runtime)

set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(${PROJECT_NAME} PROPERTIES SOVERSION 1)
set_target_properties(${PROJECT_NAME} PROPERTIES PUBLIC_HEADER "${HEADERS}")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}.pc.in ${PROJECT_NAME}.pc @ONLY)

add_subdirectory(decompressor)
add_subdirectory(perf)

include(GNUInstallDirs)

install(TARGETS ${PROJECT_NAME} ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.pc DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig)

