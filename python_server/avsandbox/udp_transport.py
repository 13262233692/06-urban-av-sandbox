import socket
import threading
import time
from typing import Optional, Tuple

from avsandbox.config import AVSandboxConfig
from avsandbox.protocol import VehicleControlMessage, VehicleStateMessage


class UDPTransport:
    def __init__(self, config: AVSandboxConfig):
        self._config = config
        self._send_socket: Optional[socket.socket] = None
        self._recv_socket: Optional[socket.socket] = None
        self._lock = threading.Lock()
        self._sequence_number = 0
        self._last_state: Optional[VehicleStateMessage] = None
        self._state_lock = threading.Lock()
        self._running = False

    def start(self) -> None:
        self._running = True

        self._send_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._send_socket.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 65536)

        self._recv_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._recv_socket.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 65536)
        self._recv_socket.bind(self._config.get_state_endpoint())
        self._recv_socket.settimeout(self._config.state_recv_timeout)

    def stop(self) -> None:
        self._running = False
        if self._send_socket:
            self._send_socket.close()
            self._send_socket = None
        if self._recv_socket:
            self._recv_socket.close()
            self._recv_socket = None

    def send_control(self, control: VehicleControlMessage) -> bool:
        if not self._send_socket:
            return False

        with self._lock:
            self._sequence_number += 1
            control.sequence_number = self._sequence_number

        data = control.serialize()
        endpoint = self._config.get_control_endpoint()

        try:
            self._send_socket.sendto(data, endpoint)
            return True
        except (OSError, socket.error) as e:
            return False

    def receive_state(self, timeout: float = None) -> Optional[VehicleStateMessage]:
        if not self._recv_socket:
            return None

        if timeout is not None:
            self._recv_socket.settimeout(timeout)

        try:
            data, addr = self._recv_socket.recvfrom(self._config.state_recv_buffer_size)
            state = VehicleStateMessage.deserialize(data)
            if state:
                with self._state_lock:
                    self._last_state = state
            return state
        except socket.timeout:
            return None
        except (OSError, socket.error):
            return None

    def receive_state_blocking(self, max_wait: float = 5.0) -> Optional[VehicleStateMessage]:
        deadline = time.time() + max_wait
        while self._running and time.time() < deadline:
            state = self.receive_state(timeout=0.1)
            if state:
                return state
        return None

    def get_last_state(self) -> Optional[VehicleStateMessage]:
        with self._state_lock:
            return self._last_state

    def send_raw(self, data: bytes, endpoint: Tuple[str, int]) -> bool:
        if not self._send_socket:
            return False
        try:
            self._send_socket.sendto(data, endpoint)
            return True
        except (OSError, socket.error):
            return False

    @property
    def sequence_number(self) -> int:
        with self._lock:
            return self._sequence_number

    def __enter__(self):
        self.start()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.stop()
        return False
