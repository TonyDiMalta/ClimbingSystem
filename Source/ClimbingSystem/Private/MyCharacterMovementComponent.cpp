#include "MyCharacterMovementComponent.h"

#include "ECustomMovement.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"


UMyCharacterMovementComponent::UMyCharacterMovementComponent(const FObjectInitializer& objectInitializer)
	: Super(objectInitializer)
{
}

void UMyCharacterMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	AnimInstance = GetCharacterOwner()->GetMesh()->GetAnimInstance();
	
	ClimbQueryParams.AddIgnoredActor(GetOwner());
}

void UMyCharacterMovementComponent::TickComponent(float deltaTime, ELevelTick tickType, FActorComponentTickFunction* thisTickFunction)
{
	Super::TickComponent(deltaTime, tickType, thisTickFunction);

	SweepAndStoreWallHits();
}

void UMyCharacterMovementComponent::SweepAndStoreWallHits()
{
	const FCollisionShape collisionShape = FCollisionShape::MakeCapsule(CollisionCapsuleRadius, CollisionCapsuleHalfHeight);

	const FVector startOffset = UpdatedComponent->GetForwardVector() * 20;

	// Avoid using the same Start/End location for a Sweep, as it doesn't trigger hits on Landscapes.
	const FVector start = UpdatedComponent->GetComponentLocation() + startOffset;
	const FVector end = start + UpdatedComponent->GetForwardVector();

	TArray<FHitResult> hits;
	const bool hitWall = GetWorld()->SweepMultiByChannel(hits, start, end, FQuat::Identity,
		  ECC_WorldStatic, collisionShape, ClimbQueryParams);

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
		
		if (horizontalDegrees <= MinHorizontalDegreesToStartClimbing &&
			isCeiling == false && IsFacingSurface(verticalDot))
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
	
	return EyeHeightTrace(baseLength * steepnessMultiplier);
}

