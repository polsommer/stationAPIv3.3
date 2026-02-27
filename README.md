# stationapi [![Build Status](https://travis-ci.org/apathyboy/stationapi.svg?branch=master)](https://travis-ci.com/apathyboy/stationapi) #

A base library at the core of applications that implement chat and login functionality across galaxies.

# stationchat

An open implementation of the chat gateway that SOE based games used to provide various social communication features such as mail, custom chat rooms, friend management, etc.

Like my work and want to support my free and open-source contributions? 

[![Donate](https://img.shields.io/badge/Donate-PayPal-green.svg)](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=8KCAU8HB9J7YU)

## Implementation ##

Uses the SOE libraries to implement chat features in a standalone utility. Ideally, the completed implementation would allow for multiple galaxies to connect and allow players to communicate across them.

## External Dependencies ##

* c++14 compatible compiler
* boost::program_options
* MariaDB Connector/C (**required**)
* udplibrary - bundled in the Star Wars Galaxies official source

## Building ##

Copy the udplibrary directory from the Star Wars Galaxies offical source to the top level swgchat directory, install the remaining dependencies via a package manager, then use either CMake directly or the Ant wrapper.

### Raspberry Pi 4 ###

On Raspberry Pi OS (Debian-based), install the core build dependencies with apt (the `udplibrary` directory is still mandatory and must be copied into the repository root before configuring):

    sudo apt update
    sudo apt install -y build-essential cmake pkg-config libboost-program-options-dev libmariadb-dev

Both 64-bit and 32-bit Raspberry Pi OS are expected to work:

* 64-bit Pi OS uses `aarch64`/`arm64` user space.
* 32-bit Pi OS uses `armhf` user space.
* MariaDB client library lookup supports the corresponding multiarch directories for both `aarch64` and `armhf` layouts.

Minimal CMake flow from the repo root:

    mkdir build && cd build
    cmake ..
    cmake --build .

### Option 1: CMake (manual) ###

    mkdir build; cd build
    cmake ..
    cmake --build .

To explicitly link Boost statically, configure with:

    cmake -DSTATIONAPI_USE_STATIC_BOOST=ON ..

### Option 2: Apache Ant (recommended for Linux command workflows) ###

From the project root:

    ant configure
    ant compile_chat

Useful Ant targets:

* `ant compile_chat` - configure then build only the `stationchat` target
* `ant test` - configure/build then run `ctest`
* `ant run` - configure/build then start `stationchat`
* `ant clean` - remove the build directory

You can also pick a build type or generator:

    ant -Dbuild.type=Debug build
    ant -Dcmake.generator=Ninja build

## Database Initialization ##

stationchat uses MariaDB for all runtime storage.

1. Create a MariaDB schema for stationchat.
2. Set **database_engine = mariadb** and fill in **database_host**, **database_port**, **database_user**, **database_password**, and **database_schema** in `swgchat.cfg`.
3. Apply the baseline migration:

       mysql -h <host> -P <port> -u <user> -p <schema> < extras/migrations/mariadb/V001__baseline.sql

## Schema Versioning and Migrations ##

stationchat validates MariaDB schema compatibility during startup:

* selected backend is logged
* backend capabilities are logged (upsert strategy, blob handling, transaction isolation)
* current schema version is logged
* known pending migrations are logged

If the schema is incompatible, stationchat exits immediately with an actionable error.

Migration scripts are MariaDB-only, versioned, and ordered in `extras/migrations/mariadb/`.

For upgrades, apply newer MariaDB migration files in version order.

## Running ##

A default configuration is created when building the project. Configure the listen address/ports and ensure **database_engine = mariadb** in **build/bin/stationchat.cfg**. Then run the following commands from the project root:

### Windows ###

    cd build/bin
    .\Debug\stationchat.exe

### Linux ###

    cd build/bin
    ./stationchat

## Final Notes ##

It is recommended to copy the **build/bin** directory to another location after building to ensure the configuration files are not overwritten by future changes to the default versions of these files.
