
// Fill out your copyright notice in the Description page of Project Settings.
#include "BlasterCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/CapsuleComponent.h"
#include "Net/UnrealNetwork.h"
#include "Blaster/Weapon/Weapon.h"
#include "Blaster/Buttons/MyButton.h"
#include "Blaster/BlasterComponents/CombatComponent.h"
#include "Blaster/BlasterComponents/InventoryComponent.h" 
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"
#include "BlasterAnimInstance.h"
#include "Blaster/Blaster.h"
#include "Blaster/PlayerController/BlasterPlayerController.h"
#include "Blaster/GameMode/BlasterGameMode.h"
#include "TimerManager.h"
#include "Blaster/Limb/Limb.h"
#include "Blaster/BlasterPlayerState/BlasterPlayerState.h"
#include "Blaster/Weapon/WeaponTypes.h"
#include "Components/SphereComponent.h"
#include "Blaster/BlasterTypes/CombatState.h"
#include "DrawDebugHelpers.h"

//	if (HasAuthority() && !IsLocallyControlled()) 
//	{
//			UE_LOG(LogTemp, Warning, TEXT("AO_Pitch: %f"), AO_Pitch);
//	}

// Sets default values
ABlasterCharacter::ABlasterCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;


	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetMesh());
	CameraBoom->TargetArmLength = 350.f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	//FirstPersonCamera->SetupAttachment(GetMesh(), TEXT("headSocket"));
	FirstPersonCamera->bUsePawnControlRotation = false;

	//this also has to be set in BP as BP will override them
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;

	OverheadWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("OverheadWidget"));
	//components are special n dont need to be registered in GetLifetimeReplicated
	//but we do need to incl the header	
	OverheadWidget->SetupAttachment(RootComponent);  

	//i believe this is bad practice to set these here but it sure is the easiest :)
	Combat = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
	Combat->SetIsReplicated(true);

	InventoryComponent = CreateDefaultSubobject<UInventoryComponent>(TEXT("InventoryComponent"));
	InventoryComponent->Combat = Combat;
	Combat->InventoryComponent = InventoryComponent;

	//this is also checked on bp character movement comp in myblasterchar
	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;

	InteractSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractSphere"));
	InteractSphere->SetupAttachment(RootComponent);
	InteractSphere->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	InteractSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	VisualTargetSphere = CreateDefaultSubobject<USphereComponent>(TEXT("VisualTargetSphere"));
	VisualTargetSphere->SetupAttachment(RootComponent);
	VisualTargetSphere->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	VisualTargetSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	//this prevents the camera from moving when another player is between camera and player mesh
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
	GetMesh()->SetCollisionObjectType(ECC_SkeletalMesh);
	GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);
	GetCharacterMovement()->RotationRate = FRotator(0.f, 0.f, 850);

		TurningInPlace = ETurningInPlace::ETIP_NotTurning;
		NetUpdateFrequency = 66.f;
		MinNetUpdateFrequency = 33.f;

		DissolveTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("DissolveTimelineComponent"));
}


void ABlasterCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ABlasterCharacter, OverlappingWeapon, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ABlasterCharacter, OverlappingButton, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ABlasterCharacter, InteractTargetLocation, COND_SkipOwner);
	DOREPLIFETIME(ABlasterCharacter, Health);
	DOREPLIFETIME(ABlasterCharacter, bDisableGameplay);
}

void ABlasterCharacter::Destroyed()
{
	Super::Destroyed();

	ABlasterGameMode* BlasterGameMode = Cast<ABlasterGameMode>(UGameplayStatics::GetGameMode(this));
	bool bMatchNotInProgress = BlasterGameMode 
		&& BlasterGameMode->GetMatchState() != MatchState::InProgress;
	if (Combat && Combat->EquippedWeapon && bMatchNotInProgress)
	{
		Combat->EquippedWeapon->Destroy();
	}
}