bool UMyCharacterMovementComponent::EyeHeightTrace(const float traceDistance) const
{
	FHitResult upperEdgeHit;

	const float baseEyeHeight = GetCharacterOwner()->BaseEyeHeight;
	const float eyeHeightOffset = IsClimbing() ? baseEyeHeight + ClimbingCollisionShrinkAmount : baseEyeHeight;
	
	const FVector start = UpdatedComponent->GetComponentLocation() + UpdatedComponent->GetUpVector() * eyeHeightOffset;
	const FVector end = start + (UpdatedComponent->GetForwardVector() * traceDistance);

	return GetWorld()->LineTraceSingleByChannel(upperEdgeHit, start, end, ECC_WorldStatic, ClimbQueryParams);
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

	const bool bWasClimbing = previousMovementMode == MOVE_Custom && previousCustomMode == CMOVE_Climbing;
	if (bWasClimbing)
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

	// Better to cache it when dash starts
	float minTime, maxTime;
	ClimbDashCurve->GetTimeRange(minTime, maxTime);
	
	if (CurrentClimbDashTime >= maxTime)
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

	UpdateClimbDashState(deltaTime);

	ComputeClimbingVelocity(deltaTime);

	const FVector oldLocation = UpdatedComponent->GetComponentLocation();
	
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
	
	for (const FHitResult& wallHit : CurrentWallHits)
	{
		const FVector end = start + (wallHit.ImpactPoint - start).GetSafeNormal() * 120;
		
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

	bWantsToClimb = false;
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

	const bool onWalkableFloor = floorHit.Normal.Z > GetWalkableFloorZ();
	
	const float downSpeed = FVector::DotProduct(Velocity, -floorHit.Normal);
	const bool isMovingTowardsFloor = downSpeed >= MaxClimbingSpeed / 3 && onWalkableFloor;
	
	const bool isClimbingFloor = CurrentClimbingNormal.Z > GetWalkableFloorZ();
	
	return isMovingTowardsFloor || (isClimbingFloor && onWalkableFloor);
}

bool UMyCharacterMovementComponent::CheckFloor(FHitResult& floorHit) const
{
	const FVector start = UpdatedComponent->GetComponentLocation() + (UpdatedComponent->GetUpVector() * - 20);
	const FVector end = start + FVector::DownVector * FloorCheckDistance;

	return GetWorld()->LineTraceSingleByChannel(floorHit, start, end, ECC_WorldStatic, ClimbQueryParams);
}

bool UMyCharacterMovementComponent::HasReachedEdge() const
{
	const UCapsuleComponent* capsule = CharacterOwner->GetCapsuleComponent();
	const float traceDistance = capsule->GetUnscaledCapsuleRadius() * 2.5f;

	return EyeHeightTrace(traceDistance) == false;
}

bool UMyCharacterMovementComponent::CanMoveToLedgeClimbLocation() const
{
	const UCapsuleComponent* capsule = CharacterOwner->GetCapsuleComponent();

	// Could use a property instead for fine-tuning.
	const FVector verticalOffset = FVector::UpVector * 160.f;
	const FVector horizontalOffset = UpdatedComponent->GetForwardVector() * 100.f;

	const FVector checkLocation = UpdatedComponent->GetComponentLocation() + horizontalOffset + verticalOffset;
	
	if (IsLocationWalkable(checkLocation) == false)
	{
		return false;
	}
	
	FHitResult capsuleHit;
	const FVector capsuleStartCheck = checkLocation - horizontalOffset;

	const bool isBlocked = GetWorld()->SweepSingleByChannel(capsuleHit, capsuleStartCheck, checkLocation,
		FQuat::Identity, ECC_WorldStatic, capsule->GetCollisionShape(), ClimbQueryParams);
	
	return isBlocked == false;
}

bool UMyCharacterMovementComponent::IsLocationWalkable(const FVector& checkLocation) const
{
	const FVector checkEnd = checkLocation + (FVector::DownVector * 250);

	FHitResult ledgeHit;
	const bool hasHitLedgeGround = GetWorld()->LineTraceSingleByChannel(ledgeHit, checkLocation, checkEnd,
	                                                                  ECC_WorldStatic, ClimbQueryParams);

	return hasHitLedgeGround && ledgeHit.Normal.Z >= GetWalkableFloorZ();
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
			Velocity = ClimbDashDirection * currentCurveSpeed;
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
	
	ClimbDashDirection = FVector::VectorPlaneProject(ClimbDashDirection, horizontalSurfaceNormal);
}

void UMyCharacterMovementComponent::StopClimbDashing()
{
	bIsClimbDashing = false;
	CurrentClimbDashTime = 0.f;
	ClimbDashDirection = FVector::ZeroVector;
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
	
	SafeMoveUpdatedComponent(adjusted, GetClimbingRotation(deltaTime), true, hit);
	
	if (hit.Time < 1.f)
	{
		HandleImpact(hit, deltaTime, adjusted);
		constexpr bool handleImpact = true;
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

bool UMyCharacterMovementComponent::TryClimbUpLedge() const
{
	if (AnimInstance && AnimInstance->Montage_IsPlaying(LedgeClimbMontage))
	{
		return false;
	}
	
	const float upSpeed = FVector::DotProduct(Velocity, UpdatedComponent->GetUpVector());
	const bool isMovingUp = upSpeed >= MaxClimbingSpeed / 3;
	
	if (isMovingUp && HasReachedEdge() && CanMoveToLedgeClimbLocation())
	{
		SetRotationToStand();
		
		AnimInstance->Montage_Play(LedgeClimbMontage);
		
		return true;
	}
	
	return false;
}

void UMyCharacterMovementComponent::SnapToClimbingSurface(float deltaTime) const
{
	const FVector forward = UpdatedComponent->GetForwardVector();
	const FVector location = UpdatedComponent->GetComponentLocation();
	const FQuat rotation = UpdatedComponent->GetComponentQuat();
	
	const FVector forwardDifference = (CurrentClimbingPosition - location).ProjectOnTo(forward);
	
	const FVector offset = -CurrentClimbingNormal * (forwardDifference.Length() - DistanceFromSurface);

	constexpr bool sweep = true;

	const float SnapSpeed = ClimbingSnapSpeed * ((Velocity.Length() / MaxClimbingSpeed) + 1);
	UpdatedComponent->MoveComponent(offset * SnapSpeed * deltaTime, rotation, sweep);
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
	if (ClimbDashCurve && bIsClimbDashing == false)
	{
		bIsClimbDashing = true;
		CurrentClimbDashTime = 0.f;
		
		StoreClimbDashDirection();
	}
}

void UMyCharacterMovementComponent::StoreClimbDashDirection()
{
	ClimbDashDirection = UpdatedComponent->GetUpVector();

	const float accelerationThreshold = MaxClimbingAcceleration / 10;
	if (Acceleration.Length() > accelerationThreshold)
	{
		ClimbDashDirection = Acceleration.GetSafeNormal();
	}
}

void UMyCharacterMovementComponent::CancelClimbing()
{
	bWantsToClimb = false;
}

FVector UMyCharacterMovementComponent::GetClimbSurfaceNormal() const
{
	return CurrentClimbingNormal;
}

FVector UMyCharacterMovementComponent::GetClimbDashDirection() const
{
	return ClimbDashDirection;
}

bool UMyCharacterMovementComponent::IsClimbing() const
{
	return MovementMode == EMovementMode::MOVE_Custom && CustomMovementMode == ECustomMovementMode::CMOVE_Climbing;
}

bool UMyCharacterMovementComponent::IsClimbDashing() const
{
	return IsClimbing() && bIsClimbDashing;
}
