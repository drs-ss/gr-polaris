INCLUDE(FindPkgConfig)
PKG_CHECK_MODULES(PC_POLARIS polaris)

FIND_PATH(
    POLARIS_INCLUDE_DIRS
    NAMES polaris/api.h
    HINTS $ENV{POLARIS_DIR}/include
        ${PC_POLARIS_INCLUDEDIR}
    PATHS ${CMAKE_INSTALL_PREFIX}/include
          /usr/local/include
          /usr/include
)

FIND_LIBRARY(
    POLARIS_LIBRARIES
    NAMES gnuradio-polaris
    HINTS $ENV{POLARIS_DIR}/lib
        ${PC_POLARIS_LIBDIR}
    PATHS ${CMAKE_INSTALL_PREFIX}/lib
          ${CMAKE_INSTALL_PREFIX}/lib64
          /usr/local/lib
          /usr/local/lib64
          /usr/lib
          /usr/lib64
)

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(POLARIS DEFAULT_MSG POLARIS_LIBRARIES POLARIS_INCLUDE_DIRS)
MARK_AS_ADVANCED(POLARIS_LIBRARIES POLARIS_INCLUDE_DIRS)

