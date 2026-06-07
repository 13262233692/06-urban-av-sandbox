#include "OpenDrive/OpenDriveParser.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/XmlParser.h"

EOpenDriveLaneType UOpenDriveParser::StringToLaneType(const FString& Str)
{
	if (Str == TEXT("none")) return EOpenDriveLaneType::None;
	if (Str == TEXT("driving")) return EOpenDriveLaneType::Driving;
	if (Str == TEXT("stop")) return EOpenDriveLaneType::Stop;
	if (Str == TEXT("shoulder")) return EOpenDriveLaneType::Shoulder;
	if (Str == TEXT("biking")) return EOpenDriveLaneType::Biking;
	if (Str == TEXT("sidewalk")) return EOpenDriveLaneType::Sidewalk;
	if (Str == TEXT("border")) return EOpenDriveLaneType::Border;
	if (Str == TEXT("restricted")) return EOpenDriveLaneType::Restricted;
	if (Str == TEXT("parking")) return EOpenDriveLaneType::Parking;
	if (Str == TEXT("median")) return EOpenDriveLaneType::Median;
	if (Str == TEXT("curb")) return EOpenDriveLaneType::Curb;
	if (Str == TEXT("exit")) return EOpenDriveLaneType::Exit;
	if (Str == TEXT("entry")) return EOpenDriveLaneType::Entry;
	if (Str == TEXT("onRamp")) return EOpenDriveLaneType::OnRamp;
	if (Str == TEXT("offRamp")) return EOpenDriveLaneType::OffRamp;
	if (Str == TEXT("ramp")) return EOpenDriveLaneType::Ramp;
	if (Str == TEXT("passing")) return EOpenDriveLaneType::Passing;
	return EOpenDriveLaneType::None;
}

EOpenDriveGeometryType UOpenDriveParser::StringToGeometryType(const FString& Str)
{
	if (Str == TEXT("line")) return EOpenDriveGeometryType::Line;
	if (Str == TEXT("arc")) return EOpenDriveGeometryType::Arc;
	if (Str == TEXT("spiral")) return EOpenDriveGeometryType::Spiral;
	if (Str == TEXT("poly3")) return EOpenDriveGeometryType::Poly3;
	return EOpenDriveGeometryType::Line;
}

double UOpenDriveParser::GetDoubleAttr(const FXmlNode* Node, const FString& AttrName, double Default)
{
	if (!Node) return Default;
	const FString* Val = Node->GetAttributes().Find(AttrName);
	if (Val && !Val->IsEmpty())
	{
		return FCString::Atod(**Val);
	}
	return Default;
}

int32 UOpenDriveParser::GetIntAttr(const FXmlNode* Node, const FString& AttrName, int32 Default)
{
	if (!Node) return Default;
	const FString* Val = Node->GetAttributes().Find(AttrName);
	if (Val && !Val->IsEmpty())
	{
		return FCString::Atoi(**Val);
	}
	return Default;
}

FString UOpenDriveParser::GetStringAttr(const FXmlNode* Node, const FString& AttrName, const FString& Default)
{
	if (!Node) return Default;
	const FString* Val = Node->GetAttributes().Find(AttrName);
	if (Val)
	{
		return *Val;
	}
	return Default;
}

void UOpenDriveParser::ParseHeader(const FXmlNode* HeaderNode, FOpenDriveMap& OutMap)
{
	if (!HeaderNode) return;
	OutMap.HeaderRevMajor = GetStringAttr(HeaderNode, TEXT("revMajor"));
	OutMap.HeaderRevMinor = GetStringAttr(HeaderNode, TEXT("revMinor"));
	OutMap.HeaderName = GetStringAttr(HeaderNode, TEXT("name"));
	OutMap.HeaderVersion = GetStringAttr(HeaderNode, TEXT("version"));
	OutMap.HeaderDate = GetStringAttr(HeaderNode, TEXT("date"));
	OutMap.HeaderNorth = GetDoubleAttr(HeaderNode, TEXT("north"));
	OutMap.HeaderSouth = GetDoubleAttr(HeaderNode, TEXT("south"));
	OutMap.HeaderEast = GetDoubleAttr(HeaderNode, TEXT("east"));
	OutMap.HeaderWest = GetDoubleAttr(HeaderNode, TEXT("west"));
	OutMap.HeaderVendor = GetStringAttr(HeaderNode, TEXT("vendor"));
}