// Called when the game starts or when spawned
void ABlasterCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	UpdateHUDHealth();

	if (HasAuthority())
	{
		OnTakeAnyDamage.AddDynamic(this, &ABlasterCharacter::ReceiveDamage);
	}

	if (HasAuthority())
	{
		InteractSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		InteractSphere->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Overlap);
		VisualTargetSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		VisualTargetSphere->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Overlap);
	}

	//if (FirstPersonCamera)
	//{
	//	FollowCamera->SetActive(false);
	//	FirstPersonCamera->SetActive(true);
	//	FirstPersonCamera->bUsePawnControlRotation = false;

	//	//this also has to be set in BP as BP will override them
	//	bUseControllerRotationYaw = true;
	//	bUseControllerRotationPitch = true;
	//	GetCharacterMovement()->bOrientRotationToMovement = false;
	//}
}

// Called every frame
void ABlasterCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (IsLocallyControlled())
	{
		ManageVisualInteractionTargetLocations();
	}
	RotateInPlace(DeltaTime);
	AimOffset(DeltaTime);
	HideCameraIfCharacterClose();
	PollInit();
}

void ABlasterCharacter::RotateInPlace(float DeltaTime)
{
	if (bDisableGameplay)
	{
		bUseControllerRotationYaw = false;
		TurningInPlace = ETurningInPlace::ETIP_NotTurning;
		return;
	}
	if (GetLocalRole() > ENetRole::ROLE_SimulatedProxy && IsLocallyControlled())
	{
		AimOffset(DeltaTime);
	}
	else
	{
		TimeSinceLastMovementReplication += DeltaTime;
		if (TimeSinceLastMovementReplication > 0.25f)
		{
			OnRep_ReplicatedMovement();
		}
		CalculateAO_Pitch();
	}
}
// Called to bind functionality to input
void ABlasterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	//overridden the old with our custom
	//PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ABlasterCharacter::Jump);

	//bind proj settings input axes to our custom functions here
	PlayerInputComponent->BindAxis("MoveForward", this, &ABlasterCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &ABlasterCharacter::MoveRight);
	
	PlayerInputComponent->BindAxis("Turn", this, &ABlasterCharacter::Turn);
	PlayerInputComponent->BindAxis("LookUp", this, &ABlasterCharacter::LookUp);

	PlayerInputComponent->BindAction("Equip", IE_Pressed, this, &ABlasterCharacter::EquipButtonPressed);
	PlayerInputComponent->BindAction("Equip", IE_Released, this, &ABlasterCharacter::EquipButtonReleased);

	PlayerInputComponent->BindAction("Crouch", IE_Pressed, this, &ABlasterCharacter::CrouchButtonPressed);
	PlayerInputComponent->BindAction("Reload", IE_Pressed, this, &ABlasterCharacter::ReloadButtonPressed);
	PlayerInputComponent->BindAction("Aim", IE_Pressed, this, &ABlasterCharacter::AimButtonPressed);
	PlayerInputComponent->BindAction("Aim", IE_Released, this, &ABlasterCharacter::AimButtonReleased);

	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &ABlasterCharacter::FireButtonPressed);
	PlayerInputComponent->BindAction("Fire", IE_Released, this, &ABlasterCharacter::FireButtonReleased);
	
	PlayerInputComponent->BindAction("Throw", IE_Pressed, this, &ABlasterCharacter::ThrowButtonPressed);
	PlayerInputComponent->BindAction("Throw", IE_Released, this, &ABlasterCharacter::ThrowButtonReleased);


	PlayerInputComponent->BindAction("ItemShuffleLeft", IE_Pressed, this, &ABlasterCharacter::ItemShuffleLeft);
	PlayerInputComponent->BindAction("ItemShuffleRight", IE_Pressed, this, &ABlasterCharacter::ItemShuffleRight);
}


void ABlasterCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	if (Combat)
	{
		Combat->Character = this;
	}
}

void ABlasterCharacter::PlayFireMontage(bool bAiming)
{
	if (Combat == nullptr || Combat->EquippedWeapon == nullptr) return;

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && FireWeaponMontage)
	{
		AnimInstance->Montage_Play(FireWeaponMontage);
		FName SectionName;
		SectionName = bAiming ? FName("RifleAim") : FName("RifleHip");
		AnimInstance->Montage_JumpToSection(SectionName);
	}
}


