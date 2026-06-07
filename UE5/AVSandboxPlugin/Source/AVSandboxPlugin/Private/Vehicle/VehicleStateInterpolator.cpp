#include "Vehicle/VehicleStateInterpolator.h"
#include "Engine/World.h"

UVehicleStateInterpolator::UVehicleStateInterpolator()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = ETickingGroup::TG_PrePhysics;

	LastReceivedSimTime = 0.0;
	InterpolationStartTime = 0.0;
	bHasPrevControl = false;
	bHasTargetControl = false;
	BlendAlpha = 0.0f;
}

void UVehicleStateInterpolator::Reset()
{
	FScopeLock Lock(&BufferMutex);
	ControlBuffer.Empty();
	DeadReckoningState.bIsValid = false;
	bHasPrevControl = false;
	bHasTargetControl = false;
	BlendAlpha = 0.0f;
	SmoothedThrottle = 0.0f;
	SmoothedBrake = 0.0f;
	SmoothedSteering = 0.0f;
	PrevAppliedControl = FControlSnapshot();
	TargetControl = FControlSnapshot();
	ActiveControl = FControlSnapshot();
}

void UVehicleStateInterpolator::EnqueueControl(
	const FVehicleControlMessage& Control,
	double CurrentSimTime)
{
	FScopeLock Lock(&BufferMutex);

	FControlSnapshot Snapshot;
	Snapshot.Throttle = FMath::Clamp(Control.Throttle, 0.0f, 1.0f);
	Snapshot.Brake = FMath::Clamp(Control.Brake, 0.0f, 1.0f);
	Snapshot.SteeringAngle = FMath::Clamp(Control.SteeringAngle, -1.0f, 1.0f);
	Snapshot.Gear = Control.Gear;
	Snapshot.Handbrake = Control.Handbrake;
	Snapshot.SimTime = CurrentSimTime + NetworkJitterBufferSeconds;
	Snapshot.SequenceNumber = Control.SequenceNumber;

	FHermiteControlPoint Point;
	Point.Control = Snapshot;
	Point.SimTime = Snapshot.SimTime;

	if (DeadReckoningState.bIsValid)
	{
		Point.VelocityAtPoint = DeadReckoningState.PredictedVelocity;
		Point.AccelerationAtPoint = DeadReckoningState.PredictedAcceleration;
	}

	if (ControlBuffer.Num() > 0)
	{
		FHermiteControlPoint& LastPoint = ControlBuffer.Last();
		double Dt = Snapshot.SimTime - LastPoint.SimTime;
		if (Dt > KINDA_SMALL_NUMBER)
		{
			FVector LastPos = LastPoint.Position2D();
			FVector CurPos = Point.Position2D();
			Point.VelocityAtPoint = (CurPos - LastPos) / Dt;

			if (LastPoint.VelocityAtPoint.SizeSquared() > KINDA_SMALL_NUMBER)
			{
				Point.AccelerationAtPoint = (Point.VelocityAtPoint - LastPoint.VelocityAtPoint) / Dt;
			}
		}
	}

	if (ControlBuffer.Num() > 0 && Snapshot.SimTime < ControlBuffer.Last().SimTime - 0.001)
	{
		DroppedOutOfOrderCount++;
		return;
	}

	ControlBuffer.Add(Point);
	TrimBuffer();

	LastReceivedSimTime = Snapshot.SimTime;
	BufferedSnapshotCount = ControlBuffer.Num();
}

void UVehicleStateInterpolator::TrimBuffer()
{
	while (ControlBuffer.Num() > MaxBufferedSnapshots)
	{
		ControlBuffer.RemoveAt(0);
	}
}

