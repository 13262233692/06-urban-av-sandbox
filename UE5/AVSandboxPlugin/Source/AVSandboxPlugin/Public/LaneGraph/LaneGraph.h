#pragma once

#include "CoreMinimal.h"
#include "LaneGraphTypes.h"
#include "OpenDrive/OpenDriveTypes.h"
#include "LaneGraph.generated.h"

UCLASS(BlueprintType)
class ULaneGraphBuilder : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "AVSandbox|LaneGraph")
	static FLaneGraph BuildFromOpenDriveMap(const FOpenDriveMap& Map, float SampleInterval = 5.0f);

	UFUNCTION(BlueprintCallable, Category = "AVSandbox|LaneGraph")
	static void TickTrafficLights(FLaneGraph& Graph, float DeltaSeconds);

private:
	static void BuildRoadLaneNodes(
		const FOpenDriveRoad& Road,
		FLaneGraph& OutGraph,
		float SampleInterval,
		TMap<FString, int32>& RoadLaneToNodeId);

	static void BuildJunctionConnections(
		const FOpenDriveJunction& Junction,
		const FOpenDriveMap& Map,
		FLaneGraph& OutGraph,
		const TMap<FString, int32>& RoadLaneToNodeId);

	static void BuildTrafficLightControllers(
		const FOpenDriveMap& Map,
		FLaneGraph& OutGraph);

	static void BuildRoadLinks(
		const FOpenDriveMap& Map,
		FLaneGraph& OutGraph,
		const TMap<FString, int32>& RoadLaneToNodeId);

	static TArray<FVector> SampleCenterlinePoints(
		const FOpenDriveRoad& Road,
		const FOpenDriveLaneSection& Section,
		const FOpenDriveLane& Lane,
		float SampleInterval);

	static double ComputeLaneWidth(
		const FOpenDriveLane& Lane,
		double S);

	static FVector SampleGeometryPoint(
		const FOpenDriveGeometry& Geo,
		double LocalS);

	static double GetSpeedLimitForRoad(const FOpenDriveRoad& Road);
};
