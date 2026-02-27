include(FindPackageHandleStandardArgs)

find_path(MariaDBClient_INCLUDE_DIR
    NAMES mysql/mysql.h mariadb/mysql.h
    HINTS
        $ENV{MariaDBClient_ROOT}
        ${MariaDBClient_ROOT}
        ${MariaDBClient_INCLUDEDIR})

find_library(MariaDBClient_LIBRARY
    NAMES mariadb mariadbclient mysqlclient libmariadb
    HINTS
        $ENV{MariaDBClient_ROOT}
        ${MariaDBClient_ROOT}
        ${MariaDBClient_LIBRARYDIR})

find_package_handle_standard_args(MariaDBClient DEFAULT_MSG
    MariaDBClient_LIBRARY
    MariaDBClient_INCLUDE_DIR)

if (MariaDBClient_FOUND AND NOT TARGET MariaDB::Client)
    add_library(MariaDB::Client UNKNOWN IMPORTED)
    set_target_properties(MariaDB::Client PROPERTIES
        IMPORTED_LOCATION "${MariaDBClient_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${MariaDBClient_INCLUDE_DIR}")
endif()

mark_as_advanced(MariaDBClient_ROOT MariaDBClient_INCLUDE_DIR MariaDBClient_LIBRARY)
