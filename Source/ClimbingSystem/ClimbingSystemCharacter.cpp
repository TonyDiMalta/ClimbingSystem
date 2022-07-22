// Copyright Epic Games, Inc. All Rights Reserved.

#include "ClimbingSystemCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "MyCharacterMovementComponent.h"

//////////////////////////////////////////////////////////////////////////
// AClimbingSystemCharacter

AClimbingSystemCharacter::AClimbingSystemCharacter(const FObjectInitializer& objectInitializer)
	: Super(objectInitializer.SetDefaultSubobjectClass<UMyCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// Set our turn rate for input
	TurnRateGamepad = 50.f;

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;

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

//////////////////////////////////////////////////////////////////////////
// Input

void AClimbingSystemCharacter::SetupPlayerInputComponent(class UInputComponent* playerInputComponent)
{
	// Set up gameplay key bindings
	check(playerInputComponent);
	playerInputComponent->BindAction("Jump", IE_Pressed, this, &ThisClass::Jump);
	playerInputComponent->BindAction("Jump", IE_Released, this, &ThisClass::StopJumping);

	playerInputComponent->BindAxis("Move Forward / Backward", this, &ThisClass::MoveForward);
	playerInputComponent->BindAxis("Move Right / Left", this, &ThisClass::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	playerInputComponent->BindAxis("Turn Right / Left Mouse", this, &ThisClass::AddControllerYawInput);
	playerInputComponent->BindAxis("Turn Right / Left Gamepad", this, &ThisClass::TurnAtRate);
	playerInputComponent->BindAxis("Look Up / Down Mouse", this, &ThisClass::AddControllerPitchInput);
	playerInputComponent->BindAxis("Look Up / Down Gamepad", this, &ThisClass::LookUpAtRate);

	// handle climbing
	playerInputComponent->BindAction("Climb", IE_Pressed, this, &ThisClass::Climb);
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

void AClimbingSystemCharacter::TurnAtRate(float rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(rate * TurnRateGamepad * GetWorld()->GetDeltaSeconds());
}

void AClimbingSystemCharacter::LookUpAtRate(float rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(rate * TurnRateGamepad * GetWorld()->GetDeltaSeconds());
}

void AClimbingSystemCharacter::MoveForward(float value)
{
	if ((Controller != nullptr) && (value != 0.0f))
	{
		// block movement inputs while running animated movements
		if (MovementComponent->IsClimbDashing() || MovementComponent->IsClimbingLedge())
		{
			return;
		}

		// find out which way is forward
		const FRotator rotation = Controller->GetControlRotation();
		const FRotator yawRotation(0, rotation.Yaw, 0);

		// get forward vector
		FVector direction;
		if (MovementComponent->IsClimbing())
		{
			direction = FVector::CrossProduct(MovementComponent->GetClimbSurfaceNormal(), -GetActorRightVector());
		}
		else
		{
			direction = FRotationMatrix(yawRotation).GetUnitAxis(EAxis::X);
		}

		// add movement in that direction
		AddMovementInput(direction, value);
	}
}

void AClimbingSystemCharacter::MoveRight(float value)
{
	if ((Controller != nullptr) && (value != 0.0f))
	{
		// block movement inputs while running animated movements
		if (MovementComponent->IsClimbDashing() || MovementComponent->IsClimbingLedge())
		{
			return;
		}

		// find out which way is right
		const FRotator rotation = Controller->GetControlRotation();
		const FRotator yawRotation(0, rotation.Yaw, 0);
	
		// get right vector
		FVector direction;
		if (MovementComponent->IsClimbing())
		{
			direction = FVector::CrossProduct(MovementComponent->GetClimbSurfaceNormal(), GetActorUpVector());
		}
		else
		{
			direction = FRotationMatrix(yawRotation).GetUnitAxis(EAxis::Y);
		}

		// add movement in that direction
		AddMovementInput(direction, value);
	}
}