//so the way this works for the future
//is there is a reload montage in the blueprints/character/animations folder
//drag in an animation after the rifle animation, right click->add section (ig: "shotgun")
//come here and add it to the switch case
//make sure its also added the WeaponTypes.h enum as well
void ABlasterCharacter::PlayReloadMontage()
{
	if (Combat == nullptr || Combat->EquippedWeapon == nullptr) return;

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && ReloadMontage)
	{
		AnimInstance->Montage_Play(ReloadMontage);
		FName SectionName;
		switch (Combat->EquippedWeapon->GetWeaponType())
		{
		case EWeaponType::EWT_AssaultRifle:
			SectionName = FName("Rifle");
			break;	
		
		case EWeaponType::EWT_RocketLauncher:
			SectionName = FName("Rifle");
			break;
		
		case EWeaponType::EWT_Pistol:
			SectionName = FName("Rifle");
			break;
		
		case EWeaponType::EWT_SubmachineGun:
			SectionName = FName("Rifle");
			break;
		
		case EWeaponType::EWT_Shotgun:
			SectionName = FName("Shotgun");
			break;

		case EWeaponType::EWT_SniperRifle:
			SectionName = FName("Rifle");
			break;

		case EWeaponType::EWT_GrenadeLauncher:
			SectionName = FName("Rifle");
			break;
		}
		AnimInstance->Montage_JumpToSection(SectionName);
	}
}



void ABlasterCharacter::PlayElimMontage()
{
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && ElimMontage)
	{
		AnimInstance->Montage_Play(ElimMontage);
	}
}


void ABlasterCharacter::Elim()
{
	if (Combat && Combat->EquippedWeapon)
	{
		Combat->EquippedWeapon->Dropped();
	}
	MulticastElim();
	GetWorldTimerManager().SetTimer(
		ElimTimer,
		this,
		&ABlasterCharacter::ElimTimerFinished,
		ElimDelay
	);
}


void ABlasterCharacter::MulticastElim_Implementation()
{
	if (BlasterPlayerController)
	{
		BlasterPlayerController->SetHUDWeaponAmmo(0);
	}
	bElimmed = true;
	PlayElimMontage();
	
	//start dissolve effect
	if (DissolveMaterialInstance)
	{
		DynamicDissolveMaterialInstance = UMaterialInstanceDynamic::Create(
			DissolveMaterialInstance,
			this);

		/// <summary>
		/// HEY this is where you set the element index as the 0th one
		/// so alternate indexes may be used if desired
		/// </summary>
		GetMesh()->SetMaterial(0, DynamicDissolveMaterialInstance);
		DynamicDissolveMaterialInstance->SetScalarParameterValue(TEXT("Dissolve"), 0.f);
		DynamicDissolveMaterialInstance->SetScalarParameterValue(TEXT("Glow"), 10.f);
	}

	if (InvisibleMaterialInstance)
	{
		GetMesh()->SetMaterial(1, InvisibleMaterialInstance);
		GetMesh()->SetMaterial(2, InvisibleMaterialInstance);
		GetMesh()->SetMaterial(3, InvisibleMaterialInstance);
		GetMesh()->SetMaterial(4, InvisibleMaterialInstance);

	}

	StartDissolve();


	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (HasAuthority()
		//&& GetController() != nullptr && GetController()->IsLocalController()
		&& LimbClass != nullptr)
	{
		HandleDeathTransition();
	}

	//disable char movement
	GetCharacterMovement()->DisableMovement();
	GetCharacterMovement()->StopMovementImmediately();
	//if (BlasterPlayerController)
	//{
	//	DisableInput(BlasterPlayerController);
	//}
	////disable collision
	bDisableGameplay = true;
	if (Combat)
	{
		Combat->FireButtonPressed(false);
	}

	//FUCKING POINTERS
	bool bHideSniperScope = 
		IsLocallyControlled()
		&& Combat
		&& Combat->bAiming
		&& Combat->EquippedWeapon
		&& Combat->EquippedWeapon->GetWeaponType() == EWeaponType::EWT_SniperRifle;
	if (bHideSniperScope)
	{
		ShowSniperScopeWidget(false);
	}

}

