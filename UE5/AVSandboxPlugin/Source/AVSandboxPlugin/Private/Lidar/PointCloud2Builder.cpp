#include "Lidar/PointCloud2Builder.h"

void UPointCloud2Builder::WriteUint8(TArray<uint8>& Buffer, uint8 Value)
{
	Buffer.Add(Value);
}

void UPointCloud2Builder::WriteUint16(TArray<uint8>& Buffer, uint16 Value)
{
	Buffer.Add(Value & 0xFF);
	Buffer.Add((Value >> 8) & 0xFF);
}

void UPointCloud2Builder::WriteUint32(TArray<uint8>& Buffer, uint32 Value)
{
	Buffer.Add(Value & 0xFF);
	Buffer.Add((Value >> 8) & 0xFF);
	Buffer.Add((Value >> 16) & 0xFF);
	Buffer.Add((Value >> 24) & 0xFF);
}

void UPointCloud2Builder::WriteFloat32(TArray<uint8>& Buffer, float Value)
{
	const uint8* Ptr = reinterpret_cast<const uint8*>(&Value);
	for (int32 i = 0; i < 4; ++i)
	{
		Buffer.Add(Ptr[i]);
	}
}

void UPointCloud2Builder::WriteString(TArray<uint8>& Buffer, const FString& Str)
{
	uint32 Len = static_cast<uint32>(Str.Len());
	WriteUint32(Buffer, Len);

	TArray<uint8> StrBytes;
	StrBytes.SetNumUninitialized(Len);
	FMemory::Memcpy(StrBytes.GetData(), StringCast<ANSICHAR>(*Str).Get(), Len);

	Buffer.Append(StrBytes);
}

void UPointCloud2Builder::WriteHeader(TArray<uint8>& Buffer, const FPointCloud2Header& Header)
{
	WriteUint32(Buffer, Header.Seq);
	WriteUint32(Buffer, static_cast<uint32>(Header.StampSec));
	WriteUint32(Buffer, Header.StampNanosec);
	WriteString(Buffer, Header.FrameId);
}

void UPointCloud2Builder::WriteField(TArray<uint8>& Buffer, const FPointCloud2Field& Field)
{
	WriteString(Buffer, Field.Name);
	WriteUint32(Buffer, Field.Offset);
	WriteUint8(Buffer, Field.DataType);
	WriteUint32(Buffer, Field.Count);
}

TArray<uint8> UPointCloud2Builder::BuildPointCloud2Message(
	const FLidarScanFrame& Frame,
	const FString& FrameId)
{
	return BuildPointCloud2MessageXYZIR(Frame, FrameId);
}

TArray<uint8> UPointCloud2Builder::BuildPointCloud2MessageXYZI(
	const FLidarScanFrame& Frame,
	const FString& FrameId)
{
	TArray<uint8> Buffer;

	FPointCloud2Header Header;
	Header.Seq = Frame.FrameNumber;
	int64 TotalNanosec = static_cast<int64>(Frame.Timestamp * 1e9);
	Header.StampSec = static_cast<int32>(TotalNanosec / 1000000000LL);
	Header.StampNanosec = static_cast<uint32>(TotalNanosec % 1000000000LL);
	Header.FrameId = FrameId;
	WriteHeader(Buffer, Header);

	uint8 IsBigEndian = 0;
	WriteUint8(Buffer, IsBigEndian);

	uint32 PointStep = POINT_STEP_XYZI;
	WriteUint32(Buffer, PointStep);

	uint32 RowStep = PointStep * Frame.Points.Num();
	WriteUint32(Buffer, RowStep);

	uint32 IsDense = Frame.ValidPointCount > 0 ? 1 : 0;
	WriteUint32(Buffer, IsDense);

	TArray<FPointCloud2Field> Fields;
	FPointCloud2Field F;
	F.Name = TEXT("x"); F.Offset = 0; F.DataType = FPointCloud2Field::FLOAT32; Fields.Add(F);
	F.Name = TEXT("y"); F.Offset = 4; F.DataType = FPointCloud2Field::FLOAT32; Fields.Add(F);
	F.Name = TEXT("z"); F.Offset = 8; F.DataType = FPointCloud2Field::FLOAT32; Fields.Add(F);
	F.Name = TEXT("intensity"); F.Offset = 12; F.DataType = FPointCloud2Field::FLOAT32; Fields.Add(F);

	WriteUint32(Buffer, static_cast<uint32>(Fields.Num()));
	for (const FPointCloud2Field& Field : Fields)
	{
		WriteField(Buffer, Field);
	}

	uint32 DataLen = PointStep * Frame.Points.Num();
	WriteUint32(Buffer, DataLen);

	for (const FLidarHitPoint& Point : Frame.Points)
	{
		float X = 0.0f, Y = 0.0f, Z = 0.0f, I = 0.0f;

		if (Point.bValid)
		{
			float RadElev = FMath::DegreesToRadians(Point.Elevation);
			float RadAzim = FMath::DegreesToRadians(Point.Azimuth);
			float Range = Point.Range / 100.0f;

			X = Range * FMath::Cos(RadElev) * FMath::Cos(RadAzim);
			Y = Range * FMath::Cos(RadElev) * FMath::Sin(RadAzim);
			Z = Range * FMath::Sin(RadElev);
			I = Point.Intensity;
		}

		WriteFloat32(Buffer, X);
		WriteFloat32(Buffer, Y);
		WriteFloat32(Buffer, Z);
		WriteFloat32(Buffer, I);
	}

	return Buffer;
}

