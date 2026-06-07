#include "LaneGraph/LaneGraph.h"
#include "Math/UnrealMathUtility.h"

TArray<FVector> ULaneGraphBuilder::SampleCenterlinePoints(
	const FOpenDriveRoad& Road,
	const FOpenDriveLaneSection& Section,
	const FOpenDriveLane& Lane,
	float SampleInterval)
{
	TArray<FVector> Points;
	if (Road.Geometries.Num() == 0) return Points;

	double LaneWidth = ComputeLaneWidth(Lane, Section.S);
	double LaneOffset = 0.0;

	for (const FOpenDriveLane& SLane : Section.Lanes)
	{
		if (SLane.Id == 0)
		{
			continue;
		}
		if (SLane.Id == Lane.Id) break;
		if ((Lane.Id > 0 && SLane.Id > 0 && SLane.Id < Lane.Id) ||
			(Lane.Id < 0 && SLane.Id < 0 && SLane.Id > Lane.Id))
		{
			LaneOffset += ComputeLaneWidth(SLane, Section.S);
		}
	}

	if (Lane.Id > 0)
	{
		LaneOffset += LaneWidth * 0.5;
	}
	else if (Lane.Id < 0)
	{
		LaneOffset += LaneWidth * 0.5;
		LaneOffset = -LaneOffset;
	}

	double CumS = 0.0;
	for (const FOpenDriveGeometry& Geo : Road.Geometries)
	{
		double GeoStartS = Geo.Line.S;
		double GeoEndS = GeoStartS + Geo.Line.Length;

		if (GeoStartS >= Section.S + (Road.LaneSections.Num() > 1 ?
			(Road.LaneSections.IndexOfByKey(Section) < Road.LaneSections.Num() - 1 ?
				Road.LaneSections[Road.LaneSections.IndexOfByKey(Section) + 1].S - Section.S :
				Road.Length - Section.S) :
			Road.Length - Section.S) + Section.S)
		{
			continue;
		}

		double Step = FMath::Max(SampleInterval, 1.0);
		double LocalS = 0.0;

		while (LocalS < Geo.Line.Length)
		{
			FVector Point = SampleGeometryPoint(Geo, LocalS);

			double Hdg = Geo.Line.Hdg;
			float PerpX = -FMath::Sin(Hdg) * LaneOffset;
			float PerpY = FMath::Cos(Hdg) * LaneOffset;
			Point.X += PerpX * 100.0f;
			Point.Y += PerpY * 100.0f;

			Points.Add(Point);
			LocalS += Step;
		}
	}

	if (Points.Num() == 0 && Road.Geometries.Num() > 0)
	{
		FVector Point = SampleGeometryPoint(Road.Geometries[0], 0.0);
		Points.Add(Point);
	}

	return Points;
}

double ULaneGraphBuilder::ComputeLaneWidth(const FOpenDriveLane& Lane, double S)
{
	if (Lane.Widths.Num() == 0) return 3.5;

	const FOpenDriveLaneWidth* BestWidth = nullptr;
	for (const FOpenDriveLaneWidth& W : Lane.Widths)
	{
		if (!BestWidth || W.SOffset <= S)
		{
			BestWidth = &W;
		}
	}

	if (!BestWidth) return 3.5;

	double DS = S - BestWidth->SOffset;
	return BestWidth->A + BestWidth->B * DS + BestWidth->C * DS * DS + BestWidth->D * DS * DS * DS;
}

FVector ULaneGraphBuilder::SampleGeometryPoint(const FOpenDriveGeometry& Geo, double LocalS)
{
	double X = Geo.Line.X;
	double Y = Geo.Line.Y;
	double Hdg = Geo.Line.Hdg;

	switch (Geo.Type)
	{
	case EOpenDriveGeometryType::Line:
	{
		X += LocalS * FMath::Cos(Hdg);
		Y += LocalS * FMath::Sin(Hdg);
		break;
	}
	case EOpenDriveGeometryType::Arc:
	{
		double Curv = Geo.Arc.Curvature;
		if (FMath::Abs(Curv) > 1e-9)
		{
			double R = 1.0 / Curv;
			double Theta = LocalS * Curv;
			X += R * (FMath::Sin(Hdg + Theta) - FMath::Sin(Hdg));
			Y += R * (FMath::Cos(Hdg) - FMath::Cos(Hdg + Theta));
		}
		else
		{
			X += LocalS * FMath::Cos(Hdg);
			Y += LocalS * FMath::Sin(Hdg);
		}
		break;
	}
	case EOpenDriveGeometryType::Spiral:
	{
		double C0 = Geo.Spiral.CurvStart;
		double C1 = Geo.Spiral.CurvEnd;
		double Len = Geo.Spiral.Length;
		double Curv = C0 + (C1 - C0) * (Len > 0 ? LocalS / Len : 0.0);
		double AvgHdg = Hdg + Curv * LocalS * 0.5;
		X += LocalS * FMath::Cos(AvgHdg);
		Y += LocalS * FMath::Sin(AvgHdg);
		break;
	}
	case EOpenDriveGeometryType::Poly3:
	{
		X += LocalS * FMath::Cos(Hdg);
		Y += LocalS * FMath::Sin(Hdg);
		double V = Geo.Poly3.A + Geo.Poly3.B * LocalS +
			Geo.Poly3.C * LocalS * LocalS + Geo.Poly3.D * LocalS * LocalS * LocalS;
		Y += V;
		break;
	}
	}

	return FVector(X * 100.0, Y * 100.0, 0.0);
}

