#pragma once

#include "CoreMinimal.h"
#include "OpenDriveTypes.generated.h"

USTRUCT(BlueprintType)
struct FOpenDriveGeometryLine
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere) double S = 0.0;
	UPROPERTY(VisibleAnywhere) double X = 0.0;
	UPROPERTY(VisibleAnywhere) double Y = 0.0;
	UPROPERTY(VisibleAnywhere) double Hdg = 0.0;
	UPROPERTY(VisibleAnywhere) double Length = 0.0;
};

USTRUCT(BlueprintType)
struct FOpenDriveGeometryArc
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere) double S = 0.0;
	UPROPERTY(VisibleAnywhere) double X = 0.0;
	UPROPERTY(VisibleAnywhere) double Y = 0.0;
	UPROPERTY(VisibleAnywhere) double Hdg = 0.0;
	UPROPERTY(VisibleAnywhere) double Length = 0.0;
	UPROPERTY(VisibleAnywhere) double Curvature = 0.0;
};

USTRUCT(BlueprintType)
struct FOpenDriveGeometrySpiral
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere) double S = 0.0;
	UPROPERTY(VisibleAnywhere) double X = 0.0;
	UPROPERTY(VisibleAnywhere) double Y = 0.0;
	UPROPERTY(VisibleAnywhere) double Hdg = 0.0;
	UPROPERTY(VisibleAnywhere) double Length = 0.0;
	UPROPERTY(VisibleAnywhere) double CurvStart = 0.0;
	UPROPERTY(VisibleAnywhere) double CurvEnd = 0.0;
};

USTRUCT(BlueprintType)
struct FOpenDriveGeometryPoly3
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere) double S = 0.0;
	UPROPERTY(VisibleAnywhere) double X = 0.0;
	UPROPERTY(VisibleAnywhere) double Y = 0.0;
	UPROPERTY(VisibleAnywhere) double Hdg = 0.0;
	UPROPERTY(VisibleAnywhere) double Length = 0.0;
	UPROPERTY(VisibleAnywhere) double A = 0.0;
	UPROPERTY(VisibleAnywhere) double B = 0.0;
	UPROPERTY(VisibleAnywhere) double C = 0.0;
	UPROPERTY(VisibleAnywhere) double D = 0.0;
};

UENUM(BlueprintType)
enum class EOpenDriveGeometryType : uint8
{
	Line,
	Arc,
	Spiral,
	Poly3
};

USTRUCT(BlueprintType)
struct FOpenDriveGeometry
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere) EOpenDriveGeometryType Type = EOpenDriveGeometryType::Line;
	UPROPERTY(VisibleAnywhere) FOpenDriveGeometryLine Line;
	UPROPERTY(VisibleAnywhere) FOpenDriveGeometryArc Arc;
	UPROPERTY(VisibleAnywhere) FOpenDriveGeometrySpiral Spiral;
	UPROPERTY(VisibleAnywhere) FOpenDriveGeometryPoly3 Poly3;
};

UENUM(BlueprintType)
enum class EOpenDriveLaneType : uint8
{
	None,
	Driving,
	Stop,
	Shoulder,
	Biking,
	Sidewalk,
	Border,
	Restricted,
	Parking,
	Median,
	Curb,
	Exit,
	Entry,
	Ramp,
	Passing,
	OffRamp,
	OnRamp
};

USTRUCT(BlueprintType)
struct FOpenDriveLaneLink
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere) int32 Predecessor = -1;
	UPROPERTY(VisibleAnywhere) int32 Successor = -1;
};

USTRUCT(BlueprintType)
struct FOpenDriveLaneWidth
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere) double SOffset = 0.0;
	UPROPERTY(VisibleAnywhere) double A = 0.0;
	UPROPERTY(VisibleAnywhere) double B = 0.0;
	UPROPERTY(VisibleAnywhere) double C = 0.0;
	UPROPERTY(VisibleAnywhere) double D = 0.0;
};

USTRUCT(BlueprintType)
struct FOpenDriveLaneRoadMark
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere) double SOffset = 0.0;
	UPROPERTY(VisibleAnywhere) FString Type;
	UPROPERTY(VisibleAnywhere) FString Weight;
	UPROPERTY(VisibleAnywhere) FString Color;
	UPROPERTY(VisibleAnywhere) double Width = 0.0;
	UPROPERTY(VisibleAnywhere) FString LaneChange;
};

