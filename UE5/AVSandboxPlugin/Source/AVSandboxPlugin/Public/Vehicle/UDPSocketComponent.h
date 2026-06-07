#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Vehicle/VehicleControlProtocol.h"
#include "Vehicle/VehicleStateProtocol.h"
#include "UDPSocketComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnControlMessageReceived, const FVehicleControlMessage&, Message);

UCLASS(BlueprintType, ClassGroup = (AVSandbox), meta = (BlueprintSpawnableComponent))
class UUDPSocketComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UUDPSocketComponent();
	virtual ~UUDPSocketComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Reason EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "AVSandbox|UDP")
	bool InitializeListenSocket(int32 InListenPort = 9000);

	UFUNCTION(BlueprintCallable, Category = "AVSandbox|UDP")
	bool InitializeSendSocket(const FString& InRemoteIP = TEXT("127.0.0.1"), int32 InRemotePort = 9001);

	UFUNCTION(BlueprintCallable, Category = "AVSandbox|UDP")
	void CloseSockets();

	UFUNCTION(BlueprintCallable, Category = "AVSandbox|UDP")
	bool SendStateMessage(const FVehicleStateMessage& StateMsg);

	UFUNCTION(BlueprintCallable, Category = "AVSandbox|UDP")
	bool SendRawData(const TArray<uint8>& Data);

	UPROPERTY(BlueprintAssignable, Category = "AVSandbox|UDP")
	FOnControlMessageReceived OnControlMessageReceived;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|UDP")
	int32 ListenPort = 9000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|UDP")
	FString RemoteIP = TEXT("127.0.0.1");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|UDP")
	int32 RemotePort = 9001;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|UDP")
	int32 ReceiveBufferSize = 65536;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AVSandbox|UDP")
	bool bIsListening = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AVSandbox|UDP")
	bool bIsSending = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AVSandbox|UDP")
	int32 MessagesReceived = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AVSandbox|UDP")
	int32 MessagesSent = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AVSandbox|UDP")
	int32 LastReceivedSequence = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AVSandbox|UDP")
	float LastReceivedTimestamp = 0.0f;

private:
	FSocket* ListenSocket;
	FSocket* SendSocket;
	TSharedPtr<FInternetAddr> RemoteAddr;

	FVehicleControlMessage PendingControl;
	bool bHasPendingControl;

	FCriticalSection ControlMutex;

	TArray<uint8> ReceiveBuffer;

	void ProcessIncomingData();
	bool SendDataInternal(const uint8* Data, int32 Size);
};
