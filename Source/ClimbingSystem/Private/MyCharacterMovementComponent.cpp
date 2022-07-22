#include "MyCharacterMovementComponent.h"

#include "ECustomMovement.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"

void UMyCharacterMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	AnimInstance = GetCharacterOwner()->GetMesh()->GetAnimInstance();

	ClimbQueryParams.AddIgnoredActor(GetOwner());

	if (ClimbDashCurve)
	{
		float minTime;
		ClimbDashCurve->GetTimeRange(minTime, ClimbDashDuration);
	}
}

void UMyCharacterMovementComponent::TickComponent(float deltaTime, ELevelTick tickType, FActorComponentTickFunction* thisTickFunction)
{
	Super::TickComponent(deltaTime, tickType, thisTickFunction);

	SweepAndStoreWallHits();
}

void UMyCharacterMovementComponent::SweepAndStoreWallHits()
{
	const FCollisionShape collisionShape = FCollisionShape::MakeCapsule(CollisionCapsuleRadius, CollisionCapsuleHalfHeight);

	const FVector startOffset = UpdatedComponent->GetForwardVector() * DistanceFromSurface;
	const FVector endOffset = (CurrentClimbingNormal.IsZero() ? UpdatedComponent->GetForwardVector() : -CurrentClimbingNormal) * FMath::Max(CollisionCapsuleRadius, CollisionCapsuleHalfHeight);

	// Avoid using the same start/end location for a Sweep, as it doesn't trigger hits on Landscapes or stick to walls past ramps.
	const FVector start = UpdatedComponent->GetComponentLocation() + startOffset;
	const FVector end = start + endOffset;

	TArray<FHitResult> hits;
	const bool hitWall = GetWorld()->SweepMultiByChannel(hits, start, end, FQuat::Identity,
		ECC_WorldStatic, collisionShape, ClimbQueryParams);

#if 0
	DrawDebugCapsule(GetWorld(), start, collisionShape.GetCapsuleHalfHeight(), collisionShape.GetCapsuleRadius(), UpdatedComponent->GetComponentQuat(), FColor::Emerald, false, -1.0f, 0U, 2.f);
#endif

	hitWall ? CurrentWallHits = hits : CurrentWallHits.Reset();
}

bool UMyCharacterMovementComponent::CanStartClimbing()
{
	for (FHitResult& hit : CurrentWallHits)
	{
		const FVector horizontalNormal = hit.Normal.GetSafeNormal2D();

		const float horizontalDot = FVector::DotProduct(UpdatedComponent->GetForwardVector(), -horizontalNormal);
		const float verticalDot = FVector::DotProduct(hit.Normal, horizontalNormal);

		const float horizontalDegrees = FMath::RadiansToDegrees(FMath::Acos(horizontalDot));

		const bool isCeiling = FMath::IsNearlyZero(verticalDot);
		const bool onWalkableFloor = IsWalkable(hit);

		if (horizontalDegrees <= MinHorizontalDegreesToStartClimbing &&
			isCeiling == false && onWalkableFloor == false && IsFacingSurface(verticalDot))
		{
			return true;
		}
	}

	return false;
}

bool UMyCharacterMovementComponent::IsFacingSurface(const float steepness) const
{
	constexpr float baseLength = 80;
	const float steepnessMultiplier = 1 + (1 - steepness) * 5;

	FHitResult surfaceHit;
	return EyeHeightTrace(surfaceHit, baseLength * steepnessMultiplier);
}

bool UMyCharacterMovementComponent::EyeHeightTrace(FHitResult& outHit, const float traceDistance, const float heightOffset) const
{
	const float baseEyeHeight = CharacterOwner->BaseEyeHeight;
	const float eyeHeightOffset = IsClimbing() ? baseEyeHeight + ClimbingCollisionShrinkAmount + heightOffset : baseEyeHeight;

	const FVector start = UpdatedComponent->GetComponentLocation() + UpdatedComponent->GetUpVector() * eyeHeightOffset;
	const FVector end = start + (UpdatedComponent->GetForwardVector() * traceDistance);

	return GetWorld()->LineTraceSingleByChannel(outHit, start, end, ECC_WorldStatic, ClimbQueryParams);
}

