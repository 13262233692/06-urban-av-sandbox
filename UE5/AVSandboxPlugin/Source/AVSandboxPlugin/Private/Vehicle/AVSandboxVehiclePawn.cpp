#include "Vehicle/AVSandboxVehiclePawn.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"

AAVSandboxVehiclePawn::AAVSandboxVehiclePawn()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = ETickingGroup::TG_PostPhysics;

	ChaosVehicleMovement = CreateDefaultSubobject<UChaosWheeledVehicleMovementComponent>(
		TEXT("ChaosVehicleMovement"));
	ChaosVehicleMovement->SetIsReplicated(false);
	ChaosVehicleMovement->bReverseAsBrake = true;

	UDPSocket = CreateDefaultSubobject<UUDPSocketComponent>(TEXT("UDPSocket"));

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->TargetArmLength = 600.0f;
	SpringArm->bUsePawnControlRotation = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm);
	Camera->bUsePawnControlRotation = false;

	CachedLaneGraph = nullptr;
}

void AAVSandboxVehiclePawn::BeginPlay()
{
	Super::BeginPlay();

	if (UDPSocket)
	{
		UDPSocket->InitializeListenSocket(ListenPort);
		UDPSocket->InitializeSendSocket(RemoteIP, RemotePort);
		UDPSocket->OnControlMessageReceived.AddDynamic(this, &AAVSandboxVehiclePawn::HandleControlMessage);
	}

	PreviousVelocity = GetVelocity();
}

void AAVSandboxVehiclePawn::EndPlay(const EEndPlayReason::Reason EndPlayReason)
{
	if (UDPSocket)
	{
		UDPSocket->OnControlMessageReceived.RemoveDynamic(this, &AAVSandboxVehiclePawn::HandleControlMessage);
		UDPSocket->CloseSockets();
	}

	Super::EndPlay(EndPlayReason);
}

void AAVSandboxVehiclePawn::HandleControlMessage(const FVehicleControlMessage& Message)
{
	ApplyControl(Message);
	OnControlReceived(Message);
}

void AAVSandboxVehiclePawn::ApplyControl(const FVehicleControlMessage& Control)
{
	CurrentControl = Control;
	ControlSequenceNumber = Control.SequenceNumber;
	ApplyControlToChaosVehicle(Control);
}

void AAVSandboxVehiclePawn::ApplyControlToChaosVehicle(const FVehicleControlMessage& Control)
{
	if (!ChaosVehicleMovement) return;

	float ThrottleCmd = FMath::Clamp(Control.Throttle, 0.0f, MaxThrottle);
	float BrakeCmd = FMath::Clamp(Control.Brake, 0.0f, MaxBrake);
	float SteeringCmd = FMath::Clamp(Control.SteeringAngle, -1.0f, 1.0f);

	ChaosVehicleMovement->SetThrottleInput(ThrottleCmd);
	ChaosVehicleMovement->SetBrakeInput(BrakeCmd);
	ChaosVehicleMovement->SetSteeringInput(SteeringCmd);

	if (Control.Handbrake > 0)
	{
		ChaosVehicleMovement->SetHandbrakeInput(true);
	}
	else
	{
		ChaosVehicleMovement->SetHandbrakeInput(false);
	}

	if (Control.Gear != 0 && ChaosVehicleMovement->GetTargetGear() != Control.Gear)
	{
		ChaosVehicleMovement->SetTargetGear(Control.Gear);
	}
}

void AAVSandboxVehiclePawn::UpdateAcceleration(float DeltaSeconds)
{
	FVector CurrentVel = GetVelocity();
	if (DeltaSeconds > KINDA_SMALL_NUMBER)
	{
		CurrentAcceleration = (CurrentVel - PreviousVelocity) / DeltaSeconds;
	}
	PreviousVelocity = CurrentVel;
}

