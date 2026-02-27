#!/usr/bin/env python3
"""Offline utility to migrate stationchat data from SQLite to MariaDB."""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import sqlite3
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable

try:
    import pymysql
    from pymysql.connections import Connection
except ImportError:  # handled in runtime when not in dry-run mode
    pymysql = None
    Connection = Any

TABLE_ORDER = [
    "avatar",
    "room",
    "room_administrator",
    "room_moderator",
    "room_ban",
    "room_invite",
    "persistent_message",
    "friend",
    "ignore",
    "schema_version",
]


@dataclass(frozen=True)
class TableConfig:
    name: str
    columns: tuple[str, ...]
    order_by: tuple[str, ...]
    auto_increment_pk: str | None = None


TABLES = {
    "avatar": TableConfig(
        name="avatar",
        columns=("id", "user_id", "name", "address", "attributes"),
        order_by=("id",),
        auto_increment_pk="id",
    ),
    "room": TableConfig(
        name="room",
        columns=(
            "id",
            "creator_id",
            "creator_name",
            "creator_address",
            "room_name",
            "room_topic",
            "room_password",
            "room_prefix",
            "room_address",
            "room_attributes",
            "room_max_size",
            "room_message_id",
            "created_at",
            "node_level",
        ),
        order_by=("id",),
        auto_increment_pk="id",
    ),
    "room_administrator": TableConfig(
        name="room_administrator",
        columns=("administrator_avatar_id", "room_id"),
        order_by=("administrator_avatar_id", "room_id"),
    ),
    "room_moderator": TableConfig(
        name="room_moderator",
        columns=("moderator_avatar_id", "room_id"),
        order_by=("moderator_avatar_id", "room_id"),
    ),
    "room_ban": TableConfig(
        name="room_ban",
        columns=("banned_avatar_id", "room_id"),
        order_by=("banned_avatar_id", "room_id"),
    ),
    "room_invite": TableConfig(
        name="room_invite",
        columns=("invited_avatar_id", "room_id"),
        order_by=("invited_avatar_id", "room_id"),
    ),
    "persistent_message": TableConfig(
        name="persistent_message",
        columns=(
            "id",
            "avatar_id",
            "from_name",
            "from_address",
            "subject",
            "sent_time",
            "status",
            "folder",
            "category",
            "message",
            "oob",
        ),
        order_by=("id",),
        auto_increment_pk="id",
    ),
    "friend": TableConfig(
        name="friend",
        columns=("avatar_id", "friend_avatar_id", "comment"),
        order_by=("avatar_id", "friend_avatar_id"),
    ),
    "ignore": TableConfig(
        name="ignore",
        columns=("avatar_id", "ignore_avatar_id"),
        order_by=("avatar_id", "ignore_avatar_id"),
    ),
    "schema_version": TableConfig(
        name="schema_version",
        columns=("version", "applied_at"),
        order_by=("version",),
    ),
}


def quote_identifier(name: str) -> str:
    return f"`{name}`"


def normalize_value(value: Any) -> str:
    if value is None:
        return "NULL"
    if isinstance(value, (bytes, bytearray, memoryview)):
        return bytes(value).hex()
    return str(value)


def update_digest(digest: "hashlib._Hash", row: tuple[Any, ...]) -> None:
    normalized = "\x1f".join(normalize_value(v) for v in row)
    digest.update(normalized.encode("utf-8"))
    digest.update(b"\n")


def fetch_sqlite_rows(conn: sqlite3.Connection, table: TableConfig) -> list[tuple[Any, ...]]:
    select_cols = ", ".join(quote_identifier(c) for c in table.columns)
    order_by = ", ".join(quote_identifier(c) for c in table.order_by)
    sql = f"SELECT {select_cols} FROM {quote_identifier(table.name)} ORDER BY {order_by}"
    return conn.execute(sql).fetchall()


def verify_target_empty(conn: Connection, table: str) -> None:
    with conn.cursor() as cur:
        cur.execute(f"SELECT COUNT(*) FROM {quote_identifier(table)}")
        count = cur.fetchone()[0]
    if count != 0:
        raise RuntimeError(
            f"Target table '{table}' is not empty ({count} rows). "
            "Start from a clean baseline schema or use --truncate-target."
        )


def truncate_target(conn: Connection, table: str) -> None:
    with conn.cursor() as cur:
        cur.execute(f"TRUNCATE TABLE {quote_identifier(table)}")


def insert_rows(conn: Connection, table: TableConfig, rows: Iterable[tuple[Any, ...]]) -> None:
    rows = list(rows)
    if not rows:
        return
    cols = ", ".join(quote_identifier(c) for c in table.columns)
    placeholders = ", ".join(["%s"] * len(table.columns))
    sql = f"INSERT INTO {quote_identifier(table.name)} ({cols}) VALUES ({placeholders})"
    with conn.cursor() as cur:
        cur.executemany(sql, rows)


def sync_auto_increment(conn: Connection, table: TableConfig) -> None:
    if not table.auto_increment_pk:
        return
    with conn.cursor() as cur:
        cur.execute(
            f"SELECT COALESCE(MAX({quote_identifier(table.auto_increment_pk)}), 0) "
            f"FROM {quote_identifier(table.name)}"
        )
        max_pk = int(cur.fetchone()[0])
        cur.execute(
            f"ALTER TABLE {quote_identifier(table.name)} AUTO_INCREMENT = %s",
            (max_pk + 1,),
        )