void ABlasterCharacter::HandleDeathTransition()
{
	
	FVector SocketLocation = GetMesh()->GetSocketLocation(FName("headSocket"));

	// Spawn ALimb pawn
	if (ALimb* LimbPawn = GetWorld()->SpawnActor<ALimb>(LimbClass, SocketLocation, GetActorRotation()))
	{
		// Smooth camera transition
		USpringArmComponent* PlayerCameraArm = CameraBoom; // Reference to your camera arm component
		USpringArmComponent* LimbCameraArm = LimbPawn->FindComponentByClass<USpringArmComponent>();

		// Interpolate camera location and rotation over time
		float CameraTransitionSpeed = 5.0f; // Adjust the speed as needed
		PlayerCameraArm->TargetArmLength = FMath::FInterpTo(PlayerCameraArm->TargetArmLength, LimbCameraArm->TargetArmLength, GetWorld()->GetDeltaSeconds(), CameraTransitionSpeed);
		PlayerCameraArm->SetWorldRotation(FMath::RInterpTo(PlayerCameraArm->GetComponentRotation(), LimbCameraArm->GetComponentRotation(), GetWorld()->GetDeltaSeconds(), CameraTransitionSpeed));

		// Possess ALimb pawn
		BlasterPlayerController = BlasterPlayerController == nullptr ? Cast<ABlasterPlayerController>(Controller) : BlasterPlayerController;
		if (BlasterPlayerController)
		{
			BlasterPlayerController->Possess(LimbPawn);
		}
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("FailedToSpawnLimb"));
	}

	// Disable input and character movement for ABlasterCharacter
	//DisableInput(BlasterPlayerController);
	//GetCharacterMovement()->DisableMovement();
	//GetCharacterMovement()->StopMovementImmediately();

	// You can also disable collision for ABlasterCharacter and perform other necessary cleanup.
}

void ABlasterCharacter::ThrowButtonPressed()
{
	if (Combat)
	{
		Combat->StartThrowCharging();
	}
}

void ABlasterCharacter::ThrowButtonReleased()
{
	if (Combat)
	{
		Combat->Throw();
	}
}

void ABlasterCharacter::StartDissolve() //and death particles lol
{
	// Create the particle system
	if (DeathParticles)
	{
		USkeletalMeshComponent* MeshComponent = GetMesh(); 
		if (MeshComponent)
		{
			// Use the "spine_0003Socket" socket name to attach the particle system
			FName SocketName("spine_003Socket");

			UParticleSystemComponent* ParticleComponent = UGameplayStatics::SpawnEmitterAttached(
				DeathParticles, // The particle system you want to spawn
				MeshComponent,        // The mesh to which you want to attach the particle system
				SocketName,     // Replace with the socket name or empty string to attach to the mesh directly
				FVector(0, 0, 0), // Relative location (offset) of the particle system
				FRotator(0, 0, 0), // Relative rotation of the particle system
				EAttachLocation::SnapToTarget, // Attachment rule
				true // Auto destroy
			);
		}
		// You can further configure the ParticleComponent here, e.g., set parameters.
	}

	DissolveTrack.BindDynamic(this, &ABlasterCharacter::UpdateDissolveMaterial);
	if (DissolveCurve && DissolveTimeline)
	{
		DissolveTimeline->AddInterpFloat(DissolveCurve, DissolveTrack);
		DissolveTimeline->Play();
	}

	if (DeathSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, DeathSound, GetActorLocation());
	}
}

void ABlasterCharacter::UpdateDissolveMaterial(float DissolveValue)
{
	if (DynamicDissolveMaterialInstance)
	{
		DynamicDissolveMaterialInstance->SetScalarParameterValue(TEXT("Dissolve"), DissolveValue);
	}
}

void ABlasterCharacter::ElimTimerFinished()
{
	ABlasterGameMode* BlasterGameMode = GetWorld()->GetAuthGameMode<ABlasterGameMode>();
	if (BlasterGameMode)
	{
		BlasterGameMode->RequestRespawn(this, Controller);
	}
}

void ABlasterCharacter::OnRep_ReplicatedMovement()
{
	Super::OnRep_ReplicatedMovement();
	SimProxiesTurn();
	TimeSinceLastMovementReplication = 0.f;
}

void ABlasterCharacter::PlayThrowMontage()
{
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && ThrowMontage)
	{
		AnimInstance->Montage_Play(ThrowMontage);
	}
}