void AAVSandboxVehiclePawn::UpdateCollisionState(float DeltaSeconds)
{
	TArray<FHitResult> HitResults;
	FVector Start = GetActorLocation();
	FVector End = Start;

	FCollisionShape CollisionShape;
	float CollisionRadius = 200.0f;
	CollisionShape.SetSphere(CollisionRadius);

	bool bHit = GetWorld()->SweepMultiByChannel(
		HitResults,
		Start,
		End,
		FQuat::Identity,
		ECC_Pawn,
		CollisionShape);

	if (bHit)
	{
		for (const FHitResult& Hit : HitResults)
		{
			if (Hit.GetActor() && Hit.GetActor() != this)
			{
				bIsCollisionDetected = true;
				LastCollisionTime = GetWorld()->GetTimeSeconds();
				OnCollisionDetected();
				break;
			}
		}
	}

	if (bIsCollisionDetected)
	{
		float TimeSinceCollision = GetWorld()->GetTimeSeconds() - LastCollisionTime;
		if (TimeSinceCollision > CollisionTimeout)
		{
			bIsCollisionDetected = false;
		}
	}
}

void AAVSandboxVehiclePawn::UpdateLanePosition()
{
	if (!CachedLaneGraph) return;

	CurrentState.CurrentLaneNodeId = FindClosestLaneNode();

	if (CurrentState.CurrentLaneNodeId >= 0 &&
		CurrentState.CurrentLaneNodeId < CachedLaneGraph->Nodes.Num())
	{
		const FLaneGraphNode& LaneNode = CachedLaneGraph->Nodes[CurrentState.CurrentLaneNodeId];

		if (LaneNode.CenterlinePoints.Num() > 1)
		{
			FVector VehicleLoc = GetActorLocation();
			float MinDist = MAX_FLT;
			float BestDist = 0.0f;
			float CumDist = 0.0f;

			for (int32 i = 1; i < LaneNode.CenterlinePoints.Num(); ++i)
			{
				FVector SegStart = LaneNode.CenterlinePoints[i - 1];
				FVector SegEnd = LaneNode.CenterlinePoints[i];
				FVector SegDir = SegEnd - SegStart;
				float SegLen = SegDir.Size();

				if (SegLen < KINDA_SMALL_NUMBER) continue;

				FVector ToVehicle = VehicleLoc - SegStart;
				float Projection = FVector::DotProduct(ToVehicle, SegDir) / SegLen;
				Projection = FMath::Clamp(Projection, 0.0f, 1.0f);

				FVector ClosestPt = SegStart + SegDir * Projection;
				float Dist = FVector::Dist(VehicleLoc, ClosestPt);

				if (Dist < MinDist)
				{
					MinDist = Dist;
					BestDist = CumDist + Projection * SegLen;
				}

				CumDist += SegLen;
			}

			CurrentState.DistanceAlongLane = BestDist;
			CurrentState.LaneOffset = MinDist;
		}
	}
}