def fetch_mariadb_rows(conn: Connection, table: TableConfig) -> list[tuple[Any, ...]]:
    select_cols = ", ".join(quote_identifier(c) for c in table.columns)
    order_by = ", ".join(quote_identifier(c) for c in table.order_by)
    sql = f"SELECT {select_cols} FROM {quote_identifier(table.name)} ORDER BY {order_by}"
    with conn.cursor() as cur:
        cur.execute(sql)
        return list(cur.fetchall())


def checksum_and_count(rows: Iterable[tuple[Any, ...]]) -> tuple[int, str]:
    digest = hashlib.sha256()
    count = 0
    for row in rows:
        update_digest(digest, row)
        count += 1
    return count, digest.hexdigest()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Migrate stationchat SQLite data into MariaDB V001 baseline schema."
    )
    parser.add_argument("--sqlite-path", default="stationchat.db", help="Path to SQLite source DB")
    parser.add_argument("--mariadb-host", default="127.0.0.1")
    parser.add_argument("--mariadb-port", type=int, default=3306)
    parser.add_argument("--mariadb-user", default="root")
    parser.add_argument("--mariadb-password", default="")
    parser.add_argument("--mariadb-schema", default="stationchat")
    parser.add_argument(
        "--report-path",
        default=f"migration_report_{dt.datetime.now(dt.timezone.utc).strftime('%Y%m%dT%H%M%SZ')}.json",
        help="JSON report output path",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Read/analyze SQLite and emit report without writing to MariaDB",
    )
    parser.add_argument(
        "--truncate-target",
        action="store_true",
        help="TRUNCATE target tables in reverse dependency order before migration",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    sqlite_path = Path(args.sqlite_path)
    if not sqlite_path.exists():
        print(f"SQLite database not found: {sqlite_path}", file=sys.stderr)
        return 2

    report: dict[str, Any] = {
        "generated_at_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "dry_run": bool(args.dry_run),
        "sqlite_path": str(sqlite_path),
        "mariadb": {
            "host": args.mariadb_host,
            "port": args.mariadb_port,
            "schema": args.mariadb_schema,
            "user": args.mariadb_user,
        },
        "table_order": TABLE_ORDER,
        "tables": {},
        "status": "in_progress",
    }

    sqlite_conn = sqlite3.connect(str(sqlite_path))
    sqlite_conn.row_factory = None

    maria_conn: Connection | None = None
    try:
        if not args.dry_run:
            if pymysql is None:
                raise RuntimeError(
                    "pymysql is required for non-dry-run execution. Install with: pip install pymysql"
                )
            maria_conn = pymysql.connect(
                host=args.mariadb_host,
                port=args.mariadb_port,
                user=args.mariadb_user,
                password=args.mariadb_password,
                database=args.mariadb_schema,
                charset="utf8mb4",
                autocommit=False,
            )

            if args.truncate_target:
                for table_name in reversed(TABLE_ORDER):
                    truncate_target(maria_conn, table_name)
                maria_conn.commit()
            else:
                for table_name in TABLE_ORDER:
                    verify_target_empty(maria_conn, table_name)

        for table_name in TABLE_ORDER:
            table = TABLES[table_name]
            source_rows = fetch_sqlite_rows(sqlite_conn, table)
            source_count, source_checksum = checksum_and_count(source_rows)
            table_report: dict[str, Any] = {
                "source_count": source_count,
                "source_checksum": source_checksum,
                "target_count": None,
                "target_checksum": None,
                "verified": None,
            }

            if args.dry_run:
                table_report["verified"] = "dry_run"
            else:
                assert maria_conn is not None
                insert_rows(maria_conn, table, source_rows)
                sync_auto_increment(maria_conn, table)
                target_rows = fetch_mariadb_rows(maria_conn, table)
                target_count, target_checksum = checksum_and_count(target_rows)
                table_report["target_count"] = target_count
                table_report["target_checksum"] = target_checksum
                table_report["verified"] = (
                    source_count == target_count and source_checksum == target_checksum
                )
                if not table_report["verified"]:
                    raise RuntimeError(
                        f"Verification failed for table '{table_name}' "
                        f"(source_count={source_count}, target_count={target_count})"
                    )

            report["tables"][table_name] = table_report

        if maria_conn is not None:
            maria_conn.commit()
        report["status"] = "ok"
        return 0
    except Exception as exc:  # keep report for operational troubleshooting
        if maria_conn is not None:
            maria_conn.rollback()
        report["status"] = "failed"
        report["error"] = str(exc)
        print(f"Migration failed: {exc}", file=sys.stderr)
        return 1
    finally:
        sqlite_conn.close()
        if maria_conn is not None:
            maria_conn.close()
        report_path = Path(args.report_path)
        report_path.parent.mkdir(parents=True, exist_ok=True)
        report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
        print(f"Migration report written to {report_path}")


if __name__ == "__main__":
    raise SystemExit(main())
