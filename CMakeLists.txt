cmake_minimum_required(VERSION 3.11)
cmake_policy(SET CMP0048 NEW)

project(QMapControl LANGUAGES CXX VERSION 1.0.99)

find_package(GDAL REQUIRED 1.11)
message("GDAL library: ${GDAL_LIBRARY}")
message("GDAL include: ${GDAL_INCLUDE_DIR}")
#include_directories(${GDAL_INCLUDE_DIR})

find_package(Qt5 5.6 COMPONENTS Widgets Core Concurrent Sql Xml PrintSupport Network REQUIRED)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

add_definitions(-DQMAPCONTROL_LIBRARY)

include_directories(
        .
        ${Qt5Widgets_INCLUDE_DIRS}
)

file(GLOB HEADERS QMapControl/src/QMapControl/*.h)
file(GLOB SOURCES QMapControl/src/QMapControl/*.cpp)

#[[
if (NOT APPLE)
    set(GDAL_LIBRARY gdal)
else()
    set(GDAL_LIBRARY GDAL::GDAL)
endif()
]]

#if (WIN32)
#    link_directories(${CMAKE_SOURCE_DIR}/install/msvc/lib)
#    set(GDAL_LIBRARY gdal_i)
#endif (WIN32)

set(LIBRARIES
        ${GDAL_LIBRARY}
        )

add_library(QMapControl SHARED
        ${SOURCES} ${HEADERS}
        ${UIS} ${QRCS}
        )

target_link_libraries(QMapControl
        ${LIBRARIES}
        Qt5::Widgets
        Qt5::Network
        )

#target_link_directories(${GDAL_LI)

target_include_directories(QMapControl
        PRIVATE
        ${GDAL_INCLUDE_DIRS}
        )

# Set target directory
install(TARGETS QMapControl
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        COMPONENT gui)