float UVehicleStateInterpolator::HermiteInterpValue(
	float Y0, float Y1, float Y2, float Y3,
	float Alpha, float Tension, float Bias) const
{
	float M0 = (Y1 - Y0) * (1.0f + Bias) * (1.0f - Tension) * 0.5f
		+ (Y2 - Y1) * (1.0f - Bias) * (1.0f - Tension) * 0.5f;
	float M1 = (Y2 - Y1) * (1.0f + Bias) * (1.0f - Tension) * 0.5f
		+ (Y3 - Y2) * (1.0f - Bias) * (1.0f - Tension) * 0.5f;

	float A = 2.0f * Alpha * Alpha * Alpha - 3.0f * Alpha * Alpha + 1.0f;
	float B = Alpha * Alpha * Alpha - 2.0f * Alpha * Alpha + Alpha;
	float C = -2.0f * Alpha * Alpha * Alpha + 3.0f * Alpha * Alpha;
	float D = Alpha * Alpha * Alpha - Alpha * Alpha;

	return A * Y1 + B * M0 + C * Y2 + D * M1;
}

FControlSnapshot UVehicleStateInterpolator::InterpolateHermite(
	const FHermiteControlPoint& P0,
	const FHermiteControlPoint& P1,
	float Alpha) const
{
	Alpha = FMath::Clamp(Alpha, 0.0f, 1.0f);

	auto GetControlValue = [](const FHermiteControlPoint& P, int32 Axis) -> float
	{
		switch (Axis)
		{
		case 0: return P.Control.SteeringAngle;
		case 1: return P.Control.Throttle;
		case 2: return P.Control.Brake;
		default: return 0.0f;
		}
	};

	int32 P0Idx = -1;
	int32 P1Idx = -1;
	for (int32 i = 0; i < ControlBuffer.Num(); ++i)
	{
		if (ControlBuffer[i].SimTime <= P0.SimTime) P0Idx = i;
		if (P1Idx < 0 && ControlBuffer[i].SimTime >= P1.SimTime) P1Idx = i;
	}

	int32 P0Prev = FMath::Max(0, P0Idx - 1);
	int32 P1Next = FMath::Min(ControlBuffer.Num() - 1, P1Idx + 1);

	FControlSnapshot Result;
	Result.SimTime = P0.SimTime + Alpha * (P1.SimTime - P0.SimTime);
	Result.SequenceNumber = Alpha < 0.5f ? P0.Control.SequenceNumber : P1.Control.SequenceNumber;
	Result.Gear = Alpha < 0.5f ? P0.Control.Gear : P1.Control.Gear;
	Result.Handbrake = Alpha < 0.5f ? P0.Control.Handbrake : P1.Control.Handbrake;

	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		float Y0 = GetControlValue(ControlBuffer.IsValidIndex(P0Prev) ? ControlBuffer[P0Prev] : P0, Axis);
		float Y1 = GetControlValue(P0, Axis);
		float Y2 = GetControlValue(P1, Axis);
		float Y3 = GetControlValue(ControlBuffer.IsValidIndex(P1Next) ? ControlBuffer[P1Next] : P1, Axis);

		float Value = HermiteInterpValue(Y0, Y1, Y2, Y3, Alpha, HermiteTension, HermiteBias);

		switch (Axis)
		{
		case 0: Result.SteeringAngle = FMath::Clamp(Value, -1.0f, 1.0f); break;
		case 1: Result.Throttle = FMath::Clamp(Value, 0.0f, 1.0f); break;
		case 2: Result.Brake = FMath::Clamp(Value, 0.0f, 1.0f); break;
		}
	}

	return Result;
}

FControlSnapshot UVehicleStateInterpolator::InterpolateLinear(
	const FControlSnapshot& From,
	const FControlSnapshot& To,
	float Alpha) const
{
	Alpha = FMath::Clamp(Alpha, 0.0f, 1.0f);

	FControlSnapshot Result;
	Result.Throttle = FMath::Lerp(From.Throttle, To.Throttle, Alpha);
	Result.Brake = FMath::Lerp(From.Brake, To.Brake, Alpha);
	Result.SteeringAngle = FMath::Lerp(From.SteeringAngle, To.SteeringAngle, Alpha);
	Result.Gear = Alpha < 0.5f ? From.Gear : To.Gear;
	Result.Handbrake = Alpha < 0.5f ? From.Handbrake : To.Handbrake;
	Result.SimTime = From.SimTime + Alpha * (To.SimTime - From.SimTime);
	Result.SequenceNumber = Alpha < 0.5f ? From.SequenceNumber : To.SequenceNumber;

	return Result;
}

