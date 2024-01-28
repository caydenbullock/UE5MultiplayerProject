// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blaster/Buttons/MyButton.h"
#include "AProcActor.h"
#include "Components/ChildActorComponent.h"

#include "Door.generated.h"

UCLASS()
class BLASTER_API ADoor : public AAProcActor
{
	GENERATED_BODY()
	
public:	

	ADoor();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	UPROPERTY(EditAnywhere, Category = "Door Mesh")
	USkeletalMeshComponent* DoorMesh;

	UFUNCTION()
	void KnobButtonPress();
	UFUNCTION()
	void KnobButtonRelease();
	UFUNCTION()
	void LockButtonPress();
	UFUNCTION()
	void KnobButtonDraggedOff();

	/*UPROPERTY(EditAnywhere, Category = "Buttons")
	AMyButton* DoorKnobButton;

	UPROPERTY(EditAnywhere,Category = "Buttons")
	AMyButton* LockButton;*/

	UPROPERTY(EditAnywhere, Category = "Components")
		UChildActorComponent* DoorKnobButtonComponent;

	UPROPERTY(EditAnywhere, Category = "Components")
		UChildActorComponent* DoorKnobButtonComponent2;

	UPROPERTY(EditAnywhere, Category = "Components")
		UChildActorComponent* LockButtonComponent;

protected:

	virtual void BeginPlay() override;

	void AttemptLockButtonCast();

public:	

	virtual void Tick(float DeltaTime) override;
	UPROPERTY(BlueprintReadOnly, Replicated)
	bool bIsOpen = false;
	UPROPERTY(BlueprintReadOnly, Replicated)
	bool bKnobTurning = false;
	UPROPERTY(BlueprintReadOnly, Replicated)
	bool bIsLocked = false;
	UPROPERTY(BlueprintReadWrite, Replicated)
	bool bAttemptOpen = false;

private:
	FTimerHandle TimerHandle_AttemptCast;
};
