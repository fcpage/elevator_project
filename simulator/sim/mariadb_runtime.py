from __future__ import annotations

import asyncio
import os
import re
import shutil
import socket
import subprocess
import sys
import time
from pathlib import Path
from typing import Any, Callable

from sim.config import DEFAULT_DB_PORT, SimulatorPaths
from sim.schema_profile import ObservedRow, SchemaProfile


def _connector() -> Any:
    try:
        import mysql.connector
    except ImportError as error:
        raise RuntimeError(
            "mysql-connector-python is missing; run simulator/setup first"
        ) from error
    return mysql.connector


def find_executable(*names: str) -> str | None:
    for name in names:
        located = shutil.which(name)
        if located:
            return located
    if os.name == "nt":
        program_files = Path(os.environ.get("ProgramFiles", "C:/Program Files"))
        for directory in sorted(program_files.glob("MariaDB */bin"), reverse=True):
            for name in names:
                candidate = directory / f"{name}.exe"
                if candidate.exists():
                    return str(candidate)
    return None


class IsolatedMariaDb:
    def __init__(
        self,
        paths: SimulatorPaths,
        profile: SchemaProfile,
        *,
        port: int = DEFAULT_DB_PORT,
    ) -> None:
        self.paths = paths
        self.profile = profile
        self.port = port
        self.process: subprocess.Popen[str] | None = None
        self.server = find_executable("mariadbd", "mysqld")
        self.installer = find_executable("mariadb-install-db", "mysql_install_db")
        self.data_dir = paths.database / "data"
        self.log_dir = paths.database / "logs"
        self.general_log_path = self.log_dir / "general.log"
        self.error_log_path = self.log_dir / "error.log"

    def preflight(self) -> list[str]:
        missing: list[str] = []
        if self.server is None:
            missing.append("mariadbd (or mysqld)")
        if self.installer is None and not (self.data_dir / "mysql").exists():
            missing.append("mariadb-install-db (or mysql_install_db)")
        return missing

    def initialize(self) -> None:
        self.paths.create()
        self.log_dir.mkdir(parents=True, exist_ok=True)
        if (self.data_dir / "mysql").exists():
            return
        if self.installer is None:
            raise RuntimeError("MariaDB installer executable was not found")
        self.data_dir.mkdir(parents=True, exist_ok=True)
        command = [self.installer, f"--datadir={self.data_dir}"]
        if os.name != "nt":
            command.extend(["--auth-root-authentication-method=normal", "--skip-test-db"])
        result = subprocess.run(command, text=True, capture_output=True, check=False)
        if result.returncode != 0:
            raise RuntimeError(
                "failed to initialize simulator MariaDB: "
                + (result.stderr.strip() or result.stdout.strip())
            )

    def start(self, *, recreate_schema: bool = False) -> None:
        if self.process is not None and self.process.poll() is None:
            return
        if self.server is None:
            raise RuntimeError("mariadbd (or mysqld) was not found")
        if _port_is_in_use(self.port):
            raise RuntimeError(
                f"port {self.port} is already in use; the simulator will not attach to an unknown database"
            )
        self.log_dir.mkdir(parents=True, exist_ok=True)
        self.general_log_path.write_text("", encoding="utf-8")
        command = [
            self.server,
            "--no-defaults",
            f"--datadir={self.data_dir}",
            "--bind-address=127.0.0.1",
            f"--port={self.port}",
            f"--pid-file={self.paths.runtime / 'mariadb.pid'}",
            f"--log-error={self.error_log_path}",
            "--general-log=1",
            f"--general-log-file={self.general_log_path}",
            "--skip-name-resolve",
        ]
        if os.name != "nt":
            command.append(f"--socket={self.paths.runtime / 'mariadb.sock'}")
        self.process = subprocess.Popen(
            command,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            text=True,
        )
        try:
            self._wait_until_ready()
            self._bootstrap(recreate_schema=recreate_schema)
        except Exception:
            self.stop()
            raise

    def stop(self) -> None:
        if self.process is None:
            return
        if self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait(timeout=2)
        self.process = None

    def reconnect_after(self, seconds: float) -> None:
        self.stop()
        time.sleep(seconds)
        self.start()

    def connect(self, *, schema: str | None = None, user: str = "root", password: str = "") -> Any:
        connector = _connector()
        return connector.connect(
            host="127.0.0.1",
            port=self.port,
            user=user,
            password=password,
            database=schema,
            connection_timeout=2,
            autocommit=True,
        )

    def _wait_until_ready(self) -> None:
        deadline = time.monotonic() + 15
        last_error: Exception | None = None
        while time.monotonic() < deadline:
            if self.process is not None and self.process.poll() is not None:
                raise RuntimeError(f"MariaDB exited with status {self.process.returncode}")
            try:
                connection = self.connect()
                connection.close()
                return
            except Exception as error:
                last_error = error
                time.sleep(0.2)
        raise RuntimeError(f"MariaDB did not become ready: {last_error}")

    def _bootstrap(self, *, recreate_schema: bool) -> None:
        connection = self.connect()
        cursor = connection.cursor()
        try:
            cursor.execute(
                "SELECT SCHEMA_NAME FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME = %s",
                (self.profile.schema_name,),
            )
            exists = cursor.fetchone() is not None
            if exists and recreate_schema:
                cursor.execute(f"DROP SCHEMA `{self.profile.schema_name}`")
                exists = False
            if not exists:
                for statement in _split_sql(self.profile.schema_sql()):
                    cursor.execute(statement)
                    if cursor.with_rows:
                        cursor.fetchall()
            cursor.execute("CREATE USER IF NOT EXISTS 'pi'@'127.0.0.1' IDENTIFIED BY 'ese'")
            cursor.execute("CREATE USER IF NOT EXISTS 'pi'@'localhost' IDENTIFIED BY 'ese'")
            cursor.execute("CREATE USER IF NOT EXISTS 'elevator_sim_observer'@'127.0.0.1'")
            cursor.execute(f"GRANT ALL ON `{self.profile.schema_name}`.* TO 'pi'@'127.0.0.1'")
            cursor.execute(f"GRANT ALL ON `{self.profile.schema_name}`.* TO 'pi'@'localhost'")
            cursor.execute(
                f"GRANT SELECT, INSERT, DELETE ON `{self.profile.schema_name}`.* "
                "TO 'elevator_sim_observer'@'127.0.0.1'"
            )
            cursor.execute("FLUSH PRIVILEGES")
            self.profile.validate(cursor)
        finally:
            cursor.close()
            connection.close()