void UMyCharacterMovementComponent::OnMovementUpdated(float deltaTime, const FVector& oldLocation, const FVector& oldVelocity)
{
	if (bWantsToClimb)
	{
		SetMovementMode(EMovementMode::MOVE_Custom, ECustomMovementMode::CMOVE_Climbing);
	}

	Super::OnMovementUpdated(deltaTime, oldLocation, oldVelocity);
}

void UMyCharacterMovementComponent::OnMovementModeChanged(EMovementMode previousMovementMode, uint8 previousCustomMode)
{
	if (IsClimbing())
	{
		bOrientRotationToMovement = false;

		UCapsuleComponent* capsule = CharacterOwner->GetCapsuleComponent();
		capsule->SetCapsuleHalfHeight(capsule->GetUnscaledCapsuleHalfHeight() - ClimbingCollisionShrinkAmount);
	}

	const bool wasClimbing = previousMovementMode == MOVE_Custom && previousCustomMode == CMOVE_Climbing;
	if (wasClimbing)
	{
		bOrientRotationToMovement = true;

		SetRotationToStand();

		UCapsuleComponent* capsule = CharacterOwner->GetCapsuleComponent();
		capsule->SetCapsuleHalfHeight(capsule->GetUnscaledCapsuleHalfHeight() + ClimbingCollisionShrinkAmount);

		StopMovementImmediately();
	}

	Super::OnMovementModeChanged(previousMovementMode, previousCustomMode);
}

void UMyCharacterMovementComponent::SetRotationToStand() const
{
	const FRotator standRotation = FRotator(0, UpdatedComponent->GetComponentRotation().Yaw, 0);
	UpdatedComponent->SetRelativeRotation(standRotation);
}

void UMyCharacterMovementComponent::PhysCustom(float deltaTime, int32 iterations)
{
	if (CustomMovementMode == ECustomMovementMode::CMOVE_Climbing)
	{
		PhysClimbing(deltaTime, iterations);
	}

	Super::PhysCustom(deltaTime, iterations);
}

void UMyCharacterMovementComponent::UpdateClimbDashState(float deltaTime)
{
	if (bIsClimbDashing == false)
	{
		return;
	}

	CurrentClimbDashTime += deltaTime;

	if (CurrentClimbDashTime >= ClimbDashDuration)
	{
		StopClimbDashing();
	}
}

void UMyCharacterMovementComponent::PhysClimbing(float deltaTime, int32 iterations)
{
	if (deltaTime < MIN_TICK_TIME)
	{
		return;
	}

	ComputeSurfaceInfo();

	if (ShouldStopClimbing() || ClimbDownToFloor())
	{
		StopClimbing(deltaTime, iterations);
		return;
	}

	const FVector oldLocation = UpdatedComponent->GetComponentLocation();
	if (bIsClimbDashing == false && bIsClimbingLedge == false)
	{
		CurrentClimbingDirection = Acceleration.GetSafeNormal();
	}

	UpdateClimbDashState(deltaTime);

	ComputeClimbingVelocity(deltaTime);

	MoveAlongClimbingSurface(deltaTime);

	TryClimbUpLedge();

	if (HasAnimRootMotion() == false && CurrentRootMotion.HasOverrideVelocity() == false)
	{
		Velocity = (UpdatedComponent->GetComponentLocation() - oldLocation) / deltaTime;
	}

	SnapToClimbingSurface(deltaTime);
}

void UMyCharacterMovementComponent::ComputeSurfaceInfo()
{
	CurrentClimbingNormal = FVector::ZeroVector;
	CurrentClimbingPosition = FVector::ZeroVector;

	if (CurrentWallHits.IsEmpty())
	{
		return;
	}

	const FVector start = UpdatedComponent->GetComponentLocation();
	const FCollisionShape collisionSphere = FCollisionShape::MakeSphere(6);
	constexpr float sweepDistance = 120;

	for (const FHitResult& wallHit : CurrentWallHits)
	{
		const FVector end = start + (wallHit.ImpactPoint - start).GetSafeNormal() * sweepDistance;

		FHitResult AssistHit;
		GetWorld()->SweepSingleByChannel(AssistHit, start, end, FQuat::Identity,
			ECC_WorldStatic, collisionSphere, ClimbQueryParams);

		CurrentClimbingPosition += AssistHit.Location;
		CurrentClimbingNormal += AssistHit.Normal;
	}

	CurrentClimbingPosition /= CurrentWallHits.Num();
	CurrentClimbingNormal = CurrentClimbingNormal.GetSafeNormal();
}

