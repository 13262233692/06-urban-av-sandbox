import struct
import time
from dataclasses import dataclass
from typing import Optional

CONTROL_MAGIC = 0x41564354
CONTROL_VERSION = 1
CONTROL_STRUCT_FORMAT = "<IHHfffBBBBI"
CONTROL_SIZE = struct.calcsize(CONTROL_STRUCT_FORMAT)

STATE_MAGIC = 0x41565354
STATE_VERSION = 1
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

    def serialize(self) -> bytes:
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
            timestamp,
        )
        checksum = sum(data) & 0xFFFFFFFF
        data += struct.pack("<I", checksum)
        return data

    @staticmethod
    def deserialize(data: bytes) -> Optional["VehicleControlMessage"]:
        if len(data) < CONTROL_SIZE + 4:
            return None
        fields = struct.unpack(CONTROL_STRUCT_FORMAT, data[:CONTROL_SIZE])
        magic, version, seq, throttle, brake, steering, gear, handbrake, r1, r2, ts = fields

        if magic != CONTROL_MAGIC:
            return None

        msg = VehicleControlMessage(
            throttle=throttle,
            brake=brake,
            steering_angle=steering,
            gear=gear,
            handbrake=handbrake,
            sequence_number=seq,
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
