// Fill out your copyright notice in the Description page of Project Settings.


#include "Fountain.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"
#include "Blaster/Limb/Limb.h"
#include "Net/UnrealNetwork.h"
#include "Particles/ParticleSystemComponent.h"
#include "Particles/ParticleSystem.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Blaster/Blaster.h"
#include "Net/UnrealNetwork.h"

// Sets default values
AFountain::AFountain()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	FountainTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("FountainTrigger"));
	FountainTrigger->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
	FountainTrigger->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	FountainTrigger->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	FountainTrigger->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);
	FountainTrigger->SetCollisionResponseToChannel(ECollisionChannel::ECC_WorldStatic, ECollisionResponse::ECR_Block);
	FountainTrigger->SetCollisionResponseToChannel(ECollisionChannel::ECC_WorldDynamic, ECollisionResponse::ECR_Block);
	FountainTrigger->SetCollisionResponseToChannel(ECC_SkeletalMesh, ECollisionResponse::ECR_Block);
}

void AFountain::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

// Called when the game starts or when spawned
void AFountain::BeginPlay()
{
	Super::BeginPlay();
	
	if (HasAuthority())
	{//hit events handled only on server
		FountainTrigger->OnComponentBeginOverlap.AddDynamic(this, &AFountain::OnFountainEntry);
//CollisionBox->OnComponentBeginOverlap.AddDynamic(this, &AFountain::OnFountainEntry);
	}
}

void AFountain::OnFountainEntry(
	UPrimitiveComponent* OverlappedComponent, 
	AActor* OtherActor, 
	UPrimitiveComponent* OtherComp, 
	int32 OtherBodyIndex, 
	bool bFromSweep, 
	const FHitResult& SweepResult)
{
	UE_LOG(LogTemp, Log, TEXT("Collision Detected"));
	ALimb* LimbCharacter = Cast<ALimb>(OtherActor);
	if (LimbCharacter && !bFountainEntered)
	{
		bFountainEntered = false;
		Multicast_OnFountainEntry();
	}	
}

void AFountain::Multicast_OnFountainEntry_Implementation()
{
		SprayBlood();
}

void AFountain::SprayBlood()
{
	if (BloodSprayParticles)
	{ //include gameplaystatics
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), BloodSprayParticles, GetActorTransform());
	}
	if (BloodSpraySound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, BloodSpraySound, GetActorLocation());
	}
}

// Called every frame
void AFountain::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}
