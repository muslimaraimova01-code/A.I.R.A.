#include "AIRAFlightBoostComponent.h"
#include "AIRAFlightBoostHUD.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogAIRAFlightBoost, Log, All);

UAIRAFlightBoostComponent::UAIRAFlightBoostComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bTickEvenWhenPaused = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UAIRAFlightBoostComponent::BeginPlay()
{
    Super::BeginPlay();
    PlayerPawn = Cast<APawn>(GetOwner());
    if (!PlayerPawn.IsValid())
    {
        UE_LOG(LogAIRAFlightBoost, Warning, TEXT("Flight Boost requires a pawn owner; disabled."));
        SetComponentTickEnabled(false);
        return;
    }

    // Reject duplicate instances before capturing possibly modified movement settings.
    TArray<UAIRAFlightBoostComponent*> BoostComponents;
    GetOwner()->GetComponents(BoostComponents);
    if (BoostComponents.Num() > 0 && BoostComponents[0] != this)
    {
        UE_LOG(LogAIRAFlightBoost, Warning, TEXT("Duplicate Flight Boost component disabled."));
        SetComponentTickEnabled(false);
        return;
    }
    Movement = PlayerPawn->FindComponentByClass<UFloatingPawnMovement>();
    if (!Movement.IsValid())
    {
        UE_LOG(LogAIRAFlightBoost, Error, TEXT("Flight Boost requires UFloatingPawnMovement; disabled without changing movement."));
        SetComponentTickEnabled(false);
        return;
    }

    OriginalMaxSpeed = Movement->MaxSpeed;
    OriginalAcceleration = Movement->Acceleration;
    OriginalDeceleration = Movement->Deceleration;
    OriginalTurningBoost = Movement->TurningBoost;
    bOriginalValuesCaptured = true;
    UE_LOG(LogAIRAFlightBoost, Log, TEXT("AIRA Flight Boost Movement captured: MaxSpeed=%g Acceleration=%g"), OriginalMaxSpeed, OriginalAcceleration);
    BoostEnergy = FMath::Max(0.0f, MaxBoostEnergy);
    // Poll Left Shift and adjust settings before the movement component consumes input.
    Movement->AddTickPrerequisiteComponent(this);
    UE_LOG(LogAIRAFlightBoost, Log, TEXT("AIRA Flight Boost Initialized. Energy: %g/%g"), BoostEnergy, MaxBoostEnergy);
    UpdateHUD(GetLocalController());
}

APlayerController* UAIRAFlightBoostComponent::GetLocalController() const
{
    APlayerController* PC = PlayerPawn.IsValid() ? Cast<APlayerController>(PlayerPawn->GetController()) : nullptr;
    return PC && PC->IsLocalController() && PC->GetPawn() == PlayerPawn.Get() ? PC : nullptr;
}

void UAIRAFlightBoostComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    APlayerController* PC = GetLocalController();
    UpdateHUD(PC);
    const bool bHeld = PC && PC->IsInputKeyDown(EKeys::LeftShift);
    if (!bHeld) bRequiresRelease = false;

    // Pause/menu/mission input locks suspend boost without touching their input state.
    const bool bPaused = !GetWorld() || GetWorld()->IsPaused();
    const bool bCanBoost = PC && !bPaused && !PC->IsMoveInputIgnored()
        && Movement.IsValid() && Movement->IsActive() && Movement->IsComponentTickEnabled();
    if (bCanBoost && bHeld && !bRequiresRelease && BoostEnergy > 0.0f) ActivateBoost();
    else DeactivateBoost();

    // Pause ticks only restore movement/respond to release; battery time stays in game time.
    if (!bPaused) UpdateBoostEnergy(DeltaTime);
}

void UAIRAFlightBoostComponent::ActivateBoost()
{
    APlayerController* PC = GetLocalController();
    if (bBoostActive || !bOriginalValuesCaptured || !Movement.IsValid() || BoostEnergy <= 0.0f
        || bRequiresRelease || !PC || !PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsMoveInputIgnored()
        || !GetWorld() || GetWorld()->IsPaused() || !Movement->IsActive() || !Movement->IsComponentTickEnabled()) return;

    Movement->MaxSpeed = OriginalMaxSpeed * FMath::Max(1.0f, BoostSpeedMultiplier);
    Movement->Acceleration = OriginalAcceleration * FMath::Max(1.0f, BoostAccelerationMultiplier);
    bBoostActive = true;
    bRecharging = false;
    UE_LOG(LogAIRAFlightBoost, Log, TEXT("AIRA Flight Boost Activated. MaxSpeed: %g -> %g; Acceleration: %g -> %g"), OriginalMaxSpeed, Movement->MaxSpeed, OriginalAcceleration, Movement->Acceleration);
}

void UAIRAFlightBoostComponent::RestoreMovement()
{
    if (bOriginalValuesCaptured && Movement.IsValid())
    {
        Movement->MaxSpeed = OriginalMaxSpeed;
        Movement->Acceleration = OriginalAcceleration;
        Movement->Deceleration = OriginalDeceleration;
        Movement->TurningBoost = OriginalTurningBoost;
    }
}