void UOpenDriveParser::ParseGeometry(const FXmlNode* GeoNode, FOpenDriveGeometry& OutGeo)
{
	if (!GeoNode) return;

	FOpenDriveGeometryLine& Line = OutGeo.Line;
	Line.S = GetDoubleAttr(GeoNode, TEXT("s"));
	Line.X = GetDoubleAttr(GeoNode, TEXT("x"));
	Line.Y = GetDoubleAttr(GeoNode, TEXT("y"));
	Line.Hdg = GetDoubleAttr(GeoNode, TEXT("hdg"));
	Line.Length = GetDoubleAttr(GeoNode, TEXT("length"));

	const FXmlNode* LineNode = GeoNode->FindChildNode(TEXT("line"));
	const FXmlNode* ArcNode = GeoNode->FindChildNode(TEXT("arc"));
	const FXmlNode* SpiralNode = GeoNode->FindChildNode(TEXT("spiral"));
	const FXmlNode* Poly3Node = GeoNode->FindChildNode(TEXT("poly3"));

	if (LineNode)
	{
		OutGeo.Type = EOpenDriveGeometryType::Line;
	}
	else if (ArcNode)
	{
		OutGeo.Type = EOpenDriveGeometryType::Arc;
		OutGeo.Arc.S = Line.S;
		OutGeo.Arc.X = Line.X;
		OutGeo.Arc.Y = Line.Y;
		OutGeo.Arc.Hdg = Line.Hdg;
		OutGeo.Arc.Length = Line.Length;
		OutGeo.Arc.Curvature = GetDoubleAttr(ArcNode, TEXT("curvature"));
	}
	else if (SpiralNode)
	{
		OutGeo.Type = EOpenDriveGeometryType::Spiral;
		OutGeo.Spiral.S = Line.S;
		OutGeo.Spiral.X = Line.X;
		OutGeo.Spiral.Y = Line.Y;
		OutGeo.Spiral.Hdg = Line.Hdg;
		OutGeo.Spiral.Length = Line.Length;
		OutGeo.Spiral.CurvStart = GetDoubleAttr(SpiralNode, TEXT("curvStart"));
		OutGeo.Spiral.CurvEnd = GetDoubleAttr(SpiralNode, TEXT("curvEnd"));
	}
	else if (Poly3Node)
	{
		OutGeo.Type = EOpenDriveGeometryType::Poly3;
		OutGeo.Poly3.S = Line.S;
		OutGeo.Poly3.X = Line.X;
		OutGeo.Poly3.Y = Line.Y;
		OutGeo.Poly3.Hdg = Line.Hdg;
		OutGeo.Poly3.Length = Line.Length;
		OutGeo.Poly3.A = GetDoubleAttr(Poly3Node, TEXT("a"));
		OutGeo.Poly3.B = GetDoubleAttr(Poly3Node, TEXT("b"));
		OutGeo.Poly3.C = GetDoubleAttr(Poly3Node, TEXT("c"));
		OutGeo.Poly3.D = GetDoubleAttr(Poly3Node, TEXT("d"));
	}
}

void UOpenDriveParser::ParseRoadLink(const FXmlNode* LinkNode, FOpenDriveRoadLink& OutLink)
{
	if (!LinkNode) return;

	const FXmlNode* PredNode = LinkNode->FindChildNode(TEXT("predecessor"));
	if (PredNode)
	{
		OutLink.bHasPredecessor = true;
		OutLink.Predecessor.ElementType = GetStringAttr(PredNode, TEXT("elementType"));
		OutLink.Predecessor.ElementId = GetIntAttr(PredNode, TEXT("elementId"));
		OutLink.Predecessor.ContactPoint = GetStringAttr(PredNode, TEXT("contactPoint"));
	}

	const FXmlNode* SuccNode = LinkNode->FindChildNode(TEXT("successor"));
	if (SuccNode)
	{
		OutLink.bHasSuccessor = true;
		OutLink.Successor.ElementType = GetStringAttr(SuccNode, TEXT("elementType"));
		OutLink.Successor.ElementId = GetIntAttr(SuccNode, TEXT("elementId"));
		OutLink.Successor.ContactPoint = GetStringAttr(SuccNode, TEXT("contactPoint"));
	}
}

