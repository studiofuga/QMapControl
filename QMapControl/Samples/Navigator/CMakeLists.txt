add_executable(Navigator
        Navigator.cpp Navigator.h
        )


target_include_directories(Navigator
        PRIVATE
        ${CMAKE_SOURCE_DIR}/QMapControl/src
        ${GDAL_INCLUDE_DIR}
        ${PROJ4_INCLUDE_DIR}
        )

target_link_libraries(Navigator QMapControl ${PROJ4_LIBRARIES})

# Set target directory
install(TARGETS Navigator
        LIBRARY DESTINATION bin
        ARCHIVE DESTINATION bin
        COMPONENT examples)
