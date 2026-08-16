from __future__ import annotations

import asyncio
import json
import os
import socket
import struct
import time
from dataclasses import dataclass
from typing import Any

from sim.can import CanFrame


@dataclass(frozen=True)
class TransportEvent:
    kind: str
    payload: dict[str, Any]


class LoopbackCanTransport:
    """Versioned NDJSON bridge used by the native Windows SA adapter."""

    def __init__(self, host: str, port: int) -> None:
        self.host = host
        self.port = port
        self.events: asyncio.Queue[TransportEvent] = asyncio.Queue()
        self._server: asyncio.Server | None = None
        self._writer: asyncio.StreamWriter | None = None
        self.ready = asyncio.Event()

    async def start(self) -> None:
        self._server = await asyncio.start_server(self._accept, self.host, self.port)

    async def stop(self) -> None:
        if self._writer is not None:
            self._writer.close()
            await self._writer.wait_closed()
            self._writer = None
        if self._server is not None:
            self._server.close()
            await self._server.wait_closed()
            self._server = None
        self.ready.clear()

    async def send(self, frame: CanFrame) -> None:
        if self._writer is None:
            raise ConnectionError("SA loopback adapter is not connected")
        message = {
            "version": 1,
            "type": "can_rx",
            "id": frame.can_id,
            "data": frame.data,
            "timestamp_ms": frame.timestamp_ms,
        }
        self._writer.write((json.dumps(message, separators=(",", ":")) + "\n").encode())
        await self._writer.drain()

    async def _accept(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> None:
        if self._writer is not None:
            writer.write(b'{"version":1,"type":"error","message":"adapter already connected"}\n')
            await writer.drain()
            writer.close()
            await writer.wait_closed()
            return
        self._writer = writer
        peer = writer.get_extra_info("peername")
        await self.events.put(TransportEvent("transport.connected", {"peer": str(peer)}))
        try:
            first = await reader.readline()
            hello = json.loads(first)
            if hello.get("type") != "hello" or hello.get("version") != 1:
                raise ValueError("expected protocol-v1 hello")
            writer.write(b'{"version":1,"type":"hello_ack"}\n')
            await writer.drain()
            self.ready.set()
            while line := await reader.readline():
                message = json.loads(line)
                message_type = message.get("type")
                if message_type == "can_tx":
                    await self.events.put(TransportEvent("can.tx", message))
                elif message_type == "diagnostic":
                    await self.events.put(TransportEvent("diagnostic", message))
                else:
                    await self.events.put(TransportEvent("transport.message", message))
        except (ConnectionError, json.JSONDecodeError, ValueError) as error:
            await self.events.put(TransportEvent("transport.error", {"message": str(error)}))
        finally:
            self.ready.clear()
            self._writer = None
            writer.close()
            await writer.wait_closed()
            await self.events.put(TransportEvent("transport.disconnected", {}))


class DiagnosticsServer:
    def __init__(self, host: str, port: int) -> None:
        self.host = host
        self.port = port
        self.events: asyncio.Queue[TransportEvent] = asyncio.Queue()
        self.ready = asyncio.Event()
        self._server: asyncio.Server | None = None
        self._writers: set[asyncio.StreamWriter] = set()

    async def start(self) -> None:
        self._server = await asyncio.start_server(self._accept, self.host, self.port)

    async def stop(self) -> None:
        for writer in tuple(self._writers):
            writer.close()
            await writer.wait_closed()
        self._writers.clear()
        if self._server is not None:
            self._server.close()
            await self._server.wait_closed()
        self.ready.clear()

    async def _accept(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> None:
        self._writers.add(writer)
        self.ready.set()
        try:
            while line := await reader.readline():
                message = json.loads(line)
                await self.events.put(TransportEvent("diagnostic", message))
        except (ConnectionError, json.JSONDecodeError) as error:
            await self.events.put(TransportEvent("diagnostic.error", {"message": str(error)}))
        finally:
            self._writers.discard(writer)
            if not self._writers:
                self.ready.clear()
            writer.close()
            await writer.wait_closed()


class SocketCanTransport:
    """Linux SocketCAN transport with retry-friendly startup."""

    _CAN_FRAME = struct.Struct("=IB3x8s")

    def __init__(self, interface: str = "vcan0") -> None:
        if os.name == "nt":
            raise RuntimeError("SocketCAN transport is available only on Linux")
        self.interface = interface
        self.events: asyncio.Queue[TransportEvent] = asyncio.Queue()
        self.ready = asyncio.Event()
        self._socket: socket.socket | None = None
        self._reader_task: asyncio.Task[None] | None = None

    async def start(self) -> None:
        while self._socket is None:
            try:
                can_socket = socket.socket(socket.AF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
                can_socket.setblocking(False)
                can_socket.bind((self.interface,))
                self._socket = can_socket
            except OSError:
                await asyncio.sleep(0.5)
        self.ready.set()
        self._reader_task = asyncio.create_task(self._read_loop())

    async def stop(self) -> None:
        if self._reader_task is not None:
            self._reader_task.cancel()
            await asyncio.gather(self._reader_task, return_exceptions=True)
        if self._socket is not None:
            self._socket.close()
            self._socket = None
        self.ready.clear()

    async def send(self, frame: CanFrame) -> None:
        if self._socket is None:
            raise ConnectionError("SocketCAN is not ready")
        payload = bytes(frame.data).ljust(8, b"\x00")
        raw = self._CAN_FRAME.pack(frame.can_id, len(frame.data), payload)
        loop = asyncio.get_running_loop()
        await loop.sock_sendall(self._socket, raw)

    async def _read_loop(self) -> None:
        assert self._socket is not None
        loop = asyncio.get_running_loop()
        while True:
            raw = await loop.sock_recv(self._socket, self._CAN_FRAME.size)
            if len(raw) != self._CAN_FRAME.size:
                continue
            can_id, dlc, data = self._CAN_FRAME.unpack(raw)
            await self.events.put(
                TransportEvent(
                    "can.tx",
                    {
                        "version": 1,
                        "type": "can_tx",
                        "id": can_id & 0x7FF,
                        "data": list(data[:dlc]),
                        "timestamp_ms": int(time.monotonic() * 1000),
                    },
                )
            )

