import struct
import time
from dataclasses import dataclass
from typing import Optional

CONTROL_MAGIC = 0x41564354
CONTROL_VERSION = 2
CONTROL_STRUCT_FORMAT = "<IHHfffBBBIdfII"
CONTROL_SIZE = struct.calcsize(CONTROL_STRUCT_FORMAT)

STATE_MAGIC = 0x41565354
STATE_VERSION = 2
STATE_STRUCT_FORMAT = "<IHHfffffffffffffffffffffffffffffBBB fffiff II"
STATE_SIZE = struct.calcsize(STATE_STRUCT_FORMAT)


@dataclass
class VehicleControlMessage:
    throttle: float = 0.0
    brake: float = 0.0
    steering_angle: float = 0.0
    gear: int = 0
    handbrake: int = 0
    sequence_number: int = 0
    sim_timestamp: float = 0.0
    time_dilation: float = 1.0

    def serialize(self) -> bytes:
        if self.sim_timestamp <= 0.0:
            self.sim_timestamp = time.time()

        timestamp = int(time.time() * 1000) & 0xFFFFFFFF
        data = struct.pack(
            CONTROL_STRUCT_FORMAT,
            CONTROL_MAGIC,
            CONTROL_VERSION,
            self.sequence_number,
            self.throttle,
            self.brake,
            self.steering_angle,
            self.gear,
            self.handbrake,
            0,
            0,
            self.sim_timestamp,
            self.time_dilation,
            timestamp,
            0,
        )
        checksum = sum(data) & 0xFFFFFFFF
        data += struct.pack("<I", checksum)
        return data

    @staticmethod
    def deserialize(data: bytes) -> Optional["VehicleControlMessage"]:
        if len(data) < CONTROL_SIZE + 4:
            return None
        fields = struct.unpack(CONTROL_STRUCT_FORMAT, data[:CONTROL_SIZE])
        magic, version, seq, throttle, brake, steering, gear, handbrake, r1, r2, sim_ts, td, ts = fields

        if magic != CONTROL_MAGIC:
            return None

        msg = VehicleControlMessage(
            throttle=throttle,
            brake=brake,
            steering_angle=steering,
            gear=gear,
            handbrake=handbrake,
            sequence_number=seq,
            sim_timestamp=sim_ts,
            time_dilation=td,
        )
        return msg


