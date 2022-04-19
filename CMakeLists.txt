cmake_minimum_required(VERSION 3.10)

project(Cluster)

set(CMAKE_CXX_STANDARD 14)


set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 COMPONENTS Widgets REQUIRED)
find_package(libssh REQUIRED)
find_package(jsoncpp REQUIRED)


file(GLOB SORCE_FILES
${PROJECT_SOURCE_DIR}/*.cpp
${PROJECT_SOURCE_DIR}/*.h
${PROJECT_SOURCE_DIR}/*.ui
${PROJECT_SOURCE_DIR}/*.qrc
${PROJECT_SOURCE_DIR}/*.json)

include_directories(
../../studio
../../data
)

add_library(${PROJECT_NAME} SHARED ${SORCE_FILES})

target_link_libraries(${PROJECT_NAME} PRIVATE Qt5::Widgets)
target_link_libraries(${PROJECT_NAME} PRIVATE ssh)
target_link_libraries(${PROJECT_NAME} PRIVATE Data)
target_link_libraries(${PROJECT_NAME} PRIVATE jsoncpp_static)


# Installation
install(TARGETS ${PROJECT_NAME} DESTINATION bin)