void ABlasterCharacter::PlayHitReactMontage()
{
	if (Combat == nullptr || Combat->EquippedWeapon == nullptr) return;
	if (!(GetCombatState() == ECombatState::ECS_Unoccupied)) return;
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && HitReactMontage)
	{
		AnimInstance->Montage_Play(HitReactMontage);
		FName SectionName("FromFront");
		AnimInstance->Montage_JumpToSection(SectionName);
	}
}

//only called on server
void ABlasterCharacter::ReceiveDamage(
	AActor* DamagedActor, 
	float Damage, 
	const UDamageType* DamageType, 
	AController* InstigatorController, 
	AActor* DamageCauser)
{
	//GIVE BLOOD FOR HITS
	ABlasterPlayerController* AttackerController = Cast<ABlasterPlayerController>(InstigatorController);
	if (!AttackerController) return;
	ABlasterPlayerState* AttackerPlayerState =
		AttackerController ?
		Cast<ABlasterPlayerState>(AttackerController->PlayerState)
		: nullptr;
	if (AttackerPlayerState)
	{
			AttackerPlayerState->AddToScore(1.666f);
	}

	//UPDATE HEALTH
	Health = FMath::Clamp(Health - Damage, 0.f, MaxHealth);
	UpdateHUDHealth();
	PlayHitReactMontage();

	//KILL
	if (Health <= 0.f)
	{
		ABlasterGameMode* BlasterGameMode = GetWorld()->GetAuthGameMode<ABlasterGameMode>();
		if (BlasterGameMode)
		{
			BlasterPlayerController = BlasterPlayerController == nullptr ? Cast<ABlasterPlayerController>(Controller) : BlasterPlayerController;
			BlasterGameMode->PlayerEliminated(this, BlasterPlayerController, AttackerController);
		}
	}

}

void ABlasterCharacter::MoveForward(float Value)
{
	if (bDisableGameplay) return;
	if (Controller != nullptr && Value != 0.f)
	{
		const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
		//Return an f vector only containing the rotation on the x axis, zero'd out on the pitch and roll
		const FVector Direction( FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X));
		//this only tells the system that movement input is applied
		//speed and direction belong in the char movement component
		AddMovementInput(Direction, Value);
	}
}

void ABlasterCharacter::MoveRight(float Value)
{
	if (bDisableGameplay) return;
	if (Controller != nullptr && Value != 0.f)
	{
		//same as move forward, BUT isolate Y axis
		const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
		const FVector Direction(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y));
		AddMovementInput(Direction, Value);
	}
}

void ABlasterCharacter::Turn(float Value)
{
	AddControllerYawInput(Value);
	
}

void ABlasterCharacter::LookUp(float Value)
{
	AddControllerPitchInput(Value);
}

void ABlasterCharacter::EquipButtonPressed()
{
	if (bDisableGameplay) return;
	if (InventoryComponent)
	{
		if (HasAuthority())
		{
			AWeapon* NewWeapon = OverlappingWeapon;
			InventoryComponent->AddItem(NewWeapon);
		}
		else
		{
			ServerEquipButtonPressed();
		}
		return;
	}
	if (OverlappingButton)
	{
		if (HasAuthority())
		{
			OverlappingButton->OnInitPress();
		}
		else
		{
			ServerEquipButtonPressed();
		}
	}
}

    //here we have the server rpc so non-authority can pickup weapon
void ABlasterCharacter::ServerEquipButtonPressed_Implementation()
{
	if (InventoryComponent)
	{
		AWeapon* NewWeapon = OverlappingWeapon;
		InventoryComponent->AddItem(NewWeapon);
		return;
	}
	if (OverlappingButton)
	{
		OverlappingButton->OnInitPress();
		return;
	}
}


void ABlasterCharacter::EquipButtonReleased()
{
	if (bDisableGameplay) return;
	if (HasAuthority())
	{
		if (OverlappingButton)
		{
			OverlappingButton->OnRelease();
		}
	}
	else
	{
		ServerEquipButtonReleased();
	}
	
}

    //here we have the server rpc so non-authority can pickup weapon
