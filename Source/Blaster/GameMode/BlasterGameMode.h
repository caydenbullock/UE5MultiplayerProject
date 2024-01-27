// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "BlasterGameMode.generated.h"

namespace MatchState
{
	extern BLASTER_API const FName Cooldown; //Match dieration has been reached. Display winner and begin cooldown timer
}

/**
 * 
 */
UCLASS()
class BLASTER_API ABlasterGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	ABlasterGameMode();
	virtual void Tick(float DeltaTime) override;
	virtual void PlayerEliminated(
		class ABlasterCharacter* ElimmedCharacter,
		class ABlasterPlayerController* VictimController,
		class ABlasterPlayerController* AttackerController
	);
	virtual void RequestRespawn(
		class ACharacter* ElimmedCharacter,
		AController* ElimmedController
	);

	UPROPERTY(EditDefaultsOnly)
	float WarmupTime = 10.f;	
	
	UPROPERTY(BlueprintReadWrite)
	float MatchTime= 120.f;

	UPROPERTY(EditDefaultsOnly)
	float CooldownTime= 10.f;

	float LevelStartingTime = 0.f;

	UPROPERTY()
	class AProcNeighborhood* ProcNeighborhood;

	UPROPERTY()
	uint32 RandomSeed;
protected:
	virtual void BeginPlay() override;
	virtual void InitGameState() override;
	virtual bool ReadyToStartMatch_Implementation() override;
	virtual void HandleMatchIsWaitingToStart() override;
	virtual void OnMatchStateSet() override;

private:
	float CountdownTime = 0.f;
public:
	FORCEINLINE float GetCountdownTime() const { return CountdownTime; }
};
