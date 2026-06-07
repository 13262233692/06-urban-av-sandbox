#include "MapMapping/NavMeshGenerator.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "Engine/World.h"
#include "NavigationSystem.h"
#include "NavMeshBoundsVolume.h"

void UNavMeshGenerator::ComputeBoundsFromLaneGraph(
	const FLaneGraph& Graph,
	FVector& OutMin,
	FVector& OutMax)
{
	FVector Min(MAX_dbl, MAX_dbl, MAX_dbl);
	FVector Max(-MAX_dbl, -MAX_dbl, -MAX_dbl);

	for (const FLaneGraphNode& Node : Graph.Nodes)
	{
		for (const FVector& Pt : Node.CenterlinePoints)
		{
			Min.X = FMath::Min(Min.X, Pt.X);
			Min.Y = FMath::Min(Min.Y, Pt.Y);
			Min.Z = FMath::Min(Min.Z, Pt.Z);
			Max.X = FMath::Max(Max.X, Pt.X);
			Max.Y = FMath::Max(Max.Y, Pt.Y);
			Max.Z = FMath::Max(Max.Z, Pt.Z);
		}
	}

	float Padding = 5000.0f;
	OutMin = Min - FVector(Padding, Padding, Padding);
	OutMax = Max + FVector(Padding, Padding, Padding);
}

ANavMeshBoundsVolume* UNavMeshGenerator::SpawnNavMeshBoundsVolume(
	UWorld* World,
	const FVector& Min,
	const FVector& Max)
{
	if (!World) return nullptr;

	FVector Center = (Min + Max) * 0.5f;
	FVector Extent = (Max - Min) * 0.5f;

	FTransform SpawnTransform(Center);

	ANavMeshBoundsVolume* BoundsVol = World->SpawnActor<ANavMeshBoundsVolume>(
		ANavMeshBoundsVolume::StaticClass(), SpawnTransform);

	if (BoundsVol)
	{
		BoundsVol->GetBrushComponent()->SetMobility(EComponentMobility::Movable);
		BoundsVol->SetActorLocation(Center);

		UBrushBuilder* BrushBuilder = BoundsVol->BrushBuilder;
		if (BrushBuilder)
		{
			BrushBuilder->Modify();
		}
	}

	return BoundsVol;
}

void UNavMeshGenerator::ConfigureRecastNavMesh(
	UWorld* World,
	float CellSize,
	float CellHeight,
	float AgentRadius,
	float AgentHeight)
{
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSys) return;

	NavSys->SetGeometryGatheringMode(GeoGatheringMode::AllStaticAndDynamic);

	TArray<AActor*> NavMeshActors;
	NavSys->GetNavMeshActors(NavMeshActors);

	for (AActor* Actor : NavMeshActors)
	{
		ARecastNavMesh* RecastNav = Cast<ARecastNavMesh>(Actor);
		if (RecastNav)
		{
			RecastNav->CellSize = CellSize;
			RecastNav->CellHeight = CellHeight;
			RecastNav->AgentRadius = AgentRadius;
			RecastNav->AgentHeight = AgentHeight;
			RecastNav->bDrawPolyEdges = false;
			RecastNav->bDrawTileBounds = false;
			RecastNav->TileSetUpdateInterval = 0.1f;
			break;
		}
	}
}

bool UNavMeshGenerator::GenerateNavMeshFromLaneGraph(
	UWorld* World,
	const FLaneGraph& Graph,
	float CellSize,
	float CellHeight,
	float AgentRadius,
	float AgentHeight)
{
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[NavMeshGenerator] World is null"));
		return false;
	}

	if (Graph.Nodes.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[NavMeshGenerator] LaneGraph has no nodes"));
		return false;
	}

	FVector Min, Max;
	ComputeBoundsFromLaneGraph(Graph, Min, Max);

	ANavMeshBoundsVolume* BoundsVol = SpawnNavMeshBoundsVolume(World, Min, Max);
	if (!BoundsVol)
	{
		UE_LOG(LogTemp, Error, TEXT("[NavMeshGenerator] Failed to spawn NavMeshBoundsVolume"));
		return false;
	}

	ConfigureRecastNavMesh(World, CellSize, CellHeight, AgentRadius, AgentHeight);

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (NavSys)
	{
		NavSys->Build();
	}

	UE_LOG(LogTemp, Log, TEXT("[NavMeshGenerator] NavMesh generation triggered for %d lane nodes"), Graph.Nodes.Num());
	return true;
}

bool UNavMeshGenerator::GenerateNavMeshFromLaneGraphAsync(
	UWorld* World,
	const FLaneGraph& Graph,
	float CellSize,
	float CellHeight,
	float AgentRadius,
	float AgentHeight)
{
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[NavMeshGenerator] World is null"));
		return false;
	}

	if (Graph.Nodes.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[NavMeshGenerator] LaneGraph has no nodes"));
		return false;
	}

	FVector Min, Max;
	ComputeBoundsFromLaneGraph(Graph, Min, Max);

	ANavMeshBoundsVolume* BoundsVol = SpawnNavMeshBoundsVolume(World, Min, Max);
	if (!BoundsVol)
	{
		UE_LOG(LogTemp, Error, TEXT("[NavMeshGenerator] Failed to spawn NavMeshBoundsVolume"));
		return false;
	}

	ConfigureRecastNavMesh(World, CellSize, CellHeight, AgentRadius, AgentHeight);

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (NavSys)
	{
		NavSys->BuildInBackground();
	}

	UE_LOG(LogTemp, Log, TEXT("[NavMeshGenerator] Async NavMesh generation triggered for %d lane nodes"), Graph.Nodes.Num());
	return true;
}