void ABlasterCharacter::ServerEquipButtonReleased_Implementation()
{
	if (OverlappingButton)
	{
		OverlappingButton->OnRelease();
	}
}

void ABlasterCharacter::ItemShuffleLeft()
{ //this is so fucking stupid
		ServerShuffleItem(true);
}

void ABlasterCharacter::ItemShuffleRight()
{
		ServerShuffleItem(false);
}

void ABlasterCharacter::ServerShuffleItem_Implementation(bool IsLeft)
{
	if (InventoryComponent)
	{
		InventoryComponent->ShuffleItem(IsLeft);
	}
}

//luckily, since we use the built in crouch functionality, crouching is already set up to be replicated
void ABlasterCharacter::CrouchButtonPressed()
{ 
	if (bDisableGameplay) return;

	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Crouch();
	}

}

void ABlasterCharacter::ReloadButtonPressed()
{
	if (bDisableGameplay) return;

	if (Combat)
	{
		Combat->Reload();
	}
}



void ABlasterCharacter::AimButtonPressed()
{
	if (bDisableGameplay) return;

	//replication of bAiming handled from server->client in CombatComponent.cpp DOREPLIFETIME
	//replication of client-> server bAiming handled with RPC in 
	if (Combat)
	{
		//SP imp
		//Combat->bAiming = true;

		//mp imp
		Combat->SetAiming(true);

	}
}
void ABlasterCharacter::AimButtonReleased()
{
	if (bDisableGameplay) return;

	if (Combat)
	{
		//Combat->bAiming = false;
		Combat->SetAiming(false);
	}
}	

float ABlasterCharacter::CalculateSpeed()
{
	FVector Velocity = GetVelocity();
	Velocity.Z = 0.f;
	return Velocity.Size();
}

void ABlasterCharacter::AimOffset(float DeltaTime)
{
	if (bDisableGameplay) return;
	if (Combat && Combat->EquippedWeapon == nullptr)
	{
		return;
	}

	bool bIsInAir = GetCharacterMovement()->IsFalling();
	float Speed = CalculateSpeed();
	if (Speed == 0.f && !bIsInAir) //standing still && not jumping
	{
		bRotateRootBone = true;
		FRotator CurrentAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
		FRotator DeltaAimRotation = UKismetMathLibrary::NormalizedDeltaRotator(CurrentAimRotation, StartingAimRotation);
		AO_Yaw = DeltaAimRotation.Yaw;
		if (TurningInPlace == ETurningInPlace::ETIP_NotTurning)
		{
			InterpAO_Yaw = AO_Yaw;
		}
		bUseControllerRotationYaw = true;
		TurnInPlace(DeltaTime);
	}
	if (Speed > 0.f || bIsInAir) //running || jumping
	{
		bRotateRootBone = false;
		bUseControllerRotationYaw = true;
		StartingAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
		AO_Yaw = 0.f;
		TurningInPlace = ETurningInPlace::ETIP_NotTurning;
	}

	CalculateAO_Pitch();
}

void ABlasterCharacter::CalculateAO_Pitch()
{
	AO_Pitch = GetBaseAimRotation().Pitch;
	if (AO_Pitch > 90.f && !IsLocallyControlled())
	{
		//map pitch from [270, 360) to [-90, 0)
		//bitwise compression for var replication takes negative numbers 
		//and loops them back to 360 descending
		//this is demonstrated in the CharMoveComp
		//but for our aim offset blendspaces we must correct that
		FVector2D InRange(270.f, 360.f);
		FVector2D OutRange(-90.f, 0.f);
		AO_Pitch = FMath::GetMappedRangeValueClamped(InRange, OutRange, AO_Pitch);
	}
}