double ULaneGraphBuilder::GetSpeedLimitForRoad(const FOpenDriveRoad& Road)
{
	for (const FOpenDriveRoadType& RdType : Road.RoadTypes)
	{
		for (const FOpenDriveRoadTypeSpeed& Speed : RdType.Speed)
		{
			if (Speed.Max > 0.0)
			{
				if (Speed.Unit == TEXT("mph"))
				{
					return Speed.Max * 0.44704;
				}
				return Speed.Max;
			}
		}
	}

	for (const FOpenDriveLaneSection& Section : Road.LaneSections)
	{
		for (const FOpenDriveLane& Lane : Section.Lanes)
		{
			if (Lane.Speed > 0.0)
			{
				if (Lane.SpeedUnit == TEXT("mph"))
				{
					return Lane.Speed * 0.44704;
				}
				return Lane.Speed;
			}
		}
	}

	return 13.89;
}

void ULaneGraphBuilder::BuildRoadLaneNodes(
	const FOpenDriveRoad& Road,
	FLaneGraph& OutGraph,
	float SampleInterval,
	TMap<FString, int32>& RoadLaneToNodeId)
{
	double SpeedLimit = GetSpeedLimitForRoad(Road);

	for (int32 SecIdx = 0; SecIdx < Road.LaneSections.Num(); ++SecIdx)
	{
		const FOpenDriveLaneSection& Section = Road.LaneSections[SecIdx];
		double SStart = Section.S;
		double SEnd = (SecIdx + 1 < Road.LaneSections.Num())
			? Road.LaneSections[SecIdx + 1].S
			: Road.Length;

		for (const FOpenDriveLane& Lane : Section.Lanes)
		{
			if (Lane.Id == 0) continue;
			if (Lane.Type != EOpenDriveLaneType::Driving &&
				Lane.Type != EOpenDriveLaneType::Entry &&
				Lane.Type != EOpenDriveLaneType::Exit &&
				Lane.Type != EOpenDriveLaneType::OnRamp &&
				Lane.Type != EOpenDriveLaneType::OffRamp &&
				Lane.Type != EOpenDriveLaneType::Ramp)
			{
				continue;
			}

			FLaneGraphNode Node;
			Node.RoadId = Road.Id;
			Node.LaneSectionIndex = SecIdx;
			Node.LaneId = Lane.Id;
			Node.SStart = SStart;
			Node.SEnd = SEnd;
			Node.SpeedLimit = SpeedLimit;
			Node.bIsJunction = !Road.Junction.IsEmpty();
			Node.Width = ComputeLaneWidth(Lane, SStart);
			Node.Length = SEnd - SStart;

			Node.CenterlinePoints = SampleCenterlinePoints(Road, Section, Lane, SampleInterval);

			if (Lane.Speed > 0.0)
			{
				if (Lane.SpeedUnit == TEXT("mph"))
				{
					Node.SpeedLimit = Lane.Speed * 0.44704;
				}
				else
				{
					Node.SpeedLimit = Lane.Speed;
				}
			}

			for (const FOpenDriveSignal& Signal : Road.Signals)
			{
				if (Signal.S >= SStart && Signal.S < SEnd && Signal.Dynamic == TEXT("yes"))
				{
					Node.TrafficLightSignalId = Signal.Id;
				}
			}

			int32 NodeIdx = OutGraph.AddNode(Node);
			FString Key = FString::Printf(TEXT("%d_%d_%d"), Road.Id, SecIdx, Lane.Id);
			RoadLaneToNodeId.Add(Key, NodeIdx);
		}

		TArray<int32> LeftIds;
		TArray<int32> RightIds;
		int32 CenterIdx = -1;

		for (const FOpenDriveLane& Lane : Section.Lanes)
		{
			if (Lane.Id == 0) continue;
			FString Key = FString::Printf(TEXT("%d_%d_%d"), Road.Id, SecIdx, Lane.Id);
			int32* NodeIdx = RoadLaneToNodeId.Find(Key);
			if (!NodeIdx) continue;

			if (Lane.Id > 0)
			{
				LeftIds.Add(*NodeIdx);
			}
			else if (Lane.Id < 0)
			{
				RightIds.Add(*NodeIdx);
			}
		}

		for (int32 i = 0; i < LeftIds.Num() - 1; ++i)
		{
			FLaneGraphEdge EdgeRight;
			EdgeRight.FromNodeId = LeftIds[i];
			EdgeRight.ToNodeId = LeftIds[i + 1];
			EdgeRight.Type = ELaneGraphEdgeType::LateralRight;
			EdgeRight.TransitionCost = 0.5;
			OutGraph.AddEdge(EdgeRight);

			FLaneGraphEdge EdgeLeft;
			EdgeLeft.FromNodeId = LeftIds[i + 1];
			EdgeLeft.ToNodeId = LeftIds[i];
			EdgeLeft.Type = ELaneGraphEdgeType::LateralLeft;
			EdgeLeft.TransitionCost = 0.5;
			OutGraph.AddEdge(EdgeLeft);
		}

		for (int32 i = 0; i < RightIds.Num() - 1; ++i)
		{
			FLaneGraphEdge EdgeRight;
			EdgeRight.FromNodeId = RightIds[i];
			EdgeRight.ToNodeId = RightIds[i + 1];
			EdgeRight.Type = ELaneGraphEdgeType::LateralRight;
			EdgeRight.TransitionCost = 0.5;
			OutGraph.AddEdge(EdgeRight);

			FLaneGraphEdge EdgeLeft;
			EdgeLeft.FromNodeId = RightIds[i + 1];
			EdgeLeft.ToNodeId = RightIds[i];
			EdgeLeft.Type = ELaneGraphEdgeType::LateralLeft;
			EdgeLeft.TransitionCost = 0.5;
			OutGraph.AddEdge(EdgeLeft);
		}
	}
}

