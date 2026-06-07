#include "Lidar/LidarTCPStreamer.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Common/TcpSocketBuilder.h"

ULidarTCPStreamer::ULidarTCPStreamer()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = ETickingGroup::TG_Last;

	ListenSocket = nullptr;
	bIsRunning = false;
}

ULidarTCPStreamer::~ULidarTCPStreamer()
{
	StopServer();
}

void ULidarTCPStreamer::BeginPlay()
{
	Super::BeginPlay();
}

void ULidarTCPStreamer::EndPlay(const EEndPlayReason::Reason EndPlayReason)
{
	StopServer();
	Super::EndPlay(EndPlayReason);
}

bool ULidarTCPStreamer::StartServer(int32 Port, int32 MaxClientCount)
{
	ListenPort = Port;
	MaxClients = MaxClientCount;

	ISocketSubsystem* SocketSub = ISocketSubsystem::Get();
	if (!SocketSub)
	{
		UE_LOG(LogTemp, Error, TEXT("[LidarTCP] Socket subsystem not available"));
		return false;
	}

	if (ListenSocket)
	{
		SocketSub->DestroySocket(ListenSocket);
		ListenSocket = nullptr;
	}

	FString SocketName = FString::Printf(TEXT("LidarTCP_%d"), ListenPort);

	ListenSocket = FTcpSocketBuilder(*SocketName)
		.AsReusable()
		.BoundToPort(ListenPort)
		.WithReceiveBufferSize(64 * 1024)
		.WithSendBufferSize(SendBufferSize)
		.Listen(MaxClients);

	if (!ListenSocket)
	{
		UE_LOG(LogTemp, Error, TEXT("[LidarTCP] Failed to start TCP server on port %d"), ListenPort);
		return false;
	}

	ListenSocket->SetNonBlocking(true);
	bIsRunning = true;

	UE_LOG(LogTemp, Log, TEXT("[LidarTCP] Server started on port %d, max %d clients"),
		ListenPort, MaxClients);
	return true;
}

void ULidarTCPStreamer::StopServer()
{
	FScopeLock Lock(&ClientsMutex);

	for (FTCPClientConnection& Client : Clients)
	{
		Client.Disconnect();
	}
	Clients.Empty();

	if (ListenSocket)
	{
		ISocketSubsystem::Get()->DestroySocket(ListenSocket);
		ListenSocket = nullptr;
	}

	bIsRunning = false;
	UE_LOG(LogTemp, Log, TEXT("[LidarTCP] Server stopped"));
}

void ULidarTCPStreamer::AcceptNewConnections()
{
	if (!ListenSocket) return;

	TSharedRef<FInternetAddr> RemoteAddr = ISocketSubsystem::Get()->CreateInternetAddr();

	while (true)
	{
		FSocket* NewSocket = ListenSocket->Accept(*RemoteAddr, TEXT("LidarTCPClient"));
		if (!NewSocket) break;

		FScopeLock Lock(&ClientsMutex);

		if (Clients.Num() >= MaxClients)
		{
			UE_LOG(LogTemp, Warning, TEXT("[LidarTCP] Max clients reached, rejecting connection from %s"),
				*RemoteAddr->ToString(true));
			NewSocket->Close();
			ISocketSubsystem::Get()->DestroySocket(NewSocket);
			continue;
		}

		NewSocket->SetNonBlocking(true);
		NewSocket->SetSendBufferSize(SendBufferSize, SendBufferSize);

		FTCPClientConnection Client;
		Client.Socket = NewSocket;
		Client.ClientAddress = RemoteAddr->ToString(true);
		Client.bIsConnected = true;
		Client.FramesSent = 0;
		Client.BytesSent = 0;

		Clients.Add(Client);

		UE_LOG(LogTemp, Log, TEXT("[LidarTCP] Client connected: %s (total: %d)"),
			*Client.ClientAddress, Clients.Num());
	}
}

