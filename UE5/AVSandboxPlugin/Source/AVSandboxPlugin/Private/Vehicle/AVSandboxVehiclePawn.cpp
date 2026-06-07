#include "Vehicle/AVSandboxVehiclePawn.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicsEngine/PhysicsSettings.h"

AAVSandboxVehiclePawn::AAVSandboxVehiclePawn()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = ETickingGroup::TG_PostPhysics;

	ChaosVehicleMovement = CreateDefaultSubobject<UChaosWheeledVehicleMovementComponent>(
		TEXT("ChaosVehicleMovement"));
	ChaosVehicleMovement->SetIsReplicated(false);
	ChaosVehicleMovement->bReverseAsBrake = true;

	UDPSocket = CreateDefaultSubobject<UUDPSocketComponent>(TEXT("UDPSocket"));

	StateInterpolator = CreateDefaultSubobject<UVehicleStateInterpolator>(TEXT("StateInterpolator"));

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->TargetArmLength = 600.0f;
	SpringArm->bUsePawnControlRotation = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm);
	Camera->bUsePawnControlRotation = false;

	CachedLaneGraph = nullptr;
	bHasLastReportedState = false;
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

	if (StateInterpolator)
	{
		StateInterpolator->SetTimeDilation(TargetTimeDilation);
	}

	PreviousVelocity = GetVelocity();
	SmoothedPosition = GetActorLocation();
	SmoothedVelocity = GetVelocity();

	ConfigurePhysicsForTimeDilation(TargetTimeDilation);
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
	if (StateInterpolator)
	{
		double SimTime = GetWorld()->GetTimeSeconds();
		StateInterpolator->EnqueueControl(Message, SimTime);
	}
	else
	{
		ApplyControl(Message);
	}

	OnControlReceived(Message);
}

void AAVSandboxVehiclePawn::ApplyControl(const FVehicleControlMessage& Control)
{
	CurrentControl = Control;
	ControlSequenceNumber = Control.SequenceNumber;
	ApplyControlToChaosVehicle(Control);
}

