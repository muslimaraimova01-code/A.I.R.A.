#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "GameFramework/SaveGame.h"
#include "GameFramework/GameModeBase.h"
#include "Framework/Application/IInputProcessor.h"
#include "DroneMenuSystem.generated.h"

USTRUCT(BlueprintType)
struct FDroneSaveMeta
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) FString DisplayName;
    UPROPERTY(SaveGame) FString SlotName;
    UPROPERTY(SaveGame) FString LevelName;
    UPROPERTY(SaveGame) FString Timestamp;
};

UCLASS()
class DRONE_API UDroneSaveGame : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY(SaveGame) FString DisplayName;
    UPROPERTY(SaveGame) FString SlotName;
    UPROPERTY(SaveGame) FString Timestamp;
    UPROPERTY(SaveGame) FString LevelName;
    UPROPERTY(SaveGame) FTransform PlayerTransform;
    UPROPERTY(SaveGame) double Energy = 0.0;
    UPROPERTY(SaveGame) bool bHasEnergy = false;
};

UCLASS()
class DRONE_API UDroneSaveIndex : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY(SaveGame) TArray<FDroneSaveMeta> Saves;
    UPROPERTY(SaveGame) int32 NextSlotNumber = 1;
};

UCLASS()
class DRONE_API ADroneMenuGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ADroneMenuGameMode();
};

class FDroneMenuInputProcessor;
class SDroneMenuRoot;
class SDroneMissionHUD;
class SDroneMissionComplete;
class UTexture2D;
class USoundBase;

enum class EDroneMissionDeviceState : uint8
{
    Locked,
    Unlocking,
    Unlocked
};

struct FDroneMissionDevice
{
    TWeakObjectPtr<AActor> Actor;
    EDroneMissionDeviceState State = EDroneMissionDeviceState::Locked;
    double StateStartTime = 0.0;
    float InteractionRange = 0.0f;
    FVector InteractionCenter = FVector::ZeroVector;
    bool bPrimary = false;
    bool bPlayerWasInRange = false;
};

UCLASS(Config=Game)
class DRONE_API UDroneGameInstance : public UGameInstance
{
    GENERATED_BODY()
public:
    virtual void Init() override;
    virtual void OnStart() override;
    virtual void Shutdown() override;

    void HandleEscape();
    void ShowMainMenu();
    void ShowPauseMenu();
    UFUNCTION(BlueprintCallable, Category="Drone|Game Flow")
    void BeginGameOver();
    void ShowSettings(bool bReturnToPause);
    void ShowSaveDialog();
    void ShowLoadDialog(bool bReturnToPause);
    void HideMenu(bool bResumeGame);
    void StartGame();
    bool IsStartGamePending() const { return bStartGamePending; }
    bool SaveGame(const FString& DisplayName);
    void LoadGame(const FString& SlotName);
    void QuitGame();
    void ApplySettings(float Master, float Music, float Sfx, const FString& WindowMode, const FString& Resolution);
    void SetMusicVolume(float Volume);
    void SetSFXVolume(float Volume);
    float GetMusicVolume() const { return MusicVolume; }
    float GetSFXVolume() const { return SFXVolume; }
    void PlayMenuClick();
    TArray<FDroneSaveMeta> GetSaveMetas() const;
    bool IsMainMenuWorld() const;
    UTexture2D* GetMainMenuBackgroundTexture() const { return MainMenuBackgroundTexture; }
private:
    friend class SDroneMissionHUD;
    bool EnsureMainMenuBackgroundTexture();
    void StartGameplayMusic(UWorld* World);
    void StopGameplayMusic();
    void OnPostLoadMap(UWorld* LoadedWorld);
    bool TryApplyPendingSave(float DeltaTime);
    bool TickMenuPresentation(float DeltaTime);
    bool TickPlayerDeathMonitor(float DeltaTime);
    bool TickGameOver(float DeltaTime);
    void BeginGameOverForActor(AActor* PlayerActor);
    bool TickMissionBriefing(float DeltaTime);
    void QueueMissionBriefing();
    void HideMissionBriefing();
    void InitializeMissionInteraction(UWorld* World);
    void ShutdownMissionInteraction();
    bool TickMissionInteraction(float DeltaTime);
    void ShowMissionObjectiveBriefing();
    void HideMissionOverlay();
    void ShowMissionComplete();
    void HideMissionComplete();
    void CompleteCurrentAccess();
    void PlayMissionInteractionSound(USoundBase* Sound) const;
    void InstallMenu(TSharedRef<SWidget> Widget, bool bPauseWorld);
    void SetupMenuPresentation(UWorld* World);
    bool ReadEnergy(APawn* Pawn, double& OutEnergy) const;
    bool WriteEnergy(APawn* Pawn, double Energy) const;
    UDroneSaveIndex* LoadIndex() const;
    bool SaveIndex(UDroneSaveIndex* Index) const;
    bool RedirectInitialGameplayWorldToMainMenu();
    void ObservePlayerPawn();
    void CompleteStartGame();

