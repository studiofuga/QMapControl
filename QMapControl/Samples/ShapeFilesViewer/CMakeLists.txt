add_executable(ShapeFilesViewer
        ShapeFilesViewer.cpp ShapeFilesViewer.h
        )


target_include_directories(ShapeFilesViewer
        PRIVATE
        ${CMAKE_SOURCE_DIR}/QMapControl/src
        )

target_link_libraries(ShapeFilesViewer QMapControl)

