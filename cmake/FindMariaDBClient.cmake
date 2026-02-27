include(FindPackageHandleStandardArgs)
include(FindPkgConfig)

set(_MariaDBClient_HINTS
    $ENV{MariaDBClient_ROOT}
    ${MariaDBClient_ROOT}
    ${MariaDBClient_INCLUDEDIR}
    ${MariaDBClient_LIBRARYDIR}
    $ENV{MARIADB_ROOT}
    ${MARIADB_ROOT}
    $ENV{MARIADB_DIR}
    ${MARIADB_DIR})

pkg_check_modules(PC_MARIADB QUIET mariadb libmariadb mysqlclient)

find_path(MariaDBClient_INCLUDE_DIR
    NAMES mysql.h mysql/mysql.h mariadb/mysql.h
    HINTS
        ${_MariaDBClient_HINTS}
        ${PC_MARIADB_INCLUDEDIR}
        ${PC_MARIADB_INCLUDE_DIRS}
    PATH_SUFFIXES
        include
        include/mysql
        include/mariadb)

find_library(MariaDBClient_LIBRARY
    NAMES mariadb mariadbclient mysqlclient libmariadb
    HINTS
        ${_MariaDBClient_HINTS}
        ${PC_MARIADB_LIBDIR}
        ${PC_MARIADB_LIBRARY_DIRS}
    PATH_SUFFIXES
        lib
        lib64
        lib/x86_64-linux-gnu
        lib/aarch64-linux-gnu)

find_package_handle_standard_args(MariaDBClient
    REQUIRED_VARS MariaDBClient_LIBRARY MariaDBClient_INCLUDE_DIR
    FAIL_MESSAGE "Could NOT find MariaDB client library (missing headers and/or library). Install MariaDB Connector/C development files (e.g. libmariadb-dev or mariadb-connector-c-devel), or set MariaDBClient_ROOT to your MariaDB installation root.")

if (MariaDBClient_FOUND AND NOT TARGET MariaDB::Client)
    add_library(MariaDB::Client UNKNOWN IMPORTED)
    set_target_properties(MariaDB::Client PROPERTIES
        IMPORTED_LOCATION "${MariaDBClient_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${MariaDBClient_INCLUDE_DIR}")
endif()

mark_as_advanced(MariaDBClient_ROOT MariaDBClient_INCLUDE_DIR MariaDBClient_LIBRARY)
unset(_MariaDBClient_HINTS)
