// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ClimbingSystemCharacter.generated.h"

class UMyCharacterMovementComponent;
UCLASS(config=Game)
class AClimbingSystemCharacter : public ACharacter
{
	GENERATED_BODY()
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class UCameraComponent* FollowCamera;

	UFUNCTION(BlueprintPure)
	FORCEINLINE UMyCharacterMovementComponent* GetCustomCharacterMovement() const { return MovementComponent; }
	
public:
	AClimbingSystemCharacter(const FObjectInitializer& objectInitializer);
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Input)
	float TurnRateGamepad;

private:
	UPROPERTY()
	UMyCharacterMovementComponent* MovementComponent;

protected:
	void Climb();

	void CancelClimb();
	
	void MoveForward(float value);
	
	void MoveRight(float value);
	
	FRotationMatrix GetControlOrientationMatrix() const;

	virtual void Jump() override;
	
	void TurnAtRate(float rate);
	
	void LookUpAtRate(float rate);

protected:
	virtual void SetupPlayerInputComponent(class UInputComponent* playerInputComponent) override;

public:
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};

