cmake_minimum_required(VERSION 3.11)
cmake_policy(SET CMP0048 NEW)

project(QMapControl LANGUAGES CXX VERSION 1.1.101)

if (EXISTS "${CMAKE_BINARY_DIR}/conanbuildinfo.cmake")
    include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
    conan_basic_setup()

    list(APPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR})
endif ()

find_package(GDAL REQUIRED 2.20)

option(INSTALL_EXAMPLES "Install everything including examples" OFF)

include(CPack)
include(GNUInstallDirs)
cpack_add_component(development
        DESCRIPTION "Development files"
        REQUIRED
        )
cpack_add_component(examples
        DESCRIPTION "Examples, demo and tests"
        DISABLED
        )


find_package(Qt5 5.6 COMPONENTS Widgets Core Concurrent Sql Xml PrintSupport Network REQUIRED)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include_directories(
        .
        ${Qt5Widgets_INCLUDE_DIRS}
)

file(GLOB HEADERS QMapControl/src/QMapControl/*.h)
file(GLOB SOURCES QMapControl/src/QMapControl/*.cpp)

add_library(QMapControl SHARED
        ${SOURCES} ${HEADERS}
        ${UIS} ${QRCS}
        QMapControl/src/QMapControl/LayerRaster.cpp QMapControl/src/QMapControl/LayerRaster.h
        QMapControl/src/QMapControl/AdapterRaster.cpp QMapControl/src/QMapControl/AdapterRaster.h
        QMapControl/src/QMapControl/utils/spimpl.h
        QMapControl/src/QMapControl/RasterUtils.cpp QMapControl/src/QMapControl/RasterUtils.h
        QMapControl/src/QMapControl/PersistentCache.cpp QMapControl/src/QMapControl/PersistentCache.h)

target_compile_definitions(QMapControl
        PRIVATE
        -DQMAPCONTROL_LIBRARY)


target_link_libraries(QMapControl
        GDAL::GDAL
        Qt5::Widgets
        Qt5::Network
        )

target_include_directories(QMapControl
        PUBLIC
        ${GDAL_INCLUDE_DIRS}
        )

set_target_properties(QMapControl PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        EXPORT_NAME qmapcontrol
        PUBLIC_HEADER "${PUB_HEADERS}"
        )

add_subdirectory(QMapControl/Samples)

# Set target directory
install(TARGETS QMapControl
        EXPORT qmapcontrol-export
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        COMPONENT development)

install(DIRECTORY "${CMAKE_SOURCE_DIR}/QMapControl/src/QMapControl"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
        COMPONENT development
        FILES_MATCHING
        PATTERN "*.h")

include(cmake/install.cmake)

