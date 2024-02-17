
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DrawDebugHelpers.h"
#include "ProcParkPart.h"  
#include "AProcActor.h"
#include "ProcPark.generated.h"


UCLASS()
class BLASTER_API AProcPark : public AAProcActor
{
    GENERATED_BODY()

public:
    virtual void ProcGen() override;

protected:
    virtual void BeginPlay() override;

private:
    void InitializePark(FVector Location, FRotator Rotation);

    void SpawnNextObject(const FTransform& ParentTransform, int32 CurrentLifetime, int32 CurrentBranchCount);
    void ProcessSpawn(AProcParkPart* SpawnedPart, const FTransform& ParentTransform, int32 CurrentLifetime, int32 CurrentBranchCount, bool IsBranch, FObjectTypeInfo SelectedType);

    UPROPERTY(EditAnywhere, Category = "Procedural Generation")
        TArray<FObjectTypeInfo> ObjectTypes;

    UPROPERTY(EditAnywhere, Category = "Procedural Generation")
        TSubclassOf<AActor> TubeSlideBlueprint;

    FVector CalculateOutputLocation(const AActor* SpawnedObject, const FVector& OutputLocation, const FRotator& Rotation);

    FObjectTypeInfo GetRandomObjectTypeExcluding(const TSubclassOf<AProcParkPart>& ExcludeType);
    TSubclassOf<AProcParkPart> LastSpawnedParkPart = nullptr;

    const int32 MaxBranchCount = 4;
    int32 BranchCount = 0;

    bool IsWithinBounds(const FVector& Position, const FVector& MinBounds, const FVector& MaxBounds);


};