void UOpenDriveParser::ParseElevationProfile(const FXmlNode* ElevNode, FOpenDriveRoad& OutRoad)
{
	if (!ElevNode) return;

	const TArray<FXmlNode*>& Children = ElevNode->GetChildrenNodes();
	for (FXmlNode* Child : Children)
	{
		if (Child->GetTag() == TEXT("elevation"))
		{
			FOpenDriveElevationProfile Prof;
			Prof.S = GetDoubleAttr(Child, TEXT("s"));
			Prof.A = GetDoubleAttr(Child, TEXT("a"));
			Prof.B = GetDoubleAttr(Child, TEXT("b"));
			Prof.C = GetDoubleAttr(Child, TEXT("c"));
			Prof.D = GetDoubleAttr(Child, TEXT("d"));
			OutRoad.ElevationProfile.Add(Prof);
		}
	}
}

void UOpenDriveParser::ParseRoadTypes(const FXmlNode* TypeNode, FOpenDriveRoad& OutRoad)
{
	if (!TypeNode) return;

	FOpenDriveRoadType RdType;
	RdType.S = GetDoubleAttr(TypeNode, TEXT("s"));
	RdType.Type = GetStringAttr(TypeNode, TEXT("type"));

	const FXmlNode* SpeedNode = TypeNode->FindChildNode(TEXT("speed"));
	if (SpeedNode)
	{
		FOpenDriveRoadTypeSpeed Speed;
		Speed.S = RdType.S;
		Speed.Max = GetDoubleAttr(SpeedNode, TEXT("max"));
		Speed.Unit = GetStringAttr(SpeedNode, TEXT("unit"));
		Speed.Type = GetStringAttr(SpeedNode, TEXT("type"));
		Speed.Country = GetStringAttr(SpeedNode, TEXT("country"));
		RdType.Speed.Add(Speed);
	}

	OutRoad.RoadTypes.Add(RdType);
}

void UOpenDriveParser::ParseLane(const FXmlNode* LaneNode, FOpenDriveLane& OutLane)
{
	if (!LaneNode) return;

	OutLane.Id = GetIntAttr(LaneNode, TEXT("id"));
	OutLane.Type = StringToLaneType(GetStringAttr(LaneNode, TEXT("type")));
	OutLane.Level = GetStringAttr(LaneNode, TEXT("level"));

	const FXmlNode* LinkNode = LaneNode->FindChildNode(TEXT("link"));
	if (LinkNode)
	{
		const FXmlNode* PredNode = LinkNode->FindChildNode(TEXT("predecessor"));
		if (PredNode)
		{
			OutLane.Link.Predecessor = GetIntAttr(PredNode, TEXT("id"));
		}
		const FXmlNode* SuccNode = LinkNode->FindChildNode(TEXT("successor"));
		if (SuccNode)
		{
			OutLane.Link.Successor = GetIntAttr(SuccNode, TEXT("id"));
		}
	}

	const TArray<FXmlNode*>& Children = LaneNode->GetChildrenNodes();
	for (FXmlNode* Child : Children)
	{
		if (Child->GetTag() == TEXT("width"))
		{
			FOpenDriveLaneWidth Width;
			Width.SOffset = GetDoubleAttr(Child, TEXT("sOffset"));
			Width.A = GetDoubleAttr(Child, TEXT("a"));
			Width.B = GetDoubleAttr(Child, TEXT("b"));
			Width.C = GetDoubleAttr(Child, TEXT("c"));
			Width.D = GetDoubleAttr(Child, TEXT("d"));
			OutLane.Widths.Add(Width);
		}
		else if (Child->GetTag() == TEXT("roadMark"))
		{
			FOpenDriveLaneRoadMark Mark;
			Mark.SOffset = GetDoubleAttr(Child, TEXT("sOffset"));
			Mark.Type = GetStringAttr(Child, TEXT("type"));
			Mark.Weight = GetStringAttr(Child, TEXT("weight"));
			Mark.Color = GetStringAttr(Child, TEXT("color"));
			Mark.Width = GetDoubleAttr(Child, TEXT("width"));
			Mark.LaneChange = GetStringAttr(Child, TEXT("laneChange"));
			OutLane.RoadMarks.Add(Mark);
		}
		else if (Child->GetTag() == TEXT("speed"))
		{
			OutLane.Speed = GetDoubleAttr(Child, TEXT("max"));
			OutLane.SpeedUnit = GetStringAttr(Child, TEXT("unit"));
		}
	}
}