@dataclass
class VehicleStateMessage:
    position_x: float = 0.0
    position_y: float = 0.0
    position_z: float = 0.0
    rotation_pitch: float = 0.0
    rotation_yaw: float = 0.0
    rotation_roll: float = 0.0
    velocity_x: float = 0.0
    velocity_y: float = 0.0
    velocity_z: float = 0.0
    angular_velocity_x: float = 0.0
    angular_velocity_y: float = 0.0
    angular_velocity_z: float = 0.0
    forward_speed: float = 0.0
    lateral_speed: float = 0.0
    up_speed: float = 0.0
    acceleration_x: float = 0.0
    acceleration_y: float = 0.0
    acceleration_z: float = 0.0
    tire_slip_fl: float = 0.0
    tire_slip_fr: float = 0.0
    tire_slip_rl: float = 0.0
    tire_slip_rr: float = 0.0
    tire_load_fl: float = 0.0
    tire_load_fr: float = 0.0
    tire_load_rl: float = 0.0
    tire_load_rr: float = 0.0
    engine_rpm: float = 0.0
    current_gear: int = 0
    collision_state: int = 0
    off_road_state: int = 0
    steering_angle: float = 0.0
    throttle_input: float = 0.0
    brake_input: float = 0.0
    current_lane_node_id: int = -1
    lane_offset: float = 0.0
    distance_along_lane: float = 0.0
    sequence_number: int = 0
    timestamp: int = 0

    STATE_STRUCT_FORMAT_FULL = "<IHH" \
        "fffffffff" \
        "fffffffff" \
        "ffffffff" \
        "fBBB" \
        "fff" \
        "iff" \
        "II"

    def serialize(self) -> bytes:
        timestamp = int(time.time() * 1000) & 0xFFFFFFFF
        data = struct.pack(
            self.STATE_STRUCT_FORMAT_FULL,
            STATE_MAGIC,
            STATE_VERSION,
            self.sequence_number,
            self.position_x,
            self.position_y,
            self.position_z,
            self.rotation_pitch,
            self.rotation_yaw,
            self.rotation_roll,
            self.velocity_x,
            self.velocity_y,
            self.velocity_z,
            self.angular_velocity_x,
            self.angular_velocity_y,
            self.angular_velocity_z,
            self.forward_speed,
            self.lateral_speed,
            self.up_speed,
            self.acceleration_x,
            self.acceleration_y,
            self.acceleration_z,
            self.tire_slip_fl,
            self.tire_slip_fr,
            self.tire_slip_rl,
            self.tire_slip_rr,
            self.tire_load_fl,
            self.tire_load_fr,
            self.tire_load_rl,
            self.tire_load_rr,
            self.engine_rpm,
            self.current_gear,
            self.collision_state,
            self.off_road_state,
            0,
            self.steering_angle,
            self.throttle_input,
            self.brake_input,
            self.current_lane_node_id,
            self.lane_offset,
            self.distance_along_lane,
            timestamp,
            0,
        )
        checksum = sum(data) & 0xFFFFFFFF
        data += struct.pack("<I", checksum)
        return data

    @staticmethod
    def deserialize(data: bytes) -> Optional["VehicleStateMessage"]:
        try:
            fmt = VehicleStateMessage.STATE_STRUCT_FORMAT_FULL
            expected_size = struct.calcsize(fmt)
            if len(data) < expected_size:
                return None

            fields = struct.unpack(fmt, data[:expected_size])
            magic = fields[0]
            if magic != STATE_MAGIC:
                return None

            msg = VehicleStateMessage()
            (
                _, _, msg.sequence_number,
                msg.position_x, msg.position_y, msg.position_z,
                msg.rotation_pitch, msg.rotation_yaw, msg.rotation_roll,
                msg.velocity_x, msg.velocity_y, msg.velocity_z,
                msg.angular_velocity_x, msg.angular_velocity_y, msg.angular_velocity_z,
                msg.forward_speed, msg.lateral_speed, msg.up_speed,
                msg.acceleration_x, msg.acceleration_y, msg.acceleration_z,
                msg.tire_slip_fl, msg.tire_slip_fr, msg.tire_slip_rl, msg.tire_slip_rr,
                msg.tire_load_fl, msg.tire_load_fr, msg.tire_load_rl, msg.tire_load_rr,
                msg.engine_rpm, msg.current_gear, msg.collision_state, msg.off_road_state, _,
                msg.steering_angle, msg.throttle_input, msg.brake_input,
                msg.current_lane_node_id, msg.lane_offset, msg.distance_along_lane,
                msg.timestamp, _,
            ) = fields

            return msg
        except (struct.error, ValueError):
            return None

    def to_observation(self) -> list:
        return [
            self.position_x / 1000.0,
            self.position_y / 1000.0,
            self.position_z / 1000.0,
            self.rotation_pitch / 180.0,
            self.rotation_yaw / 180.0,
            self.rotation_roll / 180.0,
            self.velocity_x / 30.0,
            self.velocity_y / 30.0,
            self.velocity_z / 30.0,
            self.angular_velocity_x / 3.0,
            self.angular_velocity_y / 3.0,
            self.angular_velocity_z / 3.0,
            self.forward_speed / 30.0,
            self.lateral_speed / 10.0,
            self.up_speed / 10.0,
            self.acceleration_x / 30.0,
            self.acceleration_y / 30.0,
            self.acceleration_z / 30.0,
            self.tire_slip_fl / 1.0,
            self.tire_slip_fr / 1.0,
            self.tire_slip_rl / 1.0,
            self.tire_slip_rr / 1.0,
            self.tire_load_fl / 10000.0,
            self.tire_load_fr / 10000.0,
            self.tire_load_rl / 10000.0,
            self.tire_load_rr / 10000.0,
            self.engine_rpm / 8000.0,
            self.current_gear / 6.0,
            self.collision_state,
            self.off_road_state,
            self.steering_angle,
            self.throttle_input,
            self.brake_input,
            self.current_lane_node_id / 1000.0,
            self.lane_offset / 10.0,
            self.distance_along_lane / 1000.0,
            abs(self.forward_speed - 13.89) / 30.0,
            min(abs(self.lateral_speed), 5.0) / 5.0,
            min(abs(self.lane_offset), 5.0) / 5.0,
            (abs(self.tire_slip_fl) + abs(self.tire_slip_fr) +
             abs(self.tire_slip_rl) + abs(self.tire_slip_rr)) / 4.0,
            self.rotation_yaw / 180.0,
            self.angular_velocity_z / 3.0,
            self.acceleration_x / 30.0,
            self.forward_speed / 30.0,
            self.lateral_speed / 10.0,
            self.distance_along_lane / 1000.0,
            1.0 if self.collision_state else 0.0,
            1.0 if self.off_road_state else 0.0,
            self.current_gear / 6.0,
            self.engine_rpm / 8000.0,
        ]


