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
* sqlite3
* MariaDB Connector/C (optional, required for MariaDB runtime provider)
* udplibrary - bundled in the Star Wars Galaxies official source

## Building ##

Copy the udplibrary directory from the Star Wars Galaxies offical source to the top level swgchat directory, install the remaining dependencies via a package manager, then run the following:

    mkdir build; cd build
    cmake ..
    cmake --build .

## Database Initialization ##

By default, a clean database instance is provided and placed with the default configuration files in the **build/bin** directory; therefore, nothing needs to be done for new installations, the db is already created and placed in the appropriate location.

To create a new, clean database instance, use the following commands:

    sqlite3 chat.db
    sqlite> .read /path/to/extras/init_database.sql

Then update the **database_path** config option with the full path to the database.

For MariaDB deployments, set **database_engine = mariadb** and fill in **database_host**, **database_port**, **database_user**, **database_password**, and **database_schema** in `swgchat.cfg`.


## Schema Versioning and Migrations ##

stationchat now validates schema compatibility during startup for both SQLite and MariaDB:

* selected backend is logged
* backend capabilities are logged (upsert strategy, blob handling, transaction isolation)
* current schema version is logged
* known pending migrations are logged

If the schema is incompatible, stationchat exits immediately with an actionable error.

Migration scripts are versioned and ordered in:

* `extras/migrations/sqlite/`
* `extras/migrations/mariadb/`

For a clean install, apply the baseline migration `V001__baseline.sql` for your backend.

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


## Operational Playbook: SQLite -> MariaDB (Minimal Downtime) ##

1. **Prepare MariaDB schema ahead of cutover**
   * Create target database/schema and credentials.
   * Apply baseline migration: `extras/migrations/mariadb/V001__baseline.sql`.

2. **Warm copy data from SQLite while chat is still online**
   * Export SQLite data using `.mode insert` or equivalent tooling.
   * Transform/import into MariaDB tables (`avatar`, `room`, `room_*`, `persistent_message`, `friend`, `ignore`).
   * Keep `schema_version` at `version=1`.

3. **Short write-freeze cutover window**
   * Stop stationchat briefly (or block new writes at the ingress layer).
   * Export and apply only final delta changes from SQLite to MariaDB.

4. **Switch configuration and restart**
   * Set `database_engine = mariadb`.
   * Set `database_host`, `database_port`, `database_user`, `database_password`, `database_schema`.
   * Start stationchat and verify startup logs report MariaDB backend and matching schema version.

5. **Post-cutover validation**
   * Validate login/chat room discovery/persistent mail workflows.
   * Keep original SQLite file as rollback artifact until confidence window expires.

This approach minimizes downtime to the final delta sync and restart interval.
