#pragma once

#include "CoreMinimal.h"
#include "VehicleStateProtocol.generated.h"

#pragma pack(push, 1)

USTRUCT(BlueprintType)
struct FVehicleStateMessage
{
	GENERATED_BODY()

	static constexpr uint32 MAGIC = 0x41565354;
	static constexpr uint16 VERSION = 1;

	UPROPERTY(VisibleAnywhere) uint32 Magic = MAGIC;
	UPROPERTY(VisibleAnywhere) uint16 Version = VERSION;
	UPROPERTY(VisibleAnywhere) uint16 SequenceNumber = 0;

	UPROPERTY(VisibleAnywhere) float PositionX = 0.0f;
	UPROPERTY(VisibleAnywhere) float PositionY = 0.0f;
	UPROPERTY(VisibleAnywhere) float PositionZ = 0.0f;

	UPROPERTY(VisibleAnywhere) float RotationPitch = 0.0f;
	UPROPERTY(VisibleAnywhere) float RotationYaw = 0.0f;
	UPROPERTY(VisibleAnywhere) float RotationRoll = 0.0f;

	UPROPERTY(VisibleAnywhere) float VelocityX = 0.0f;
	UPROPERTY(VisibleAnywhere) float VelocityY = 0.0f;
	UPROPERTY(VisibleAnywhere) float VelocityZ = 0.0f;

	UPROPERTY(VisibleAnywhere) float AngularVelocityX = 0.0f;
	UPROPERTY(VisibleAnywhere) float AngularVelocityY = 0.0f;
	UPROPERTY(VisibleAnywhere) float AngularVelocityZ = 0.0f;

	UPROPERTY(VisibleAnywhere) float ForwardSpeed = 0.0f;
	UPROPERTY(VisibleAnywhere) float LateralSpeed = 0.0f;
	UPROPERTY(VisibleAnywhere) float UpSpeed = 0.0f;

	UPROPERTY(VisibleAnywhere) float AccelerationX = 0.0f;
	UPROPERTY(VisibleAnywhere) float AccelerationY = 0.0f;
	UPROPERTY(VisibleAnywhere) float AccelerationZ = 0.0f;

	UPROPERTY(VisibleAnywhere) float TireSlipFL = 0.0f;
	UPROPERTY(VisibleAnywhere) float TireSlipFR = 0.0f;
	UPROPERTY(VisibleAnywhere) float TireSlipRL = 0.0f;
	UPROPERTY(VisibleAnywhere) float TireSlipRR = 0.0f;

	UPROPERTY(VisibleAnywhere) float TireLoadFL = 0.0f;
	UPROPERTY(VisibleAnywhere) float TireLoadFR = 0.0f;
	UPROPERTY(VisibleAnywhere) float TireLoadRL = 0.0f;
	UPROPERTY(VisibleAnywhere) float TireLoadRR = 0.0f;

	UPROPERTY(VisibleAnywhere) float EngineRPM = 0.0f;
	UPROPERTY(VisibleAnywhere) int8 CurrentGear = 0;
	UPROPERTY(VisibleAnywhere) uint8 CollisionState = 0;
	UPROPERTY(VisibleAnywhere) uint8 OffRoadState = 0;
	UPROPERTY(VisibleAnywhere) uint8 Reserved1 = 0;

	UPROPERTY(VisibleAnywhere) float SteeringAngle = 0.0f;
	UPROPERTY(VisibleAnywhere) float ThrottleInput = 0.0f;
	UPROPERTY(VisibleAnywhere) float BrakeInput = 0.0f;

	UPROPERTY(VisibleAnywhere) int32 CurrentLaneNodeId = -1;
	UPROPERTY(VisibleAnywhere) float LaneOffset = 0.0f;
	UPROPERTY(VisibleAnywhere) float DistanceAlongLane = 0.0f;

	UPROPERTY(VisibleAnywhere) uint32 Timestamp = 0;
	UPROPERTY(VisibleAnywhere) uint32 Checksum = 0;

	static constexpr int32 SERIALIZED_SIZE = sizeof(FVehicleStateMessage);

	bool IsValid() const
	{
		return Magic == MAGIC;
	}

	void ComputeChecksum()
	{
		Checksum = 0;
		const uint8* Data = reinterpret_cast<const uint8*>(this);
		uint32 Sum = 0;
		for (int32 i = 0; i < SERIALIZED_SIZE - 4; ++i)
		{
			Sum += Data[i];
		}
		Checksum = Sum;
	}

	bool VerifyChecksum() const
	{
		uint32 StoredChecksum = Checksum;
		FVehicleStateMessage Copy = *this;
		Copy.Checksum = 0;
		Copy.ComputeChecksum();
		return Copy.Checksum == StoredChecksum;
	}

	TArray<uint8> Serialize() const
	{
		TArray<uint8> Bytes;
		Bytes.SetNumUninitialized(SERIALIZED_SIZE);
		FMemory::Memcpy(Bytes.GetData(), this, SERIALIZED_SIZE);
		return Bytes;
	}

	static bool Deserialize(const TArray<uint8>& Bytes, FVehicleStateMessage& OutMsg)
	{
		if (Bytes.Num() < SERIALIZED_SIZE) return false;
		FMemory::Memcpy(&OutMsg, Bytes.GetData(), SERIALIZED_SIZE);
		return OutMsg.IsValid();
	}
};

#pragma pack(pop)
