# don't allow in-source project generation
if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(WARNING "\n\nCMake generation is not allowed within the source directory!\n")
    message(STATUS "Cancelling CMake and cleaning up source tree...")
    execute_process(COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_BINARY_DIR}/CMakeFiles")
    math(EXPR Crash 0/0)
    message(FATAL_ERROR "CMake should have crashed - this is a failsafe in case the call used to trigger the crash gets fixed.")
endif()

cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

project(stationapi VERSION 1.1)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(ModernCpp)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(UDPLIBRARY_SOURCE_DIR "${PROJECT_SOURCE_DIR}/externals/udplibrary")

if (EXISTS "${UDPLIBRARY_SOURCE_DIR}/CMakeLists.txt")
    message(STATUS "Detected udplibrary in externals/udplibrary")
    set(HAVE_UDPLIBRARY 1)
else()
    find_package(Git QUIET)

    if (GIT_FOUND)
        set(UDPLIBRARY_FETCH_ROOT "${CMAKE_BINARY_DIR}/_deps")
        set(UDPLIBRARY_REPOSITORY_DIR "${UDPLIBRARY_FETCH_ROOT}/stationapi-old")
        set(UDPLIBRARY_SOURCE_DIR "${UDPLIBRARY_REPOSITORY_DIR}/udplibrary")

        file(MAKE_DIRECTORY "${UDPLIBRARY_FETCH_ROOT}")

        if (NOT EXISTS "${UDPLIBRARY_REPOSITORY_DIR}/.git")
            message(STATUS "udplibrary not found locally. Cloning SWG-Source/stationapi-old...")
            execute_process(
                COMMAND "${GIT_EXECUTABLE}" clone --depth 1 https://github.com/SWG-Source/stationapi-old.git "${UDPLIBRARY_REPOSITORY_DIR}"
                RESULT_VARIABLE UDPLIBRARY_CLONE_RESULT
                OUTPUT_VARIABLE UDPLIBRARY_CLONE_OUTPUT
                ERROR_VARIABLE UDPLIBRARY_CLONE_ERROR)

            if (NOT UDPLIBRARY_CLONE_RESULT EQUAL 0)
                message(FATAL_ERROR "Failed to clone SWG-Source/stationapi-old: ${UDPLIBRARY_CLONE_ERROR}")
            endif()

            message(STATUS "Downloaded SWG-Source/stationapi-old for udplibrary")
        else()
            message(STATUS "Using existing SWG-Source/stationapi-old clone in build dependencies")
        endif()

        if (EXISTS "${UDPLIBRARY_SOURCE_DIR}/CMakeLists.txt")
            message(STATUS "Detected udplibrary in downloaded SWG-Source/stationapi-old repository")
            set(HAVE_UDPLIBRARY 1)
        endif()
    endif()

    if (NOT HAVE_UDPLIBRARY)
        message(FATAL_ERROR "udplibrary required but unavailable. Install git and rerun CMake, or copy swg source udplibrary into externals/udplibrary")
    endif()
endif()

add_definitions(-DBOOST_ALL_NO_LIB)
option(STATIONAPI_USE_STATIC_BOOST "Link Boost libraries statically" OFF)
set(Boost_USE_STATIC_LIBS ${STATIONAPI_USE_STATIC_BOOST})
set(Boost_USE_MULTITHREADED ON)

option(STATIONCHAT_WITH_MARIADB "Build stationchat with MariaDB backend" ON)

if (NOT STATIONCHAT_WITH_MARIADB)
    message(FATAL_ERROR "stationchat requires MariaDB backend enabled")
endif()

find_package(Boost COMPONENTS program_options REQUIRED)

if (STATIONCHAT_WITH_MARIADB)
    find_package(MariaDBClient REQUIRED)
endif()

add_subdirectory(externals)
add_subdirectory(src)
add_subdirectory(tests)

install(FILES
    extras/logger.cfg.dist
    DESTINATION etc/stationapi
    RENAME logger.cfg)

install(FILES
    extras/swgchat.cfg.dist
    DESTINATION etc/stationapi
    RENAME swgchat.cfg)

install(FILES
    extras/init_database_mariadb.sql
    extras/migrations/mariadb/V001__baseline.sql
    DESTINATION share/stationapi)
