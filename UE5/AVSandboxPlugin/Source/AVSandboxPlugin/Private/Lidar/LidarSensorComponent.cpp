#include "Lidar/LidarSensorComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Materials/MaterialInterface.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Kismet/KismetMathLibrary.h"
#include "CollisionQueryParams.h"
#include "Async/Async.h"

ULidarSensorComponent::ULidarSensorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = ETickingGroup::TG_PostPhysics;

	bIsScanning = false;
	HorizontalAngleAccumulator = 0.0f;
	FrameCounter = 0;
}

void ULidarSensorComponent::BeginPlay()
{
	Super::BeginPlay();
	PrecomputeRayDirections();
}

void ULidarSensorComponent::EndPlay(const EEndPlayReason::Reason EndPlayReason)
{
	StopScanning();
	Super::EndPlay(EndPlayReason);
}

void ULidarSensorComponent::Initialize(const FLidarConfig& InConfig)
{
	Config = InConfig;
	PrecomputeRayDirections();
}

void ULidarSensorComponent::PrecomputeRayDirections()
{
	PrecomputedRayDirections.Empty(Config.GetTotalPointCount());

	float VertStep = Config.GetVerticalAngleStep();
	float HorizStep = Config.GetHorizontalAngleStep();

	for (int32 Ch = 0; Ch < Config.ChannelCount; ++Ch)
	{
		float Elevation = Config.MinVerticalAngle + Ch * VertStep;
		float ElevRad = FMath::DegreesToRadians(Elevation);

		for (int32 Pt = 0; Pt < Config.PointsPerChannel; ++Pt)
		{
			float Azimuth = Pt * HorizStep;
			float AzimRad = FMath::DegreesToRadians(Azimuth);

			float CosElev = FMath::Cos(ElevRad);
			float SinElev = FMath::Sin(ElevRad);
			float CosAzim = FMath::Cos(AzimRad);
			float SinAzim = FMath::Sin(AzimRad);

			FVector Dir(CosElev * CosAzim, CosElev * SinAzim, SinElev);
			PrecomputedRayDirections.Add(Dir);
		}
	}
}

void ULidarSensorComponent::StartScanning()
{
	if (bIsScanning) return;
	bIsScanning = true;
	HorizontalAngleAccumulator = 0.0f;
	FrameCounter = 0;
	UE_LOG(LogTemp, Log, TEXT("[Lidar] Scanning started: %d channels, %d pts/ch, %.1f Hz"),
		Config.ChannelCount, Config.PointsPerChannel, Config.RotationFrequency);
}

void ULidarSensorComponent::StopScanning()
{
	bIsScanning = false;
}

FTransform ULidarSensorComponent::GetSensorWorldTransform() const
{
	AActor* Owner = GetOwner();
	if (!Owner) return FTransform::Identity;

	FVector Offset = Config.RelativeOffset;
	if (Owner->FindSocketByName(Config.RoofSocketName))
	{
		FTransform SocketTransform = Owner->GetSocketTransform(Config.RoofSocketName);
		return SocketTransform;
	}

	FVector WorldLoc = Owner->GetActorLocation() + Owner->GetActorRotation().RotateVector(Offset);
	return FTransform(Owner->GetActorRotation(), WorldLoc);
}

float ULidarSensorComponent::GetVerticalAngle(int32 ChannelIndex) const
{
	return Config.MinVerticalAngle + ChannelIndex * Config.GetVerticalAngleStep();
}

float ULidarSensorComponent::FRandNormal(float StdDev)
{
	if (StdDev <= 0.0f) return 0.0f;

	float U1 = FMath::FRand();
	float U2 = FMath::FRand();
	if (U1 < KINDA_SMALL_NUMBER) U1 = KINDA_SMALL_NUMBER;

	float Z0 = FMath::Sqrt(-2.0f * FMath::Loge(U1)) * FMath::Cos(2.0f * PI * U2);
	return Z0 * StdDev;
}

uint8 ULidarSensorComponent::ClassifyHitObject(AActor* HitActor) const
{
	if (!HitActor) return 0;

	FString Name = HitActor->GetName();
	FString Class = HitActor->GetClass()->GetName();

	if (Class.Contains(TEXT("Vehicle")) || Class.Contains(TEXT("Car")) || Class.Contains(TEXT("Truck")))
		return 1;
	if (Class.Contains(TEXT("Walker")) || Class.Contains(TEXT("Character")) || Class.Contains(TEXT("Pawn")))
		return 2;
	if (Class.Contains(TEXT("TrafficLight")) || Class.Contains(TEXT("Signal")))
		return 3;
	if (Class.Contains(TEXT("Sign")))
		return 4;
	if (Class.Contains(TEXT("Building")) || Class.Contains(TEXT("Wall")))
		return 5;
	if (Class.Contains(TEXT("Road")) || Class.Contains(TEXT("Ground")))
		return 6;

	return 7;
}

