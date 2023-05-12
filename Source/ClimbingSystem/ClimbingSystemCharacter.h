// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "ClimbingSystemCharacter.generated.h"

class UCameraComponent;
class UInputAction;
class UInputComponent;
class UInputMappingContext;
class USpringArmComponent;
class UMyCharacterMovementComponent;

UCLASS(config=Game)
class AClimbingSystemCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AClimbingSystemCharacter(const FObjectInitializer& objectInitializer = FObjectInitializer::Get());

	/** Dash if currently climbing, otherwise jump. */
	void Jump() override;

	/** Start to climb or cancel it depending on its current state. */
	virtual void Climb();

	/** Returns CameraBoom subobject **/
	FORCEINLINE USpringArmComponent* GetCameraBoom() const;

	/** Returns FollowCamera subobject **/
	FORCEINLINE UCameraComponent* GetFollowCamera() const;

	/** Returns MovementComponent subobject **/
	FORCEINLINE UMyCharacterMovementComponent* GetMyCharacterMovement() const;

protected:
	// APawn interface
	virtual void SetupPlayerInputComponent(UInputComponent* playerInputComponent) override;
	// End of APawn interface

private:
	/** Called for movement input */
	void Move(const FInputActionValue& value);

	/** Called for looking input */
	void Look(const FInputActionValue& value);

	/** Movement component handling the character's climbing mechanic */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Component, meta = (AllowPrivateAccess = "true"))
		UMyCharacterMovementComponent* MovementComponent;

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
		TObjectPtr<USpringArmComponent> CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
		TObjectPtr<UCameraComponent> FollowCamera;

	/** Input Mapping Context */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
		TObjectPtr<UInputMappingContext> DefaultMappingContext;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
		TObjectPtr<UInputAction> MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
		TObjectPtr<UInputAction> LookAction;

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
		TObjectPtr<UInputAction> JumpAction;

	/** Sprint Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
		TObjectPtr<UInputAction> ClimbAction;
};
