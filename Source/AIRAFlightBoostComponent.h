#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AIRAFlightBoostComponent.generated.h"

class APawn;
class APlayerController;
class UFloatingPawnMovement;
class UGameViewportClient;
class ULocalPlayer;
class SWidget;

/** Independent local-player afterburner. Add once to BP_BotPawn. */
UCLASS(ClassGroup=(Movement), meta=(BlueprintSpawnableComponent, DisplayName="AIRAFlightBoostComponent"))
class DRONE_API UAIRAFlightBoostComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UAIRAFlightBoostComponent();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="A.I.R.A.Flight Boost", meta=(ClampMin="0.01"))
    float MaxBoostEnergy = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="A.I.R.A.Flight Boost", meta=(ClampMin="1.0"))
    float BoostSpeedMultiplier = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="A.I.R.A.Flight Boost", meta=(ClampMin="1.0"))
    float BoostAccelerationMultiplier = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="A.I.R.A.Flight Boost", meta=(ClampMin="0.01", Units="s"))
    float ContinuousBoostDuration = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="A.I.R.A.Flight Boost", meta=(ClampMin="0.0", Units="s"))
    float RechargeDelay = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="A.I.R.A.Flight Boost", meta=(ClampMin="0.01", Units="s"))
    float FullRechargeDuration = 15.0f;

    UFUNCTION(BlueprintPure, Category="A.I.R.A.Flight Boost")
    bool IsBoostActive() const { return bBoostActive; }

    UFUNCTION(BlueprintPure, Category="A.I.R.A.Flight Boost")
    float GetBoostEnergy() const { return BoostEnergy; }

    UFUNCTION(BlueprintPure, Category="A.I.R.A.Flight Boost")
    float GetBoostEnergyPercent() const;

    UFUNCTION(BlueprintPure, Category="A.I.R.A.Flight Boost")
    float GetBoostDrainRate() const;

    UFUNCTION(BlueprintPure, Category="A.I.R.A.Flight Boost")
    float GetBoostRechargeRate() const;

    bool IsRecharging() const { return bRecharging; }
    void ActivateBoost();
    void DeactivateBoost();
    void UpdateBoostEnergy(float DeltaTime);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="A.I.R.A.Flight Boost", meta=(AllowPrivateAccess="true"))
    float BoostEnergy = 0.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="A.I.R.A.Flight Boost", meta=(AllowPrivateAccess="true"))
    bool bBoostActive = false;

    TWeakObjectPtr<APawn> PlayerPawn;
    TWeakObjectPtr<UFloatingPawnMovement> Movement;
    TWeakObjectPtr<UGameViewportClient> HUDViewport;
    TWeakObjectPtr<ULocalPlayer> HUDPlayer;
    TSharedPtr<SWidget> HUDWidget;
    float OriginalMaxSpeed = 0.0f;
    float OriginalAcceleration = 0.0f;
    float OriginalDeceleration = 0.0f;
    float OriginalTurningBoost = 0.0f;
    float RechargeDelayRemaining = 0.0f;
    bool bOriginalValuesCaptured = false;
    bool bRequiresRelease = false;
    bool bRecharging = false;

    APlayerController* GetLocalController() const;
    bool HasMovement() const;
    void RestoreMovement();
    void UpdateHUD(APlayerController* Controller);
    void RemoveHUD();
};