    UFUNCTION()
    void OnObservedPlayerDestroyed(AActor* DestroyedActor);

    TSharedPtr<SWidget> ActiveMenu;
    TSharedPtr<SWidget> MissionBriefing;
    UPROPERTY() TObjectPtr<UTexture2D> MainMenuBackgroundTexture;
    TSharedPtr<FDroneMenuInputProcessor> InputProcessor;
    TStrongObjectPtr<UDroneSaveGame> PendingLoad;
    FTSTicker::FDelegateHandle PendingLoadTicker;
    FTSTicker::FDelegateHandle PresentationTicker;
    FTSTicker::FDelegateHandle PlayerDeathMonitorTicker;
    FTSTicker::FDelegateHandle GameOverTicker;
    FTSTicker::FDelegateHandle MissionBriefingTicker;
    FTSTicker::FDelegateHandle MissionInteractionTicker;
    TWeakObjectPtr<AActor> PresentationDrone;
    FVector PresentationOrigin = FVector::ZeroVector;
    float PresentationTime = 0.f;
    bool bPauseMenuOpen = false;
    bool bReturningToPause = false;
    bool bGameplayStartedFromMainMenu = false;
    bool bStartGamePending = false;
    FTimerHandle StartGameDelayTimer;
    bool bStartupRedirectInProgress = false;
    bool bGameOverInProgress = false;
    bool bGameOverScreenVisible = false;
    double GameOverFreezeTime = 0.0;
    double GameOverReturnTime = 0.0;
    double MissionBriefingStartTime = 0.0;
    bool bShowMissionBriefingAfterLoad = false;
    bool bStartupVoicePlayed = false;
    TWeakObjectPtr<APawn> ObservedPlayerPawn;
    TArray<FDroneMissionDevice> MissionRelays;
    FDroneMissionDevice MissionPrimary;
    FDroneMissionDevice* ActiveMissionDevice = nullptr;
    TSharedPtr<SWidget> MissionOverlay;
    TSharedPtr<SWidget> MissionCompleteOverlay;
    double MissionObjectiveBriefingEndTime = 0.0;
    double MissionStatusEndTime = 0.0;
    FString MissionStatusTitle;
    FString MissionStatusBody;
    int32 UnlockedRelayCount = 0;
    bool bMissionInteractionReady = false;
    bool bMissionObjectiveStarted = false;
    bool bMissionCompleted = false;
    bool bTransmissionFinalMessageShown = false;
    double TransmissionCompleteMessageTime = 0.0;
    int32 MissionTransmissionStage = 0;
    double MissionSequenceNextTime = 0.0;
    double MissionCompleteReturnTime = 0.0;
    bool bMissionPawnMissingLogged = false;
    bool bMissionHudMissingLogged = false;
    double MissionDebugLogTime = 0.0;
    TWeakObjectPtr<APawn> MissionControlledPawn;
    UPROPERTY(Transient) TObjectPtr<class UAudioComponent> MenuMusicComponent;
    UPROPERTY(Transient) TObjectPtr<class UAudioComponent> GameplayMusicComponent;
    UPROPERTY(Transient) TObjectPtr<class USoundBase> MissionReadySound;
    UPROPERTY(Transient) TObjectPtr<class USoundBase> MissionSuccessSound;
    float MusicVolume = 0.25f;
    float SFXVolume = 1.0f;
};