void AAVSandboxVehiclePawn::ApplySmoothedControlToChaosVehicle(const FControlSnapshot& Snapshot)
{
	if (!ChaosVehicleMovement) return;

	float ThrottleCmd = FMath::Clamp(Snapshot.Throttle, 0.0f, MaxThrottle);
	float BrakeCmd = FMath::Clamp(Snapshot.Brake, 0.0f, MaxBrake);
	float SteeringCmd = FMath::Clamp(Snapshot.SteeringAngle, -1.0f, 1.0f);

	ChaosVehicleMovement->SetThrottleInput(ThrottleCmd);
	ChaosVehicleMovement->SetBrakeInput(BrakeCmd);
	ChaosVehicleMovement->SetSteeringInput(SteeringCmd);

	if (Snapshot.Handbrake > 0)
	{
		ChaosVehicleMovement->SetHandbrakeInput(true);
	}
	else
	{
		ChaosVehicleMovement->SetHandbrakeInput(false);
	}

	if (Snapshot.Gear != 0 && ChaosVehicleMovement->GetTargetGear() != Snapshot.Gear)
	{
		ChaosVehicleMovement->SetTargetGear(Snapshot.Gear);
	}
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

void AAVSandboxVehiclePawn::SetTimeDilation(float Dilation)
{
	TargetTimeDilation = FMath::Max(Dilation, 0.1f);

	if (StateInterpolator)
	{
		StateInterpolator->SetTimeDilation(TargetTimeDilation);
	}

	ConfigurePhysicsForTimeDilation(TargetTimeDilation);

	EffectiveTimeDilation = TargetTimeDilation;

	UE_LOG(LogTemp, Log, TEXT("[AVSandboxVehicle] TimeDilation set to %.2f"), TargetTimeDilation);
}

void AAVSandboxVehiclePawn::ConfigurePhysicsForTimeDilation(float Dilation)
{
	if (!bAutoConfigurePhysicsSubstep || !ChaosVehicleMovement) return;

	float AdjustedSubstepDt = BaseSubstepDeltaTime / Dilation;
	int32 RequiredSubsteps = FMath::CeilToInt(Dilation);

	int32 FinalSubsteps = FMath::Min(RequiredSubsteps, MaxSubsteps);

	if (ChaosVehicleMovement)
	{
		ChaosVehicleMovement->MaxEngineRPM = ChaosVehicleMovement->MaxEngineRPM;
	}

	UPhysicsSettings* PhysSettings = UPhysicsSettings::Get();
	if (PhysSettings)
	{
		PhysSettings->MaxSubstepDeltaTime = AdjustedSubstepDt;
		PhysSettings->MaxSubsteps = FinalSubsteps;
		PhysSettings->bSubstepping = true;
	}

	UE_LOG(LogTemp, Log, TEXT("[AVSandboxVehicle] Physics configured for %.1fx: substep_dt=%.5f, max_substeps=%d"),
		Dilation, AdjustedSubstepDt, FinalSubsteps);
}

FVector AAVSandboxVehiclePawn::ApplyExponentialSmoothing(
	const FVector& Raw,
	const FVector& Previous,
	float Alpha)
{
	return Alpha * Raw + (1.0f - Alpha) * Previous;
}

FVector AAVSandboxVehiclePawn::ClampStateJump(
	const FVector& Current,
	const FVector& Previous,
	float MaxDelta)
{
	FVector Delta = Current - Previous;
	float DeltaMag = Delta.Size();

	if (DeltaMag > MaxDelta && DeltaMag > KINDA_SMALL_NUMBER)
	{
		return Previous + Delta.GetSafeNormal() * MaxDelta;
	}

	return Current;
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

void AAVSandboxVehiclePawn::UpdateDeadReckoning(float DeltaSeconds)
{
	if (!StateInterpolator) return;

	FVector CurrentPos = GetActorLocation();
	FVector CurrentVel = GetVelocity();

	StateInterpolator->UpdateDeadReckoning(
		CurrentPos,
		CurrentVel,
		CurrentAcceleration,
		ChaosVehicleMovement ? ChaosVehicleMovement->GetSteeringInput() : 0.0f,
		GetWorld()->GetTimeSeconds());
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

	FVector RawLocation = GetActorLocation();
	FVector RawVelocity = GetVelocity();

	if (!bHasLastReportedState)
	{
		SmoothedPosition = RawLocation;
		SmoothedVelocity = RawVelocity;
		bHasLastReportedState = true;
	}
	else
	{
		FVector ClampedPosition = ClampStateJump(
			RawLocation, SmoothedPosition, MaxPositionJumpThreshold * EffectiveTimeDilation);

		SmoothedPosition = ApplyExponentialSmoothing(
			ClampedPosition, SmoothedPosition, StateSmoothingFactor);

		FVector ClampedVelocity = ClampStateJump(
			RawVelocity, SmoothedVelocity, MaxVelocityJumpThreshold * EffectiveTimeDilation);

		SmoothedVelocity = ApplyExponentialSmoothing(
			ClampedVelocity, SmoothedVelocity, StateSmoothingFactor);
	}

	FVector ReportPosition = SmoothedPosition;
	FVector ReportVelocity = SmoothedVelocity;

	FRotator RawRotation = GetActorRotation();

	State.PositionX = ReportPosition.X;
	State.PositionY = ReportPosition.Y;
	State.PositionZ = ReportPosition.Z;

	State.RotationPitch = RawRotation.Pitch;
	State.RotationYaw = RawRotation.Yaw;
	State.RotationRoll = RawRotation.Roll;

	State.VelocityX = ReportVelocity.X;
	State.VelocityY = ReportVelocity.Y;
	State.VelocityZ = ReportVelocity.Z;

	FVector AngVel = RawRotation.Vector();
	State.AngularVelocityX = AngVel.X;
	State.AngularVelocityY = AngVel.Y;
	State.AngularVelocityZ = AngVel.Z;

	FVector ForwardVec = GetActorForwardVector();
	FVector RightVec = GetActorRightVector();
	FVector UpVec = GetActorUpVector();

	State.ForwardSpeed = FVector::DotProduct(ReportVelocity, ForwardVec);
	State.LateralSpeed = FVector::DotProduct(ReportVelocity, RightVec);
	State.UpSpeed = FVector::DotProduct(ReportVelocity, UpVec);

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

	LastReportedPosition = ReportPosition;
	LastReportedVelocity = ReportVelocity;

	return State;
}

FVehicleStateMessage AAVSandboxVehiclePawn::CaptureState(float DeltaSeconds)
{
	UpdateAcceleration(DeltaSeconds);
	UpdateCollisionState(DeltaSeconds);
	UpdateLanePosition();
	UpdateDeadReckoning(DeltaSeconds);

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

	EffectiveTimeDilation = GetWorld()->GetTimeSeconds() > 0
		? FMath::Max(GetActorTimeDilation(), 0.1f)
		: TargetTimeDilation;

	if (StateInterpolator)
	{
		double CurrentSimTime = GetWorld()->GetTimeSeconds();
		FControlSnapshot InterpolatedControl = StateInterpolator->GetInterpolatedControl(CurrentSimTime);

		CurrentControl.Throttle = InterpolatedControl.Throttle;
		CurrentControl.Brake = InterpolatedControl.Brake;
		CurrentControl.SteeringAngle = InterpolatedControl.SteeringAngle;
		CurrentControl.Gear = InterpolatedControl.Gear;
		CurrentControl.Handbrake = InterpolatedControl.Handbrake;
		ControlSequenceNumber = InterpolatedControl.SequenceNumber;

		ApplySmoothedControlToChaosVehicle(InterpolatedControl);
	}

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
	bHasLastReportedState = false;

	SmoothedPosition = NewTransform.GetTranslation();
	SmoothedVelocity = FVector::ZeroVector;

	if (StateInterpolator)
	{
		StateInterpolator->Reset();
	}

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
