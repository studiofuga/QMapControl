add_executable(ShapeFilesViewer
        ShapeFilesViewer.cpp ShapeFilesViewer.h
        )

if (LibPal_FOUND)
    target_compile_definitions(ShapeFilesViewer
            PRIVATE
            -DHAVE_LIBPAL
            )
endif ()

target_include_directories(ShapeFilesViewer
        PRIVATE
        ${CMAKE_SOURCE_DIR}/QMapControl/src
        ${GDAL_INCLUDE_DIR}
        ${PROJ4_INCLUDE_DIR}
        ${LibPal_INCLUDE_DIR}
        )

target_link_libraries(ShapeFilesViewer QMapControl ${PROJ4_LIBRARIES} ${LibPal_LIBRARIES})

# Set target directory
install(TARGETS ShapeFilesViewer
        LIBRARY DESTINATION bin
        ARCHIVE DESTINATION bin
        COMPONENT examples)