void ABlasterCharacter::SimProxiesTurn()
{
	if (Combat == nullptr || Combat->EquippedWeapon == nullptr) return;
	bRotateRootBone = false;

	float Speed = CalculateSpeed();
	if (Speed > 0.f)
	{
		TurningInPlace = ETurningInPlace::ETIP_NotTurning;
		return;
	}

	ProxyRotationLastFrame = ProxyRotation;
	ProxyRotation = GetActorRotation();
	ProxyYaw = UKismetMathLibrary::NormalizedDeltaRotator(ProxyRotation, ProxyRotationLastFrame).Yaw;

	//UE_LOG(LogTemp, Warning, TEXT("ProxyYaw: %f"), ProxyYaw);

	if (FMath::Abs(ProxyYaw) > TurnThreshold)
	{
		if (ProxyYaw > TurnThreshold)
		{
			TurningInPlace = ETurningInPlace::ETIP_Right;
		}
		else if (ProxyYaw < -TurnThreshold)
		{
			TurningInPlace = ETurningInPlace::ETIP_Left;
		}
		else
		{
			TurningInPlace = ETurningInPlace::ETIP_NotTurning;
		}
		return;
	}
	TurningInPlace = ETurningInPlace::ETIP_NotTurning;

}

void ABlasterCharacter::Jump()
{
	if (bDisableGameplay) return;

	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Super::Jump();
	}
}

void ABlasterCharacter::FireButtonPressed()
{
	if (bDisableGameplay) return;

	if (Combat)
	{
		Combat->FireButtonPressed(true);
	}
}

void ABlasterCharacter::FireButtonReleased()
{
	if (bDisableGameplay) return;

	if (Combat)
	{
		Combat->FireButtonPressed(false);
	}
}

void ABlasterCharacter::TurnInPlace(float DeltaTime)
{
	//UE_LOG(LogTemp, Warning, TEXT("AO_Yaw: %f"), AO_Yaw);
	if (AO_Yaw > 45.f)
	{
		TurningInPlace = ETurningInPlace::ETIP_Right;
	}
	else if (AO_Yaw < -45.f)
	{
		TurningInPlace = ETurningInPlace::ETIP_Left;
	}
	if (TurningInPlace != ETurningInPlace::ETIP_NotTurning)
	{
		InterpAO_Yaw = FMath::FInterpTo(InterpAO_Yaw, 0.f, DeltaTime, 4.f);
		AO_Yaw = InterpAO_Yaw;
		if (FMath::Abs(AO_Yaw) < 15.f)
		{
			TurningInPlace = ETurningInPlace::ETIP_NotTurning;
			StartingAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);

		}
	}
}

//void ABlasterCharacter::MulticastHit_Implementation()
//{
//	PlayHitReactMontage();
//}

void ABlasterCharacter::HideCameraIfCharacterClose()
{
	if (!IsLocallyControlled()) return;
	if ((FollowCamera->GetComponentLocation() - GetActorLocation()).Size() < CameraThreshold)
	{
		GetMesh()->SetVisibility(false);
		if (Combat && Combat->EquippedWeapon && Combat->EquippedWeapon->GetWeaponMesh())
		{
			//Combat->EquippedWeapon->GetWeaponMesh()->bOwnerNoSee = true;
		}
	}
	else
	{
		GetMesh()->SetVisibility(true);
		if (Combat && Combat->EquippedWeapon && Combat->EquippedWeapon->GetWeaponMesh())
		{
			Combat->EquippedWeapon->GetWeaponMesh()->bOwnerNoSee = false;
		}
	}
}



void ABlasterCharacter::OnRep_Health()
{
	UpdateHUDHealth();
	PlayHitReactMontage();
}

void ABlasterCharacter::UpdateHUDHealth()
{
	BlasterPlayerController = BlasterPlayerController == nullptr ? Cast<ABlasterPlayerController>(Controller) : BlasterPlayerController;
	if (BlasterPlayerController)
	{
		BlasterPlayerController->SetHUDHealth(Health, MaxHealth);
	}
}

void ABlasterCharacter::PollInit()
{
	if (BlasterPlayerState == nullptr)
	{
		//this isn't a cast? something about "templates"
		BlasterPlayerState = GetPlayerState<ABlasterPlayerState>();
		if (BlasterPlayerState)
		{
			BlasterPlayerState->AddToScore(0.f);
			BlasterPlayerState->AddToDebt(0.f);
		}
	}
}

void ABlasterCharacter::ManageVisualInteractionTargetLocations()
{
	FVector_NetQuantize CurrentHitTarget = GetHitTarget();
	if (LastHitTarget != CurrentHitTarget)
	{
		ServerSetInteractTarget(CurrentHitTarget);
		SetInteractAndVisualTargetSphereLocation(CurrentHitTarget);
		LastHitTarget = CurrentHitTarget;
	}
}

