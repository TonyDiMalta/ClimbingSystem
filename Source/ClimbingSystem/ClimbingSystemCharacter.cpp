// Copyright Epic Games, Inc. All Rights Reserved.

#include "ClimbingSystemCharacter.h"

#include "Public/MyCharacterMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"

//////////////////////////////////////////////////////////////////////////
// AClimbingSystemCharacter

AClimbingSystemCharacter::AClimbingSystemCharacter(const FObjectInitializer& objectInitializer)
	: Super(objectInitializer.SetDefaultSubobjectClass<UMyCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
	
	TurnRateGamepad = 50.f;
	
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
	
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;
	
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	MovementComponent = Cast<UMyCharacterMovementComponent>(GetCharacterMovement());
}

void AClimbingSystemCharacter::SetupPlayerInputComponent(UInputComponent* playerInputComponent)
{
	// Set up gameplay key bindings
	check(playerInputComponent);
	playerInputComponent->BindAction("Jump", IE_Pressed, this, &ThisClass::Jump);
	playerInputComponent->BindAction("Jump", IE_Released, this, &ThisClass::StopJumping);

	playerInputComponent->BindAction("Climb", IE_Pressed, this, &ThisClass::Climb);
	playerInputComponent->BindAction("Cancel Climb", IE_Released, this, &ThisClass::CancelClimb);

	playerInputComponent->BindAxis("Move Forward / Backward", this, &ThisClass::MoveForward);
	playerInputComponent->BindAxis("Move Right / Left", this, &ThisClass::MoveRight);

	playerInputComponent->BindAxis("Turn Right / Left Mouse", this, &ThisClass::AddControllerYawInput);
	playerInputComponent->BindAxis("Turn Right / Left Gamepad", this, &ThisClass::TurnAtRate);
	playerInputComponent->BindAxis("Look Up / Down Mouse", this, &ThisClass::AddControllerPitchInput);
	playerInputComponent->BindAxis("Look Up / Down Gamepad", this, &ThisClass::LookUpAtRate);
}

void AClimbingSystemCharacter::Climb()
{
	MovementComponent->TryClimbing();
}

void AClimbingSystemCharacter::CancelClimb()
{
	MovementComponent->CancelClimbing();
}

void AClimbingSystemCharacter::MoveForward(float value)
{
	if (Controller == nullptr || value == 0.0f)
	{
		return;
	}

	FVector direction;
	if (MovementComponent->IsClimbing())
	{
		direction = FVector::CrossProduct(MovementComponent->GetClimbSurfaceNormal(), -GetActorRightVector());
	}
	else
	{
		direction = GetControlOrientationMatrix().GetUnitAxis(EAxis::X);
	}
	
	AddMovementInput(direction, value);
}

void AClimbingSystemCharacter::MoveRight(float value)
{
	if (Controller == nullptr || value == 0.0f)
	{
		return;
	}

	FVector direction;
	if (MovementComponent->IsClimbing())
	{
		direction = FVector::CrossProduct(MovementComponent->GetClimbSurfaceNormal(), GetActorUpVector());
	}
	else
	{
		direction = GetControlOrientationMatrix().GetUnitAxis(EAxis::Y);
	}
	
	AddMovementInput(direction, value);
}

FRotationMatrix AClimbingSystemCharacter::GetControlOrientationMatrix() const
{
	const FRotator rotation = Controller->GetControlRotation();
	const FRotator yawRotation(0, rotation.Yaw, 0);

	return FRotationMatrix(yawRotation);
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
