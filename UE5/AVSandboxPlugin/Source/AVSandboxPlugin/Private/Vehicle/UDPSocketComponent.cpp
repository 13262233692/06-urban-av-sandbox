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
	HighestDeliveredSequence = 0;
	DrainAccumulator = 0.0f;
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

bool UUDPSocketComponent::IsSequenceNewer(uint16 Incoming, uint16 Reference)
{
	int32 Diff = static_cast<int32>(Incoming) - static_cast<int32>(Reference);
	return Diff > 0 && Diff < 32768;
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

	UE_LOG(LogTemp, Log, TEXT("[UDPSocket] Listening on port %d (mode=%s)"),
		ListenPort, DeliveryMode == EUDPDeliveryMode::Immediate ? TEXT("Immediate") : TEXT("BufferedOrdered"));
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

	OrderedBuffer.Empty();
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
				if (!CtrlMsg.VerifyChecksum())
				{
					UE_LOG(LogTemp, Warning, TEXT("[UDPSocket] Control message checksum mismatch"));
					continue;
				}

				MessagesReceived++;
				LastReceivedSequence = CtrlMsg.SequenceNumber;

				if (DeliveryMode == EUDPDeliveryMode::Immediate)
				{
					FScopeLock Lock(&ControlMutex);
					PendingControl = CtrlMsg;
					bHasPendingControl = true;
				}
				else
				{
					FScopeLock Lock(&ControlMutex);

					if (bDropOutOfOrder && HighestDeliveredSequence > 0)
					{
						if (!IsSequenceNewer(CtrlMsg.SequenceNumber, HighestDeliveredSequence))
						{
							DroppedOutOfOrder++;
							continue;
						}
					}

					int32 InsertIdx = 0;
					for (int32 i = OrderedBuffer.Num() - 1; i >= 0; --i)
					{
						if (IsSequenceNewer(OrderedBuffer[i].SequenceNumber, CtrlMsg.SequenceNumber))
						{
							InsertIdx = i + 1;
							break;
						}
					}

					OrderedBuffer.Insert(CtrlMsg, InsertIdx);

					if (OrderedBuffer.Num() > MaxBufferSize)
					{
						OrderedBuffer.RemoveAt(0, OrderedBuffer.Num() - MaxBufferSize);
					}

					BufferQueueSize = OrderedBuffer.Num();
				}
			}
		}
	}
}

void UUDPSocketComponent::ProcessOrderedBuffer()
{
	if (OrderedBuffer.Num() == 0) return;

	FScopeLock Lock(&ControlMutex);

	int32 DeliverCount = 0;
	if (BufferDrainInterval <= 0.0f)
	{
		DeliverCount = 1;
	}
	else
	{
		DeliverCount = FMath::Max(1, FMath::FloorToInt(DrainAccumulator / BufferDrainInterval));
	}

	for (int32 i = 0; i < DeliverCount && OrderedBuffer.Num() > 0; ++i)
	{
		FVehicleControlMessage Msg = OrderedBuffer[0];
		OrderedBuffer.RemoveAt(0);

		HighestDeliveredSequence = Msg.SequenceNumber;

		PendingControl = Msg;
		bHasPendingControl = true;

		OnControlMessageReceived.Broadcast(Msg);
	}

	BufferQueueSize = OrderedBuffer.Num();
}

void UUDPSocketComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ProcessIncomingData();

	if (DeliveryMode == EUDPDeliveryMode::BufferedOrdered)
	{
		DrainAccumulator += DeltaTime;
		ProcessOrderedBuffer();
		DrainAccumulator = FMath::Fmod(DrainAccumulator, FMath::Max(BufferDrainInterval, 0.001f));
	}
	else
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