void ULaneGraphBuilder::BuildJunctionConnections(
	const FOpenDriveJunction& Junction,
	const FOpenDriveMap& Map,
	FLaneGraph& OutGraph,
	const TMap<FString, int32>& RoadLaneToNodeId)
{
	for (const FOpenDriveJunctionConnection& Conn : Junction.Connections)
	{
		const FOpenDriveRoad* ConnectingRoad = nullptr;
		for (const FOpenDriveRoad& Rd : Map.Roads)
		{
			if (Rd.Id == Conn.ConnectingRoad)
			{
				ConnectingRoad = &Rd;
				break;
			}
		}

		if (!ConnectingRoad) continue;

		for (const FOpenDriveJunctionConnectionLaneLink& LL : Conn.LaneLinks)
		{
			FString FromKey = FString::Printf(TEXT("%d_%d_%d"), Conn.IncomingRoad, 0, LL.From);
			FString ToKey = FString::Printf(TEXT("%d_%d_%d"), Conn.ConnectingRoad, 0, LL.To);

			const int32* FromIdx = RoadLaneToNodeId.Find(FromKey);
			const int32* ToIdx = RoadLaneToNodeId.Find(ToKey);

			if (FromIdx && ToIdx)
			{
				FLaneGraphEdge Edge;
				Edge.FromNodeId = *FromIdx;
				Edge.ToNodeId = *ToIdx;
				Edge.Type = ELaneGraphEdgeType::JunctionEntry;
				Edge.JunctionConnectionId = Conn.Id;
				Edge.TransitionCost = 2.0;
				OutGraph.AddEdge(Edge);
			}
		}
	}
}

void ULaneGraphBuilder::BuildTrafficLightControllers(
	const FOpenDriveMap& Map,
	FLaneGraph& OutGraph)
{
	for (const FOpenDriveController& Ctrl : Map.Controllers)
	{
		FTrafficLightController TLC;
		TLC.ControllerId = Ctrl.Id;
		TLC.Name = Ctrl.Name;
		TLC.ControlledSignalIds = Ctrl.ControlledSignals;

		for (const FOpenDriveControllerPhase& Phase : Ctrl.Phases)
		{
			FTrafficLightPhase TLPhase;
			TLPhase.PhaseName = Phase.Name;
			TLPhase.Duration = Phase.Duration;

			for (const FOpenDriveControllerPhaseState& State : Phase.States)
			{
				int32 SigId = FCString::Atoi(*State.SignalId);
				TLPhase.SignalStates.Add(SigId, State.State);
			}

			TLC.Phases.Add(TLPhase);
		}

		OutGraph.TrafficLightControllers.Add(TLC);
	}
}

