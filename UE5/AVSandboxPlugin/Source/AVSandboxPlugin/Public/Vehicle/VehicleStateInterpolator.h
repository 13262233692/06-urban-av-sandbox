#pragma once

#include "CoreMinimal.h"
#include "Vehicle/VehicleControlProtocol.h"
#include "VehicleStateInterpolator.generated.h"

UENUM(BlueprintType)
enum class EInterpolationMode : uint8
{
	Immediate,
	LinearBlend,
	HermiteSpline
};

UENUM(BlueprintType)
enum class EDeadReckoningMode : uint8
{
	None,
	Kinematic,
	PhysicsBased
};

USTRUCT(BlueprintType)
struct FControlSnapshot
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere) float Throttle = 0.0f;
	UPROPERTY(VisibleAnywhere) float Brake = 0.0f;
	UPROPERTY(VisibleAnywhere) float SteeringAngle = 0.0f;
	UPROPERTY(VisibleAnywhere) int8 Gear = 0;
	UPROPERTY(VisibleAnywhere) uint8 Handbrake = 0;
	UPROPERTY(VisibleAnywhere) double SimTime = 0.0;
	UPROPERTY(VisibleAnywhere) uint16 SequenceNumber = 0;
};

USTRUCT(BlueprintType)
struct FHermiteControlPoint
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere) FControlSnapshot Control;
	UPROPERTY(VisibleAnywhere) FVector VelocityAtPoint = FVector::ZeroVector;
	UPROPERTY(VisibleAnywhere) FVector AccelerationAtPoint = FVector::ZeroVector;
	UPROPERTY(VisibleAnywhere) double SimTime = 0.0;

	FVector Position2D() const
	{
		return FVector(Control.SteeringAngle, Control.Throttle, Control.Brake);
	}
};

USTRUCT(BlueprintType)
struct FDeadReckoningState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere) FVector PredictedPosition = FVector::ZeroVector;
	UPROPERTY(VisibleAnywhere) FVector PredictedVelocity = FVector::ZeroVector;
	UPROPERTY(VisibleAnywhere) FVector PredictedAcceleration = FVector::ZeroVector;
	UPROPERTY(VisibleAnywhere) float PredictedSteering = 0.0f;
	UPROPERTY(VisibleAnywhere) double LastUpdateSimTime = 0.0;
	UPROPERTY(VisibleAnywhere) bool bIsValid = false;
};

UCLASS(BlueprintType, ClassGroup = (AVSandbox), meta = (BlueprintSpawnableComponent))
class UVehicleStateInterpolator : public UActorComponent
{
	GENERATED_BODY()

public:
	UVehicleStateInterpolator();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "AVSandbox|Interpolator")
	void EnqueueControl(const FVehicleControlMessage& Control, double CurrentSimTime);

	UFUNCTION(BlueprintCallable, Category = "AVSandbox|Interpolator")
	FControlSnapshot GetInterpolatedControl(double CurrentSimTime);

	UFUNCTION(BlueprintCallable, Category = "AVSandbox|Interpolator")
	FDeadReckoningState GetDeadReckoningState() const { return DeadReckoningState; }

	UFUNCTION(BlueprintCallable, Category = "AVSandbox|Interpolator")
	void SetTimeDilation(float Dilation) { TimeDilation = FMath::Max(Dilation, 0.1f); }

	UFUNCTION(BlueprintCallable, Category = "AVSandbox|Interpolator")
	float GetTimeDilation() const { return TimeDilation; }

	UFUNCTION(BlueprintCallable, Category = "AVSandbox|Interpolator")
	void SetInterpolationMode(EInterpolationMode Mode) { InterpolationMode = Mode; }

	UFUNCTION(BlueprintCallable, Category = "AVSandbox|Interpolator")
	void SetDeadReckoningMode(EDeadReckoningMode Mode) { DeadReckoningMode = Mode; }

	UFUNCTION(BlueprintCallable, Category = "AVSandbox|Interpolator")
	void Reset();

	UFUNCTION(BlueprintCallable, Category = "AVSandbox|Interpolator")
	void UpdateDeadReckoning(
		const FVector& CurrentPosition,
		const FVector& CurrentVelocity,
		const FVector& CurrentAcceleration,
		float CurrentSteering,
		double CurrentSimTime);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|Interpolator")
	EInterpolationMode InterpolationMode = EInterpolationMode::HermiteSpline;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|Interpolator")
	EDeadReckoningMode DeadReckoningMode = EDeadReckoningMode::PhysicsBased;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|Interpolator")
	float TimeDilation = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|Interpolator")
	float BlendDuration = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|Interpolator")
	float MaxPositionDeviation = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|Interpolator")
	float MaxVelocityDeviation = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|Interpolator")
	float HermiteTension = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|Interpolator")
	float HermiteBias = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|Interpolator")
	int32 MaxBufferedSnapshots = 64;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|Interpolator")
	float NetworkJitterBufferSeconds = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|Interpolator")
	float ConvergenceRate = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|Interpolator")
	bool bSnapOnLargeDeviation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AVSandbox|Interpolator")
	float LargeDeviationThreshold = 300.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AVSandbox|Interpolator")
	int32 BufferedSnapshotCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AVSandbox|Interpolator")
	int32 DroppedOutOfOrderCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AVSandbox|Interpolator")
	float LastInterpolationAlpha = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AVSandbox|Interpolator")
	float SmoothedThrottle = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AVSandbox|Interpolator")
	float SmoothedBrake = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AVSandbox|Interpolator")
	float SmoothedSteering = 0.0f;

private:
	TArray<FHermiteControlPoint> ControlBuffer;

	FDeadReckoningState DeadReckoningState;

	FControlSnapshot PrevAppliedControl;
	FControlSnapshot TargetControl;
	FControlSnapshot ActiveControl;

	double LastReceivedSimTime;
	double InterpolationStartTime;
	bool bHasPrevControl;
	bool bHasTargetControl;
	float BlendAlpha;

	FCriticalSection BufferMutex;

	FControlSnapshot InterpolateHermite(
		const FHermiteControlPoint& P0,
		const FHermiteControlPoint& P1,
		float Alpha) const;

	FControlSnapshot InterpolateLinear(
		const FControlSnapshot& From,
		const FControlSnapshot& To,
		float Alpha) const;

	FControlSnapshot ComputeDeadReckoningControl(double CurrentSimTime) const;

	float HermiteInterpValue(
		float Y0, float Y1, float Y2, float Y3,
		float Alpha, float Tension, float Bias) const;

	void TrimBuffer();

	void DetectAndHandleDeviation(
		const FVector& CurrentPosition,
		const FVector& CurrentVelocity);
};
