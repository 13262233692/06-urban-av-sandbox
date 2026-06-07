#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Lidar/LidarTypes.h"
#include "LidarTCPStreamer.generated.h"

USTRUCT()
struct FTCPClientConnection
{
	GENERATED_BODY()

	FSocket* Socket = nullptr;
	FString ClientAddress;
	bool bIsConnected = false;
	uint32 FramesSent = 0;
	uint32 BytesSent = 0;

	void Disconnect()
	{
		if (Socket)
		{
			ISocketSubsystem::Get()->DestroySocket(Socket);
			Socket = nullptr;
		}
		bIsConnected = false;
	}
};

UCLASS(BlueprintType, ClassGroup = (AVSandbox), meta = (BlueprintSpawnableComponent))
class ULidarTCPStreamer : public UActorComponent
{
	GENERATED_BODY()

public:
	ULidarTCPStreamer();
	virtual ~ULidarTCPStreamer();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Reason EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "AVSandbox|LidarTCP")
	bool StartServer(int32 Port = 9100, int32 MaxClients = 4);

	UFUNCTION(BlueprintCallable, Category = "AVSandbox|LidarTCP")
	void StopServer();

	UFUNCTION(BlueprintCallable, Category = "AVSandbox|LidarTCP")
	bool BroadcastPointCloud2(const TArray<uint8>& Data);

	UFUNCTION(BlueprintCallable, Category = "AVSandbox|LidarTCP")
	int32 GetConnectedClientCount() const;

	UFUNCTION(BlueprintCallable, Category = "AVSandbox|LidarTCP")
	bool IsServerRunning() const { return bIsRunning; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|LidarTCP")
	int32 ListenPort = 9100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|LidarTCP")
	int32 MaxClients = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|LidarTCP")
	int32 SendBufferSize = 2 * 1024 * 1024;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|LidarTCP")
	bool bIncludeFrameHeader = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AVSandbox|LidarTCP")
	bool bIsRunning = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AVSandbox|LidarTCP")
	int32 TotalFramesSent = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AVSandbox|LidarTCP")
	int64 TotalBytesSent = 0;

private:
	FSocket* ListenSocket;
	TArray<FTCPClientConnection> Clients;

	FCriticalSection ClientsMutex;

	void AcceptNewConnections();
	void RemoveDisconnectedClients();
	bool SendToClient(FTCPClientConnection& Client, const uint8* Data, int32 Size);
	TArray<uint8> WrapWithFrameHeader(const TArray<uint8>& PointCloud2Data, int32 FrameNumber);
};