USTRUCT(BlueprintType)
struct FOpenDriveLane
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere) int32 Id = 0;
	UPROPERTY(VisibleAnywhere) EOpenDriveLaneType Type = EOpenDriveLaneType::None;
	UPROPERTY(VisibleAnywhere) FString Level;
	UPROPERTY(VisibleAnywhere) FOpenDriveLaneLink Link;
	UPROPERTY(VisibleAnywhere) TArray<FOpenDriveLaneWidth> Widths;
	UPROPERTY(VisibleAnywhere) TArray<FOpenDriveLaneRoadMark> RoadMarks;
	UPROPERTY(VisibleAnywhere) double Speed = 0.0;
	UPROPERTY(VisibleAnywhere) FString SpeedUnit;
};

USTRUCT(BlueprintType)
struct FOpenDriveLaneSection
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere) double S = 0.0;
	UPROPERTY(VisibleAnywhere) FString SingleSide;
	UPROPERTY(VisibleAnywhere) TArray<FOpenDriveLane> Lanes;
};

USTRUCT(BlueprintType)
struct FOpenDriveElevationProfile
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere) double S = 0.0;
	UPROPERTY(VisibleAnywhere) double A = 0.0;
	UPROPERTY(VisibleAnywhere) double B = 0.0;
	UPROPERTY(VisibleAnywhere) double C = 0.0;
	UPROPERTY(VisibleAnywhere) double D = 0.0;
};

USTRUCT(BlueprintType)
struct FOpenDriveRoadTypeSpeed
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere) double S = 0.0;
	UPROPERTY(VisibleAnywhere) FString Type;
	UPROPERTY(VisibleAnywhere) FString Country;
	UPROPERTY(VisibleAnywhere) double Max = 0.0;
	UPROPERTY(VisibleAnywhere) FString Unit;
};

USTRUCT(BlueprintType)
struct FOpenDriveRoadType
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere) double S = 0.0;
	UPROPERTY(VisibleAnywhere) FString Type;
	UPROPERTY(VisibleAnywhere) TArray<FOpenDriveRoadTypeSpeed> Speed;
};

USTRUCT(BlueprintType)
struct FOpenDriveRoadLinkElement
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere) FString ElementType;
	UPROPERTY(VisibleAnywhere) int32 ElementId = -1;
	UPROPERTY(VisibleAnywhere) FString ContactPoint;
};

USTRUCT(BlueprintType)
struct FOpenDriveRoadLink
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere) bool bHasPredecessor = false;
	UPROPERTY(VisibleAnywhere) FOpenDriveRoadLinkElement Predecessor;
	UPROPERTY(VisibleAnywhere) bool bHasSuccessor = false;
	UPROPERTY(VisibleAnywhere) FOpenDriveRoadLinkElement Successor;
};

USTRUCT(BlueprintType)
struct FOpenDriveSignalReference
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere) int32 Id = -1;
	UPROPERTY(VisibleAnywhere) FString Type;
	UPROPERTY(VisibleAnywhere) double S = 0.0;
	UPROPERTY(VisibleAnywhere) FString Orientation;
};

USTRUCT(BlueprintType)
struct FOpenDriveSignalPhase
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere) FString SignalId;
	UPROPERTY(VisibleAnywhere) FString Type;
	UPROPERTY(VisibleAnywhere) double S = 0.0;
	UPROPERTY(VisibleAnywhere) FString Orientation;
	UPROPERTY(VisibleAnywhere) double Value = 0.0;
	UPROPERTY(VisibleAnywhere) FString Unit;
};

USTRUCT(BlueprintType)
struct FOpenDriveControllerPhaseState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere) FString SignalId;
	UPROPERTY(VisibleAnywhere) FString State;
	UPROPERTY(VisibleAnywhere) double Duration = 0.0;
};

USTRUCT(BlueprintType)
struct FOpenDriveControllerPhase
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere) FString Name;
	UPROPERTY(VisibleAnywhere) double Duration = 0.0;
	UPROPERTY(VisibleAnywhere) TArray<FOpenDriveControllerPhaseState> States;
};

USTRUCT(BlueprintType)
struct FOpenDriveController
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere) int32 Id = -1;
	UPROPERTY(VisibleAnywhere) FString Name;
	UPROPERTY(VisibleAnywhere) int32 Sequence = 0;
	UPROPERTY(VisibleAnywhere) TArray<FOpenDriveControllerPhase> Phases;
	UPROPERTY(VisibleAnywhere) TArray<int32> ControlledSignals;
};

