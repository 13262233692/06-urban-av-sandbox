#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Lidar/LidarTypes.h"
#include "LidarSensorComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLidarScanComplete, const FLidarScanFrame&, Frame);

UCLASS(BlueprintType, ClassGroup = (AVSandbox), meta = (BlueprintSpawnableComponent))
class ULidarSensorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULidarSensorComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Reason EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "AVSandbox|Lidar")
	void Initialize(const FLidarConfig& InConfig);

	UFUNCTION(BlueprintCallable, Category = "AVSandbox|Lidar")
	void StartScanning();

	UFUNCTION(BlueprintCallable, Category = "AVSandbox|Lidar")
	void StopScanning();

	UFUNCTION(BlueprintCallable, Category = "AVSandbox|Lidar")
	FLidarScanFrame GetLatestFrame() const { return LatestFrame; }

	UFUNCTION(BlueprintCallable, Category = "AVSandbox|Lidar")
	bool IsScanning() const { return bIsScanning; }

	UFUNCTION(BlueprintCallable, Category = "AVSandbox|Lidar")
	FTransform GetSensorWorldTransform() const;

	UPROPERTY(BlueprintAssignable, Category = "AVSandbox|Lidar")
	FOnLidarScanComplete OnLidarScanComplete;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|Lidar")
	FLidarConfig Config;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AVSandbox|Lidar")
	int32 FramesCompleted = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AVSandbox|Lidar")
	float CurrentHorizontalAngle = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AVSandbox|Lidar")
	float LastScanDurationMs = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AVSandbox|Lidar")
	int32 LastFrameValidPoints = 0;

private:
	FLidarScanFrame LatestFrame;

	bool bIsScanning;
	float HorizontalAngleAccumulator;
	int32 FrameCounter;

	TArray<FVector> PrecomputedRayDirections;

	void PrecomputeRayDirections();

	void PerformAsyncRaycastScan(float DeltaTime);

	void ProcessRaycastBatch(
		const TArray<FVector>& Origins,
		const TArray<FVector>& Directions,
		TArray<FLidarHitPoint>& OutPoints,
		int32 ChannelStart,
		int32 ChannelEnd,
		float HorizontalStart,
		float HorizontalStep);

	FLidarHitPoint ComputeHitPoint(
		const FHitResult& Hit,
		int32 ChannelIndex,
		int32 PointIndex,
		float Azimuth,
		float Elevation,
		const FVector& Origin) const;

	float ComputeReflectivity(const FHitResult& Hit) const;

	void AddNoise(FLidarHitPoint& Point) const;

	void BuildPermutationTable();

	float GetVerticalAngle(int32 ChannelIndex) const;

	uint8 ClassifyHitObject(AActor* HitActor) const;

	static float FRandNormal(float StdDev);
};
