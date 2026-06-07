#pragma once

#include "CoreMinimal.h"
#include "Lidar/LidarTypes.h"
#include "PointCloud2Builder.generated.h"

USTRUCT(BlueprintType)
struct FPointCloud2Field
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere) FString Name;
	UPROPERTY(VisibleAnywhere) uint8 Offset = 0;
	UPROPERTY(VisibleAnywhere) uint8 DataType = 7;
	UPROPERTY(VisibleAnywhere) uint32 Count = 1;

	static const uint8 INT8 = 1;
	static const uint8 UINT8 = 2;
	static const uint8 INT16 = 3;
	static const uint8 UINT16 = 4;
	static const uint8 INT32 = 5;
	static const uint8 UINT32 = 6;
	static const uint8 FLOAT32 = 7;
	static const uint8 FLOAT64 = 8;
};

USTRUCT(BlueprintType)
struct FPointCloud2Header
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere) uint32 Seq = 0;
	UPROPERTY(VisibleAnywhere) int32 StampSec = 0;
	UPROPERTY(VisibleAnywhere) uint32 StampNanosec = 0;
	UPROPERTY(VisibleAnywhere) FString FrameId = TEXT("lidar");
};

UCLASS(BlueprintType)
class UPointCloud2Builder : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "AVSandbox|PointCloud2")
	static TArray<uint8> BuildPointCloud2Message(
		const FLidarScanFrame& Frame,
		const FString& FrameId = TEXT("lidar"));

	UFUNCTION(BlueprintCallable, Category = "AVSandbox|PointCloud2")
	static TArray<uint8> BuildPointCloud2MessageXYZI(
		const FLidarScanFrame& Frame,
		const FString& FrameId = TEXT("lidar"));

	UFUNCTION(BlueprintCallable, Category = "AVSandbox|PointCloud2")
	static TArray<uint8> BuildPointCloud2MessageXYZIR(
		const FLidarScanFrame& Frame,
		const FString& FrameId = TEXT("lidar"));

private:
	static void WriteUint8(TArray<uint8>& Buffer, uint8 Value);
	static void WriteUint16(TArray<uint8>& Buffer, uint16 Value);
	static void WriteUint32(TArray<uint8>& Buffer, uint32 Value);
	static void WriteFloat32(TArray<uint8>& Buffer, float Value);
	static void WriteString(TArray<uint8>& Buffer, const FString& Str);
	static void WriteHeader(TArray<uint8>& Buffer, const FPointCloud2Header& Header);
	static void WriteField(TArray<uint8>& Buffer, const FPointCloud2Field& Field);

	static constexpr int32 POINT_STEP_XYZI = 16;
	static constexpr int32 POINT_STEP_XYZIR = 20;
};