USTRUCT(BlueprintType)
struct FOpenDriveSignal
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere) int32 Id = -1;
	UPROPERTY(VisibleAnywhere) FString Name;
	UPROPERTY(VisibleAnywhere) double S = 0.0;
	UPROPERTY(VisibleAnywhere) double T = 0.0;
	UPROPERTY(VisibleAnywhere) FString Dynamic;
	UPROPERTY(VisibleAnywhere) FString Orientation;
	UPROPERTY(VisibleAnywhere) double ZOffset = 0.0;
	UPROPERTY(VisibleAnywhere) FString Country;
	UPROPERTY(VisibleAnywhere) FString Type;
	UPROPERTY(VisibleAnywhere) FString Subtype;
	UPROPERTY(VisibleAnywhere) double Value = 0.0;
	UPROPERTY(VisibleAnywhere) FString Unit;
	UPROPERTY(VisibleAnywhere) double Height = 0.0;
	UPROPERTY(VisibleAnywhere) double Width = 0.0;
	UPROPERTY(VisibleAnywhere) FString Text;
	UPROPERTY(VisibleAnywhere) int32 HOffset = 0;
	UPROPERTY(VisibleAnywhere) double Pitch = 0.0;
	UPROPERTY(VisibleAnywhere) double Roll = 0.0;
};

USTRUCT(BlueprintType)
struct FOpenDriveJunctionConnectionLaneLink
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere) int32 From = 0;
	UPROPERTY(VisibleAnywhere) int32 To = 0;
};

USTRUCT(BlueprintType)
struct FOpenDriveJunctionConnection
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere) int32 Id = -1;
	UPROPERTY(VisibleAnywhere) int32 IncomingRoad = -1;
	UPROPERTY(VisibleAnywhere) int32 ConnectingRoad = -1;
	UPROPERTY(VisibleAnywhere) FString ContactPoint;
	UPROPERTY(VisibleAnywhere) TArray<FOpenDriveJunctionConnectionLaneLink> LaneLinks;
};

USTRUCT(BlueprintType)
struct FOpenDriveJunction
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere) int32 Id = -1;
	UPROPERTY(VisibleAnywhere) FString Name;
	UPROPERTY(VisibleAnywhere) FString Type;
	UPROPERTY(VisibleAnywhere) TArray<FOpenDriveJunctionConnection> Connections;
};

USTRUCT(BlueprintType)
struct FOpenDriveRoad
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere) int32 Id = -1;
	UPROPERTY(VisibleAnywhere) FString Name;
	UPROPERTY(VisibleAnywhere) double Length = 0.0;
	UPROPERTY(VisibleAnywhere) FString Junction;
	UPROPERTY(VisibleAnywhere) FOpenDriveRoadLink Link;
	UPROPERTY(VisibleAnywhere) TArray<FOpenDriveGeometry> Geometries;
	UPROPERTY(VisibleAnywhere) TArray<FOpenDriveElevationProfile> ElevationProfile;
	UPROPERTY(VisibleAnywhere) TArray<FOpenDriveRoadType> RoadTypes;
	UPROPERTY(VisibleAnywhere) TArray<FOpenDriveLaneSection> LaneSections;
	UPROPERTY(VisibleAnywhere) TArray<FOpenDriveSignal> Signals;
	UPROPERTY(VisibleAnywhere) TArray<FOpenDriveSignalReference> SignalReferences;
};

USTRUCT(BlueprintType)
struct FOpenDriveMap
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere) FString HeaderRevMajor;
	UPROPERTY(VisibleAnywhere) FString HeaderRevMinor;
	UPROPERTY(VisibleAnywhere) FString HeaderName;
	UPROPERTY(VisibleAnywhere) FString HeaderVersion;
	UPROPERTY(VisibleAnywhere) FString HeaderDate;
	UPROPERTY(VisibleAnywhere) double HeaderNorth = 0.0;
	UPROPERTY(VisibleAnywhere) double HeaderSouth = 0.0;
	UPROPERTY(VisibleAnywhere) double HeaderEast = 0.0;
	UPROPERTY(VisibleAnywhere) double HeaderWest = 0.0;
	UPROPERTY(VisibleAnywhere) FString HeaderVendor;

	UPROPERTY(VisibleAnywhere) TArray<FOpenDriveRoad> Roads;
	UPROPERTY(VisibleAnywhere) TArray<FOpenDriveJunction> Junctions;
	UPROPERTY(VisibleAnywhere) TArray<FOpenDriveController> Controllers;
};