TArray<uint8> UPointCloud2Builder::BuildPointCloud2MessageXYZIR(
	const FLidarScanFrame& Frame,
	const FString& FrameId)
{
	TArray<uint8> Buffer;

	FPointCloud2Header Header;
	Header.Seq = Frame.FrameNumber;
	int64 TotalNanosec = static_cast<int64>(Frame.Timestamp * 1e9);
	Header.StampSec = static_cast<int32>(TotalNanosec / 1000000000LL);
	Header.StampNanosec = static_cast<uint32>(TotalNanosec % 1000000000LL);
	Header.FrameId = FrameId;
	WriteHeader(Buffer, Header);

	uint8 IsBigEndian = 0;
	WriteUint8(Buffer, IsBigEndian);

	uint32 PointStep = POINT_STEP_XYZIR;
	WriteUint32(Buffer, PointStep);

	uint32 RowStep = PointStep * Frame.Points.Num();
	WriteUint32(Buffer, RowStep);

	uint32 IsDense = Frame.ValidPointCount > 0 ? 1 : 0;
	WriteUint32(Buffer, IsDense);

	TArray<FPointCloud2Field> Fields;
	FPointCloud2Field F;
	F.Name = TEXT("x"); F.Offset = 0; F.DataType = FPointCloud2Field::FLOAT32; Fields.Add(F);
	F.Name = TEXT("y"); F.Offset = 4; F.DataType = FPointCloud2Field::FLOAT32; Fields.Add(F);
	F.Name = TEXT("z"); F.Offset = 8; F.DataType = FPointCloud2Field::FLOAT32; Fields.Add(F);
	F.Name = TEXT("intensity"); F.Offset = 12; F.DataType = FPointCloud2Field::FLOAT32; Fields.Add(F);
	F.Name = TEXT("ring"); F.Offset = 16; F.DataType = FPointCloud2Field::UINT16; Fields.Add(F);

	WriteUint32(Buffer, static_cast<uint32>(Fields.Num()));
	for (const FPointCloud2Field& Field : Fields)
	{
		WriteField(Buffer, Field);
	}

	uint32 DataLen = PointStep * Frame.Points.Num();
	WriteUint32(Buffer, DataLen);

	for (const FLidarHitPoint& Point : Frame.Points)
	{
		float X = 0.0f, Y = 0.0f, Z = 0.0f, I = 0.0f;
		uint16 Ring = 0;

		if (Point.bValid)
		{
			float RadElev = FMath::DegreesToRadians(Point.Elevation);
			float RadAzim = FMath::DegreesToRadians(Point.Azimuth);
			float Range = Point.Range / 100.0f;

			X = Range * FMath::Cos(RadElev) * FMath::Cos(RadAzim);
			Y = Range * FMath::Cos(RadElev) * FMath::Sin(RadAzim);
			Z = Range * FMath::Sin(RadElev);
			I = Point.Intensity;
			Ring = static_cast<uint16>(Point.ChannelIndex);
		}

		WriteFloat32(Buffer, X);
		WriteFloat32(Buffer, Y);
		WriteFloat32(Buffer, Z);
		WriteFloat32(Buffer, I);
		WriteUint16(Buffer, Ring);

		Buffer.Add(0);
		Buffer.Add(0);
	}

	return Buffer;
}
