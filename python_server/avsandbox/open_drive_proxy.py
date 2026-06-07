import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple


@dataclass
class RoadLinkElement:
    element_type: str = ""
    element_id: int = -1
    contact_point: str = ""


@dataclass
class RoadLink:
    predecessor: Optional[RoadLinkElement] = None
    successor: Optional[RoadLinkElement] = None


@dataclass
class LaneWidth:
    s_offset: float = 0.0
    a: float = 0.0
    b: float = 0.0
    c: float = 0.0
    d: float = 0.0


@dataclass
class Lane:
    lane_id: int = 0
    lane_type: str = "none"
    level: str = ""
    predecessor_id: int = -1
    successor_id: int = -1
    widths: List[LaneWidth] = field(default_factory=list)
    speed_max: float = 0.0
    speed_unit: str = ""


@dataclass
class LaneSection:
    s: float = 0.0
    single_side: str = ""
    lanes: List[Lane] = field(default_factory=list)


@dataclass
class Geometry:
    s: float = 0.0
    x: float = 0.0
    y: float = 0.0
    hdg: float = 0.0
    length: float = 0.0
    geo_type: str = "line"
    curvature: float = 0.0
    curv_start: float = 0.0
    curv_end: float = 0.0


@dataclass
class Signal:
    signal_id: int = -1
    name: str = ""
    s: float = 0.0
    t: float = 0.0
    dynamic: str = ""
    orientation: str = ""
    signal_type: str = ""
    subtype: str = ""
    value: float = 0.0
    unit: str = ""


@dataclass
class SpeedLimit:
    s: float = 0.0
    max_speed: float = 0.0
    unit: str = ""


@dataclass
class Road:
    road_id: int = -1
    name: str = ""
    length: float = 0.0
    junction: str = ""
    link: RoadLink = field(default_factory=RoadLink)
    geometries: List[Geometry] = field(default_factory=list)
    lane_sections: List[LaneSection] = field(default_factory=list)
    signals: List[Signal] = field(default_factory=list)
    speed_limits: List[SpeedLimit] = field(default_factory=list)


@dataclass
class JunctionConnectionLaneLink:
    from_lane: int = 0
    to_lane: int = 0


@dataclass
class JunctionConnection:
    connection_id: int = -1
    incoming_road: int = -1
    connecting_road: int = -1
    contact_point: str = ""
    lane_links: List[JunctionConnectionLaneLink] = field(default_factory=list)


@dataclass
class Junction:
    junction_id: int = -1
    name: str = ""
    junction_type: str = ""
    connections: List[JunctionConnection] = field(default_factory=list)


@dataclass
class ControllerPhaseState:
    signal_id: str = ""
    state: str = ""


@dataclass
class ControllerPhase:
    name: str = ""
    duration: float = 0.0
    states: List[ControllerPhaseState] = field(default_factory=list)


@dataclass
class Controller:
    controller_id: int = -1
    name: str = ""
    sequence: int = 0
    phases: List[ControllerPhase] = field(default_factory=list)
    controlled_signals: List[int] = field(default_factory=list)


@dataclass
class OpenDriveMap:
    roads: Dict[int, Road] = field(default_factory=dict)
    junctions: Dict[int, Junction] = field(default_factory=dict)
    controllers: Dict[int, Controller] = field(default_factory=dict)