void ULidarTCPStreamer::RemoveDisconnectedClients()
{
	FScopeLock Lock(&ClientsMutex);

	for (int32 i = Clients.Num() - 1; i >= 0; --i)
	{
		if (!Clients[i].bIsConnected || !Clients[i].Socket)
		{
			UE_LOG(LogTemp, Log, TEXT("[LidarTCP] Client disconnected: %s"), *Clients[i].ClientAddress);
			Clients[i].Disconnect();
			Clients.RemoveAt(i);
			continue;
		}

		TArray<uint8> PeekBuf;
		PeekBuf.SetNumUninitialized(1);
		int32 BytesRead = 0;
		bool bPeekOk = Clients[i].Socket->Recv(PeekBuf.GetData(), 1, BytesRead, ESocketReceiveFlags::Peek);

		if (!bPeekOk)
		{
			int32 Err = ISocketSubsystem::Get()->GetLastErrorCode();
			if (Err != SE_EWOULDBLOCK && Err != SE_EAGAIN)
			{
				UE_LOG(LogTemp, Log, TEXT("[LidarTCP] Client %s connection lost (error: %d)"),
					*Clients[i].ClientAddress, Err);
				Clients[i].Disconnect();
				Clients.RemoveAt(i);
			}
		}
	}
}

TArray<uint8> ULidarTCPStreamer::WrapWithFrameHeader(const TArray<uint8>& PointCloud2Data, int32 FrameNumber)
{
	TArray<uint8> Wrapped;

	uint32 Magic = 0x4C444152;
	uint16 Version = 1;
	uint16 Frame = static_cast<uint16>(FrameNumber & 0xFFFF);
	uint32 PayloadSize = static_cast<uint32>(PointCloud2Data.Num());
	uint32 HeaderChecksum = Magic + Version + Frame + PayloadSize;

	Wrapped.SetNumUninitialized(16 + PointCloud2Data.Num());

	auto WriteU32 = [](uint8* Dst, uint32 Val)
	{
		Dst[0] = Val & 0xFF;
		Dst[1] = (Val >> 8) & 0xFF;
		Dst[2] = (Val >> 16) & 0xFF;
		Dst[3] = (Val >> 24) & 0xFF;
	};

	auto WriteU16 = [](uint8* Dst, uint16 Val)
	{
		Dst[0] = Val & 0xFF;
		Dst[1] = (Val >> 8) & 0xFF;
	};

	WriteU32(&Wrapped[0], Magic);
	WriteU16(&Wrapped[4], Version);
	WriteU16(&Wrapped[6], Frame);
	WriteU32(&Wrapped[8], PayloadSize);
	WriteU32(&Wrapped[12], HeaderChecksum);

	FMemory::Memcpy(&Wrapped[16], PointCloud2Data.GetData(), PointCloud2Data.Num());

	return Wrapped;
}

bool ULidarTCPStreamer::SendToClient(FTCPClientConnection& Client, const uint8* Data, int32 Size)
{
	if (!Client.bIsConnected || !Client.Socket) return false;

	int32 BytesSent = 0;
	int32 Remaining = Size;
	const uint8* Ptr = Data;

	while (Remaining > 0)
	{
		int32 ChunkSize = FMath::Min(Remaining, 65536);
		bool bSuccess = Client.Socket->Send(Ptr, ChunkSize, BytesSent);

		if (!bSuccess || BytesSent <= 0)
		{
			Client.bIsConnected = false;
			return false;
		}

		Ptr += BytesSent;
		Remaining -= BytesSent;
		Client.BytesSent += BytesSent;
	}

	return true;
}

bool ULidarTCPStreamer::BroadcastPointCloud2(const TArray<uint8>& Data)
{
	if (!bIsRunning || Data.Num() == 0) return false;

	FScopeLock Lock(&ClientsMutex);

	if (Clients.Num() == 0) return true;

	TArray<uint8> SendData;
	if (bIncludeFrameHeader)
	{
		SendData = WrapWithFrameHeader(Data, TotalFramesSent);
	}
	else
	{
		SendData = Data;
	}

	bool bAllSuccess = true;
	for (FTCPClientConnection& Client : Clients)
	{
		if (!SendToClient(Client, SendData.GetData(), SendData.Num()))
		{
			bAllSuccess = false;
		}
		else
		{
			Client.FramesSent++;
		}
	}

	TotalFramesSent++;
	TotalBytesSent += SendData.Num();

	return bAllSuccess;
}

int32 ULidarTCPStreamer::GetConnectedClientCount() const
{
	FScopeLock Lock(&ClientsMutex);
	int32 Count = 0;
	for (const FTCPClientConnection& Client : Clients)
	{
		if (Client.bIsConnected) Count++;
	}
	return Count;
}

void ULidarTCPStreamer::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsRunning) return;

	AcceptNewConnections();
	RemoveDisconnectedClients();
}