void ULaneGraphBuilder::BuildRoadLinks(
	const FOpenDriveMap& Map,
	FLaneGraph& OutGraph,
	const TMap<FString, int32>& RoadLaneToNodeId)
{
	TMap<int32, const FOpenDriveRoad*> RoadMap;
	for (const FOpenDriveRoad& Rd : Map.Roads)
	{
		RoadMap.Add(Rd.Id, &Rd);
	}

	for (const FOpenDriveRoad& Road : Map.Roads)
	{
		int32 LastSecIdx = Road.LaneSections.Num() - 1;
		int32 FirstSecIdx = 0;

		if (Road.Link.bHasSuccessor)
		{
			int32 SuccId = Road.Link.Successor.ElementId;

			if (Road.Link.Successor.ElementType == TEXT("road"))
			{
				const FOpenDriveRoad* SuccRoad = RoadMap.FindRef(SuccId);
				if (!SuccRoad) continue;

				int32 SuccSecIdx = 0;
				if (Road.Link.Successor.ContactPoint == TEXT("end"))
				{
					SuccSecIdx = SuccRoad->LaneSections.Num() - 1;
				}

				for (const FOpenDriveLane& Lane : Road.LaneSections[LastSecIdx].Lanes)
				{
					if (Lane.Id == 0) continue;
					if (Lane.Link.Successor == -1) continue;

					FString FromKey = FString::Printf(TEXT("%d_%d_%d"), Road.Id, LastSecIdx, Lane.Id);
					FString ToKey = FString::Printf(TEXT("%d_%d_%d"), SuccId, SuccSecIdx, Lane.Link.Successor);

					const int32* FromIdx = RoadLaneToNodeId.Find(FromKey);
					const int32* ToIdx = RoadLaneToNodeId.Find(ToKey);

					if (FromIdx && ToIdx)
					{
						FLaneGraphEdge Edge;
						Edge.FromNodeId = *FromIdx;
						Edge.ToNodeId = *ToIdx;
						Edge.Type = ELaneGraphEdgeType::Successor;
						Edge.TransitionCost = 0.1;
						OutGraph.AddEdge(Edge);
					}
				}
			}
			else if (Road.Link.Successor.ElementType == TEXT("junction"))
			{
				// Junction connections handled separately
			}
		}

		if (Road.Link.bHasPredecessor)
		{
			int32 PredId = Road.Link.Predecessor.ElementId;

			if (Road.Link.Predecessor.ElementType == TEXT("road"))
			{
				const FOpenDriveRoad* PredRoad = RoadMap.FindRef(PredId);
				if (!PredRoad) continue;

				int32 PredSecIdx = 0;
				if (Road.Link.Predecessor.ContactPoint == TEXT("end"))
				{
					PredSecIdx = PredRoad->LaneSections.Num() - 1;
				}

				for (const FOpenDriveLane& Lane : Road.LaneSections[FirstSecIdx].Lanes)
				{
					if (Lane.Id == 0) continue;
					if (Lane.Link.Predecessor == -1) continue;

					FString FromKey = FString::Printf(TEXT("%d_%d_%d"), PredId, PredSecIdx, Lane.Link.Predecessor);
					FString ToKey = FString::Printf(TEXT("%d_%d_%d"), Road.Id, FirstSecIdx, Lane.Id);

					const int32* FromIdx = RoadLaneToNodeId.Find(FromKey);
					const int32* ToIdx = RoadLaneToNodeId.Find(ToKey);

					if (FromIdx && ToIdx)
					{
						FLaneGraphEdge Edge;
						Edge.FromNodeId = *FromIdx;
						Edge.ToNodeId = *ToIdx;
						Edge.Type = ELaneGraphEdgeType::Predecessor;
						Edge.TransitionCost = 0.1;
						OutGraph.AddEdge(Edge);
					}
				}
			}
		}
	}
}

FLaneGraph ULaneGraphBuilder::BuildFromOpenDriveMap(const FOpenDriveMap& Map, float SampleInterval)
{
	FLaneGraph Graph;
	TMap<FString, int32> RoadLaneToNodeId;

	for (const FOpenDriveRoad& Road : Map.Roads)
	{
		BuildRoadLaneNodes(Road, Graph, SampleInterval, RoadLaneToNodeId);
	}

	for (const FOpenDriveJunction& Junction : Map.Junctions)
	{
		BuildJunctionConnections(Junction, Map, Graph, RoadLaneToNodeId);
	}

	BuildRoadLinks(Map, Graph, RoadLaneToNodeId);
	BuildTrafficLightControllers(Map, Graph);

	UE_LOG(LogTemp, Log, TEXT("[LaneGraph] Built graph: %d nodes, %d edges, %d traffic light controllers"),
		Graph.Nodes.Num(), Graph.Edges.Num(), Graph.TrafficLightControllers.Num());

	return Graph;
}

void ULaneGraphBuilder::TickTrafficLights(FLaneGraph& Graph, float DeltaSeconds)
{
	for (FTrafficLightController& Controller : Graph.TrafficLightControllers)
	{
		Controller.Tick(DeltaSeconds);
	}
}
