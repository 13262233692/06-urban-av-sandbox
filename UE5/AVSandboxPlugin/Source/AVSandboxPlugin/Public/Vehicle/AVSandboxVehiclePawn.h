#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Vehicle/UDPSocketComponent.h"
#include "Vehicle/VehicleControlProtocol.h"
#include "Vehicle/VehicleStateProtocol.h"
#include "LaneGraph/LaneGraphTypes.h"
#include "AVSandboxVehiclePawn.generated.h"

class UCameraComponent;
class USpringArmComponent;

UCLASS(BlueprintType, Blueprintable)
class AAVSandboxVehiclePawn : public APawn
{
	GENERATED_BODY()

public:
	AAVSandboxVehiclePawn();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Reason EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AVSandbox|Vehicle")
	UChaosWheeledVehicleMovementComponent* ChaosVehicleMovement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AVSandbox|Vehicle")
	UUDPSocketComponent* UDPSocket;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AVSandbox|Vehicle")
	USpringArmComponent* SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AVSandbox|Vehicle")
	UCameraComponent* Camera;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|Network")
	int32 ListenPort = 9000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|Network")
	FString RemoteIP = TEXT("127.0.0.1");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|Network")
	int32 RemotePort = 9001;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|Vehicle")
	float MaxThrottle = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|Vehicle")
	float MaxBrake = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|Vehicle")
	float MaxSteeringAngle = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|Vehicle")
	bool bAutoReverse = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|Vehicle")
	float CollisionTimeout = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|Vehicle")
	bool bSendStateEveryTick = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AVSandbox|Vehicle")
	FVehicleControlMessage CurrentControl;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AVSandbox|Vehicle")
	FVehicleStateMessage CurrentState;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AVSandbox|Vehicle")
	uint16 StateSequenceNumber = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AVSandbox|Vehicle")
	uint16 ControlSequenceNumber = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AVSandbox|Vehicle")
	bool bIsCollisionDetected = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AVSandbox|Vehicle")
	float LastCollisionTime = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AVSandbox|Vehicle")
	FVector PreviousVelocity = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AVSandbox|Vehicle")
	FVector CurrentAcceleration = FVector::ZeroVector;

	UFUNCTION(BlueprintCallable, Category = "AVSandbox|Vehicle")
	void ApplyControl(const FVehicleControlMessage& Control);

	UFUNCTION(BlueprintCallable, Category = "AVSandbox|Vehicle")
	FVehicleStateMessage CaptureState(float DeltaSeconds);

	UFUNCTION(BlueprintCallable, Category = "AVSandbox|Vehicle")
	bool SendState();

	UFUNCTION(BlueprintCallable, Category = "AVSandbox|Vehicle")
	void ResetVehicle(const FTransform& NewTransform);

	UFUNCTION(BlueprintCallable, Category = "AVSandbox|Vehicle")
	void SetLaneGraph(const FLaneGraph* InGraph);

	UFUNCTION(BlueprintCallable, Category = "AVSandbox|Vehicle")
	int32 FindClosestLaneNode() const;

	UFUNCTION(BlueprintImplementableEvent, Category = "AVSandbox|Vehicle")
	void OnControlReceived(const FVehicleControlMessage& Control);

	UFUNCTION(BlueprintImplementableEvent, Category = "AVSandbox|Vehicle")
	void OnStateSent(const FVehicleStateMessage& State);

	UFUNCTION(BlueprintImplementableEvent, Category = "AVSandbox|Vehicle")
	void OnCollisionDetected();

protected:
	UFUNCTION()
	void HandleControlMessage(const FVehicleControlMessage& Message);

private:
	const FLaneGraph* CachedLaneGraph;

	void ApplyControlToChaosVehicle(const FVehicleControlMessage& Control);
	FVehicleStateMessage BuildStateMessage(float DeltaSeconds);
	void UpdateCollisionState(float DeltaSeconds);
	void UpdateAcceleration(float DeltaSeconds);
	void UpdateLanePosition();
};
