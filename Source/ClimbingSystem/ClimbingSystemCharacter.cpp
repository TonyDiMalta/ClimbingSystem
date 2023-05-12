// Copyright Epic Games, Inc. All Rights Reserved.

#include "ClimbingSystemCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "MyCharacterMovementComponent.h"

//////////////////////////////////////////////////////////////////////////
// AClimbingSystemCharacter

AClimbingSystemCharacter::AClimbingSystemCharacter(const FObjectInitializer& objectInitializer)
	: Super(objectInitializer.SetDefaultSubobjectClass<UMyCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	MovementComponent = Cast<UMyCharacterMovementComponent>(GetCharacterMovement());

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

USpringArmComponent* AClimbingSystemCharacter::GetCameraBoom() const
{
	return CameraBoom;
}

UCameraComponent* AClimbingSystemCharacter::GetFollowCamera() const
{
	return FollowCamera;
}

UMyCharacterMovementComponent* AClimbingSystemCharacter::GetMyCharacterMovement() const
{
	return MovementComponent;
}

//////////////////////////////////////////////////////////////////////////
// Input

void AClimbingSystemCharacter::SetupPlayerInputComponent(class UInputComponent* playerInputComponent)
{
	// Set up gameplay key bindings
	check(playerInputComponent);
	if (UEnhancedInputComponent* enhancedInputComponent = CastChecked<UEnhancedInputComponent>(playerInputComponent))
	{
		//Moving
		enhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ThisClass::Move);

		//Looking
		enhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ThisClass::Look);

		//Jumping
		enhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Triggered, this, &ThisClass::Jump);
		enhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ThisClass::StopJumping);

		//Climbing
		enhancedInputComponent->BindAction(ClimbAction, ETriggerEvent::Triggered, this, &ThisClass::Climb);

		// Set up input mappings
		check(Controller);
		if (APlayerController* playerController = Cast<APlayerController>(Controller))
		{
			if (UEnhancedInputLocalPlayerSubsystem* subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(playerController->GetLocalPlayer()))
			{
				// Clear out existing mapping, and add our mapping
				subsystem->ClearAllMappings();
				subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}
}

void AClimbingSystemCharacter::Jump()
{
	if (MovementComponent->IsClimbing())
	{
		MovementComponent->TryClimbDashing();
	}
	else
	{
		Super::Jump();
	}
}

void AClimbingSystemCharacter::Climb()
{
	if (MovementComponent->IsClimbing())
	{
		MovementComponent->CancelClimbing();
	}
	else
	{
		MovementComponent->TryClimbing();
	}
}

void AClimbingSystemCharacter::Move(const FInputActionValue& value)
{
	if (Controller != nullptr)
	{
		// block movement inputs while running animated movements
		if (MovementComponent->IsClimbDashing() || MovementComponent->IsClimbingLedge())
		{
			return;
		}

		const FVector2D moveValue = value.Get<FVector2D>();
		const FRotator movementRotation(0, Controller->GetControlRotation().Yaw, 0);

		// Forward/Backward direction
		if (moveValue.Y != 0.f)
		{
			// get forward vector
			FVector direction;
			if (MovementComponent->IsClimbing())
			{
				direction = FVector::CrossProduct(MovementComponent->GetClimbSurfaceNormal(), -GetActorRightVector());
			}
			else
			{
				direction = movementRotation.RotateVector(FVector::ForwardVector);
			}

			// add movement in that direction
			AddMovementInput(direction, moveValue.Y);
		}

		// Right/Left direction
		if (moveValue.X != 0.f)
		{
			// Get right vector
			FVector direction;
			if (MovementComponent->IsClimbing())
			{
				direction = FVector::CrossProduct(MovementComponent->GetClimbSurfaceNormal(), GetActorUpVector());
			}
			else
			{
				direction = movementRotation.RotateVector(FVector::RightVector);
			}

			// add movement in that direction
			AddMovementInput(direction, moveValue.X);
		}
	}
}

void AClimbingSystemCharacter::Look(const FInputActionValue& value)
{
	if (Controller != nullptr)
	{
		const FVector2D lookValue = value.Get<FVector2D>();

		if (lookValue.X != 0.f)
		{
			AddControllerYawInput(lookValue.X);
		}

		if (lookValue.Y != 0.f)
		{
			AddControllerPitchInput(lookValue.Y);
		}
	}
}
