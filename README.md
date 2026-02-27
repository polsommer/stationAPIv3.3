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
* MariaDB Connector/C
* udplibrary - bundled in the Star Wars Galaxies official source

## Building ##

Copy the udplibrary directory from the Star Wars Galaxies offical source to the top level swgchat directory, install the remaining dependencies via a package manager, then run the following:

    mkdir build; cd build
    cmake ..
    cmake --build .

## Database Initialization ##

stationchat uses MariaDB for runtime storage. Set **database_engine = mariadb** and fill in **database_host**, **database_port**, **database_user**, **database_password**, and **database_schema** in `swgchat.cfg`.


## Schema Versioning and Migrations ##

stationchat validates MariaDB schema compatibility during startup:

* selected backend is logged
* backend capabilities are logged (upsert strategy, blob handling, transaction isolation)
* current schema version is logged
* known pending migrations are logged

If the schema is incompatible, stationchat exits immediately with an actionable error.

Migration scripts are versioned and ordered in `extras/migrations/mariadb/`.

For a clean install, apply the baseline migration `V001__baseline.sql` for MariaDB.

## Running ##

A default configuration and database is created when building the project. Configure the listen address/ports in **build/bin/stationchat.cfg**. Then run the following commands from the project root:

### Windows ###

    cd build/bin
    .\Debug\stationchat.exe

### Linux ###

    cd build/bin
    ./stationchat

## Final Notes ##

It is recommended to copy the **build/bin** directory to another location after building to ensure the configuration files are not overwritten by future changes to the default versions of these files.
