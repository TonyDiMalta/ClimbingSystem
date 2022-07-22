// Copyright Epic Games, Inc. All Rights Reserved.

// Base Character from UE5 Third Person Template

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

AClimbingSystemCharacter::AClimbingSystemCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UMyCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
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

	MovementComponent = Cast<UMyCharacterMovementComponent>(GetCharacterMovement()); // <--
}

void AClimbingSystemCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up gameplay key bindings
	check(PlayerInputComponent);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	PlayerInputComponent->BindAction("Climb", IE_Pressed, this, &AClimbingSystemCharacter::Climb);
	PlayerInputComponent->BindAction("Cancel Climb", IE_Released, this, &AClimbingSystemCharacter::CancelClimb);

	PlayerInputComponent->BindAxis("Move Forward / Backward", this, &AClimbingSystemCharacter::MoveForward);
	PlayerInputComponent->BindAxis("Move Right / Left", this, &AClimbingSystemCharacter::MoveRight);


	PlayerInputComponent->BindAxis("Turn Right / Left Mouse", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("Turn Right / Left Gamepad", this, &AClimbingSystemCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("Look Up / Down Mouse", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("Look Up / Down Gamepad", this, &AClimbingSystemCharacter::LookUpAtRate);
}

void AClimbingSystemCharacter::Climb()
{
	MovementComponent->TryClimbing();
}

void AClimbingSystemCharacter::CancelClimb()
{
	MovementComponent->CancelClimbing();
}

void AClimbingSystemCharacter::MoveForward(float Value)
{
	if (Controller == nullptr || Value == 0.0f)
	{
		return;
	}

	FVector Direction;
	
	if (MovementComponent->IsClimbing())
	{
		Direction = FVector::CrossProduct(MovementComponent->GetClimbSurfaceNormal(), -GetActorRightVector());
	}
	else
	{
		Direction = GetControlOrientationMatrix().GetUnitAxis(EAxis::X);
	}
	
	AddMovementInput(Direction, Value);
}

void AClimbingSystemCharacter::MoveRight(float Value)
{
	if (Controller == nullptr || Value == 0.0f)
	{
		return;
	}

	FVector Direction;
	if (MovementComponent->IsClimbing())
	{
		Direction = FVector::CrossProduct(MovementComponent->GetClimbSurfaceNormal(), GetActorUpVector());
	}
	else
	{
		Direction = GetControlOrientationMatrix().GetUnitAxis(EAxis::Y);
	}
	
	AddMovementInput(Direction, Value);
}

FRotationMatrix AClimbingSystemCharacter::GetControlOrientationMatrix() const
{
	const FRotator Rotation = Controller->GetControlRotation();
	const FRotator YawRotation(0, Rotation.Yaw, 0);

	return FRotationMatrix(YawRotation);
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

void AClimbingSystemCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * TurnRateGamepad * GetWorld()->GetDeltaSeconds());
}

void AClimbingSystemCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * TurnRateGamepad * GetWorld()->GetDeltaSeconds());
}
