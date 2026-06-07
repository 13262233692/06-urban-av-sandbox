#pragma once

#include "CoreMinimal.h"
#include "LaneGraphTypes.generated.h"

USTRUCT(BlueprintType)
struct FLaneGraphNode
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere) int32 NodeId = -1;
	UPROPERTY(VisibleAnywhere) int32 RoadId = -1;
	UPROPERTY(VisibleAnywhere) int32 LaneSectionIndex = -1;
	UPROPERTY(VisibleAnywhere) int32 LaneId = 0;
	UPROPERTY(VisibleAnywhere) double SStart = 0.0;
	UPROPERTY(VisibleAnywhere) double SEnd = 0.0;
	UPROPERTY(VisibleAnywhere) double SpeedLimit = 0.0;
	UPROPERTY(VisibleAnywhere) FString SpeedUnit;
	UPROPERTY(VisibleAnywhere) bool bIsJunction = false;
	UPROPERTY(VisibleAnywhere) int32 JunctionId = -1;
	UPROPERTY(VisibleAnywhere) TArray<FVector> CenterlinePoints;
	UPROPERTY(VisibleAnywhere) TArray<FVector> LeftBoundary;
	UPROPERTY(VisibleAnywhere) TArray<FVector> RightBoundary;
	UPROPERTY(VisibleAnywhere) double Width = 0.0;
	UPROPERTY(VisibleAnywhere) double Length = 0.0;
	UPROPERTY(VisibleAnywhere) int32 TrafficLightSignalId = -1;
	UPROPERTY(VisibleAnywhere) FString TrafficLightState;
	UPROPERTY(VisibleAnywhere) FString TrafficLightPhaseGroup;
};

UENUM(BlueprintType)
enum class ELaneGraphEdgeType : uint8
{
	Successor,
	Predecessor,
	JunctionEntry,
	JunctionExit,
	LateralLeft,
	LateralRight
};

USTRUCT(BlueprintType)
struct FLaneGraphEdge
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere) int32 FromNodeId = -1;
	UPROPERTY(VisibleAnywhere) int32 ToNodeId = -1;
	UPROPERTY(VisibleAnywhere) ELaneGraphEdgeType Type = ELaneGraphEdgeType::Successor;
	UPROPERTY(VisibleAnywhere) double TransitionCost = 1.0;
	UPROPERTY(VisibleAnywhere) int32 JunctionConnectionId = -1;
};

USTRUCT(BlueprintType)
struct FTrafficLightPhase
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere) FString PhaseName;
	UPROPERTY(VisibleAnywhere) double Duration = 0.0;
	UPROPERTY(VisibleAnywhere) TMap<int32, FString> SignalStates;
};

USTRUCT(BlueprintType)
struct FTrafficLightController
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere) int32 ControllerId = -1;
	UPROPERTY(VisibleAnywhere) FString Name;
	UPROPERTY(VisibleAnywhere) TArray<FTrafficLightPhase> Phases;
	UPROPERTY(VisibleAnywhere) int32 CurrentPhaseIndex = 0;
	UPROPERTY(VisibleAnywhere) double CurrentPhaseElapsed = 0.0;
	UPROPERTY(VisibleAnywhere) TArray<int32> ControlledSignalIds;

	void Tick(double DeltaSeconds)
	{
		if (Phases.Num() == 0) return;
		CurrentPhaseElapsed += DeltaSeconds;
		while (CurrentPhaseElapsed >= Phases[CurrentPhaseIndex].Duration)
		{
			CurrentPhaseElapsed -= Phases[CurrentPhaseIndex].Duration;
			CurrentPhaseIndex = (CurrentPhaseIndex + 1) % Phases.Num();
		}
	}

	FString GetSignalState(int32 SignalId) const
	{
		if (Phases.Num() == 0) return TEXT("off");
		const FTrafficLightPhase& Phase = Phases[CurrentPhaseIndex];
		const FString* State = Phase.SignalStates.Find(SignalId);
		return State ? *State : TEXT("off");
	}
};

USTRUCT(BlueprintType)
struct FLaneGraph
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere) TArray<FLaneGraphNode> Nodes;
	UPROPERTY(VisibleAnywhere) TArray<FLaneGraphEdge> Edges;
	UPROPERTY(VisibleAnywhere) TArray<FTrafficLightController> TrafficLightControllers;
	UPROPERTY(VisibleAnywhere) TMap<int32, int32> RoadIdToNodeIndices;
	UPROPERTY(VisibleAnywhere) TMap<int32, int32> JunctionIdToNodeIndices;

	int32 AddNode(const FLaneGraphNode& Node)
	{
		int32 Idx = Nodes.Num();
		Nodes.Add(Node);
		Nodes[Idx].NodeId = Idx;
		return Idx;
	}

	void AddEdge(const FLaneGraphEdge& Edge)
	{
		Edges.Add(Edge);
	}

	TArray<int32> GetSuccessorNodes(int32 NodeId) const
	{
		TArray<int32> Result;
		for (const FLaneGraphEdge& Edge : Edges)
		{
			if (Edge.FromNodeId == NodeId && Edge.Type == ELaneGraphEdgeType::Successor)
			{
				Result.Add(Edge.ToNodeId);
			}
		}
		return Result;
	}

	TArray<int32> GetPredecessorNodes(int32 NodeId) const
	{
		TArray<int32> Result;
		for (const FLaneGraphEdge& Edge : Edges)
		{
			if (Edge.ToNodeId == NodeId && Edge.Type == ELaneGraphEdgeType::Predecessor)
			{
				Result.Add(Edge.FromNodeId);
			}
		}
		return Result;
	}

	TArray<int32> GetLateralNeighbors(int32 NodeId, ELaneGraphEdgeType Direction) const
	{
		TArray<int32> Result;
		for (const FLaneGraphEdge& Edge : Edges)
		{
			if (Edge.FromNodeId == NodeId && Edge.Type == Direction)
			{
				Result.Add(Edge.ToNodeId);
			}
		}
		return Result;
	}

	TArray<int32> FindPath(int32 StartNodeId, int32 EndNodeId) const;
};
