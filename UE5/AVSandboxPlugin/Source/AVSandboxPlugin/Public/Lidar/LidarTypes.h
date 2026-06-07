#pragma once

#include "CoreMinimal.h"
#include "LidarTypes.generated.h"

UENUM(BlueprintType)
enum class ELidarTracingMode : uint8
{
	AsyncRaycast,
	HardwareRayTracing
};

USTRUCT(BlueprintType)
struct FLidarConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|Lidar")
	int32 ChannelCount = 64;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|Lidar")
	int32 PointsPerChannel = 1800;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|Lidar")
	float MinVerticalAngle = -25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|Lidar")
	float MaxVerticalAngle = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|Lidar")
	float HorizontalFOV = 360.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|Lidar")
	float RotationFrequency = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|Lidar")
	float MinRange = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|Lidar")
	float MaxRange = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|Lidar")
	float RangeResolution = 0.002f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|Lidar")
	float RangeNoiseStdDev = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|Lidar")
	float IntensityNoiseStdDev = 0.01f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|Lidar")
	ELidarTracingMode TracingMode = ELidarTracingMode::AsyncRaycast;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|Lidar")
	bool bShowDebugBeams = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|Lidar")
	float DebugBeamLifeTime = 0.03f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|Lidar")
	bool bIgnoreSelf = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|Lidar")
	FName RoofSocketName = FName(TEXT("LidarSocket"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|Lidar")
	FVector RelativeOffset = FVector(0.0f, 0.0f, 150.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|Lidar")
	int32 TCPPort = 9100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|Lidar")
	int32 MaxTCPClients = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|Lidar")
	int32 RaycastBatchSize = 128;

	int32 GetTotalPointCount() const
	{
		return ChannelCount * PointsPerChannel;
	}

	float GetVerticalAngleStep() const
	{
		if (ChannelCount <= 1) return 0.0f;
		return (MaxVerticalAngle - MinVerticalAngle) / static_cast<float>(ChannelCount - 1);
	}

	float GetHorizontalAngleStep() const
	{
		if (PointsPerChannel <= 1) return 0.0f;
		return HorizontalFOV / static_cast<float>(PointsPerChannel);
	}

	float GetScanPeriod() const
	{
		if (RotationFrequency <= 0.0f) return 1.0f;
		return 1.0f / RotationFrequency;
	}
};

USTRUCT(BlueprintType)
struct FLidarHitPoint
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere) FVector WorldPosition = FVector::ZeroVector;
	UPROPERTY(VisibleAnywhere) float Range = 0.0f;
	UPROPERTY(VisibleAnywhere) float Azimuth = 0.0f;
	UPROPERTY(VisibleAnywhere) float Elevation = 0.0f;
	UPROPERTY(VisibleAnywhere) float Intensity = 0.0f;
	UPROPERTY(VisibleAnywhere) int32 ChannelIndex = 0;
	UPROPERTY(VisibleAnywhere) int32 PointIndex = 0;
	UPROPERTY(VisibleAnywhere) bool bValid = false;
	UPROPERTY(VisibleAnywhere) FString HitMaterialName;
	UPROPERTY(VisibleAnywhere) uint8 ObjectClass = 0;
};

USTRUCT(BlueprintType)
struct FLidarScanFrame
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere) int32 FrameNumber = 0;
	UPROPERTY(VisibleAnywhere) double Timestamp = 0.0;
	UPROPERTY(VisibleAnywhere) TArray<FLidarHitPoint> Points;
	UPROPERTY(VisibleAnywhere) FTransform SensorTransform;
	UPROPERTY(VisibleAnywhere) int32 ValidPointCount = 0;
	UPROPERTY(VisibleAnywhere) float ScanDurationMs = 0.0f;

	int32 GetTotalPoints() const { return Points.Num(); }
};
