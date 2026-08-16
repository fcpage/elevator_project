from __future__ import annotations

import json
import re
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Any, Protocol


class DatabaseCursor(Protocol):
    def execute(self, operation: str, params: tuple[Any, ...] | None = None) -> Any:
        ...

    def fetchall(self) -> list[Any]:
        ...


@dataclass(frozen=True)
class ObservedRow:
    table: str
    index: int
    values: dict[str, Any]


_IDENTIFIER = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
_GUI_SOURCES = {"date", "time", "floor", "remote"}


def _identifier(value: object, *, field: str) -> str:
    if not isinstance(value, str):
        raise ValueError(f"schema profile {field} must be a string SQL identifier")
    text = str(value)
    if not _IDENTIFIER.fullmatch(text):
        raise ValueError(f"schema profile {field} must be a SQL identifier, got {text!r}")
    return text


def _quoted(value: str) -> str:
    return f"`{value}`"


class SchemaProfile:
    """A data-driven description of one simulator-owned database schema."""

    def __init__(self, manifest_path: Path) -> None:
        self.manifest_path = manifest_path
        try:
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        except json.JSONDecodeError as error:
            raise ValueError(f"invalid schema profile {manifest_path}: {error}") from error
        if manifest.get("version") != 1:
            raise ValueError(f"{manifest_path}: expected schema profile version 1")

        self.name = str(manifest.get("name", ""))
        if not self.name:
            raise ValueError(f"{manifest_path}: profile name is required")
        self.schema_name = _identifier(manifest.get("schema_name"), field="schema_name")
        sql_file = manifest.get("sql_file")
        if not isinstance(sql_file, str) or not sql_file:
            raise ValueError(f"{manifest_path}: sql_file is required")
        self.schema_sql_path = (manifest_path.parent / sql_file).resolve()
        if not self.schema_sql_path.is_file():
            raise ValueError(f"{manifest_path}: SQL file does not exist: {sql_file}")

        raw_tables = manifest.get("tables")
        if not isinstance(raw_tables, dict) or not raw_tables:
            raise ValueError(f"{manifest_path}: tables must be a non-empty object")
        self._tables: dict[str, dict[str, Any]] = {}
        self.required_columns: dict[str, tuple[str, ...]] = {}
        self._table_by_name: dict[str, dict[str, Any]] = {}
        for role, raw_table in raw_tables.items():
            if not isinstance(raw_table, dict):
                raise ValueError(f"{manifest_path}: tables.{role} must be an object")
            table_name = _identifier(raw_table.get("name"), field=f"tables.{role}.name")
            index_column = _identifier(
                raw_table.get("index_column"), field=f"tables.{role}.index_column"
            )
            raw_columns = raw_table.get("columns")
            if not isinstance(raw_columns, list) or not raw_columns:
                raise ValueError(f"{manifest_path}: tables.{role}.columns must be a non-empty list")
            columns = tuple(
                _identifier(column, field=f"tables.{role}.columns") for column in raw_columns
            )
            if index_column not in columns:
                raise ValueError(f"{manifest_path}: {index_column} is not a column of {table_name}")
            table = {
                "name": table_name,
                "index_column": index_column,
                "columns": columns,
                "reset": bool(raw_table.get("reset", False)),
                "observe": bool(raw_table.get("observe", True)),
            }
            self._tables[str(role)] = table
            self._table_by_name[table_name] = table
            self.required_columns[table_name] = columns

        gui_request = manifest.get("gui_request")
        if not isinstance(gui_request, dict):
            raise ValueError(f"{manifest_path}: gui_request must be an object")
        gui_table_role = str(gui_request.get("table", ""))
        if gui_table_role not in self._tables:
            raise ValueError(f"{manifest_path}: gui_request.table must name a table role")
        raw_fields = gui_request.get("fields")
        if not isinstance(raw_fields, list) or not raw_fields:
            raise ValueError(f"{manifest_path}: gui_request.fields must be a non-empty list")
        fields: list[tuple[str, str]] = []
        gui_columns = self._tables[gui_table_role]["columns"]
        for field in raw_fields:
            if not isinstance(field, dict):
                raise ValueError(f"{manifest_path}: gui_request fields must be objects")
            column = _identifier(field.get("column"), field="gui_request field column")
            source = str(field.get("source", ""))
            if column not in gui_columns or source not in _GUI_SOURCES:
                raise ValueError(f"{manifest_path}: invalid gui_request field {field!r}")
            fields.append((column, source))
        self._gui_table_role = gui_table_role
        self._gui_fields = tuple(fields)

        self.inbound_table = self._tables[gui_table_role]["name"]
        outbound = manifest.get("outbound_table")
        if not isinstance(outbound, str) or outbound not in self._tables:
            raise ValueError(f"{manifest_path}: outbound_table must name a table role")
        self.outbound_table = self._tables[outbound]["name"]
        self.reset_tables = tuple(table["name"] for table in self._tables.values() if table["reset"])
        self.observed_tables = tuple(table["name"] for table in self._tables.values() if table["observe"])

    def schema_sql(self) -> str:
        return self.schema_sql_path.read_text(encoding="utf-8-sig")

    def validate(self, cursor: DatabaseCursor) -> None:
        cursor.execute(
            "SELECT TABLE_NAME, COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS "
            "WHERE TABLE_SCHEMA = %s ORDER BY TABLE_NAME, ORDINAL_POSITION",
            (self.schema_name,),
        )
        found: dict[str, list[str]] = {}
        canonical_tables = {name.casefold(): name for name in self.required_columns}
        for table, column in cursor.fetchall():
            canonical = canonical_tables.get(str(table).casefold(), str(table))
            found.setdefault(canonical, []).append(str(column))
        for table, expected_columns in self.required_columns.items():
            actual = tuple(found.get(table, []))
            if actual != expected_columns:
                raise RuntimeError(
                    f"{self.name}: table {table} columns do not match its profile; "
                    f"expected={expected_columns}, actual={actual}"
                )

    def reset(self, cursor: DatabaseCursor) -> None:
        for table in self.reset_tables:
            cursor.execute(f"DELETE FROM {_quoted(self.schema_name)}.{_quoted(table)}")

    def insert_gui_request(
        self,
        cursor: DatabaseCursor,
        *,
        floor: int,
        remote: int = 0,
        timestamp: datetime | None = None,
    ) -> None:
        if floor not in (1, 2, 3):
            raise ValueError("GUI request floor must be 1, 2, or 3")
        observed_at = timestamp or datetime.now()
        source_values: dict[str, Any] = {
            "date": observed_at.date(),
            "time": observed_at.time().replace(microsecond=0),
            "floor": floor,
            "remote": remote,
        }
        columns = ", ".join(_quoted(column) for column, _ in self._gui_fields)
        values = tuple(source_values[source] for _, source in self._gui_fields)
        cursor.execute(
            f"INSERT INTO {_quoted(self.schema_name)}.{_quoted(self.inbound_table)} "
            f"({columns}) VALUES ({', '.join('%s' for _ in values)})",
            values,
        )

    def fetch_new_rows(
        self,
        cursor: DatabaseCursor,
        table: str,
        after_index: int,
    ) -> list[ObservedRow]:
        descriptor = self._table_by_name.get(table)
        if descriptor is None or table not in self.observed_tables:
            raise ValueError(f"unsupported observed table: {table}")
        index_column = descriptor["index_column"]
        cursor.execute(
            f"SELECT * FROM {_quoted(self.schema_name)}.{_quoted(table)} "
            f"WHERE {_quoted(index_column)} > %s ORDER BY {_quoted(index_column)} ASC",
            (after_index,),
        )
        rows: list[ObservedRow] = []
        for raw in cursor.fetchall():
            values = dict(zip(descriptor["columns"], raw))
            rows.append(ObservedRow(table=table, index=int(values[index_column]), values=values))
        return rows


def load_schema_profile(schema_directory: Path, name: str = "agreed-v1") -> SchemaProfile:
    for manifest_path in sorted(schema_directory.glob("*.profile.json")):
        profile = SchemaProfile(manifest_path)
        if profile.name == name:
            return profile
    available = [SchemaProfile(path).name for path in sorted(schema_directory.glob("*.profile.json"))]
    raise ValueError(f"unknown schema profile {name!r}; available profiles: {', '.join(available) or '(none)'}")