class OpenDriveProxy:
    def __init__(self):
        self._map: Optional[OpenDriveMap] = None

    def load_file(self, file_path: str) -> OpenDriveMap:
        tree = ET.parse(file_path)
        root = tree.getroot()
        self._map = self._parse_root(root)
        return self._map

    def load_string(self, xml_content: str) -> OpenDriveMap:
        root = ET.fromstring(xml_content)
        self._map = self._parse_root(root)
        return self._map

    @property
    def map(self) -> Optional[OpenDriveMap]:
        return self._map

    def _parse_root(self, root: ET.Element) -> OpenDriveMap:
        result = OpenDriveMap()

        for child in root:
            tag = child.tag
            if tag == "road":
                road = self._parse_road(child)
                result.roads[road.road_id] = road
            elif tag == "junction":
                junction = self._parse_junction(child)
                result.junctions[junction.junction_id] = junction
            elif tag == "controller":
                controller = self._parse_controller(child)
                result.controllers[controller.controller_id] = controller

        return result

    def _parse_road(self, elem: ET.Element) -> Road:
        road = Road()
        road.road_id = int(elem.get("id", "-1"))
        road.name = elem.get("name", "")
        road.length = float(elem.get("length", "0"))
        road.junction = elem.get("junction", "")

        link_elem = elem.find("link")
        if link_elem is not None:
            road.link = self._parse_road_link(link_elem)

        plan_view = elem.find("planView")
        if plan_view is not None:
            for geo_elem in plan_view.findall("geometry"):
                road.geometries.append(self._parse_geometry(geo_elem))

        lanes_elem = elem.find("lanes")
        if lanes_elem is not None:
            for sec_elem in lanes_elem.findall("laneSection"):
                road.lane_sections.append(self._parse_lane_section(sec_elem))

        signals_elem = elem.find("signals")
        if signals_elem is not None:
            for sig_elem in signals_elem.findall("signal"):
                road.signals.append(self._parse_signal(sig_elem))

        for type_elem in elem.findall("type"):
            speed_elem = type_elem.find("speed")
            if speed_elem is not None:
                sl = SpeedLimit()
                sl.s = float(type_elem.get("s", "0"))
                sl.max_speed = float(speed_elem.get("max", "0"))
                sl.unit = speed_elem.get("unit", "")
                road.speed_limits.append(sl)

        return road

    def _parse_road_link(self, elem: ET.Element) -> RoadLink:
        link = RoadLink()
        pred = elem.find("predecessor")
        if pred is not None:
            link.predecessor = RoadLinkElement(
                element_type=pred.get("elementType", ""),
                element_id=int(pred.get("elementId", "-1")),
                contact_point=pred.get("contactPoint", ""),
            )
        succ = elem.find("successor")
        if succ is not None:
            link.successor = RoadLinkElement(
                element_type=succ.get("elementType", ""),
                element_id=int(succ.get("elementId", "-1")),
                contact_point=succ.get("contactPoint", ""),
            )
        return link

    def _parse_geometry(self, elem: ET.Element) -> Geometry:
        geo = Geometry()
        geo.s = float(elem.get("s", "0"))
        geo.x = float(elem.get("x", "0"))
        geo.y = float(elem.get("y", "0"))
        geo.hdg = float(elem.get("hdg", "0"))
        geo.length = float(elem.get("length", "0"))

        line = elem.find("line")
        arc = elem.find("arc")
        spiral = elem.find("spiral")

        if line is not None:
            geo.geo_type = "line"
        elif arc is not None:
            geo.geo_type = "arc"
            geo.curvature = float(arc.get("curvature", "0"))
        elif spiral is not None:
            geo.geo_type = "spiral"
            geo.curv_start = float(spiral.get("curvStart", "0"))
            geo.curv_end = float(spiral.get("curvEnd", "0"))

        return geo

    def _parse_lane_section(self, elem: ET.Element) -> LaneSection:
        section = LaneSection()
        section.s = float(elem.get("s", "0"))
        section.single_side = elem.get("singleSide", "")

        for group_tag in ["left", "center", "right"]:
            group = elem.find(group_tag)
            if group is not None:
                for lane_elem in group.findall("lane"):
                    section.lanes.append(self._parse_lane(lane_elem))

        return section

    def _parse_lane(self, elem: ET.Element) -> Lane:
        lane = Lane()
        lane.lane_id = int(elem.get("id", "0"))
        lane.lane_type = elem.get("type", "none")
        lane.level = elem.get("level", "")

        link = elem.find("link")
        if link is not None:
            pred = link.find("predecessor")
            if pred is not None:
                lane.predecessor_id = int(pred.get("id", "-1"))
            succ = link.find("successor")
            if succ is not None:
                lane.successor_id = int(succ.get("id", "-1"))

        for width_elem in elem.findall("width"):
            w = LaneWidth()
            w.s_offset = float(width_elem.get("sOffset", "0"))
            w.a = float(width_elem.get("a", "0"))
            w.b = float(width_elem.get("b", "0"))
            w.c = float(width_elem.get("c", "0"))
            w.d = float(width_elem.get("d", "0"))
            lane.widths.append(w)

        speed_elem = elem.find("speed")
        if speed_elem is not None:
            lane.speed_max = float(speed_elem.get("max", "0"))
            lane.speed_unit = speed_elem.get("unit", "")

        return lane

    def _parse_signal(self, elem: ET.Element) -> Signal:
        sig = Signal()
        sig.signal_id = int(elem.get("id", "-1"))
        sig.name = elem.get("name", "")
        sig.s = float(elem.get("s", "0"))
        sig.t = float(elem.get("t", "0"))
        sig.dynamic = elem.get("dynamic", "")
        sig.orientation = elem.get("orientation", "")
        sig.signal_type = elem.get("type", "")
        sig.subtype = elem.get("subtype", "")
        sig.value = float(elem.get("value", "0"))
        sig.unit = elem.get("unit", "")
        return sig

    def _parse_junction(self, elem: ET.Element) -> Junction:
        junction = Junction()
        junction.junction_id = int(elem.get("id", "-1"))
        junction.name = elem.get("name", "")
        junction.junction_type = elem.get("type", "")

        for conn_elem in elem.findall("connection"):
            conn = JunctionConnection()
            conn.connection_id = int(conn_elem.get("id", "-1"))
            conn.incoming_road = int(conn_elem.get("incomingRoad", "-1"))
            conn.connecting_road = int(conn_elem.get("connectingRoad", "-1"))
            conn.contact_point = conn_elem.get("contactPoint", "")

            for ll_elem in conn_elem.findall("laneLink"):
                ll = JunctionConnectionLaneLink()
                ll.from_lane = int(ll_elem.get("from", "0"))
                ll.to_lane = int(ll_elem.get("to", "0"))
                conn.lane_links.append(ll)

            junction.connections.append(conn)

        return junction

    def _parse_controller(self, elem: ET.Element) -> Controller:
        ctrl = Controller()
        ctrl.controller_id = int(elem.get("id", "-1"))
        ctrl.name = elem.get("name", "")
        ctrl.sequence = int(elem.get("sequence", "0"))

        for ctrl_elem in elem.findall("control"):
            sig_id = int(ctrl_elem.get("signalId", "-1"))
            ctrl.controlled_signals.append(sig_id)

        for phase_elem in elem.findall("phase"):
            phase = ControllerPhase()
            phase.name = phase_elem.get("name", "")
            phase.duration = float(phase_elem.get("duration", "0"))

            for sig_elem in phase_elem.findall("signal"):
                state = ControllerPhaseState()
                state.signal_id = sig_elem.get("id", "")
                state.state = sig_elem.get("state", "")
                phase.states.append(state)

            ctrl.phases.append(phase)

        return ctrl