bool UMyCharacterMovementComponent::ShouldStopClimbing() const
{
	const bool isOnCeiling = FVector::Parallel(CurrentClimbingNormal, FVector::UpVector);

	return bWantsToClimb == false || CurrentClimbingNormal.IsZero() || isOnCeiling;
}

void UMyCharacterMovementComponent::StopClimbing(float deltaTime, int32 iterations)
{
	StopClimbDashing();
	StopClimbUpLedge();

	bWantsToClimb = false;
	CurrentClimbingNormal = FVector::ZeroVector;
	CurrentClimbingPosition = FVector::ZeroVector;
	CurrentClimbingDirection = FVector::ZeroVector;
	TargetLedgePosition = FVector::ZeroVector;
	SetMovementMode(EMovementMode::MOVE_Falling);
	StartNewPhysics(deltaTime, iterations);
}

bool UMyCharacterMovementComponent::ClimbDownToFloor() const
{
	FHitResult floorHit;
	if (CheckFloor(floorHit) == false)
	{
		return false;
	}

	const bool onWalkableFloor = IsWalkable(floorHit);

	const float downAcceleration = FVector::DotProduct(CurrentClimbingDirection, -floorHit.Normal);
	const bool isMovingTowardsFloor = downAcceleration > 0.0f && onWalkableFloor;

	const bool isClimbingFloor = CurrentClimbingNormal.Z >= GetWalkableFloorZ();

	return isMovingTowardsFloor || (isClimbingFloor && onWalkableFloor);
}

bool UMyCharacterMovementComponent::CheckFloor(FHitResult& floorHit) const
{
	const FVector start = UpdatedComponent->GetComponentLocation() + (UpdatedComponent->GetUpVector() * -20);
	const FVector end = start + FVector::DownVector * FloorCheckDistance;

	return GetWorld()->LineTraceSingleByChannel(floorHit, start, end, ECC_WorldStatic, ClimbQueryParams);
}

bool UMyCharacterMovementComponent::HasReachedEdge() const
{
	const UCapsuleComponent* capsule = CharacterOwner->GetCapsuleComponent();
	const float traceDistance = capsule->GetUnscaledCapsuleRadius() * 2 + DistanceFromSurface;

	FHitResult surfaceHit;
	return EyeHeightTrace(surfaceHit, traceDistance, LedgeEyeHeightOffset) == false || IsWalkable(surfaceHit);
}

bool UMyCharacterMovementComponent::CanMoveToLedgeClimbLocation()
{
	const UCapsuleComponent* capsule = CharacterOwner->GetCapsuleComponent();
	const float baseEyeHeight = CharacterOwner->BaseEyeHeight;
	const float eyeHeightOffset = IsClimbing() ? baseEyeHeight + ClimbingCollisionShrinkAmount + LedgeEyeHeightOffset : baseEyeHeight;

	// take steepness into account, the eyes will be closer to the ledge on a ramp than a wall as the collider is rotated
	const FVector forward = UpdatedComponent->GetForwardVector();
	const float upZAxis = UpdatedComponent->GetUpVector().Z;
	// the more forwardZAxis tends towards 0 (facing a wall), the more we need to adjust horizontally based on the capsule radius
	const float wallDistance = (1.f - FMath::Abs(forward.Z)) * (capsule->GetUnscaledCapsuleRadius() * 2 + DistanceFromSurface);
	// the more forwardZAxis tends towards -1, the more we need to adjust horizontally based on the capsule height (to compensate for the steepness)
	const float steepCorrection = -forward.Z * (capsule->GetUnscaledCapsuleHalfHeight() + eyeHeightOffset) * (forward.Z <= 0.f ? 1.f : 0.25f);
	const float distanceToClimbLedge = wallDistance + steepCorrection;

	const FVector horizontalOffset = FVector(forward.X * distanceToClimbLedge, forward.Y * distanceToClimbLedge, 0.);
	const FVector verticalOffset = FVector::UpVector * (capsule->GetUnscaledCapsuleHalfHeight() + eyeHeightOffset * upZAxis);

	TargetLedgePosition = UpdatedComponent->GetComponentLocation() + horizontalOffset + verticalOffset;

	if (IsLocationWalkable(TargetLedgePosition) == false)
	{
		return false;
	}

	FHitResult capsuleHit;
	const FVector capsuleStartCheck = TargetLedgePosition - horizontalOffset;

	const bool isBlocked = GetWorld()->SweepSingleByChannel(capsuleHit, capsuleStartCheck, TargetLedgePosition,
		FQuat::Identity, ECC_WorldStatic, capsule->GetCollisionShape(), ClimbQueryParams);

	return isBlocked == false || IsWalkable(capsuleHit);
}

