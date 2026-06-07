#include "LaneGraph/LaneGraphTypes.h"
#include "Algo/RemoveIf.h"

struct FLaneGraphAStarNode
{
	int32 NodeId = -1;
	int32 ParentId = -1;
	double GCost = 0.0;
	double HCost = 0.0;
	double FCost() const { return GCost + HCost; }
};

TArray<int32> FLaneGraph::FindPath(int32 StartNodeId, int32 EndNodeId) const
{
	if (StartNodeId < 0 || StartNodeId >= Nodes.Num() ||
		EndNodeId < 0 || EndNodeId >= Nodes.Num())
	{
		return TArray<int32>();
	}

	if (StartNodeId == EndNodeId)
	{
		return { StartNodeId };
	}

	TMap<int32, FLaneGraphAStarNode> OpenSet;
	TMap<int32, FLaneGraphAStarNode> ClosedSet;
	TSet<int32> InOpen;

	TMap<int32, TArray<FLaneGraphEdge>> Adjacency;
	for (const FLaneGraphEdge& Edge : Edges)
	{
		if (!Adjacency.Contains(Edge.FromNodeId))
		{
			Adjacency.Add(Edge.FromNodeId, TArray<FLaneGraphEdge>());
		}
		Adjacency[Edge.FromNodeId].Add(Edge);
	}

	FVector EndPos = Nodes[EndNodeId].CenterlinePoints.Num() > 0
		? Nodes[EndNodeId].CenterlinePoints[0]
		: FVector::ZeroVector;

	FLaneGraphAStarNode StartNode;
	StartNode.NodeId = StartNodeId;
	StartNode.GCost = 0.0;
	if (Nodes[StartNodeId].CenterlinePoints.Num() > 0)
	{
		StartNode.HCost = FVector::Dist(Nodes[StartNodeId].CenterlinePoints[0], EndPos);
	}
	OpenSet.Add(StartNodeId, StartNode);
	InOpen.Add(StartNodeId);

	while (OpenSet.Num() > 0)
	{
		int32 CurrentId = -1;
		double BestF = MAX_dbl;
		for (const TPair<int32, FLaneGraphAStarNode>& Pair : OpenSet)
		{
			if (Pair.Value.FCost() < BestF)
			{
				BestF = Pair.Value.FCost();
				CurrentId = Pair.Key;
			}
		}

		if (CurrentId == EndNodeId)
		{
			TArray<int32> Path;
			int32 TraceId = CurrentId;
			while (TraceId != -1)
			{
				Path.Insert(TraceId, 0);
				FLaneGraphAStarNode* N = ClosedSet.Find(TraceId);
				if (!N) N = OpenSet.Find(TraceId);
				TraceId = N ? N->ParentId : -1;
			}
			return Path;
		}

		FLaneGraphAStarNode Current = OpenSet.FindAndRemoveChecked(CurrentId);
		InOpen.Remove(CurrentId);
		ClosedSet.Add(CurrentId, Current);

		const TArray<FLaneGraphEdge>* Neighbors = Adjacency.Find(CurrentId);
		if (!Neighbors) continue;

		for (const FLaneGraphEdge& Edge : *Neighbors)
		{
			if (ClosedSet.Contains(Edge.ToNodeId)) continue;

			double TentativeG = Current.GCost + Edge.TransitionCost;

			FLaneGraphAStarNode Neighbor;
			Neighbor.NodeId = Edge.ToNodeId;
			Neighbor.ParentId = CurrentId;
			Neighbor.GCost = TentativeG;

			if (Nodes[Edge.ToNodeId].CenterlinePoints.Num() > 0)
			{
				Neighbor.HCost = FVector::Dist(Nodes[Edge.ToNodeId].CenterlinePoints[0], EndPos);
			}

			FLaneGraphAStarNode* Existing = OpenSet.Find(Edge.ToNodeId);
			if (Existing)
			{
				if (TentativeG < Existing->GCost)
				{
					Existing->GCost = TentativeG;
					Existing->ParentId = CurrentId;
				}
			}
			else
			{
				OpenSet.Add(Edge.ToNodeId, Neighbor);
				InOpen.Add(Edge.ToNodeId);
			}
		}
	}

	return TArray<int32>();
}
