#include "Vehicle/UDPSocketComponent.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Common/UdpSocketBuilder.h"
#include "Common/UdpSocketReceiver.h"

UUDPSocketComponent::UUDPSocketComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = ETickingGroup::TG_PrePhysics;

	ListenSocket = nullptr;
	SendSocket = nullptr;
	bHasPendingControl = false;
	ReceiveBuffer.SetNumUninitialized(FVehicleControlMessage::SERIALIZED_SIZE + 64);
}

UUDPSocketComponent::~UUDPSocketComponent()
{
	CloseSockets();
}

void UUDPSocketComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UUDPSocketComponent::EndPlay(const EEndPlayReason::Reason EndPlayReason)
{
	CloseSockets();
	Super::EndPlay(EndPlayReason);
}

bool UUDPSocketComponent::InitializeListenSocket(int32 InListenPort)
{
	ListenPort = InListenPort;

	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get();
	if (!SocketSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("[UDPSocket] Failed to get socket subsystem"));
		return false;
	}

	if (ListenSocket)
	{
		SocketSubsystem->DestroySocket(ListenSocket);
		ListenSocket = nullptr;
	}

	FString SocketName = FString::Printf(TEXT("AVSandboxListen_%d"), ListenPort);

	ListenSocket = FUdpSocketBuilder(*SocketName)
		.BoundToPort(ListenPort)
		.WithReceiveBufferSize(ReceiveBufferSize)
		.WithSendBufferSize(ReceiveBufferSize)
		.Build();

	if (!ListenSocket)
	{
		UE_LOG(LogTemp, Error, TEXT("[UDPSocket] Failed to create listen socket on port %d"), ListenPort);
		return false;
	}

	ListenSocket->SetNonBlocking(true);
	bIsListening = true;

	UE_LOG(LogTemp, Log, TEXT("[UDPSocket] Listening on port %d"), ListenPort);
	return true;
}

bool UUDPSocketComponent::InitializeSendSocket(const FString& InRemoteIP, int32 InRemotePort)
{
	RemoteIP = InRemoteIP;
	RemotePort = InRemotePort;

	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get();
	if (!SocketSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("[UDPSocket] Failed to get socket subsystem"));
		return false;
	}

	if (SendSocket)
	{
		SocketSubsystem->DestroySocket(SendSocket);
		SendSocket = nullptr;
	}

	FIPv4Address RemoteIPv4;
	if (!FIPv4Address::Parse(RemoteIP, RemoteIPv4))
	{
		UE_LOG(LogTemp, Error, TEXT("[UDPSocket] Invalid remote IP: %s"), *RemoteIP);
		return false;
	}

	RemoteAddr = ISocketSubsystem::Get()->CreateInternetAddr();
	RemoteAddr->SetIp(RemoteIPv4.Value);
	RemoteAddr->SetPort(RemotePort);

	FString SocketName = FString::Printf(TEXT("AVSandboxSend_%s_%d"), *RemoteIP, RemotePort);

	SendSocket = FUdpSocketBuilder(*SocketName)
		.WithReceiveBufferSize(ReceiveBufferSize)
		.WithSendBufferSize(ReceiveBufferSize)
		.Build();

	if (!SendSocket)
	{
		UE_LOG(LogTemp, Error, TEXT("[UDPSocket] Failed to create send socket to %s:%d"), *RemoteIP, RemotePort);
		return false;
	}

	SendSocket->SetNonBlocking(true);
	bIsSending = true;

	UE_LOG(LogTemp, Log, TEXT("[UDPSocket] Sending to %s:%d"), *RemoteIP, RemotePort);
	return true;
}

void UUDPSocketComponent::CloseSockets()
{
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get();

	if (ListenSocket)
	{
		if (SocketSubsystem)
		{
			SocketSubsystem->DestroySocket(ListenSocket);
		}
		ListenSocket = nullptr;
		bIsListening = false;
	}

	if (SendSocket)
	{
		if (SocketSubsystem)
		{
			SocketSubsystem->DestroySocket(SendSocket);
		}
		SendSocket = nullptr;
		bIsSending = false;
	}
}

void UUDPSocketComponent::ProcessIncomingData()
{
	if (!ListenSocket) return;

	TSharedRef<FInternetAddr> SenderAddr = ISocketSubsystem::Get()->CreateInternetAddr();

	while (true)
	{
		int32 BytesRead = 0;
		bool bReceived = ListenSocket->RecvFrom(
			ReceiveBuffer.GetData(),
			ReceiveBuffer.Num(),
			BytesRead,
			*SenderAddr);

		if (!bReceived || BytesRead <= 0)
		{
			break;
		}

		if (BytesRead >= FVehicleControlMessage::SERIALIZED_SIZE)
		{
			TArray<uint8> MsgBytes;
			MsgBytes.SetNumUninitialized(FVehicleControlMessage::SERIALIZED_SIZE);
			FMemory::Memcpy(MsgBytes.GetData(), ReceiveBuffer.GetData(),
				FVehicleControlMessage::SERIALIZED_SIZE);

			FVehicleControlMessage CtrlMsg;
			if (FVehicleControlMessage::Deserialize(MsgBytes, CtrlMsg))
			{
				if (CtrlMsg.VerifyChecksum())
				{
					FScopeLock Lock(&ControlMutex);
					PendingControl = CtrlMsg;
					bHasPendingControl = true;
					MessagesReceived++;
					LastReceivedSequence = CtrlMsg.SequenceNumber;
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("[UDPSocket] Control message checksum mismatch"));
				}
			}
		}
	}
}

void UUDPSocketComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ProcessIncomingData();

	{
		FScopeLock Lock(&ControlMutex);
		if (bHasPendingControl)
		{
			OnControlMessageReceived.Broadcast(PendingControl);
			bHasPendingControl = false;
		}
	}
}

bool UUDPSocketComponent::SendStateMessage(const FVehicleStateMessage& StateMsg)
{
	if (!SendSocket || !RemoteAddr.IsValid())
	{
		return false;
	}

	FVehicleStateMessage MsgCopy = StateMsg;
	MsgCopy.ComputeChecksum();
	TArray<uint8> Data = MsgCopy.Serialize();

	return SendDataInternal(Data.GetData(), Data.Num());
}

bool UUDPSocketComponent::SendRawData(const TArray<uint8>& Data)
{
	return SendDataInternal(Data.GetData(), Data.Num());
}

bool UUDPSocketComponent::SendDataInternal(const uint8* Data, int32 Size)
{
	if (!SendSocket || !RemoteAddr.IsValid())
	{
		return false;
	}

	int32 BytesSent = 0;
	bool bSuccess = SendSocket->SendTo(Data, Size, BytesSent, *RemoteAddr);

	if (bSuccess && BytesSent == Size)
	{
		MessagesSent++;
		return true;
	}

	return false;
}
