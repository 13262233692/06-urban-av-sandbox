import time
import logging
from typing import Any, Dict, List, Optional, Tuple

import gymnasium as gym
import numpy as np

try:
    import ray
    from ray.rllib.env import MultiAgentEnv
    HAS_RAY = True
except ImportError:
    HAS_RAY = False
    MultiAgentEnv = object

from avsandbox.config import AVSandboxConfig
from avsandbox.protocol import VehicleControlMessage, VehicleStateMessage
from avsandbox.udp_transport import UDPTransport
from avsandbox.open_drive_proxy import OpenDriveProxy

logger = logging.getLogger(__name__)


class AVSandboxEnv(gym.Env if not HAS_RAY else MultiAgentEnv):
    metadata = {"render_modes": ["human"]}

    def __init__(self, config: Optional[Dict[str, Any]] = None):
        super().__init__()

        self._config = AVSandboxConfig(**{k: v for k, v in (config or {}).items()
                                          if hasattr(AVSandboxConfig, k)})

        obs_dim = self._config.observation_dim
        act_dim = self._config.action_dim

        self.observation_space = gym.spaces.Box(
            low=-np.inf,
            high=np.inf,
            shape=(obs_dim,),
            dtype=np.float32,
        )

        self.action_space = gym.spaces.Box(
            low=np.array([-1.0, 0.0, -1.0], dtype=np.float32),
            high=np.array([1.0, 1.0, 1.0], dtype=np.float32),
            dtype=np.float32,
        )

        self._transport: Optional[UDPTransport] = None
        self._current_state: Optional[VehicleStateMessage] = None
        self._prev_distance: float = 0.0
        self._step_count: int = 0
        self._episode_start_time: float = 0.0
        self._done: bool = False
        self._info: Dict[str, Any] = {}

        self._open_drive: Optional[OpenDriveProxy] = None
        if self._config.open_drive_map_path:
            self._open_drive = OpenDriveProxy()
            self._open_drive.load_file(self._config.open_drive_map_path)

    def _ensure_transport(self) -> UDPTransport:
        if self._transport is None:
            self._transport = UDPTransport(self._config)
            self._transport.start()
            logger.info("UDP transport started: send=%s:%d, recv=%s:%d",
                        self._config.ue5_listen_host, self._config.ue5_listen_port,
                        self._config.python_listen_host, self._config.python_listen_port)
        return self._transport

    def reset(
        self,
        *,
        seed: Optional[int] = None,
        options: Optional[Dict[str, Any]] = None,
    ) -> Tuple[np.ndarray, Dict[str, Any]]:
        super().reset(seed=seed)

        self._step_count = 0
        self._episode_start_time = time.time()
        self._done = False
        self._prev_distance = 0.0
        self._info = {}

        transport = self._ensure_transport()

        reset_control = VehicleControlMessage(
            throttle=0.0,
            brake=1.0,
            steering_angle=0.0,
            gear=0,
            handbrake=1,
        )
        transport.send_control(reset_control)

        self._current_state = transport.receive_state_blocking(max_wait=5.0)
        if self._current_state is None:
            self._current_state = VehicleStateMessage()

        obs = self._state_to_observation(self._current_state)
        return obs, self._info

    def step(
        self, action: np.ndarray
    ) -> Tuple[np.ndarray, float, bool, bool, Dict[str, Any]]:
        if self._done:
            obs = self._state_to_observation(self._current_state)
            return obs, 0.0, True, False, self._info

        transport = self._ensure_transport()

        throttle = float(np.clip(action[0], 0.0, 1.0))
        brake = float(np.clip(action[1], 0.0, 1.0))
        steering = float(np.clip(action[2], -1.0, 1.0))

        control = VehicleControlMessage(
            throttle=throttle,
            brake=brake,
            steering_angle=steering,
        )
        transport.send_control(control)

        self._current_state = transport.receive_state(timeout=self._config.state_recv_timeout)
        if self._current_state is None:
            self._current_state = VehicleStateMessage()

        self._step_count += 1

        reward = self._compute_reward(self._current_state, throttle, brake, steering)

        terminated = self._check_terminated(self._current_state)
        truncated = self._check_truncated()

        self._done = terminated or truncated

        obs = self._state_to_observation(self._current_state)

        self._info.update({
            "step_count": self._step_count,
            "forward_speed": self._current_state.forward_speed,
            "lane_offset": self._current_state.lane_offset,
            "collision": self._current_state.collision_state,
            "off_road": self._current_state.off_road_state,
            "distance_along_lane": self._current_state.distance_along_lane,
            "reward": reward,
        })

        return obs, reward, terminated, truncated, self._info

    def close(self) -> None:
        if self._transport is not None:
            stop_control = VehicleControlMessage(
                throttle=0.0, brake=1.0, steering_angle=0.0, handbrake=1
            )
            self._transport.send_control(stop_control)
            self._transport.stop()
            self._transport = None

    def _state_to_observation(self, state: VehicleStateMessage) -> np.ndarray:
        raw = state.to_observation()
        obs_dim = self._config.observation_dim

        if len(raw) < obs_dim:
            raw.extend([0.0] * (obs_dim - len(raw)))
        elif len(raw) > obs_dim:
            raw = raw[:obs_dim]

        return np.array(raw, dtype=np.float32)

    def _compute_reward(
        self,
        state: VehicleStateMessage,
        throttle: float,
        brake: float,
        steering: float,
    ) -> float:
        reward = 0.0

        if state.collision_state:
            reward += self._config.collision_penalty

        if state.off_road_state:
            reward += self._config.off_road_penalty

        speed_diff = abs(state.forward_speed - self._config.target_speed)
        max_speed = self._config.max_speed
        if state.forward_speed > max_speed:
            reward -= (state.forward_speed - max_speed) * 0.5
        else:
            speed_reward = max(0.0, 1.0 - speed_diff / self._config.target_speed)
            reward += speed_reward * self._config.speed_reward_coeff

        lane_offset = abs(state.lane_offset)
        if lane_offset < self._config.max_lateral_offset:
            centering_reward = max(0.0, 1.0 - lane_offset / self._config.max_lateral_offset)
            reward += centering_reward * self._config.lane_centering_reward_coeff
        else:
            reward -= lane_offset * 0.1

        distance_progress = state.distance_along_lane - self._prev_distance
        if distance_progress > 0:
            reward += distance_progress * self._config.progress_reward_coeff * 0.01
        self._prev_distance = state.distance_along_lane

        tire_slip = (abs(state.tire_slip_fl) + abs(state.tire_slip_fr) +
                     abs(state.tire_slip_rl) + abs(state.tire_slip_rr)) / 4.0
        reward -= tire_slip * 0.01

        reward -= abs(steering) * 0.005

        return float(reward)

    def _check_terminated(self, state: VehicleStateMessage) -> bool:
        if state.collision_state:
            return True
        return False

    def _check_truncated(self) -> bool:
        if self._step_count >= self._config.max_episode_steps:
            return True
        if time.time() - self._episode_start_time > self._config.episode_timeout_seconds:
            return True
        return False


