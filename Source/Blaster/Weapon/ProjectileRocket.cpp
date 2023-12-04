// Fill out your copyright notice in the Description page of Project Settings.


#include "ProjectileRocket.h"
#include "Kismet/GameplayStatics.h"

AProjectileRocket::AProjectileRocket()
{
	RocketMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Rocket Mesh"));
	RocketMesh->SetupAttachment(RootComponent);
	RocketMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

}

void AProjectileRocket::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	APawn* FiringPawn = GetInstigator();
	if (FiringPawn)
	{
		AController* FiringController = FiringPawn->GetController();
		if (FiringController)
		{
			UGameplayStatics::ApplyRadialDamageWithFalloff(
				this,//world context object
				Damage,//base damage
				10.f,//minimum damage (at outer radius)
				GetActorLocation(),//origin location
				200.f,//inner radius
				500.f,//outer radius
				1.f, //damage falloff
				UDamageType::StaticClass(),//damage type class
				TArray<AActor*>(),//ignore actors (empty array)
				this,//damage causer
				FiringController //InstigatorController
			);
		}
	}

	Super::OnHit(HitComp, OtherActor, OtherComp, NormalImpulse, Hit);
}