FVehicleStateMessage AAVSandboxVehiclePawn::BuildStateMessage(float DeltaSeconds)
{
	FVehicleStateMessage State;

	FTransform Transform = GetActorTransform();
	FVector Location = Transform.GetTranslation();
	FRotator Rotation = Transform.GetRotation().Rotator();

	State.PositionX = Location.X;
	State.PositionY = Location.Y;
	State.PositionZ = Location.Z;

	State.RotationPitch = Rotation.Pitch;
	State.RotationYaw = Rotation.Yaw;
	State.RotationRoll = Rotation.Roll;

	FVector Velocity = GetVelocity();
	State.VelocityX = Velocity.X;
	State.VelocityY = Velocity.Y;
	State.VelocityZ = Velocity.Z;

	FVector AngVel = GetActorRotation().Vector();
	State.AngularVelocityX = AngVel.X;
	State.AngularVelocityY = AngVel.Y;
	State.AngularVelocityZ = AngVel.Z;

	FVector ForwardVec = GetActorForwardVector();
	FVector RightVec = GetActorRightVector();
	FVector UpVec = GetActorUpVector();

	State.ForwardSpeed = FVector::DotProduct(Velocity, ForwardVec);
	State.LateralSpeed = FVector::DotProduct(Velocity, RightVec);
	State.UpSpeed = FVector::DotProduct(Velocity, UpVec);

	State.AccelerationX = CurrentAcceleration.X;
	State.AccelerationY = CurrentAcceleration.Y;
	State.AccelerationZ = CurrentAcceleration.Z;

	if (ChaosVehicleMovement)
	{
		auto* WheelController = ChaosVehicleMovement;
		if (WheelController->Wheels.Num() >= 4)
		{
			State.TireSlipFL = WheelController->Wheels[0]->GetSlipAngle();
			State.TireSlipFR = WheelController->Wheels[1]->GetSlipAngle();
			State.TireSlipRL = WheelController->Wheels[2]->GetSlipAngle();
			State.TireSlipRR = WheelController->Wheels[3]->GetSlipAngle();

			State.TireLoadFL = WheelController->Wheels[0]->GetLoad();
			State.TireLoadFR = WheelController->Wheels[1]->GetLoad();
			State.TireLoadRL = WheelController->Wheels[2]->GetLoad();
			State.TireLoadRR = WheelController->Wheels[3]->GetLoad();
		}

		State.EngineRPM = ChaosVehicleMovement->GetEngineRotationSpeed();
		State.CurrentGear = ChaosVehicleMovement->GetCurrentGear();
		State.SteeringAngle = ChaosVehicleMovement->GetSteeringInput();
		State.ThrottleInput = ChaosVehicleMovement->GetThrottleInput();
		State.BrakeInput = ChaosVehicleMovement->GetBrakeInput();
	}

	State.CollisionState = bIsCollisionDetected ? 1 : 0;

	State.CurrentLaneNodeId = CurrentState.CurrentLaneNodeId;
	State.LaneOffset = CurrentState.LaneOffset;
	State.DistanceAlongLane = CurrentState.DistanceAlongLane;

	State.SequenceNumber = ++StateSequenceNumber;

	uint32 Ticks = FDateTime::UtcNow().GetTicks() / ETimespan::TicksPerMillisecond;
	State.Timestamp = Ticks & 0xFFFFFFFF;

	return State;
}

FVehicleStateMessage AAVSandboxVehiclePawn::CaptureState(float DeltaSeconds)
{
	UpdateAcceleration(DeltaSeconds);
	UpdateCollisionState(DeltaSeconds);
	UpdateLanePosition();

	CurrentState = BuildStateMessage(DeltaSeconds);
	return CurrentState;
}

bool AAVSandboxVehiclePawn::SendState()
{
	if (!UDPSocket) return false;
	bool bSent = UDPSocket->SendStateMessage(CurrentState);
	if (bSent)
	{
		OnStateSent(CurrentState);
	}
	return bSent;
}

void AAVSandboxVehiclePawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	CaptureState(DeltaSeconds);

	if (bSendStateEveryTick)
	{
		SendState();
	}
}

void AAVSandboxVehiclePawn::ResetVehicle(const FTransform& NewTransform)
{
	SetActorTransform(NewTransform);

	PreviousVelocity = FVector::ZeroVector;
	CurrentAcceleration = FVector::ZeroVector;
	bIsCollisionDetected = false;

	if (ChaosVehicleMovement)
	{
		ChaosVehicleMovement->SetThrottleInput(0.0f);
		ChaosVehicleMovement->SetBrakeInput(0.0f);
		ChaosVehicleMovement->SetSteeringInput(0.0f);
		ChaosVehicleMovement->SetHandbrakeInput(false);
		ChaosVehicleMovement->SetTargetGear(0);
	}
}

void AAVSandboxVehiclePawn::SetLaneGraph(const FLaneGraph* InGraph)
{
	CachedLaneGraph = InGraph;
}

int32 AAVSandboxVehiclePawn::FindClosestLaneNode() const
{
	if (!CachedLaneGraph || CachedLaneGraph->Nodes.Num() == 0) return -1;

	FVector VehicleLoc = GetActorLocation();
	int32 BestNode = -1;
	float BestDist = MAX_FLT;

	for (int32 i = 0; i < CachedLaneGraph->Nodes.Num(); ++i)
	{
		const FLaneGraphNode& Node = CachedLaneGraph->Nodes[i];
		for (const FVector& Pt : Node.CenterlinePoints)
		{
			float Dist = FVector::DistSquared(VehicleLoc, Pt);
			if (Dist < BestDist)
			{
				BestDist = Dist;
				BestNode = i;
			}
		}
	}

	return BestNode;
}