void UOpenDriveParser::ParseLaneSection(const FXmlNode* SectionNode, FOpenDriveLaneSection& OutSection)
{
	if (!SectionNode) return;

	OutSection.S = GetDoubleAttr(SectionNode, TEXT("s"));
	OutSection.SingleSide = GetStringAttr(SectionNode, TEXT("singleSide"));

	const FXmlNode* LeftNode = SectionNode->FindChildNode(TEXT("left"));
	const FXmlNode* CenterNode = SectionNode->FindChildNode(TEXT("center"));
	const FXmlNode* RightNode = SectionNode->FindChildNode(TEXT("right"));

	auto ParseLaneGroup = [](const FXmlNode* GroupNode, TArray<FOpenDriveLane>& OutLanes)
	{
		if (!GroupNode) return;
		const TArray<FXmlNode*>& Children = GroupNode->GetChildrenNodes();
		for (FXmlNode* Child : Children)
		{
			if (Child->GetTag() == TEXT("lane"))
			{
				FOpenDriveLane Lane;
				ParseLane(Child, Lane);
				OutLanes.Add(Lane);
			}
		}
	};

	ParseLaneGroup(LeftNode, OutSection.Lanes);
	ParseLaneGroup(CenterNode, OutSection.Lanes);
	ParseLaneGroup(RightNode, OutSection.Lanes);
}

void UOpenDriveParser::ParseLanes(const FXmlNode* LanesNode, FOpenDriveRoad& OutRoad)
{
	if (!LanesNode) return;

	const TArray<FXmlNode*>& Children = LanesNode->GetChildrenNodes();
	for (FXmlNode* Child : Children)
	{
		if (Child->GetTag() == TEXT("laneSection"))
		{
			FOpenDriveLaneSection Section;
			ParseLaneSection(Child, Section);
			OutRoad.LaneSections.Add(Section);
		}
	}
}

void UOpenDriveParser::ParseSignals(const FXmlNode* SignalsNode, FOpenDriveRoad& OutRoad)
{
	if (!SignalsNode) return;

	const TArray<FXmlNode*>& Children = SignalsNode->GetChildrenNodes();
	for (FXmlNode* Child : Children)
	{
		if (Child->GetTag() == TEXT("signal"))
		{
			FOpenDriveSignal Signal;
			Signal.Id = GetIntAttr(Child, TEXT("id"));
			Signal.Name = GetStringAttr(Child, TEXT("name"));
			Signal.S = GetDoubleAttr(Child, TEXT("s"));
			Signal.T = GetDoubleAttr(Child, TEXT("t"));
			Signal.Dynamic = GetStringAttr(Child, TEXT("dynamic"));
			Signal.Orientation = GetStringAttr(Child, TEXT("orientation"));
			Signal.ZOffset = GetDoubleAttr(Child, TEXT("zOffset"));
			Signal.Country = GetStringAttr(Child, TEXT("country"));
			Signal.Type = GetStringAttr(Child, TEXT("type"));
			Signal.Subtype = GetStringAttr(Child, TEXT("subtype"));
			Signal.Value = GetDoubleAttr(Child, TEXT("value"));
			Signal.Unit = GetStringAttr(Child, TEXT("unit"));
			Signal.Height = GetDoubleAttr(Child, TEXT("height"));
			Signal.Width = GetDoubleAttr(Child, TEXT("width"));
			Signal.Text = GetStringAttr(Child, TEXT("text"));
			Signal.HOffset = GetIntAttr(Child, TEXT("hOffset"));
			Signal.Pitch = GetDoubleAttr(Child, TEXT("pitch"));
			Signal.Roll = GetDoubleAttr(Child, TEXT("roll"));
			OutRoad.Signals.Add(Signal);
		}
		else if (Child->GetTag() == TEXT("signalReference"))
		{
			FOpenDriveSignalReference Ref;
			Ref.Id = GetIntAttr(Child, TEXT("id"));
			Ref.Type = GetStringAttr(Child, TEXT("type"));
			Ref.S = GetDoubleAttr(Child, TEXT("s"));
			Ref.Orientation = GetStringAttr(Child, TEXT("orientation"));
			OutRoad.SignalReferences.Add(Ref);
		}
	}
}

