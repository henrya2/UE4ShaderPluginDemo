// Copyright 2016-2020 Cadic AB. All Rights Reserved.
// @Author	Fredrik Lindh [Temaran] (temaran@gmail.com) {https://github.com/Temaran}
///////////////////////////////////////////////////////////////////////////////////////

#include "ShaderUsageDemoCharacter.h"

#include "ShaderDeclarationDemoModule.h"

#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/InputSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Runtime/Engine/Classes/Materials/MaterialInstanceDynamic.h"

AShaderUsageDemoCharacter::AShaderUsageDemoCharacter()
{
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);

	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCameraComponent->SetupAttachment(GetCapsuleComponent());
	FirstPersonCameraComponent->SetRelativeLocation(FVector(-39.56f, 1.75f, 64.f)); // Position the camera
	FirstPersonCameraComponent->bUsePawnControlRotation = true;

	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh1P"));
	Mesh1P->SetOnlyOwnerSee(true);
	Mesh1P->SetupAttachment(FirstPersonCameraComponent);
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->CastShadow = false;
	Mesh1P->SetRelativeRotation(FRotator(1.9f, -19.19f, 5.2f));
	Mesh1P->SetRelativeLocation(FVector(-0.5f, -4.4f, -155.7f));

	FP_Gun = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FP_Gun"));
	FP_Gun->SetOnlyOwnerSee(true);
	FP_Gun->bCastDynamicShadow = false;
	FP_Gun->CastShadow = false;
	FP_Gun->SetupAttachment(RootComponent);

	FP_MuzzleLocation = CreateDefaultSubobject<USceneComponent>(TEXT("MuzzleLocation"));
	FP_MuzzleLocation->SetupAttachment(FP_Gun);
	FP_MuzzleLocation->SetRelativeLocation(FVector(0.2f, 48.4f, -10.6f));

	GunOffset = FVector(100.0f, 0.0f, 10.0f);

	EndColorBuildup = 0;
	EndColorBuildupDirection = 1;
	StartColor = FColor::Green;
	ComputeShaderSimulationSpeed = 1.0;
	ComputeShaderBlend = 0.5f;
	TotalTimeSecs = 0.0f;
}

void AShaderUsageDemoCharacter::BeginPlay()
{
	Super::BeginPlay();
	FP_Gun->AttachToComponent(Mesh1P, FAttachmentTransformRules::SnapToTargetIncludingScale, TEXT("GripPoint"));
	FShaderDeclarationDemoModule::Get().BeginRendering();
}

void AShaderUsageDemoCharacter::BeginDestroy()
{
	FShaderDeclarationDemoModule::Get().EndRendering();
	Super::BeginDestroy();
}

void AShaderUsageDemoCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TotalTimeSecs += DeltaSeconds;
	float ComputeShaderBlendScalar = 0.0f;

	// Do axis logic here since we cannot use input bindings in a plugin...
	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if(PlayerController)
	{ 
		if (PlayerController->IsInputKeyDown(EKeys::E)) { ComputeShaderBlendScalar += 1.0f; }
		if (PlayerController->IsInputKeyDown(EKeys::Q)) { ComputeShaderBlendScalar -= 1.0f; }
	}

	EndColorBuildup = FMath::Clamp(EndColorBuildup + DeltaSeconds * EndColorBuildupDirection, 0.0f, 1.0f);
	if (EndColorBuildup >= 1.0 || EndColorBuildup <= 0) 
	{
		EndColorBuildupDirection *= -1;
	}
		
	ComputeShaderBlend = FMath::Clamp(ComputeShaderBlend + ComputeShaderBlendScalar * DeltaSeconds, 0.0f, 1.0f);

	FShaderUsageExampleParameters DrawParameters(RenderTarget);
	{
		DrawParameters.SimulationState = ComputeShaderSimulationSpeed * TotalTimeSecs;
		DrawParameters.ComputeShaderBlend = ComputeShaderBlend;
		DrawParameters.StartColor = StartColor;
		DrawParameters.EndColor = FColor(EndColorBuildup * 255, 0, 0, 255);
	}

	// If doing this for realsies, you should avoid doing this every frame unless you have to of course.
	// We set it every frame here since we're updating the end color and simulation state. Boop.
	FShaderDeclarationDemoModule::Get().UpdateParameters(DrawParameters);
	FShaderDeclarationDemoModule::Get().DrawTarget();
}

void AShaderUsageDemoCharacter::OnFire()
{
	// Try to set a texture to the object we hit!
	FHitResult HitResult;
	FVector StartLocation = FirstPersonCameraComponent->GetComponentLocation();
	FRotator Direction = FirstPersonCameraComponent->GetComponentRotation();
	FVector EndLocation = StartLocation + Direction.Vector() * 10000;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	if (GetWorld()->LineTraceSingleByChannel(HitResult, StartLocation, EndLocation, ECC_Visibility, QueryParams))
	{
		AActor* HitActor = HitResult.GetActor();

		if (HitActor)
		{
			TArray<UStaticMeshComponent*> StaticMeshComponents = TArray<UStaticMeshComponent*>();
			HitActor->GetComponents<UStaticMeshComponent>(StaticMeshComponents);

			for (int32 i = 0; i < StaticMeshComponents.Num(); i++) 
			{
				UStaticMeshComponent* CurrentStaticMeshPtr = StaticMeshComponents[i];
				CurrentStaticMeshPtr->SetMaterial(0, MaterialToApplyToClickedObject);
				UMaterialInstanceDynamic* MID =	CurrentStaticMeshPtr->CreateAndSetMaterialInstanceDynamic(0);
				MID->SetTextureParameterValue("InputTexture", (UTexture*)RenderTarget);
			}
		}
	}

	if (FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, GetActorLocation());
	}

	UAnimInstance* AnimInstance = Mesh1P->GetAnimInstance();
	if (AnimInstance && FireAnimation)
	{
		AnimInstance->Montage_Play(FireAnimation, 1.f);
	}
}

void AShaderUsageDemoCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// set up gameplay key bindings
	check(PlayerInputComponent);

	// Set up gameplay key bindings

	// Bind jump events
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	// Bind fire event
	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &AShaderUsageDemoCharacter::OnFire);

	// Attempt to enable touch screen movement
	TryEnableTouchscreenMovement(PlayerInputComponent);

	// Bind movement events
	PlayerInputComponent->BindAxis("MoveForward", this, &AShaderUsageDemoCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AShaderUsageDemoCharacter::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &AShaderUsageDemoCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &AShaderUsageDemoCharacter::LookUpAtRate);
}


void AShaderUsageDemoCharacter::BeginTouch(const ETouchIndex::Type FingerIndex, const FVector Location)
{
	// If touch is already pressed check the index. If it is not the same as the current touch assume a second touch and thus we want to fire
	if (TouchItem.bIsPressed == true)
	{
		if (TouchItem.FingerIndex != FingerIndex)
		{
			OnFire();
		}
	}
	else
	{
		// Cache the finger index and touch location and flag we are processing a touch
		TouchItem.bIsPressed = true;
		TouchItem.FingerIndex = FingerIndex;
		TouchItem.Location = Location;
		TouchItem.bMoved = false;
	}
}

void AShaderUsageDemoCharacter::EndTouch(const ETouchIndex::Type FingerIndex, const FVector Location)
{
	// If we didn't record the start event do nothing, or this is a different index
	if ((TouchItem.bIsPressed == false) || (TouchItem.FingerIndex != FingerIndex))
	{
		return;
	}

	// If the index matches the start index and we didn't process any movement we assume we want to fire
	if ((FingerIndex == TouchItem.FingerIndex) && (TouchItem.bMoved == false))
	{
		OnFire();
	}

	// Flag we are no longer processing the touch event
	TouchItem.bIsPressed = false;
}

void AShaderUsageDemoCharacter::TouchUpdate(const ETouchIndex::Type FingerIndex, const FVector Location)
{
	// If we are processing a touch event and this index matches the initial touch event process movement
	if ((TouchItem.bIsPressed == true) && (TouchItem.FingerIndex == FingerIndex))
	{
		if (GetWorld() != nullptr)
		{
			UGameViewportClient* ViewportClient = GetWorld()->GetGameViewport();
			if (ViewportClient != nullptr)
			{
				FVector MoveDelta = Location - TouchItem.Location;
				FVector2D ScreenSize;
				ViewportClient->GetViewportSize(ScreenSize);
				FVector2D ScaledDelta = FVector2D(MoveDelta.X, MoveDelta.Y) / ScreenSize;
				if (FMath::Abs(ScaledDelta.X) >= (4.0f / ScreenSize.X))
				{
					TouchItem.bMoved = true;
					float Value = ScaledDelta.X * BaseTurnRate;
					AddControllerYawInput(Value);
				}
				if (FMath::Abs(ScaledDelta.Y) >= (4.0f / ScreenSize.Y))
				{
					TouchItem.bMoved = true;
					float Value = ScaledDelta.Y* BaseTurnRate;
					AddControllerPitchInput(Value);
				}
				TouchItem.Location = Location;
			}
			TouchItem.Location = Location;
		}
	}
}

void AShaderUsageDemoCharacter::MoveForward(float Value)
{
	if (Value != 0.0f)
	{
		// Add movement in that direction
		AddMovementInput(GetActorForwardVector(), Value);
	}
}

void AShaderUsageDemoCharacter::MoveRight(float Value)
{
	if (Value != 0.0f)
	{
		// Add movement in that direction
		AddMovementInput(GetActorRightVector(), Value);
	}
}

void AShaderUsageDemoCharacter::TurnAtRate(float Rate)
{
	// Calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void AShaderUsageDemoCharacter::LookUpAtRate(float Rate)
{
	// Calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

void AShaderUsageDemoCharacter::TryEnableTouchscreenMovement(UInputComponent* PlayerInputComponent)
{
	PlayerInputComponent->BindTouch(EInputEvent::IE_Pressed, this, &AShaderUsageDemoCharacter::BeginTouch);
	PlayerInputComponent->BindTouch(EInputEvent::IE_Released, this, &AShaderUsageDemoCharacter::EndTouch);
	PlayerInputComponent->BindTouch(EInputEvent::IE_Repeat, this, &AShaderUsageDemoCharacter::TouchUpdate);
}