if HAS_RAY:
    class AVSandboxMultiAgentEnv(MultiAgentEnv):
        def __init__(self, config: Optional[Dict[str, Any]] = None):
            super().__init__()
            self._num_agents = config.get("num_agents", 4) if config else 4
            self._agent_envs: Dict[str, AVSandboxEnv] = {}
            self._agent_ids = set()

            base_config = config or {}
            for i in range(self._num_agents):
                agent_id = f"agent_{i}"
                self._agent_ids.add(agent_id)

                agent_config = dict(base_config)
                agent_config["python_listen_port"] = base_config.get("python_listen_port", 9001) + i
                agent_config["ue5_listen_port"] = base_config.get("ue5_listen_port", 9000) + i

                self._agent_envs[agent_id] = AVSandboxEnv(config=agent_config)

            self.observation_space = gym.spaces.Box(
                low=-np.inf, high=np.inf, shape=(48,), dtype=np.float32
            )
            self.action_space = gym.spaces.Box(
                low=np.array([-1.0, 0.0, -1.0], dtype=np.float32),
                high=np.array([1.0, 1.0, 1.0], dtype=np.float32),
            )

        @property
        def get_agent_ids(self):
            return self._agent_ids

        def reset(self, *, seed=None, options=None):
            obs = {}
            infos = {}
            for agent_id, env in self._agent_envs.items():
                o, i = env.reset(seed=seed, options=options)
                obs[agent_id] = o
                infos[agent_id] = i
            return obs, infos

        def step(self, actions):
            obs = {}
            rewards = {}
            terminateds = {}
            truncateds = {}
            infos = {}

            for agent_id, action in actions.items():
                if agent_id in self._agent_envs:
                    o, r, d, t, i = self._agent_envs[agent_id].step(action)
                    obs[agent_id] = o
                    rewards[agent_id] = r
                    terminateds[agent_id] = d
                    truncateds[agent_id] = t
                    infos[agent_id] = i

            terminateds["__all__"] = all(terminateds.values())
            truncateds["__all__"] = all(truncateds.values())

            return obs, rewards, terminateds, truncateds, infos
