#pragma once

#include "CoreMinimal.h"
#include "OpenDriveTypes.generated.h"
#include "OpenDriveParser.generated.h"

class UOpenDriveParser : public UObject
{
	GENERATED_BODY()

public:
	static FOpenDriveMap ParseFromString(const FString& XmlContent);
	static FOpenDriveMap ParseFromFile(const FString& FilePath);

	static bool WriteDebugSummary(const FOpenDriveMap& Map, const FString& OutputPath);

private:
	static void ParseHeader(const FXmlNode* HeaderNode, FOpenDriveMap& OutMap);
	static void ParseRoad(const FXmlNode* RoadNode, FOpenDriveRoad& OutRoad);
	static void ParseRoadLink(const FXmlNode* LinkNode, FOpenDriveRoadLink& OutLink);
	static void ParseGeometry(const FXmlNode* GeoNode, FOpenDriveGeometry& OutGeo);
	static void ParseElevationProfile(const FXmlNode* ElevNode, FOpenDriveRoad& OutRoad);
	static void ParseRoadTypes(const FXmlNode* TypeNode, FOpenDriveRoad& OutRoad);
	static void ParseLanes(const FXmlNode* LanesNode, FOpenDriveRoad& OutRoad);
	static void ParseLaneSection(const FXmlNode* SectionNode, FOpenDriveLaneSection& OutSection);
	static void ParseLane(const FXmlNode* LaneNode, FOpenDriveLane& OutLane);
	static void ParseSignals(const FXmlNode* SignalsNode, FOpenDriveRoad& OutRoad);
	static void ParseJunction(const FXmlNode* JunctionNode, FOpenDriveJunction& OutJunction);
	static void ParseController(const FXmlNode* ControllerNode, FOpenDriveController& OutController);

	static EOpenDriveLaneType StringToLaneType(const FString& Str);
	static EOpenDriveGeometryType StringToGeometryType(const FString& Str);
	static double GetDoubleAttr(const FXmlNode* Node, const FString& AttrName, double Default = 0.0);
	static int32 GetIntAttr(const FXmlNode* Node, const FString& AttrName, int32 Default = 0);
	static FString GetStringAttr(const FXmlNode* Node, const FString& AttrName, const FString& Default = TEXT(""));
};