void UOpenDriveParser::ParseJunction(const FXmlNode* JunctionNode, FOpenDriveJunction& OutJunction)
{
	if (!JunctionNode) return;

	OutJunction.Id = GetIntAttr(JunctionNode, TEXT("id"));
	OutJunction.Name = GetStringAttr(JunctionNode, TEXT("name"));
	OutJunction.Type = GetStringAttr(JunctionNode, TEXT("type"));

	const TArray<FXmlNode*>& Children = JunctionNode->GetChildrenNodes();
	for (FXmlNode* Child : Children)
	{
		if (Child->GetTag() == TEXT("connection"))
		{
			FOpenDriveJunctionConnection Conn;
			Conn.Id = GetIntAttr(Child, TEXT("id"));
			Conn.IncomingRoad = GetIntAttr(Child, TEXT("incomingRoad"));
			Conn.ConnectingRoad = GetIntAttr(Child, TEXT("connectingRoad"));
			Conn.ContactPoint = GetStringAttr(Child, TEXT("contactPoint"));

			const TArray<FXmlNode*>& SubChildren = Child->GetChildrenNodes();
			for (FXmlNode* SubChild : SubChildren)
			{
				if (SubChild->GetTag() == TEXT("laneLink"))
				{
					FOpenDriveJunctionConnectionLaneLink LL;
					LL.From = GetIntAttr(SubChild, TEXT("from"));
					LL.To = GetIntAttr(SubChild, TEXT("to"));
					Conn.LaneLinks.Add(LL);
				}
			}

			OutJunction.Connections.Add(Conn);
		}
	}
}

void UOpenDriveParser::ParseController(const FXmlNode* ControllerNode, FOpenDriveController& OutController)
{
	if (!ControllerNode) return;

	OutController.Id = GetIntAttr(ControllerNode, TEXT("id"));
	OutController.Name = GetStringAttr(ControllerNode, TEXT("name"));
	OutController.Sequence = GetIntAttr(ControllerNode, TEXT("sequence"));

	const TArray<FXmlNode*>& Children = ControllerNode->GetChildrenNodes();
	for (FXmlNode* Child : Children)
	{
		if (Child->GetTag() == TEXT("control"))
		{
			int32 SignalId = GetIntAttr(Child, TEXT("signalId"));
			OutController.ControlledSignals.Add(SignalId);
		}
		else if (Child->GetTag() == TEXT("phase"))
		{
			FOpenDriveControllerPhase Phase;
			Phase.Name = GetStringAttr(Child, TEXT("name"));
			Phase.Duration = GetDoubleAttr(Child, TEXT("duration"));

			const TArray<FXmlNode*>& PhaseChildren = Child->GetChildrenNodes();
			for (FXmlNode* PhaseChild : PhaseChildren)
			{
				if (PhaseChild->GetTag() == TEXT("signal"))
				{
					FOpenDriveControllerPhaseState State;
					State.SignalId = GetStringAttr(PhaseChild, TEXT("id"));
					State.State = GetStringAttr(PhaseChild, TEXT("state"));
					State.Duration = Phase.Duration;
					Phase.States.Add(State);
				}
			}

			OutController.Phases.Add(Phase);
		}
	}
}

void UOpenDriveParser::ParseRoad(const FXmlNode* RoadNode, FOpenDriveRoad& OutRoad)
{
	if (!RoadNode) return;

	OutRoad.Id = GetIntAttr(RoadNode, TEXT("id"));
	OutRoad.Name = GetStringAttr(RoadNode, TEXT("name"));
	OutRoad.Length = GetDoubleAttr(RoadNode, TEXT("length"));
	OutRoad.Junction = GetStringAttr(RoadNode, TEXT("junction"));

	const FXmlNode* LinkNode = RoadNode->FindChildNode(TEXT("link"));
	ParseRoadLink(LinkNode, OutRoad.Link);

	const FXmlNode* PlanViewNode = RoadNode->FindChildNode(TEXT("planView"));
	if (PlanViewNode)
	{
		const TArray<FXmlNode*>& GeoChildren = PlanViewNode->GetChildrenNodes();
		for (FXmlNode* GeoChild : GeoChildren)
		{
			if (GeoChild->GetTag() == TEXT("geometry"))
			{
				FOpenDriveGeometry Geo;
				ParseGeometry(GeoChild, Geo);
				OutRoad.Geometries.Add(Geo);
			}
		}
	}

	const FXmlNode* ElevationNode = RoadNode->FindChildNode(TEXT("elevationProfile"));
	ParseElevationProfile(ElevationNode, OutRoad);

	const TArray<FXmlNode*>& AllChildren = RoadNode->GetChildrenNodes();
	for (FXmlNode* Child : AllChildren)
	{
		if (Child->GetTag() == TEXT("type"))
		{
			ParseRoadTypes(Child, OutRoad);
		}
	}

	const FXmlNode* LanesNode = RoadNode->FindChildNode(TEXT("lanes"));
	ParseLanes(LanesNode, OutRoad);

	const FXmlNode* SignalsNode = RoadNode->FindChildNode(TEXT("signals"));
	ParseSignals(SignalsNode, OutRoad);
}

