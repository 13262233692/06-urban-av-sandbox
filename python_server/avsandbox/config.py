from dataclasses import dataclass, field
from typing import Optional


@dataclass
class AVSandboxConfig:
    ue5_listen_host: str = "127.0.0.1"
    ue5_listen_port: int = 9000
    python_listen_host: str = "127.0.0.1"
    python_listen_port: int = 9001

    control_send_timeout: float = 0.1
    state_recv_timeout: float = 0.1
    state_recv_buffer_size: int = 65536

    max_episode_steps: int = 10000
    episode_timeout_seconds: float = 300.0

    collision_penalty: float = -100.0
    off_road_penalty: float = -50.0
    speed_reward_coeff: float = 0.1
    lane_centering_reward_coeff: float = 0.05
    progress_reward_coeff: float = 1.0

    target_speed: float = 13.89
    max_speed: float = 40.0
    max_steering_angle: float = 70.0
    max_lateral_offset: float = 5.0

    observation_dim: int = 48
    action_dim: int = 3

    ray_num_workers: int = 4
    ray_num_envs_per_worker: int = 2
    ray_framework: str = "torch"

    open_drive_map_path: Optional[str] = None

    def get_control_endpoint(self) -> tuple:
        return (self.ue5_listen_host, self.ue5_listen_port)

    def get_state_endpoint(self) -> tuple:
        return (self.python_listen_host, self.python_listen_port)