float ULidarSensorComponent::ComputeReflectivity(const FHitResult& Hit) const
{
	float Reflectivity = 0.5f;

	if (Hit.Component.IsValid())
	{
		UMaterialInterface* Mat = Hit.Component->GetMaterial(0);
		if (Mat)
		{
			FLinearColor BaseColor;
			if (Mat->GetVectorParameterValue(FName(TEXT("BaseColor")), BaseColor))
			{
				Reflectivity = (BaseColor.R + BaseColor.G + BaseColor.B) / 3.0f;
			}
			else
			{
				Reflectivity = 0.3f;
			}

			float Roughness = 0.5f;
			Mat->GetScalarParameterValue(FName(TEXT("Roughness")), Roughness);
			Reflectivity *= FMath::Lerp(1.0f, 0.3f, Roughness);
		}
	}

	if (Hit.PhysMaterial.IsValid())
	{
		EPhysicalSurface SurfaceType = Hit.PhysMaterial->SurfaceType;
		switch (SurfaceType)
		{
		case EPhysicalSurface::SurfaceType_Default: Reflectivity *= 0.5f; break;
		case EPhysicalSurface::SurfaceType1: Reflectivity *= 0.9f; break;
		case EPhysicalSurface::SurfaceType2: Reflectivity *= 0.2f; break;
		case EPhysicalSurface::SurfaceType3: Reflectivity *= 0.7f; break;
		default: break;
		}
	}

	return FMath::Clamp(Reflectivity, 0.0f, 1.0f);
}

void ULidarSensorComponent::AddNoise(FLidarHitPoint& Point) const
{
	if (Config.RangeNoiseStdDev > 0.0f)
	{
		Point.Range += FRandNormal(Config.RangeNoiseStdDev);
		Point.Range = FMath::Clamp(Point.Range, Config.MinRange, Config.MaxRange);

		float RadAzim = FMath::DegreesToRadians(Point.Azimuth);
		float RadElev = FMath::DegreesToRadians(Point.Elevation);
		float Range = Point.Range;

		Point.WorldPosition.X = Range * FMath::Cos(RadElev) * FMath::Cos(RadAzim);
		Point.WorldPosition.Y = Range * FMath::Cos(RadElev) * FMath::Sin(RadAzim);
		Point.WorldPosition.Z = Range * FMath::Sin(RadElev);
	}

	if (Config.IntensityNoiseStdDev > 0.0f)
	{
		Point.Intensity += FRandNormal(Config.IntensityNoiseStdDev);
		Point.Intensity = FMath::Clamp(Point.Intensity, 0.0f, 1.0f);
	}
}

FLidarHitPoint ULidarSensorComponent::ComputeHitPoint(
	const FHitResult& Hit,
	int32 ChannelIndex,
	int32 PointIndex,
	float Azimuth,
	float Elevation,
	const FVector& Origin) const
{
	FLidarHitPoint Point;
	Point.bValid = true;
	Point.ChannelIndex = ChannelIndex;
	Point.PointIndex = PointIndex;
	Point.Azimuth = Azimuth;
	Point.Elevation = Elevation;
	Point.WorldPosition = Hit.ImpactPoint - Origin;

	float Dist = Hit.Distance;
	Point.Range = (Dist >= Config.MinRange && Dist <= Config.MaxRange) ? Dist : 0.0f;
	Point.Intensity = ComputeReflectivity(Hit);
	Point.ObjectClass = ClassifyHitObject(Hit.GetActor());

	if (Hit.Component.IsValid())
	{
		Point.HitMaterialName = Hit.Component->GetFullName();
	}

	return Point;
}

void ULidarSensorComponent::ProcessRaycastBatch(
	const TArray<FVector>& Origins,
	const TArray<FVector>& Directions,
	TArray<FLidarHitPoint>& OutPoints,
	int32 ChannelStart,
	int32 ChannelEnd,
	float HorizontalStart,
	float HorizontalStep)
{
	UWorld* World = GetWorld();
	if (!World) return;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(LidarScan), true);
	if (Config.bIgnoreSelf && GetOwner())
	{
		Params.AddIgnoredActor(GetOwner());
	}

	float VertStep = Config.GetVerticalAngleStep();

	for (int32 Ch = ChannelStart; Ch < ChannelEnd && Ch < Config.ChannelCount; ++Ch)
	{
		float Elevation = Config.MinVerticalAngle + Ch * VertStep;

		for (int32 Pt = 0; Pt < Config.PointsPerChannel; ++Pt)
		{
			float Azimuth = HorizontalStart + Pt * HorizontalStep;
			if (Azimuth >= 360.0f) Azimuth -= 360.0f;

			int32 RayIdx = Ch * Config.PointsPerChannel + Pt;
			if (RayIdx >= PrecomputedRayDirections.Num()) continue;

			FVector RayDir = PrecomputedRayDirections[RayIdx];

			FQuat AzimRotation(FVector::UpVector, FMath::DegreesToRadians(HorizontalAngleAccumulator));
			FVector RotatedDir = AzimRotation.RotateVector(RayDir);

			FVector Origin = Origins.IsValidIndex(0) ? Origins[0] : GetSensorWorldTransform().GetTranslation();
			FVector End = Origin + RotatedDir * Config.MaxRange;

			FHitResult HitResult;
			bool bHit = World->LineTraceSingleByChannel(
				HitResult,
				Origin,
				End,
				ECC_Visibility,
				Params);

			FLidarHitPoint Point;
			if (bHit && HitResult.Distance >= Config.MinRange)
			{
				Point = ComputeHitPoint(HitResult, Ch, Pt, Azimuth, Elevation, Origin);
			}
			else
			{
				Point.bValid = false;
				Point.ChannelIndex = Ch;
				Point.PointIndex = Pt;
				Point.Azimuth = Azimuth;
				Point.Elevation = Elevation;
				Point.Range = 0.0f;
				Point.Intensity = 0.0f;
			}

			if (Point.bValid)
			{
				AddNoise(Point);
			}

			OutPoints.Add(Point);

			if (Config.bShowDebugBeams && bHit)
			{
				DrawDebugLine(
					World,
					Origin,
					HitResult.ImpactPoint,
					FColor::Green,
					false,
					Config.DebugBeamLifeTime,
					0,
					1.0f);
			}
		}
	}
}

