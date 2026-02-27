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


## SQLite -> MariaDB Migration Utility (Offline Cutover) ##

Use `extras/sqlite_to_mariadb_migrator.py` to copy data from `stationchat.db` (SQLite) into a MariaDB schema initialized with `extras/migrations/mariadb/V001__baseline.sql`.

### What it migrates ###

The tool migrates in foreign-key-safe order and preserves protocol-visible primary keys:

1. `avatar`
2. `room`
3. `room_administrator`
4. `room_moderator`
5. `room_ban`
6. `room_invite`
7. `persistent_message`
8. `friend`
9. `ignore`
10. `schema_version`

For each table, it records source/target row counts and SHA-256 checksums in a JSON migration report.

### Prerequisites ###

1. Apply MariaDB baseline schema first:

       mariadb -u <user> -p <schema> < extras/migrations/mariadb/V001__baseline.sql

2. Install Python dependency used for MariaDB connectivity:

       pip install pymysql

### Dry run (no writes) ###

A dry run reads SQLite only and emits a report with source counts/checksums:

    python3 extras/sqlite_to_mariadb_migrator.py \
      --sqlite-path /path/to/stationchat.db \
      --dry-run \
      --report-path /tmp/stationchat-migration-dryrun.json

### Cutover run ###

Run against an empty baseline schema (default safety check). Use `--truncate-target` only when intentionally re-running against an existing target:

    python3 extras/sqlite_to_mariadb_migrator.py \
      --sqlite-path /path/to/stationchat.db \
      --mariadb-host 127.0.0.1 \
      --mariadb-port 3306 \
      --mariadb-user stationchat \
      --mariadb-password '***' \
      --mariadb-schema stationchat \
      --report-path /tmp/stationchat-migration.json

### Rollback notes ###

* The utility is operational/offline only; it does not change runtime chat protocol behavior.
* Keep the original SQLite database file unchanged until MariaDB validation is complete.
* If verification fails, the MariaDB transaction is rolled back and the report includes the error.
* To revert a completed cutover, point `swgchat.cfg` back to SQLite settings and restart stationchat.