bool UMyCharacterMovementComponent::IsLocationWalkable(const FVector& checkLocation) const
{
	const UCapsuleComponent* capsule = CharacterOwner->GetCapsuleComponent();

	const FVector checkEnd = checkLocation + (FVector::DownVector * capsule->GetUnscaledCapsuleHalfHeight() * 1.5f);

	FHitResult ledgeHit;
	GetWorld()->LineTraceSingleByChannel(ledgeHit, checkLocation, checkEnd,
		ECC_WorldStatic, ClimbQueryParams);

#if 0
	DrawDebugDirectionalArrow(GetWorld(), checkLocation, checkEnd, 4.f, FColor::Purple, true, -1.0f, 0U, 2.f);
#endif

	return IsWalkable(ledgeHit);
}

void UMyCharacterMovementComponent::ComputeClimbingVelocity(float deltaTime)
{
	RestorePreAdditiveRootMotionVelocity();

	if (HasAnimRootMotion() == false && CurrentRootMotion.HasOverrideVelocity() == false)
	{
		if (bIsClimbDashing)
		{
			AlignClimbDashDirection();

			const float currentCurveSpeed = ClimbDashCurve->GetFloatValue(CurrentClimbDashTime);
			Velocity = CurrentClimbingDirection * currentCurveSpeed;
		}
		else
		{
			constexpr float friction = 0.0f;
			constexpr bool fluid = false;
			CalcVelocity(deltaTime, friction, fluid, BrakingDecelerationClimbing);
		}
	}

	ApplyRootMotionToVelocity(deltaTime);
}


void UMyCharacterMovementComponent::AlignClimbDashDirection()
{
	const FVector horizontalSurfaceNormal = GetClimbSurfaceNormal();

	CurrentClimbingDirection = FVector::VectorPlaneProject(CurrentClimbingDirection, horizontalSurfaceNormal);
}

void UMyCharacterMovementComponent::StopClimbDashing()
{
	// Dirty hack to fix the dash animations offset (not in place)
#if 0
	if (bIsClimbDashing && bIsClimbingLedge == false)
	{
		FHitResult hit(1.f);
		constexpr bool sweep = true;
		SafeMoveUpdatedComponent(CurrentClimbingDirection * 40.f, UpdatedComponent->GetComponentQuat(), sweep, hit);
	}
#endif

	bIsClimbDashing = false;
	CurrentClimbDashTime = 0.f;
}

float UMyCharacterMovementComponent::GetMaxSpeed() const
{
	return IsClimbing() ? MaxClimbingSpeed : Super::GetMaxSpeed();
}

float UMyCharacterMovementComponent::GetMaxAcceleration() const
{
	return IsClimbing() ? MaxClimbingAcceleration : Super::GetMaxAcceleration();
}

void UMyCharacterMovementComponent::MoveAlongClimbingSurface(float deltaTime)
{
	const FVector adjusted = Velocity * deltaTime;

	FHitResult hit(1.f);
	constexpr bool sweep = true;

	SafeMoveUpdatedComponent(adjusted, GetClimbingRotation(deltaTime), sweep, hit);

	if (hit.Time < 1.f)
	{
		constexpr bool handleImpact = true;
		HandleImpact(hit, deltaTime, adjusted);
		SlideAlongSurface(adjusted, (1.f - hit.Time), hit.Normal, hit, handleImpact);
	}
}

