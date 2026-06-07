#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/SplineComponent.h"
#include "LaneGraph/LaneGraphTypes.h"
#include "SplineMapperComponent.generated.h"

UCLASS(BlueprintType, ClassGroup = (AVSandbox), meta = (BlueprintSpawnableComponent))
class USplineMapperComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USplineMapperComponent();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "AVSandbox|SplineMapper")
	bool BuildSplinesFromLaneGraph(const FLaneGraph& Graph);

	UFUNCTION(BlueprintCallable, Category = "AVSandbox|SplineMapper")
	void ClearSplines();

	UFUNCTION(BlueprintCallable, Category = "AVSandbox|SplineMapper")
	TArray<USplineComponent*> GetLaneSplines() const { return LaneSplines; }

	UFUNCTION(BlueprintCallable, Category = "AVSandbox|SplineMapper")
	USplineComponent* GetSplineForLaneNode(int32 NodeId) const;

	UFUNCTION(BlueprintCallable, Category = "AVSandbox|SplineMapper")
	FTransform GetTransformAtDistanceAlongSpline(int32 NodeId, float Distance) const;

	UFUNCTION(BlueprintCallable, Category = "AVSandbox|SplineMapper")
	float GetTotalSplineLength(int32 NodeId) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|SplineMapper")
	float SplineWidthScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|SplineMapper")
	bool bDrawDebug = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|SplineMapper")
	FLinearColor DrivingLaneColor = FLinearColor(0.2f, 0.6f, 0.2f, 0.5f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|SplineMapper")
	FLinearColor JunctionLaneColor = FLinearColor(0.6f, 0.6f, 0.2f, 0.5f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|SplineMapper")
	float MeshWidth = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|SplineMapper")
	UStaticMesh* RoadMesh = nullptr;

protected:
	UPROPERTY()
	TArray<USplineComponent*> LaneSplines;

	UPROPERTY()
	TMap<int32, USplineComponent*> NodeIdToSpline;

private:
	void CreateSplineForNode(const FLaneGraphNode& Node, int32 NodeIdx);
};
