#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "MyCharacterMovementComponent.generated.h"

UCLASS()
class CLIMBINGSYSTEM_API UMyCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure)
	bool IsClimbing() const;

	UFUNCTION(BlueprintPure)
	bool IsClimbDashing() const;

	UFUNCTION(BlueprintPure)
	bool IsClimbingLedge() const;

	UFUNCTION(BlueprintPure)
	FVector GetClimbSurfaceNormal() const;

	UFUNCTION(BlueprintPure)
	FVector GetClimbingDirection() const;

	UFUNCTION(BlueprintCallable)
	void TryClimbing();

	UFUNCTION(BlueprintCallable)
	void TryClimbDashing();

	UFUNCTION(BlueprintCallable)
	void CancelClimbing(bool ignoreAnimations = false);

private:
	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere)
	int CollisionCapsuleRadius = 50;

	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere)
	int CollisionCapsuleHalfHeight = 72;

	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere, meta = (ClampMin = "10.0", ClampMax = "500.0"))
	float MaxClimbingSpeed = 120;

	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere, meta = (ClampMin = "10.0", ClampMax = "2000.0"))
	float MaxClimbingAcceleration = 380;

	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "3000.0"))
	float BrakingDecelerationClimbing = 550.f;

	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "60.0"))
	float ClimbingSnapSpeed = 4.f;

	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "80.0"))
	float DistanceFromSurface = 45.f;

	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere, meta = (ClampMin = "1.0", ClampMax = "60.0"))
	int ClimbingRotationSpeed = 5;

	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "80.0"))
	float ClimbingCollisionShrinkAmount = 30;

	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere, meta = (ClampMin = "1.0", ClampMax = "500.0"))
	float FloorCheckDistance = 90.f;

	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere, meta = (ClampMin = "1.0", ClampMax = "75.0"))
	float MinHorizontalDegreesToStartClimbing = 25;

	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "120.0"))
	float LedgeEyeHeightOffset = 60.f;

	UPROPERTY(Category = "Character Movement: Climbing", EditDefaultsOnly)
	UAnimMontage* LedgeClimbMontage;

	UPROPERTY(Category = "Character Movement: Climbing", EditDefaultsOnly)
	UCurveFloat* ClimbDashCurve;

	UPROPERTY()
	UAnimInstance* AnimInstance;

	TArray<FHitResult> CurrentWallHits;

	FCollisionQueryParams ClimbQueryParams;

	bool bWantsToClimb = false;

	bool bIsClimbDashing = false;

	bool bIsClimbingLedge = false;

	float CurrentClimbDashTime = 0.f;

	float ClimbDashDuration = 0.f;

	FVector CurrentClimbingNormal = FVector::ZeroVector;

	FVector CurrentClimbingPosition = FVector::ZeroVector;

	FVector CurrentClimbingDirection = FVector::ZeroVector;

	FVector TargetLedgePosition = FVector::ZeroVector;

private:
	virtual void BeginPlay() override;

	virtual void TickComponent(float deltaTime, ELevelTick tickType, FActorComponentTickFunction* thisTickFunction) override;

	virtual void OnMovementUpdated(float deltaTime, const FVector& oldLocation, const FVector& oldVelocity) override;

	virtual void OnMovementModeChanged(EMovementMode previousMovementMode, uint8 previousCustomMode) override;

	virtual float GetMaxSpeed() const override;

	virtual float GetMaxAcceleration() const override;

	virtual void PhysCustom(float deltaTime, int32 iterations) override;

	bool EyeHeightTrace(FHitResult& outHit, const float traceDistance, const float heightOffset = 0.f) const;

	bool ShouldStopClimbing() const;

	bool ClimbDownToFloor() const;

	bool CheckFloor(FHitResult& floorHit) const;

	bool TryClimbUpLedge();

	bool HasReachedEdge() const;

	bool IsLocationWalkable(const FVector& checkLocation) const;

	bool CanMoveToLedgeClimbLocation();

	bool CanStartClimbing();

	bool IsFacingSurface(float steepness) const;

	FQuat GetClimbingRotation(float deltaTime) const;

	void UpdateClimbDashState(float deltaTime);

	void PhysClimbing(float deltaTime, int32 iterations);

	void SetRotationToStand() const;

	void StopClimbing(float deltaTime, int32 iterations);

	void AlignClimbDashDirection();

	void StoreClimbDashDirection();

	void StopClimbDashing();

	void StopClimbUpLedge();

	void ComputeClimbingVelocity(float deltaTime);

	void MoveAlongClimbingSurface(float deltaTime);

	void SnapToClimbingSurface(float deltaTime) const;

	void ComputeSurfaceInfo();

	void SweepAndStoreWallHits();
};