class DatabaseObserver:
    def __init__(self, database: IsolatedMariaDb, profile: SchemaProfile) -> None:
        self.database = database
        self.profile = profile
        self.cursors = {table: 0 for table in profile.observed_tables}

    def reset(self) -> None:
        connection = self.database.connect(
            schema=self.profile.schema_name,
            user="elevator_sim_observer",
        )
        cursor = connection.cursor()
        try:
            self.profile.reset(cursor)
            self.cursors = {table: 0 for table in self.profile.observed_tables}
        finally:
            cursor.close()
            connection.close()

    def insert_gui_request(self, floor: int, remote: int = 0) -> None:
        connection = self.database.connect(
            schema=self.profile.schema_name,
            user="elevator_sim_observer",
        )
        cursor = connection.cursor()
        try:
            self.profile.insert_gui_request(cursor, floor=floor, remote=remote)
        finally:
            cursor.close()
            connection.close()

    def poll(self) -> list[ObservedRow]:
        connection = self.database.connect(
            schema=self.profile.schema_name,
            user="elevator_sim_observer",
        )
        cursor = connection.cursor()
        observed: list[ObservedRow] = []
        try:
            for table, after_index in tuple(self.cursors.items()):
                rows = self.profile.fetch_new_rows(cursor, table, after_index)
                if rows:
                    self.cursors[table] = rows[-1].index
                    observed.extend(rows)
        finally:
            cursor.close()
            connection.close()
        return observed


def _port_is_in_use(port: int) -> bool:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as check:
        check.settimeout(0.2)
        return check.connect_ex(("127.0.0.1", port)) == 0


def _split_sql(script: str) -> list[str]:
    without_comments = re.sub(r"/\*.*?\*/", "", script, flags=re.DOTALL)
    return [statement.strip() for statement in without_comments.split(";") if statement.strip()]
