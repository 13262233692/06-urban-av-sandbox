#pragma once

#include "CoreMinimal.h"
#include "LaneGraph/LaneGraphTypes.h"
#include "NavMeshGenerator.generated.h"

class UNavigationSystemV1;
class ANavMeshBoundsVolume;
class URecastNavMesh;

UCLASS(BlueprintType)
class UNavMeshGenerator : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "AVSandbox|NavMesh")
	static bool GenerateNavMeshFromLaneGraph(
		UWorld* World,
		const FLaneGraph& Graph,
		float CellSize = 50.0f,
		float CellHeight = 10.0f,
		float AgentRadius = 150.0f,
		float AgentHeight = 200.0f);

	UFUNCTION(BlueprintCallable, Category = "AVSandbox|NavMesh")
	static bool GenerateNavMeshFromLaneGraphAsync(
		UWorld* World,
		const FLaneGraph& Graph,
		float CellSize = 50.0f,
		float CellHeight = 10.0f,
		float AgentRadius = 150.0f,
		float AgentHeight = 200.0f);

private:
	static void ComputeBoundsFromLaneGraph(
		const FLaneGraph& Graph,
		FVector& OutMin,
		FVector& OutMax);

	static ANavMeshBoundsVolume* SpawnNavMeshBoundsVolume(
		UWorld* World,
		const FVector& Min,
		const FVector& Max);

	static void ConfigureRecastNavMesh(
		UWorld* World,
		float CellSize,
		float CellHeight,
		float AgentRadius,
		float AgentHeight);
};