void UAIRAFlightBoostComponent::DeactivateBoost()
{
    if (!bBoostActive) return;
    bBoostActive = false;
    RestoreMovement();
    RechargeDelayRemaining = FMath::Max(0.0f, RechargeDelay);
    UE_LOG(LogAIRAFlightBoost, Log, TEXT("AIRA Flight Boost Deactivated. Restored MaxSpeed=%g Acceleration=%g"), OriginalMaxSpeed, OriginalAcceleration);
}

bool UAIRAFlightBoostComponent::HasMovement() const
{
    return Movement.IsValid() && (Movement->Velocity.SizeSquared() > FMath::Square(1.0f)
        || !Movement->GetPendingInputVector().IsNearlyZero());
}

float UAIRAFlightBoostComponent::GetBoostEnergyPercent() const
{
    return MaxBoostEnergy > 0.0f ? FMath::Clamp(BoostEnergy / MaxBoostEnergy, 0.0f, 1.0f) : 0.0f;
}

float UAIRAFlightBoostComponent::GetBoostDrainRate() const
{
    return FMath::Max(0.0f, MaxBoostEnergy) / FMath::Max(0.01f, ContinuousBoostDuration);
}

float UAIRAFlightBoostComponent::GetBoostRechargeRate() const
{
    return FMath::Max(0.0f, MaxBoostEnergy) / FMath::Max(0.01f, FullRechargeDuration);
}

void UAIRAFlightBoostComponent::UpdateBoostEnergy(float DeltaTime)
{
    if (!bOriginalValuesCaptured || DeltaTime <= 0.0f) return;
    const float Capacity = FMath::Max(0.0f, MaxBoostEnergy);
    BoostEnergy = FMath::Clamp(BoostEnergy, 0.0f, Capacity);
    if (bBoostActive)
    {
        if (HasMovement())
        {
            BoostEnergy = FMath::Max(0.0f, BoostEnergy - GetBoostDrainRate() * DeltaTime);
            RechargeDelayRemaining = FMath::Max(0.0f, RechargeDelay);
            if (BoostEnergy <= 0.0f)
            {
                bRequiresRelease = true;
                UE_LOG(LogAIRAFlightBoost, Log, TEXT("AIRA Flight Boost Energy depleted"));
                DeactivateBoost();
            }
        }
        return;
    }

    // Account only for the portion of this frame after the recharge delay expires.
    const float RechargeTime = FMath::Max(0.0f, DeltaTime - RechargeDelayRemaining);
    RechargeDelayRemaining = FMath::Max(0.0f, RechargeDelayRemaining - DeltaTime);
    if (RechargeTime > 0.0f && BoostEnergy < Capacity)
    {
        if (!bRecharging)
        {
            bRecharging = true;
            UE_LOG(LogAIRAFlightBoost, Log, TEXT("AIRA Flight Boost Recharge started"));
        }
        BoostEnergy = FMath::Min(Capacity, BoostEnergy + GetBoostRechargeRate() * RechargeTime);
        if (BoostEnergy >= Capacity)
        {
            bRecharging = false;
            UE_LOG(LogAIRAFlightBoost, Log, TEXT("AIRA Flight Boost Fully recharged"));
        }
    }
}

void UAIRAFlightBoostComponent::UpdateHUD(APlayerController* Controller)
{
    ULocalPlayer* LocalPlayer = Controller ? Controller->GetLocalPlayer() : nullptr;
    UGameViewportClient* Viewport = LocalPlayer ? LocalPlayer->ViewportClient : nullptr;
    if (HUDPlayer.Get() != LocalPlayer || HUDViewport.Get() != Viewport) RemoveHUD();
    if (!HUDWidget.IsValid() && LocalPlayer && Viewport && Viewport->GetGameViewportWidget().IsValid() && bOriginalValuesCaptured)
    {
        HUDWidget = SNew(SAIRAFlightBoostHUD).BoostComponent(this);
        HUDPlayer = LocalPlayer;
        HUDViewport = Viewport;
        // Share the viewport overlay ordering used by the existing mission HUD and menus.
        // Above normal HUD (500-510), below modal overlays and menus (9000-10000).
        Viewport->AddViewportWidgetContent(HUDWidget.ToSharedRef(), 1000);
        UE_LOG(LogAIRAFlightBoost, Log, TEXT("AIRA Flight Boost HUD mounted: viewport overlay, ZOrder=1000, right-center."));
    }
}

void UAIRAFlightBoostComponent::RemoveHUD()
{
    if (HUDWidget.IsValid() && HUDViewport.IsValid())
    {
        HUDViewport->RemoveViewportWidgetContent(HUDWidget.ToSharedRef());
        UE_LOG(LogAIRAFlightBoost, Log, TEXT("AIRA Flight Boost HUD removed."));
    }
    HUDWidget.Reset();
    HUDPlayer.Reset();
    HUDViewport.Reset();
}

void UAIRAFlightBoostComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    DeactivateBoost();
    RestoreMovement();
    if (Movement.IsValid()) Movement->RemoveTickPrerequisiteComponent(this);
    bOriginalValuesCaptured = false;
    RemoveHUD();
    Super::EndPlay(EndPlayReason);
}

void UAIRAFlightBoostComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
    DeactivateBoost();
    RestoreMovement();
    RemoveHUD();
    Super::OnComponentDestroyed(bDestroyingHierarchy);
}
