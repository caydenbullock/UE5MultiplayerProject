#include "MyButton.h"

AMyButton::AMyButton()
{
    PrimaryActorTick.bCanEverTick = true;

    AreaBox = CreateDefaultSubobject<UBoxComponent>(TEXT("AreaBox"));
    AreaBox->SetupAttachment(RootComponent);
    AreaBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    PickupWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("PickupWidget"));
    PickupWidget->SetupAttachment(RootComponent);
}

void AMyButton::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        AreaBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        AreaBox->SetCollisionResponseToChannel(ECollisionChannel::ECC_WorldDynamic, ECollisionResponse::ECR_Overlap);
        AreaBox->OnComponentBeginOverlap.AddDynamic(this, &AMyButton::OnSphereOverlap);
        AreaBox->OnComponentEndOverlap.AddDynamic(this, &AMyButton::OnSphereEndOverlap);
    }

    if (PickupWidget)
    {
        PickupWidget->SetVisibility(false);
    }
}

void AMyButton::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (IsActivelyPressed) WhileHeld();
}

void AMyButton::OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (OtherComp->GetName() == FString("InteractSphere"))
    {
        ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(OtherActor);
        if (BlasterCharacter)
        {
            BlasterCharacter->SetOverlappingButton(this); // Assuming SetOverlappingButton is implemented in ABlasterCharacter
        }
    }
}

void AMyButton::OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (OtherComp->GetName() == FString("InteractSphere"))
    {
        ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(OtherActor);
        if (BlasterCharacter)
        {
            BlasterCharacter->SetOverlappingButton(nullptr);
        }
    }

    if (IsActivelyPressed)
    {
        OnRelease();
    }
}

void AMyButton::OnInitPress()
{
    IsActivelyPressed = true;
    UE_LOG(LogTemp, Log, TEXT("i am pressed"));
}

void AMyButton::WhileHeld()
{

}

void AMyButton::OnRelease()
{
    IsActivelyPressed = false;
    UE_LOG(LogTemp, Log, TEXT("I am released"));
}
