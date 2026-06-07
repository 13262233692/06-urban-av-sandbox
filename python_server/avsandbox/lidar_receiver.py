import struct
import socket
import threading
import time
import logging
from dataclasses import dataclass, field
from typing import Optional, List, Callable

import numpy as np

logger = logging.getLogger(__name__)

FRAME_HEADER_MAGIC = 0x4C444152
FRAME_HEADER_SIZE = 16
FRAME_HEADER_FORMAT = "<IHHII"


@dataclass
class PointCloud2Header:
    seq: int = 0
    stamp_sec: int = 0
    stamp_nanosec: int = 0
    frame_id: str = ""


@dataclass
class PointCloud2Field:
    name: str = ""
    offset: int = 0
    data_type: int = 7
    count: int = 1


@dataclass
class PointCloud2Message:
    header: PointCloud2Header = field(default_factory=PointCloud2Header)
    height: int = 0
    width: int = 0
    fields: List[PointCloud2Field] = field(default_factory=list)
    is_bigendian: bool = False
    point_step: int = 0
    row_step: int = 0
    is_dense: bool = False
    data: bytes = b""
    point_count: int = 0


class LidarTCPReceiver:
    def __init__(
        self,
        host: str = "127.0.0.1",
        port: int = 9100,
        on_frame_callback: Optional[Callable[[PointCloud2Message], None]] = None,
    ):
        self._host = host
        self._port = port
        self._on_frame_callback = on_frame_callback
        self._socket: Optional[socket.socket] = None
        self._running = False
        self._recv_thread: Optional[threading.Thread] = None
        self._latest_frame: Optional[PointCloud2Message] = None
        self._frame_lock = threading.Lock()
        self._frames_received = 0
        self._bytes_received = 0
        self._reconnect_interval = 1.0

    @property
    def frames_received(self) -> int:
        return self._frames_received

    @property
    def bytes_received(self) -> int:
        return self._bytes_received

    def start(self) -> None:
        self._running = True
        self._recv_thread = threading.Thread(target=self._recv_loop, daemon=True)
        self._recv_thread.start()
        logger.info("[LidarTCPReceiver] Started, connecting to %s:%d", self._host, self._port)

    def stop(self) -> None:
        self._running = False
        if self._socket:
            try:
                self._socket.close()
            except OSError:
                pass
            self._socket = None
        if self._recv_thread:
            self._recv_thread.join(timeout=3.0)
            self._recv_thread = None
        logger.info("[LidarTCPReceiver] Stopped")

    def get_latest_frame(self) -> Optional[PointCloud2Message]:
        with self._frame_lock:
            return self._latest_frame

    def _connect(self) -> bool:
        try:
            self._socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self._socket.settimeout(5.0)
            self._socket.connect((self._host, self._port))
            self._socket.settimeout(0.5)
            logger.info("[LidarTCPReceiver] Connected to %s:%d", self._host, self._port)
            return True
        except (OSError, ConnectionRefusedError) as e:
            logger.warning("[LidarTCPReceiver] Connection failed: %s", e)
            if self._socket:
                self._socket.close()
                self._socket = None
            return False

    def _recv_exact(self, size: int) -> Optional[bytes]:
        if not self._socket:
            return None
        data = bytearray()
        while len(data) < size:
            try:
                chunk = self._socket.recv(size - len(data))
                if not chunk:
                    return None
                data.extend(chunk)
            except socket.timeout:
                if not self._running:
                    return None
                continue
            except OSError:
                return None
        return bytes(data)

    def _recv_loop(self) -> None:
        while self._running:
            if not self._socket:
                if not self._connect():
                    time.sleep(self._reconnect_interval)
                    continue

            header_data = self._recv_exact(FRAME_HEADER_SIZE)
            if not header_data:
                if self._running:
                    self._socket = None
                    time.sleep(self._reconnect_interval)
                continue

            magic, version, frame_num, payload_size, checksum = struct.unpack(
                FRAME_HEADER_FORMAT, header_data
            )

            if magic != FRAME_HEADER_MAGIC:
                logger.warning("[LidarTCPReceiver] Invalid frame header magic: 0x%08X", magic)
                self._socket = None
                continue

            expected_checksum = (magic + version + frame_num + payload_size) & 0xFFFFFFFF
            if checksum != expected_checksum:
                logger.warning("[LidarTCPReceiver] Frame header checksum mismatch")
                self._socket = None
                continue

            payload = self._recv_exact(payload_size)
            if not payload:
                self._socket = None
                continue

            pc2_msg = decode_pointcloud2(payload)
            if pc2_msg:
                with self._frame_lock:
                    self._latest_frame = pc2_msg
                self._frames_received += 1
                self._bytes_received += len(header_data) + len(payload)

                if self._on_frame_callback:
                    try:
                        self._on_frame_callback(pc2_msg)
                    except Exception as e:
                        logger.error("[LidarTCPReceiver] Callback error: %s", e)

    def __enter__(self):
        self.start()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.stop()
        return False


