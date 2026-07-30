#include "MidanPIDController.h"

float FMidanPIDController::Update(float Error, float DeltaSeconds)
{
	if (DeltaSeconds <= 0.f)
	{
		return ProportionalGain * Error;
	}

	IntegralAccumulator = FMath::Clamp(IntegralAccumulator + Error * DeltaSeconds, -IntegralClamp, IntegralClamp);

	const float DerivativeTerm = bHasPreviousError ? (Error - PreviousError) / DeltaSeconds : 0.f;
	PreviousError = Error;
	bHasPreviousError = true;

	return (ProportionalGain * Error)
		+ (IntegralGain * IntegralAccumulator)
		+ (DerivativeGain * DerivativeTerm);
}

void FMidanPIDController::Reset()
{
	IntegralAccumulator = 0.f;
	PreviousError = 0.f;
	bHasPreviousError = false;
}