void ABlasterCharacter::SetInteractAndVisualTargetSphereLocation(FVector_NetQuantize Target)
{
	FVector_NetQuantize CharacterLocation = GetActorLocation();
	float Distance = FVector::Dist(CharacterLocation, Target);
	if (IsLocallyControlled() || HasAuthority())
	{
		//UE_LOG(LogTemp, Log, TEXT("%f"), Distance);
		if (Distance < 156.f)
		{
			InteractSphere->SetWorldLocation(Target);
		}
		else
		{
			InteractSphere->SetWorldLocation(CharacterLocation);
		}
	}
	VisualTargetSphere->SetWorldLocation(Target);
}


void ABlasterCharacter::ServerSetInteractTarget_Implementation(FVector_NetQuantize InteractTarget)
{
	InteractTargetLocation = InteractTarget;
	SetInteractAndVisualTargetSphereLocation(InteractTarget);
}

void ABlasterCharacter::OnRep_InteractTargetLocation()
{
	SetInteractAndVisualTargetSphereLocation(InteractTargetLocation);
}

void ABlasterCharacter::SetOverlappingButton(AMyButton* Button)
{
	//this exact if statement disables the widget for the server on exit. wjat
	if (OverlappingButton)
	{
		//OverlappingButton->ShowPickupWidget(false);
	}
	OverlappingButton = Button;
	//logic to only show widget for the character controlling the pawn
	//onsphereoverlap is only called w/in the server... what are we to do?
	//check if THIS function being called is being called by the character being controlled
	if (IsLocallyControlled())
	{
		if (OverlappingButton)
		{
			//OverlappingButton->ShowPickupWidget(true);
		}
	}
}

void ABlasterCharacter::OnRep_OverlappingButton(AMyButton* LastButton)
{
	if (OverlappingButton) //this is the new var, LatWeapon is the old one
	{
		//LastButton->ShowPickupWidget(true);
	}
	if (LastButton)//if lastweapon is not null then it is implied it now is... this seems like overlapping weapons could cause problems
	{
		//LastButton->ShowPickupWidget(false);
	}
}

void ABlasterCharacter::SetOverlappingWeapon(AWeapon *Weapon)
{	
	//this exact if statement disables the widget for the server on exit. wjat
	if (OverlappingWeapon)
	{
		OverlappingWeapon->ShowPickupWidget(false);
	}
	OverlappingWeapon = Weapon;
	//logic to only show widget for the character controlling the pawn
	//onsphereoverlap is only called w/in the server... what are we to do?
	//check if THIS function being called is being called by the character being controlled
	if (IsLocallyControlled())
	{
		if (OverlappingWeapon)
		{
			OverlappingWeapon->ShowPickupWidget(true);
		}
	}
}

//repnotify is not called on server. 
//gosh this is gee golly hard lmao
void ABlasterCharacter::OnRep_OverlappingWeapon(AWeapon* LastWeapon)
{
	if (OverlappingWeapon) //this is the new var, LatWeapon is the old one
	{
		OverlappingWeapon->ShowPickupWidget(true);
	}
	if (LastWeapon)//if lastweapon is not null then it is implied it now is... this seems like overlapping weapons could cause problems
	{
		LastWeapon->ShowPickupWidget(false);
	}
}


//currently called by anim instance
bool ABlasterCharacter::IsWeaponEquipped()
{
    return (Combat && Combat->EquippedWeapon);
}

//another getter accessed by anim instance
bool ABlasterCharacter::IsAiming()
{
    return(Combat && Combat->bAiming);
}

AWeapon* ABlasterCharacter::GetEquippedWeapon()
{
	if (Combat == nullptr) return nullptr;
	return Combat->EquippedWeapon;
}

FVector ABlasterCharacter::GetHitTarget() const
{
	if (Combat == nullptr) return FVector();
    return Combat->HitTarget;
}

ECombatState ABlasterCharacter::GetCombatState()
{
	if (Combat == nullptr) return ECombatState::ECS_MAX;
	return Combat->CombatState;
}