void ULidarSensorComponent::PerformAsyncRaycastScan(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World) return;

	float ScanPeriod = Config.GetScanPeriod();
	float AnglePerTick = (DeltaTime / ScanPeriod) * 360.0f;

	FTransform SensorTransform = GetSensorWorldTransform();
	FVector Origin = SensorTransform.GetTranslation();
	FQuat SensorRotation = SensorTransform.GetRotation();

	FLidarScanFrame Frame;
	Frame.FrameNumber = FrameCounter;
	Frame.Timestamp = World->GetTimeSeconds();
	Frame.SensorTransform = SensorTransform;

	double ScanStart = FPlatformTime::Seconds();

	int32 TotalPoints = Config.GetTotalPointCount();
	Frame.Points.Reserve(TotalPoints);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(LidarScan), true);
	if (Config.bIgnoreSelf && GetOwner())
	{
		Params.AddIgnoredActor(GetOwner());
	}

	float VertStep = Config.GetVerticalAngleStep();
	float HorizStep = Config.GetHorizontalAngleStep();

	for (int32 Ch = 0; Ch < Config.ChannelCount; ++Ch)
	{
		float Elevation = Config.MinVerticalAngle + Ch * VertStep;
		float ElevRad = FMath::DegreesToRadians(Elevation);

		for (int32 Pt = 0; Pt < Config.PointsPerChannel; ++Pt)
		{
			float Azimuth = HorizontalAngleAccumulator + Pt * HorizStep;
			if (Azimuth >= 360.0f) Azimuth -= 360.0f;
			float AzimRad = FMath::DegreesToRadians(Azimuth);

			int32 RayIdx = Ch * Config.PointsPerChannel + Pt;
			FVector LocalDir = (RayIdx < PrecomputedRayDirections.Num())
				? PrecomputedRayDirections[RayIdx]
				: FVector(FMath::Cos(ElevRad) * FMath::Cos(AzimRad),
					FMath::Cos(ElevRad) * FMath::Sin(AzimRad),
					FMath::Sin(ElevRad));

			FVector WorldDir = SensorRotation.RotateVector(LocalDir);
			FVector End = Origin + WorldDir * Config.MaxRange;

			FHitResult HitResult;
			bool bHit = World->LineTraceSingleByChannel(
				HitResult,
				Origin,
				End,
				ECC_Visibility,
				Params);

			FLidarHitPoint Point;
			if (bHit && HitResult.Distance >= Config.MinRange)
			{
				Point = ComputeHitPoint(HitResult, Ch, Pt, Azimuth, Elevation, Origin);
				Frame.ValidPointCount++;
			}
			else
			{
				Point.bValid = false;
				Point.ChannelIndex = Ch;
				Point.PointIndex = Pt;
				Point.Azimuth = Azimuth;
				Point.Elevation = Elevation;
				Point.Range = 0.0f;
				Point.Intensity = 0.0f;
			}

			if (Point.bValid)
			{
				AddNoise(Point);
			}

			Frame.Points.Add(Point);
		}
	}

	double ScanEnd = FPlatformTime::Seconds();
	Frame.ScanDurationMs = static_cast<float>((ScanEnd - ScanStart) * 1000.0);

	HorizontalAngleAccumulator += AnglePerTick;
	if (HorizontalAngleAccumulator >= 360.0f)
	{
		HorizontalAngleAccumulator -= 360.0f;
	}

	CurrentHorizontalAngle = HorizontalAngleAccumulator;
	LastScanDurationMs = Frame.ScanDurationMs;
	LastFrameValidPoints = Frame.ValidPointCount;

	LatestFrame = Frame;
	FrameCounter++;
	FramesCompleted++;

	OnLidarScanComplete.Broadcast(Frame);
}

void ULidarSensorComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsScanning) return;

	PerformAsyncRaycastScan(DeltaTime);
}
