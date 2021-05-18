if (UNIX)
    include (GNUInstallDirs)
    set (INSTALL_EXPORTS_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/QMapControl)
else()
    set(CMAKE_INSTALL_LIBDIR lib)
    set(CMAKE_INSTALL_BINDIR bin)
    set(CMAKE_INSTALL_INCLUDEDIR include)
endif()

if (WIN32)
    set (INSTALL_EXPORTS_DIR share/QMapControl)
endif(WIN32)

set(INCLUDE_INSTALL_DIR include/)
set(LIB_INSTALL_DIR lib/)
set(SYSCONFIG_INSTALL_DIR etc/msqlitecpp/)

# Configuration
set(version_config "${generated_dir}/${PROJECT_NAME}ConfigVersion.cmake")
set(project_config "${generated_dir}/${PROJECT_NAME}Config.cmake")
set(TARGETS_EXPORT_NAME "${PROJECT_NAME}Targets")
set(namespace "${PROJECT_NAME}::")

include(CMakePackageConfigHelpers)

configure_package_config_file(
        ${CMAKE_SOURCE_DIR}/cmake/QMapControlConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/QMapControlConfig.cmake
        INSTALL_DESTINATION ${INSTALL_EXPORTS_DIR}
        PATH_VARS INCLUDE_INSTALL_DIR SYSCONFIG_INSTALL_DIR)

write_basic_package_version_file(
        ${CMAKE_BINARY_DIR}/QMapControlConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion)

install(EXPORT qmapcontrol-export
        FILE
        QMapControlTargets.cmake
        NAMESPACE
        qmapcontrol::
        DESTINATION
        ${INSTALL_EXPORTS_DIR}
        )

install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/QMapControlConfig.cmake
        ${CMAKE_BINARY_DIR}/QMapControlConfigVersion.cmake
        DESTINATION ${INSTALL_EXPORTS_DIR}
        )