FQuat UMyCharacterMovementComponent::GetClimbingRotation(float deltaTime) const
{
	const FQuat current = UpdatedComponent->GetComponentQuat();

	if (HasAnimRootMotion() || CurrentRootMotion.HasOverrideVelocity())
	{
		return current;
	}

	const FQuat target = FRotationMatrix::MakeFromX(-CurrentClimbingNormal).ToQuat();
	const float rotationSpeed = ClimbingRotationSpeed * FMath::Max(1, Velocity.Length() / MaxClimbingSpeed);

	return FMath::QInterpTo(current, target, deltaTime, rotationSpeed);
}

bool UMyCharacterMovementComponent::TryClimbUpLedge()
{
	if (bIsClimbingLedge)
	{
		// Finished climbing up the ledge, let's move the component (work around for animation root motion not working).
		if (AnimInstance->Montage_IsPlaying(LedgeClimbMontage) == false)
		{
			UpdatedComponent->SetWorldLocation(TargetLedgePosition);
			bIsClimbingLedge = false;
		}

		return false;
	}

	const float upAcceleration = FVector::DotProduct(CurrentClimbingDirection, UpdatedComponent->GetUpVector());
	const bool isMovingUp = upAcceleration > 0.0f;

	if (isMovingUp && HasReachedEdge() && CanMoveToLedgeClimbLocation())
	{
		bIsClimbingLedge = true;
		StopClimbDashing();

		SetRotationToStand();
		AnimInstance->Montage_Play(LedgeClimbMontage);

		return true;
	}

	return false;
}

void UMyCharacterMovementComponent::StopClimbUpLedge()
{
	if (bIsClimbingLedge)
	{
		AnimInstance->Montage_Stop(0.f, LedgeClimbMontage);
		TargetLedgePosition = FVector::ZeroVector;
		bIsClimbingLedge = false;
	}
}

void UMyCharacterMovementComponent::SnapToClimbingSurface(float deltaTime) const
{
	const FVector forward = UpdatedComponent->GetForwardVector();
	const FVector location = UpdatedComponent->GetComponentLocation();
	const FQuat rotation = UpdatedComponent->GetComponentQuat();

	const FVector forwardDifference = (CurrentClimbingPosition - location).ProjectOnTo(forward);

	const FVector offset = -CurrentClimbingNormal * (forwardDifference.Length() - DistanceFromSurface);

	constexpr bool sweep = true;

	const float snapSpeed = ClimbingSnapSpeed * FMath::Max(1, Velocity.Length() / MaxClimbingSpeed);
	UpdatedComponent->MoveComponent(offset * snapSpeed * deltaTime, rotation, sweep);
}

void UMyCharacterMovementComponent::TryClimbing()
{
	if (CanStartClimbing())
	{
		bWantsToClimb = true;
	}
}

void UMyCharacterMovementComponent::TryClimbDashing()
{
	if (ClimbDashDuration > 0.f && IsClimbing() && bIsClimbDashing == false && bIsClimbingLedge == false)
	{
		bIsClimbDashing = true;
		CurrentClimbDashTime = 0.f;

		StoreClimbDashDirection();
	}
}

void UMyCharacterMovementComponent::StoreClimbDashDirection()
{
	CurrentClimbingDirection = UpdatedComponent->GetUpVector();

	const float accelerationThreshold = MaxClimbingAcceleration / 10;
	if (Acceleration.Length() > accelerationThreshold)
	{
		CurrentClimbingDirection = Acceleration.GetSafeNormal();
	}
}

void UMyCharacterMovementComponent::CancelClimbing(bool ignoreAnimations)
{
	if (ignoreAnimations == false && (bIsClimbDashing || bIsClimbingLedge))
	{
		return;
	}

	bWantsToClimb = false;
}

FVector UMyCharacterMovementComponent::GetClimbSurfaceNormal() const
{
	return CurrentClimbingNormal;
}

FVector UMyCharacterMovementComponent::GetClimbingDirection() const
{
	return CurrentClimbingDirection;
}

bool UMyCharacterMovementComponent::IsClimbing() const
{
	return MovementMode == EMovementMode::MOVE_Custom && CustomMovementMode == ECustomMovementMode::CMOVE_Climbing;
}

bool UMyCharacterMovementComponent::IsClimbDashing() const
{
	return IsClimbing() && bIsClimbDashing;
}

bool UMyCharacterMovementComponent::IsClimbingLedge() const
{
	return IsClimbing() && bIsClimbingLedge;
}