def _read_uint8(data: bytes, offset: int) -> tuple:
    return data[offset], offset + 1


def _read_uint16(data: bytes, offset: int) -> tuple:
    val = struct.unpack_from("<H", data, offset)[0]
    return val, offset + 2


def _read_uint32(data: bytes, offset: int) -> tuple:
    val = struct.unpack_from("<I", data, offset)[0]
    return val, offset + 4


def _read_float32(data: bytes, offset: int) -> tuple:
    val = struct.unpack_from("<f", data, offset)[0]
    return val, offset + 4


def _read_string(data: bytes, offset: int) -> tuple:
    length, offset = _read_uint32(data, offset)
    s = data[offset : offset + length].decode("ascii", errors="replace")
    return s, offset + length


def decode_pointcloud2(data: bytes) -> Optional[PointCloud2Message]:
    try:
        offset = 0

        header = PointCloud2Header()
        header.seq, offset = _read_uint32(data, offset)
        header.stamp_sec, offset = _read_uint32(data, offset)
        header.stamp_nanosec, offset = _read_uint32(data, offset)
        header.frame_id, offset = _read_string(data, offset)

        is_bigendian, offset = _read_uint8(data, offset)
        point_step, offset = _read_uint32(data, offset)
        row_step, offset = _read_uint32(data, offset)
        is_dense, offset = _read_uint32(data, offset)

        num_fields, offset = _read_uint32(data, offset)
        fields = []
        for _ in range(num_fields):
            f = PointCloud2Field()
            f.name, offset = _read_string(data, offset)
            f.offset, offset = _read_uint32(data, offset)
            f.data_type, offset = _read_uint8(data, offset)
            f.count, offset = _read_uint32(data, offset)
            fields.append(f)

        data_length, offset = _read_uint32(data, offset)
        point_data = data[offset : offset + data_length]

        point_count = data_length // point_step if point_step > 0 else 0

        msg = PointCloud2Message(
            header=header,
            height=1,
            width=point_count,
            fields=fields,
            is_bigendian=bool(is_bigendian),
            point_step=point_step,
            row_step=row_step,
            is_dense=bool(is_dense),
            data=point_data,
            point_count=point_count,
        )

        return msg
    except (struct.error, IndexError, ValueError) as e:
        logger.error("[PointCloud2] Decode error: %s", e)
        return None


def pointcloud2_to_numpy(msg: PointCloud2Message) -> Optional[np.ndarray]:
    if not msg or msg.point_count == 0 or not msg.data:
        return None

    field_map = {}
    for f in msg.fields:
        field_map[f.name] = f

    has_x = "x" in field_map
    has_y = "y" in field_map
    has_z = "z" in field_map
    has_intensity = "intensity" in field_map
    has_ring = "ring" in field_map

    if not (has_x and has_y and has_z):
        logger.warning("[PointCloud2] Missing x/y/z fields")
        return None

    num_points = msg.point_count
    dtype_list = []

    if has_x:
        dtype_list.append(("x", np.float32))
    if has_y:
        dtype_list.append(("y", np.float32))
    if has_z:
        dtype_list.append(("z", np.float32))
    if has_intensity:
        dtype_list.append(("intensity", np.float32))
    if has_ring:
        dtype_list.append(("ring", np.uint16))

    result = np.zeros(num_points, dtype=dtype_list)

    raw = np.frombuffer(msg.data, dtype=np.uint8)

    for i in range(num_points):
        base = i * msg.point_step

        if has_x:
            off = base + field_map["x"].offset
            result["x"][i] = struct.unpack_from("<f", msg.data, off)[0]
        if has_y:
            off = base + field_map["y"].offset
            result["y"][i] = struct.unpack_from("<f", msg.data, off)[0]
        if has_z:
            off = base + field_map["z"].offset
            result["z"][i] = struct.unpack_from("<f", msg.data, off)[0]
        if has_intensity:
            off = base + field_map["intensity"].offset
            result["intensity"][i] = struct.unpack_from("<f", msg.data, off)[0]
        if has_ring:
            off = base + field_map["ring"].offset
            result["ring"][i] = struct.unpack_from("<H", msg.data, off)[0]

    return result


def pointcloud2_to_xyz_numpy(msg: PointCloud2Message) -> Optional[np.ndarray]:
    structured = pointcloud2_to_numpy(msg)
    if structured is None:
        return None

    xyz = np.zeros((len(structured), 3), dtype=np.float32)
    xyz[:, 0] = structured["x"]
    xyz[:, 1] = structured["y"]
    xyz[:, 2] = structured["z"]

    if "intensity" in structured.dtype.names:
        valid = structured["intensity"] > 0
        xyz = xyz[valid]

    return xyz
