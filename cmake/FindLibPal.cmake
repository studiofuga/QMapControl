# - Try to find LibPal
# Once done this will define
#  LibPal_FOUND - System has LibXml2
#  LibPal_INCLUDE_DIRS - The LibXml2 include directories
#  LibPal_LIBRARIES - The libraries needed to use LibXml2
#  LibPal_DEFINITIONS - Compiler switches required for using LibXml2

find_package(PkgConfig)
pkg_check_modules(PC_LibPal QUIET libpal)

set(LibPal_DEFINITIONS ${PC_LibPal_CFLAGS_OTHER})

find_path(LibPal_INCLUDE_DIR pal/pal.h
        HINTS ${PC_LibPal_INCLUDEDIR} ${PC_LibPal_INCLUDE_DIRS}
        PATH_SUFFIXES pal)

find_library(LibPal_LIBRARY NAMES pal libpal
        HINTS ${PC_LibPal_LIBDIR} ${PC_LibPal_LIBRARY_DIRS})

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LibPal_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(LibPal DEFAULT_MSG
        LibPal_LIBRARY LibPal_INCLUDE_DIR)

mark_as_advanced(LibPal_INCLUDE_DIR LibPal_LIBRARY)

set(LibPal_LIBRARIES ${LibPal_LIBRARY})
set(LibPal_INCLUDE_DIRS ${LibPal_INCLUDE_DIR})
