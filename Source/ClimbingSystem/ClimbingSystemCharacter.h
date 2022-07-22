// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ClimbingSystemCharacter.generated.h"

UCLASS(config=Game)
class AClimbingSystemCharacter : public ACharacter
{
	GENERATED_UCLASS_BODY()

public:
	/** Dash if currently climbing, otherwise jump. */
	void Jump() override;

	/** Start to climb or cancel it depending on its current state. */
	virtual void Climb();

	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	/** Returns MovementComponent subobject **/
	UFUNCTION(BlueprintPure)
		FORCEINLINE class UMyCharacterMovementComponent* GetMyCharacterMovement() const { return MovementComponent; }

protected:
	/** Called for forwards/backward input */
	void MoveForward(float value);

	/** Called for side to side input */
	void MoveRight(float value);

	/** 
	 * Called via input to turn at a given rate. 
	 * @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	 */
	void TurnAtRate(float rate);

	/**
	 * Called via input to turn look up/down at a given rate. 
	 * @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	 */
	void LookUpAtRate(float rate);

	// APawn interface
	virtual void SetupPlayerInputComponent(class UInputComponent* playerInputComponent) override;
	// End of APawn interface

private:
	UPROPERTY()
	class UMyCharacterMovementComponent* MovementComponent;

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
		class USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
		class UCameraComponent* FollowCamera;

	/** Base turn rate, in deg/sec. Other scaling may affect final turn rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
		float TurnRateGamepad;

};