FOpenDriveMap UOpenDriveParser::ParseFromString(const FString& XmlContent)
{
	FOpenDriveMap Result;

	FXmlFile XmlFile;
	if (!XmlFile.LoadFile(XmlContent, EXmlFileMode::Processing))
	{
		UE_LOG(LogTemp, Error, TEXT("[OpenDriveParser] Failed to parse XML: %s"), *XmlFile.GetLastError());
		return Result;
	}

	const FXmlNode* RootNode = XmlFile.GetRootNode();
	if (!RootNode)
	{
		UE_LOG(LogTemp, Error, TEXT("[OpenDriveParser] No root node found"));
		return Result;
	}

	const FXmlNode* HeaderNode = RootNode->FindChildNode(TEXT("header"));
	ParseHeader(HeaderNode, Result);

	const TArray<FXmlNode*>& Children = RootNode->GetChildrenNodes();
	for (const FXmlNode* Child : Children)
	{
		if (Child->GetTag() == TEXT("road"))
		{
			FOpenDriveRoad Road;
			ParseRoad(Child, Road);
			Result.Roads.Add(Road);
		}
		else if (Child->GetTag() == TEXT("junction"))
		{
			FOpenDriveJunction Junction;
			ParseJunction(Child, Junction);
			Result.Junctions.Add(Junction);
		}
		else if (Child->GetTag() == TEXT("controller"))
		{
			FOpenDriveController Controller;
			ParseController(Child, Controller);
			Result.Controllers.Add(Controller);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[OpenDriveParser] Parsed %d roads, %d junctions, %d controllers"),
		Result.Roads.Num(), Result.Junctions.Num(), Result.Controllers.Num());

	return Result;
}

FOpenDriveMap UOpenDriveParser::ParseFromFile(const FString& FilePath)
{
	FString FileContent;
	if (!FFileHelper::LoadFileToString(FileContent, *FilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("[OpenDriveParser] Failed to load file: %s"), *FilePath);
		return FOpenDriveMap();
	}

	UE_LOG(LogTemp, Log, TEXT("[OpenDriveParser] Loaded file: %s (%d chars)"), *FilePath, FileContent.Len());
	return ParseFromString(FileContent);
}

bool UOpenDriveParser::WriteDebugSummary(const FOpenDriveMap& Map, const FString& OutputPath)
{
	FString Summary;
	Summary += FString::Printf(TEXT("OpenDRIVE Map Summary\n"));
	Summary += FString::Printf(TEXT("  Roads: %d\n"), Map.Roads.Num());
	Summary += FString::Printf(TEXT("  Junctions: %d\n"), Map.Junctions.Num());
	Summary += FString::Printf(TEXT("  Controllers: %d\n"), Map.Controllers.Num());

	for (int32 i = 0; i < Map.Roads.Num(); ++i)
	{
		const FOpenDriveRoad& Road = Map.Roads[i];
		Summary += FString::Printf(TEXT("  Road[%d]: id=%d, name=%s, length=%.2f, junction=%s\n"),
			i, Road.Id, *Road.Name, Road.Length, *Road.Junction);
		Summary += FString::Printf(TEXT("    Geometries: %d, LaneSections: %d, Signals: %d\n"),
			Road.Geometries.Num(), Road.LaneSections.Num(), Road.Signals.Num());

		for (int32 j = 0; j < Road.LaneSections.Num(); ++j)
		{
			const FOpenDriveLaneSection& Sec = Road.LaneSections[j];
			Summary += FString::Printf(TEXT("    LaneSection[%d]: s=%.2f, lanes=%d\n"),
				j, Sec.S, Sec.Lanes.Num());
		}
	}

	return FFileHelper::SaveStringToFile(Summary, *OutputPath);
}