FControlSnapshot UVehicleStateInterpolator::ComputeDeadReckoningControl(double CurrentSimTime) const
{
	if (!DeadReckoningState.bIsValid)
	{
		return ActiveControl;
	}

	double Dt = CurrentSimTime - DeadReckoningState.LastUpdateSimTime;
	if (Dt < 0.0) Dt = 0.0;

	float PredictedSteering = DeadReckoningState.PredictedSteering;

	float SteeringDrift = FMath::Clamp(
		ActiveControl.SteeringAngle - PredictedSteering, -0.3f, 0.3f);

	FControlSnapshot Result = ActiveControl;
	Result.SteeringAngle = PredictedSteering + SteeringDrift * FMath::Exp(-ConvergenceRate * Dt);

	return Result;
}

void UVehicleStateInterpolator::UpdateDeadReckoning(
	const FVector& CurrentPosition,
	const FVector& CurrentVelocity,
	const FVector& CurrentAcceleration,
	float CurrentSteering,
	double CurrentSimTime)
{
	double Dt = DeadReckoningState.bIsValid
		? (CurrentSimTime - DeadReckoningState.LastUpdateSimTime)
		: 0.0;

	if (Dt > KINDA_SMALL_NUMBER && DeadReckoningState.bIsValid)
	{
		FVector PredictedPos = DeadReckoningState.PredictedPosition +
			DeadReckoningState.PredictedVelocity * Dt +
			DeadReckoningState.PredictedAcceleration * 0.5 * Dt * Dt;

		FVector PredictionError = CurrentPosition - PredictedPos;
		float ErrorMag = PredictionError.Size();

		if (ErrorMag > LargeDeviationThreshold && bSnapOnLargeDeviation)
		{
			DeadReckoningState.PredictedPosition = CurrentPosition;
			DeadReckoningState.PredictedVelocity = CurrentVelocity;
			DeadReckoningState.PredictedAcceleration = CurrentAcceleration;
		}
		else
		{
			float ConvergenceFactor = 1.0f - FMath::Exp(-ConvergenceRate * Dt);
			DeadReckoningState.PredictedPosition = FMath::Lerp(
				PredictedPos, CurrentPosition, ConvergenceFactor);
			DeadReckoningState.PredictedVelocity = FMath::Lerp(
				DeadReckoningState.PredictedVelocity, CurrentVelocity, ConvergenceFactor);
			DeadReckoningState.PredictedAcceleration = FMath::Lerp(
				DeadReckoningState.PredictedAcceleration, CurrentAcceleration, ConvergenceFactor);
		}
	}
	else
	{
		DeadReckoningState.PredictedPosition = CurrentPosition;
		DeadReckoningState.PredictedVelocity = CurrentVelocity;
		DeadReckoningState.PredictedAcceleration = CurrentAcceleration;
	}

	DeadReckoningState.PredictedSteering = CurrentSteering;
	DeadReckoningState.LastUpdateSimTime = CurrentSimTime;
	DeadReckoningState.bIsValid = true;
}

void UVehicleStateInterpolator::DetectAndHandleDeviation(
	const FVector& CurrentPosition,
	const FVector& CurrentVelocity)
{
	if (!DeadReckoningState.bIsValid) return;

	FVector PredictionError = CurrentPosition - DeadReckoningState.PredictedPosition;
	float PosError = PredictionError.Size();

	float VelError = (CurrentVelocity - DeadReckoningState.PredictedVelocity).Size();

	if (PosError > MaxPositionDeviation || VelError > MaxVelocityDeviation)
	{
		FScopeLock Lock(&BufferMutex);
		if (bHasTargetControl)
		{
			InterpolationStartTime = GetWorld()->GetTimeSeconds();
			BlendAlpha = 0.0f;
		}
	}
}