class StateSmoother:
    def __init__(
        self,
        smoothing_factor: float = 0.3,
        max_position_jump: float = 50.0,
        max_velocity_jump: float = 500.0,
    ):
        self._smoothing_factor = smoothing_factor
        self._max_position_jump = max_position_jump
        self._max_velocity_jump = max_velocity_jump
        self._smoothed_position = None
        self._smoothed_velocity = None
        self._initialized = False

    def smooth(self, state: VehicleStateMessage) -> VehicleStateMessage:
        raw_pos = [state.position_x, state.position_y, state.position_z]
        raw_vel = [state.velocity_x, state.velocity_y, state.velocity_z]

        if not self._initialized:
            self._smoothed_position = raw_pos[:]
            self._smoothed_velocity = raw_vel[:]
            self._initialized = True
        else:
            alpha = self._smoothing_factor
            for i in range(3):
                pos_delta = raw_pos[i] - self._smoothed_position[i]
                if abs(pos_delta) > self._max_position_jump:
                    pos_delta = (
                        self._max_position_jump
                        * (1 if pos_delta > 0 else -1)
                    )
                self._smoothed_position[i] += alpha * pos_delta

                vel_delta = raw_vel[i] - self._smoothed_velocity[i]
                if abs(vel_delta) > self._max_velocity_jump:
                    vel_delta = (
                        self._max_velocity_jump
                        * (1 if vel_delta > 0 else -1)
                    )
                self._smoothed_velocity[i] += alpha * vel_delta

        smoothed = VehicleStateMessage(
            position_x=self._smoothed_position[0],
            position_y=self._smoothed_position[1],
            position_z=self._smoothed_position[2],
            velocity_x=self._smoothed_velocity[0],
            velocity_y=self._smoothed_velocity[1],
            velocity_z=self._smoothed_velocity[2],
            rotation_pitch=state.rotation_pitch,
            rotation_yaw=state.rotation_yaw,
            rotation_roll=state.rotation_roll,
            angular_velocity_x=state.angular_velocity_x,
            angular_velocity_y=state.angular_velocity_y,
            angular_velocity_z=state.angular_velocity_z,
            forward_speed=state.forward_speed,
            lateral_speed=state.lateral_speed,
            up_speed=state.up_speed,
            acceleration_x=state.acceleration_x,
            acceleration_y=state.acceleration_y,
            acceleration_z=state.acceleration_z,
            tire_slip_fl=state.tire_slip_fl,
            tire_slip_fr=state.tire_slip_fr,
            tire_slip_rl=state.tire_slip_rl,
            tire_slip_rr=state.tire_slip_rr,
            tire_load_fl=state.tire_load_fl,
            tire_load_fr=state.tire_load_fr,
            tire_load_rl=state.tire_load_rl,
            tire_load_rr=state.tire_load_rr,
            engine_rpm=state.engine_rpm,
            current_gear=state.current_gear,
            collision_state=state.collision_state,
            off_road_state=state.off_road_state,
            steering_angle=state.steering_angle,
            throttle_input=state.throttle_input,
            brake_input=state.brake_input,
            current_lane_node_id=state.current_lane_node_id,
            lane_offset=state.lane_offset,
            distance_along_lane=state.distance_along_lane,
            sequence_number=state.sequence_number,
            timestamp=state.timestamp,
        )

        fwd = [1.0, 0.0, 0.0]
        right = [0.0, 1.0, 0.0]
        up = [0.0, 0.0, 1.0]

        yaw_rad = state.rotation_yaw * 3.14159265 / 180.0
        fwd = [
            smoothed.velocity_x * 1 + smoothed.velocity_y * 0,
            smoothed.velocity_x * 0 + smoothed.velocity_y * 1,
            smoothed.velocity_z,
        ]

        return smoothed

    def reset(self):
        self._smoothed_position = None
        self._smoothed_velocity = None
        self._initialized = False
