#pragma once

#include "CoreMinimal.h"
#include "VehicleControlProtocol.generated.h"

#pragma pack(push, 1)

USTRUCT(BlueprintType)
struct FVehicleControlMessage
{
	GENERATED_BODY()

	static constexpr uint32 MAGIC = 0x41564354;
	static constexpr uint16 VERSION = 1;

	UPROPERTY(VisibleAnywhere) uint32 Magic = MAGIC;
	UPROPERTY(VisibleAnywhere) uint16 Version = VERSION;
	UPROPERTY(VisibleAnywhere) uint16 SequenceNumber = 0;
	UPROPERTY(VisibleAnywhere) float Throttle = 0.0f;
	UPROPERTY(VisibleAnywhere) float Brake = 0.0f;
	UPROPERTY(VisibleAnywhere) float SteeringAngle = 0.0f;
	UPROPERTY(VisibleAnywhere) int8 Gear = 0;
	UPROPERTY(VisibleAnywhere) uint8 Handbrake = 0;
	UPROPERTY(VisibleAnywhere) uint8 Reserved1 = 0;
	UPROPERTY(VisibleAnywhere) uint8 Reserved2 = 0;
	UPROPERTY(VisibleAnywhere) uint32 Timestamp = 0;
	UPROPERTY(VisibleAnywhere) uint32 Checksum = 0;

	static constexpr int32 SERIALIZED_SIZE = 4 + 2 + 2 + 4 + 4 + 4 + 1 + 1 + 1 + 1 + 4 + 4;

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
		FVehicleControlMessage Copy = *this;
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

	static bool Deserialize(const TArray<uint8>& Bytes, FVehicleControlMessage& OutMsg)
	{
		if (Bytes.Num() < SERIALIZED_SIZE) return false;
		FMemory::Memcpy(&OutMsg, Bytes.GetData(), SERIALIZED_SIZE);
		return OutMsg.IsValid();
	}
};

#pragma pack(pop)