FControlSnapshot UVehicleStateInterpolator::GetInterpolatedControl(double CurrentSimTime)
{
	FScopeLock Lock(&BufferMutex);

	if (ControlBuffer.Num() == 0)
	{
		if (DeadReckoningMode != EDeadReckoningMode::None)
		{
			return ComputeDeadReckoningControl(CurrentSimTime);
		}
		return ActiveControl;
	}

	if (ControlBuffer.Num() == 1)
	{
		return ControlBuffer[0].Control;
	}

	int32 FromIdx = 0;
	int32 ToIdx = 1;

	for (int32 i = 0; i < ControlBuffer.Num() - 1; ++i)
	{
		if (ControlBuffer[i].SimTime <= CurrentSimTime &&
			ControlBuffer[i + 1].SimTime > CurrentSimTime)
		{
			FromIdx = i;
			ToIdx = i + 1;
			break;
		}
		if (i == ControlBuffer.Num() - 2)
		{
			FromIdx = i;
			ToIdx = i + 1;
		}
	}

	FControlSnapshot Result;

	switch (InterpolationMode)
	{
	case EInterpolationMode::Immediate:
	{
		Result = ControlBuffer[FromIdx].Control;
		break;
	}
	case EInterpolationMode::LinearBlend:
	{
		double TimeRange = ControlBuffer[ToIdx].SimTime - ControlBuffer[FromIdx].SimTime;
		float Alpha = (TimeRange > KINDA_SMALL_NUMBER)
			? FMath::Clamp((CurrentSimTime - ControlBuffer[FromIdx].SimTime) / TimeRange, 0.0, 1.0)
			: 1.0f;

		Result = InterpolateLinear(ControlBuffer[FromIdx].Control, ControlBuffer[ToIdx].Control, Alpha);
		LastInterpolationAlpha = Alpha;
		break;
	}
	case EInterpolationMode::HermiteSpline:
	{
		double TimeRange = ControlBuffer[ToIdx].SimTime - ControlBuffer[FromIdx].SimTime;
		float Alpha = (TimeRange > KINDA_SMALL_NUMBER)
			? FMath::Clamp((CurrentSimTime - ControlBuffer[FromIdx].SimTime) / TimeRange, 0.0, 1.0)
			: 1.0f;

		Result = InterpolateHermite(ControlBuffer[FromIdx], ControlBuffer[ToIdx], Alpha);
		LastInterpolationAlpha = Alpha;
		break;
	}
	}

	if (DeadReckoningMode != EDeadReckoningMode::None && DeadReckoningState.bIsValid)
	{
		double TimeSinceLastUpdate = CurrentSimTime - DeadReckoningState.LastUpdateSimTime;
		if (TimeSinceLastUpdate > NetworkJitterBufferSeconds * 2.0)
		{
			FControlSnapshot DRControl = ComputeDeadReckoningControl(CurrentSimTime);
			float DRWeight = FMath::Clamp(
				(TimeSinceLastUpdate - NetworkJitterBufferSeconds * 2.0) / 0.5, 0.0f, 0.7f);

			Result.Throttle = FMath::Lerp(Result.Throttle, DRControl.Throttle, DRWeight);
			Result.Brake = FMath::Lerp(Result.Brake, DRControl.Brake, DRWeight);
			Result.SteeringAngle = FMath::Lerp(Result.SteeringAngle, DRControl.SteeringAngle, DRWeight);
		}
	}

	while (ControlBuffer.Num() > 2 && ControlBuffer[0].SimTime < CurrentSimTime - 1.0)
	{
		ControlBuffer.RemoveAt(0);
	}

	BufferedSnapshotCount = ControlBuffer.Num();

	SmoothedThrottle = Result.Throttle;
	SmoothedBrake = Result.Brake;
	SmoothedSteering = Result.SteeringAngle;

	ActiveControl = Result;
	return Result;
}

void UVehicleStateInterpolator::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}
