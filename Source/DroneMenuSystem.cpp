#include "DroneMenuSystem.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "AudioDevice.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/StaticMeshActor.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/AudioComponent.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UObject/UnrealType.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Text/STextBlock.h"
#include "HAL/PlatformTime.h"
#include "Brushes/SlateImageBrush.h"
#include "Styling/CoreStyle.h"
#include "Misc/DateTime.h"
#include "TimerManager.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogDroneMenu, Log, All);

namespace DroneMenu
{
    const FLinearColor Cyan(0.04f, 0.78f, 1.f, 1.f);
    const FLinearColor CyanDim(0.03f, 0.35f, 0.48f, 0.82f);
    const FLinearColor White(0.72f, 0.92f, 1.f, 1.f);
    const FLinearColor Panel(0.005f, 0.018f, 0.035f, 0.94f);
    const FLinearColor Disabled(0.22f, 0.28f, 0.32f, 0.42f);
    const FName IndexSlot(TEXT("AIRA_SaveIndex"));

    static FText T(const FString& S) { return FText::FromString(S); }
}

class FDroneMenuInputProcessor : public IInputProcessor
{
public:
    explicit FDroneMenuInputProcessor(UDroneGameInstance* InOwner) : Owner(InOwner) {}
    virtual void Tick(const float, FSlateApplication&, TSharedRef<ICursor>) override {}
    virtual bool HandleKeyDownEvent(FSlateApplication&, const FKeyEvent& Event) override
    {
        if (Event.GetKey() == EKeys::Escape && Owner.IsValid())
        {
            Owner->HandleEscape();
            return true;
        }
        return false;
    }
    virtual bool HandleKeyUpEvent(FSlateApplication&, const FKeyEvent& Event) override
    {
        return false;
    }
private:
    TWeakObjectPtr<UDroneGameInstance> Owner;
};

class SDroneMenuRoot : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SDroneMenuRoot) {}
        SLATE_ARGUMENT(UDroneGameInstance*, Owner)
        SLATE_ARGUMENT(bool, PauseMode)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args)
    {
        Owner = Args._Owner;
        bPauseMode = Args._PauseMode;
        if (!bPauseMode && Owner && Owner->GetMainMenuBackgroundTexture())
        {
            UTexture2D* Texture = Owner->GetMainMenuBackgroundTexture();
            const FIntPoint ImportedSize = Texture->GetImportedSize();
            BackgroundBrush = MakeShared<FSlateImageBrush>(
                Texture,
                FVector2D(ImportedSize.X, ImportedSize.Y),
                FLinearColor::White);
        }
        ShowRoot();
    }

private:
    TSharedRef<SWidget> Title(const FString& Top, const FString& Bottom)
    {
        return SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(DroneMenu::T(Top)).ColorAndOpacity(DroneMenu::Cyan).Font(FCoreStyle::GetDefaultFontStyle("Bold", 30))]
            + SVerticalBox::Slot().AutoHeight().Padding(0,4,0,22)[SNew(STextBlock).Text(DroneMenu::T(Bottom)).ColorAndOpacity(DroneMenu::CyanDim).Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))];
    }

    TSharedRef<SWidget> Button(const FString& Label, TFunction<void()> Action, bool bEnabled=true, bool bIsMainMenuPlay=false)
    {
        return SNew(SBox).WidthOverride(310).HeightOverride(46).Padding(FMargin(0,3))
        [
            SNew(SButton)
            .IsEnabled_Lambda([this, bEnabled, bIsMainMenuPlay]()
            {
                return bEnabled && (!bIsMainMenuPlay || (Owner && !Owner->IsStartGamePending()));
            })
            .ButtonColorAndOpacity(bEnabled ? FLinearColor(0.015f,0.10f,0.15f,0.88f) : DroneMenu::Disabled)
            .ForegroundColor(bEnabled ? DroneMenu::White : DroneMenu::Disabled)
            .HAlign(HAlign_Left)
            .ContentPadding(FMargin(18,8))
            .OnClicked_Lambda([this, Action, bIsMainMenuPlay]()
            {
                // StartGame owns the main-menu PLAY click so its duplicate-click guard
                // runs before audio and before the delayed level transition are queued.
                if (Owner && !bIsMainMenuPlay) Owner->PlayMenuClick();
                Action();
                return FReply::Handled();
            })
            [SNew(STextBlock).Text(DroneMenu::T(TEXT("[  ") + Label + TEXT("  ]"))).Font(FCoreStyle::GetDefaultFontStyle("Bold", 15))]
        ];
    }

    void SetContent(TSharedRef<SWidget> Widget)
    {
        TSharedRef<SOverlay> Overlay = SNew(SOverlay);
        if (!bPauseMode && BackgroundBrush.IsValid())
        {
            Overlay->AddSlot()
            .HAlign(HAlign_Fill)
            .VAlign(VAlign_Fill)
            [
                SNew(SScaleBox)
                .Stretch(EStretch::ScaleToFill)
                .StretchDirection(EStretchDirection::Both)
                .Clipping(EWidgetClipping::ClipToBounds)
                [SNew(SImage).Image(BackgroundBrush.Get())]
            ];
        }
        if (bPauseMode)
        {
            Overlay->AddSlot()[SNew(SBorder).BorderBackgroundColor(FLinearColor(0.005f,0.008f,0.012f,0.80f))];
        }
        Overlay->AddSlot().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(FMargin(82,20))[Widget];
        Overlay->AddSlot().HAlign(HAlign_Fill).VAlign(VAlign_Top)[SNew(SBorder).BorderBackgroundColor(DroneMenu::CyanDim).Padding(FMargin(0,1))];
        ChildSlot[Overlay];
    }

    void ShowRoot()
    {
        TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
        Box->AddSlot().AutoHeight()[Title(bPauseMode ? TEXT("A.I.R.A // PAUSED") : TEXT("A.I.R.A"), bPauseMode ? TEXT("FLIGHT PROCESS SUSPENDED") : TEXT("AUTONOMOUS INTERVENTION & RECLAMATION AIRCRAFT"))];
        Box->AddSlot().AutoHeight()[Button(TEXT("PLAY"), [this](){ bPauseMode ? Owner->HideMenu(true) : Owner->StartGame(); }, true, !bPauseMode)];
        Box->AddSlot().AutoHeight()[Button(TEXT("SAVE GAME"), [this](){ Owner->ShowSaveDialog(); }, bPauseMode)];
        Box->AddSlot().AutoHeight()[Button(TEXT("LOAD GAME"), [this](){ Owner->ShowLoadDialog(bPauseMode); })];
        Box->AddSlot().AutoHeight()[Button(TEXT("SETTING"), [this](){ Owner->ShowSettings(bPauseMode); })];
        Box->AddSlot().AutoHeight()[Button(TEXT("EXIT"), [this](){ ShowExitConfirm(); })];
        Box->AddSlot().AutoHeight().Padding(0,24,0,0)[SNew(STextBlock).Text(DroneMenu::T(bPauseMode ? TEXT("A.I.R.A. LINK // SESSION HELD") : TEXT("A.I.R.A. // INITIALIZATION NODE"))).ColorAndOpacity(DroneMenu::CyanDim).Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))];
        SetContent(Box);
    }

    void ShowExitConfirm()
    {
        TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
        Box->AddSlot().AutoHeight()[Title(TEXT("EXIT GAME?"), TEXT("UNSAVED PROGRESS MAY BE LOST"))];
        Box->AddSlot().AutoHeight()[Button(TEXT("YES"), [this](){ Owner->QuitGame(); })];
        Box->AddSlot().AutoHeight()[Button(TEXT("NO"), [this](){ ShowRoot(); })];
        SetContent(Box);
    }

public:
    void ShowSave()
    {
        TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
        Box->AddSlot().AutoHeight()[Title(TEXT("SAVE GAME"), TEXT("CREATE NEW MEMORY RECORD"))];
        Box->AddSlot().AutoHeight().Padding(0,0,0,12)
        [SAssignNew(SaveName, SEditableTextBox).Text(DroneMenu::T(TEXT("Save 01"))).HintText(DroneMenu::T(TEXT("SAVE NAME"))).MinDesiredWidth(310).ForegroundColor(DroneMenu::White)];
        Box->AddSlot().AutoHeight()[Button(TEXT("SAVE"), [this](){ ShowSaveResult(Owner->SaveGame(SaveName->GetText().ToString())); })];
        Box->AddSlot().AutoHeight()[Button(TEXT("CANCEL"), [this](){ ShowRoot(); })];
        SetContent(Box);
    }

    void ShowSaveResult(bool bSucceeded)
    {
        TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
        Box->AddSlot().AutoHeight()[Title(bSucceeded ? TEXT("SAVE COMPLETE") : TEXT("SAVE FAILED"), bSucceeded ? TEXT("MEMORY RECORD COMMITTED") : TEXT("MEMORY RECORD COULD NOT BE WRITTEN"))];
        Box->AddSlot().AutoHeight()[Button(bSucceeded ? TEXT("BACK") : TEXT("RETRY"), [this, bSucceeded](){ bSucceeded ? ShowRoot() : ShowSave(); })];
        SetContent(Box);
    }

    void ShowLoad(bool bBackToPause)
    {
        bPauseMode = bBackToPause;
        TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
        Box->AddSlot().AutoHeight()[Title(TEXT("LOAD GAME"), TEXT("SELECT MEMORY RECORD"))];
        const TArray<FDroneSaveMeta> Saves = Owner->GetSaveMetas();
        if (Saves.IsEmpty())
            Box->AddSlot().AutoHeight().Padding(0,12)[SNew(STextBlock).Text(DroneMenu::T(TEXT("NO SAVED GAMES"))).ColorAndOpacity(DroneMenu::CyanDim)];
        else
            for (const FDroneSaveMeta& Meta : Saves)
                Box->AddSlot().AutoHeight()[Button(Meta.DisplayName + TEXT("  //  ") + Meta.LevelName + TEXT("  //  ") + Meta.Timestamp, [this, Slot=Meta.SlotName](){ Owner->LoadGame(Slot); })];
        Box->AddSlot().AutoHeight().Padding(0,12,0,0)[Button(TEXT("BACK"), [this](){ ShowRoot(); })];
        SetContent(Box);
    }

    void ShowSettingsPanel(bool bBackToPause)
    {
        bPauseMode = bBackToPause;
        Master = 1.f;
        Music = Owner ? Owner->GetMusicVolume() : 0.25f;
        Sfx = Owner ? Owner->GetSFXVolume() : 1.0f;
        WindowModes = { MakeShared<FString>(TEXT("WINDOWED")), MakeShared<FString>(TEXT("BORDERLESS")), MakeShared<FString>(TEXT("FULLSCREEN")) };
        Resolutions = { MakeShared<FString>(TEXT("1280x720")), MakeShared<FString>(TEXT("1920x1080")), MakeShared<FString>(TEXT("2560x1440")), MakeShared<FString>(TEXT("3840x2160")) };
        SelectedWindow = WindowModes[1]; SelectedResolution = Resolutions[1];
        TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
        Box->AddSlot().AutoHeight()[Title(TEXT("SETTING"), TEXT("AIRA SYSTEM CONFIGURATION"))];
        auto SliderRow=[&](const FString& Label, float* Value, TFunction<void(float)> OnChanged){
            return SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text_Lambda([Label, Value](){ return DroneMenu::T(Label + FString::Printf(TEXT("     %d%%"), FMath::RoundToInt(*Value * 100.f))); }).ColorAndOpacity(DroneMenu::CyanDim)]
                + SVerticalBox::Slot().AutoHeight().Padding(0,3,0,10)[SNew(SSlider).Value(*Value).OnValueChanged_Lambda([Value, OnChanged](float V){*Value=FMath::Clamp(V,0.f,1.f); OnChanged(*Value);})];
        };
        Box->AddSlot().AutoHeight()[SliderRow(TEXT("MUSIC VOLUME"), &Music, [this](float V){ if (Owner) Owner->SetMusicVolume(V); })];
        Box->AddSlot().AutoHeight()[SliderRow(TEXT("SFX VOLUME"), &Sfx, [this](float V){ if (Owner) Owner->SetSFXVolume(V); })];
        Box->AddSlot().AutoHeight().Padding(0,4)[SNew(STextBlock).Text(DroneMenu::T(TEXT("WINDOW MODE"))).ColorAndOpacity(DroneMenu::CyanDim)];
        Box->AddSlot().AutoHeight()[SNew(SComboBox<TSharedPtr<FString>>).OptionsSource(&WindowModes).InitiallySelectedItem(SelectedWindow).OnSelectionChanged_Lambda([this](TSharedPtr<FString> V,ESelectInfo::Type){SelectedWindow=V;}).OnGenerateWidget_Lambda([](TSharedPtr<FString> V){return SNew(STextBlock).Text(DroneMenu::T(*V));})[SNew(STextBlock).Text_Lambda([this](){return DroneMenu::T(*SelectedWindow);})]];
        Box->AddSlot().AutoHeight().Padding(0,10,0,4)[SNew(STextBlock).Text(DroneMenu::T(TEXT("RESOLUTION"))).ColorAndOpacity(DroneMenu::CyanDim)];
        Box->AddSlot().AutoHeight()[SNew(SComboBox<TSharedPtr<FString>>).OptionsSource(&Resolutions).InitiallySelectedItem(SelectedResolution).OnSelectionChanged_Lambda([this](TSharedPtr<FString> V,ESelectInfo::Type){SelectedResolution=V;}).OnGenerateWidget_Lambda([](TSharedPtr<FString> V){return SNew(STextBlock).Text(DroneMenu::T(*V));})[SNew(STextBlock).Text_Lambda([this](){return DroneMenu::T(*SelectedResolution);})]];
        Box->AddSlot().AutoHeight().Padding(0,14,0,0)[Button(TEXT("APPLY"), [this](){Owner->ApplySettings(Master,Music,Sfx,*SelectedWindow,*SelectedResolution);})];
        Box->AddSlot().AutoHeight()[Button(TEXT("BACK"), [this](){ShowRoot();})];
        SetContent(Box);
    }

private:
    UDroneGameInstance* Owner = nullptr;
    bool bPauseMode = false;
    TSharedPtr<FSlateBrush> BackgroundBrush;
    TSharedPtr<SEditableTextBox> SaveName;
    float Master=1.f, Music=1.f, Sfx=1.f;
    TArray<TSharedPtr<FString>> WindowModes, Resolutions;
    TSharedPtr<FString> SelectedWindow, SelectedResolution;
};

class SDroneGameOverRoot : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SDroneGameOverRoot) {}
    SLATE_END_ARGS()

    void Construct(const FArguments&)
    {
        ChildSlot
        [
            SNew(SOverlay)
            + SOverlay::Slot()
            [
                SNew(SBorder)
                .BorderBackgroundColor(FLinearColor(0.0f, 0.01f, 0.025f, 0.72f))
            ]
            + SOverlay::Slot()
            .HAlign(HAlign_Center)
            .VAlign(VAlign_Center)
            [
                SNew(SBorder)
                .Padding(FMargin(70.f, 30.f))
                .BorderBackgroundColor(FLinearColor(0.005f, 0.04f, 0.07f, 0.94f))
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("GAME OVER")))
                    .Justification(ETextJustify::Center)
                    .ColorAndOpacity(DroneMenu::Cyan)
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 64))
                    .ShadowOffset(FVector2D(2.f, 2.f))
                    .ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.9f))
                ]
            ]
        ];
    }
};

class SDroneMissionComplete : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SDroneMissionComplete) {}
    SLATE_END_ARGS()

    void Construct(const FArguments&)
    {
        ChildSlot
        [
            SNew(SOverlay)
            + SOverlay::Slot()
            [
                SNew(SBorder)
                .BorderBackgroundColor(FLinearColor(0.0f, 0.01f, 0.025f, 0.58f))
            ]
            + SOverlay::Slot()
            .HAlign(HAlign_Center)
            .VAlign(VAlign_Center)
            [
                SNew(SBorder)
                .Padding(FMargin(82.f, 36.f))
                .BorderBackgroundColor(FLinearColor(0.005f, 0.04f, 0.07f, 0.96f))
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("MISSION COMPLETE")))
                        .Justification(ETextJustify::Center)
                        .ColorAndOpacity(DroneMenu::Cyan)
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 64))
                        .ShadowOffset(FVector2D(2.f, 2.f))
                        .ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.9f))
                    ]
                    + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 10.f, 0.f, 0.f)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("DIAGNOSTIC TRANSMISSION COMPLETE")))
                        .Justification(ETextJustify::Center)
                        .ColorAndOpacity(DroneMenu::White)
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 18))
                    ]
                ]
            ]
        ];
        SetVisibility(EVisibility::HitTestInvisible);
    }
};

class SDroneMissionBriefing : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SDroneMissionBriefing) {}
        SLATE_ARGUMENT(double, StartTime)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args)
    {
        StartTime = Args._StartTime;

        ChildSlot
        [
            SNew(SBox)
            .WidthOverride(1180.f)
            [
                SNew(SBorder)
                .Padding(FMargin(2.f))
                .BorderBackgroundColor(DroneMenu::CyanDim)
                [
                    SNew(SBorder)
                    .Padding(FMargin(40.f, 26.f, 40.f, 30.f))
                    .BorderBackgroundColor(FLinearColor(0.003f, 0.018f, 0.032f, 0.94f))
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight()
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot().AutoWidth()
                            [
                                SNew(STextBlock)
                                .Text(DroneMenu::T(TEXT("// MESSAGE")))
                                .ColorAndOpacity(DroneMenu::Cyan)
                                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 22))
                            ]
                            + SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(24.f, 0.f, 0.f, 0.f)
                            [
                                SNew(SBorder)
                                .Padding(FMargin(0.f, 1.f))
                                .BorderBackgroundColor(DroneMenu::CyanDim)
                            ]
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0.f, 20.f, 0.f, 0.f)
                        [
                            SNew(STextBlock)
                            .Text(this, &SDroneMissionBriefing::GetInitializationText)
                            .ColorAndOpacity(FLinearColor(0.02f, 0.02f, 0.02f, 1.0f))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 13))
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0.f, 22.f, 0.f, 0.f)
                        [
                            SNew(STextBlock)
                            .Visibility(this, &SDroneMissionBriefing::GetMissionVisibility)
                            .Text(DroneMenu::T(TEXT("MISSION: Locate and reactivate eight Emergency Uplink Relay Nodes. Unlock each relay node, proceed to the Primary Transmission System, and transmit the station failure diagnostics to Central Command.")))
                            .ColorAndOpacity(FLinearColor(0.02f, 0.02f, 0.02f, 1.0f))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 15))
                            .AutoWrapText(true)
                        ]
                    ]
                ]
            ]
        ];

        SetVisibility(EVisibility::HitTestInvisible);
    }

    virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
    {
        SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
        SetRenderOpacity(GetSequenceOpacity());
    }

    float GetSequenceOpacity() const
    {
        const double Elapsed = FPlatformTime::Seconds() - StartTime;
        if (Elapsed < 0.45) return FMath::Clamp(static_cast<float>(Elapsed / 0.45), 0.f, 1.f);
        if (Elapsed > 19.0) return FMath::Clamp(static_cast<float>(20.0 - Elapsed), 0.f, 1.f);
        return 1.f;
    }

private:
    FText GetInitializationText() const
    {
        const double Elapsed = FPlatformTime::Seconds() - StartTime;
        FString Text(TEXT("REMOTE LINK INITIALIZING..."));
        if (Elapsed >= 0.85) Text += TEXT("\nNEURAL INTERFACE SYNCHRONIZING...");
        if (Elapsed >= 1.55) Text += TEXT("\nCONNECTION ESTABLISHED.");
        if (Elapsed >= 2.2) Text += TEXT("\nREMOTE CONTROL OF A.I.R.A. UNIT TRANSFERRED TO OPERATOR.");
        return DroneMenu::T(Text);
    }

    EVisibility GetMissionVisibility() const
    {
        return FPlatformTime::Seconds() - StartTime >= 2.8 ? EVisibility::Visible : EVisibility::Collapsed;
    }

    double StartTime = 0.0;
};

class SDroneMissionHUD : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SDroneMissionHUD) {} SLATE_ARGUMENT(UDroneGameInstance*, Owner) SLATE_END_ARGS()
    void Construct(const FArguments& Args)
    {
        Owner = Args._Owner;
        ChildSlot
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
            [
                SNew(SBox).WidthOverride(760.f)
                [
                    SNew(SBorder).Padding(2.f).BorderBackgroundColor(DroneMenu::CyanDim)
                    [
                        SNew(SBorder).Padding(FMargin(24.f, 14.f)).BorderBackgroundColor(DroneMenu::Panel)
                        [
                            SNew(SVerticalBox)
                            + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(this, &SDroneMissionHUD::GetTitle).ColorAndOpacity(DroneMenu::Cyan).Font(FCoreStyle::GetDefaultFontStyle("Bold", 18))]
                            + SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f)[SNew(STextBlock).Text(this, &SDroneMissionHUD::GetBody).ColorAndOpacity(DroneMenu::White).Font(FCoreStyle::GetDefaultFontStyle("Regular", 14)).AutoWrapText(true)]
                            + SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)
                            [SNew(SProgressBar).Visibility(this, &SDroneMissionHUD::GetProgressVisibility).Percent(this, &SDroneMissionHUD::GetProgress).FillColorAndOpacity(DroneMenu::Cyan)]
                            + SVerticalBox::Slot().AutoHeight().Padding(0.f, 7.f, 0.f, 0.f)
                            [SNew(STextBlock).Visibility(this, &SDroneMissionHUD::GetProgressVisibility).Text(this, &SDroneMissionHUD::GetProgressText).ColorAndOpacity(DroneMenu::Cyan).Justification(ETextJustify::Center)]
                        ]
                    ]
                ]
            ]
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.f, 12.f)
            [SNew(STextBlock).Text(this, &SDroneMissionHUD::GetCounter).ColorAndOpacity(DroneMenu::Cyan).Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))]
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 125.f, 0.f, 0.f)
            [
                SNew(STextBlock)
                .Visibility(this, &SDroneMissionHUD::GetPromptVisibility)
                .Text(this, &SDroneMissionHUD::GetPrompt)
                .ColorAndOpacity(DroneMenu::Cyan)
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 26))
                .Justification(ETextJustify::Center)
                .ShadowOffset(FVector2D(2.f, 2.f))
                .ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.9f))
            ]
        ];
    }
private:
    FText GetTitle() const
    {
        if (!Owner) return FText::GetEmpty();
        const double Now = FPlatformTime::Seconds();
        const FDroneMissionDevice* D = Owner->ActiveMissionDevice;
        if (D && D->bPrimary) return DroneMenu::T(TEXT("PRIMARY TRANSMISSION SYSTEM"));
        if (D) return DroneMenu::T(TEXT("RELAY NODE"));
        if (Now < Owner->MissionStatusEndTime) return DroneMenu::T(Owner->MissionStatusTitle);
        return DroneMenu::T(Now < Owner->MissionObjectiveBriefingEndTime ? TEXT("// MISSION UPDATE") : TEXT("// OBJECTIVE"));
    }
    FText GetBody() const
    {
        if (!Owner) return FText::GetEmpty();
        const double Now = FPlatformTime::Seconds();
        const FDroneMissionDevice* D = Owner->ActiveMissionDevice;
        if (D)
        {
            if (D->bPrimary && Owner->UnlockedRelayCount < Owner->MissionRelays.Num())
                return DroneMenu::T(FString::Printf(TEXT("RELAY NETWORK INCOMPLETE\nRELAYS ONLINE: %d / %d"), Owner->UnlockedRelayCount, Owner->MissionRelays.Num()));
            if (D->bPrimary) return FText::GetEmpty();
            return DroneMenu::T(TEXT("COMMUNICATION RELAY OFFLINE"));
        }
        if (Now < Owner->MissionStatusEndTime) return DroneMenu::T(Owner->MissionStatusBody);
        if (Now < Owner->MissionObjectiveBriefingEndTime)
            return DroneMenu::T(TEXT("The station relay network is offline.\n\nLocate and unlock all eight Antemma relay nodes.\n\nOnce all eight relay nodes are online, proceed to the Primary Transmission System and transmit the station failure diagnostics to Central Command."));
        return DroneMenu::T(TEXT("Locate the remaining relay nodes."));
    }
    FText GetPrompt() const
    {
        if (!Owner || !Owner->ActiveMissionDevice) return FText::GetEmpty();
        const FDroneMissionDevice* D = Owner->ActiveMissionDevice;
        if (D->bPrimary)
        {
            return Owner->UnlockedRelayCount == Owner->MissionRelays.Num()
                ? DroneMenu::T(TEXT("PRESS [ I ] TO ESTABLISH UPLINK"))
                : FText::GetEmpty();
        }
        return DroneMenu::T(TEXT("PRESS [ E ] TO UNLOCK"));
    }
    EVisibility GetPromptVisibility() const { return GetPrompt().IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; }
    EVisibility GetProgressVisibility() const { return Owner && Owner->ActiveMissionDevice && Owner->ActiveMissionDevice->State == EDroneMissionDeviceState::Unlocking ? EVisibility::Visible : EVisibility::Collapsed; }
    TOptional<float> GetProgress() const
    {
        if (!Owner || !Owner->ActiveMissionDevice) return 0.f;
        const FDroneMissionDevice* D = Owner->ActiveMissionDevice;
        return FMath::Clamp(static_cast<float>((FPlatformTime::Seconds() - D->StateStartTime) / (D->bPrimary ? 10.0 : 5.0)), 0.f, 1.f);
    }
    FText GetProgressText() const { return DroneMenu::T(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(GetProgress().Get(0.f) * 100.f))); }
    FText GetCounter() const { return Owner ? DroneMenu::T(FString::Printf(TEXT("RELAY NETWORK: %d / %d ONLINE"), Owner->UnlockedRelayCount, Owner->MissionRelays.Num())) : FText::GetEmpty(); }
    UDroneGameInstance* Owner = nullptr;
};

bool UDroneGameInstance::EnsureMainMenuBackgroundTexture()
{
    static const TCHAR* BackgroundPath = TEXT("/Game/ConceptArt/DronePreview.DronePreview");
    if (!MainMenuBackgroundTexture)
    {
        MainMenuBackgroundTexture = LoadObject<UTexture2D>(nullptr, BackgroundPath);
    }

    if (!MainMenuBackgroundTexture)
    {
        UE_LOG(LogDroneMenu, Error, TEXT("Failed to load startup Main Menu background Texture2D: %s"), BackgroundPath);
        return false;
    }

    const FIntPoint ImportedSize = MainMenuBackgroundTexture->GetImportedSize();
    UE_LOG(LogDroneMenu, Display, TEXT("Startup Main Menu background loaded: %s (imported %dx%d, resident %dx%d)"),
        *MainMenuBackgroundTexture->GetPathName(),
        ImportedSize.X,
        ImportedSize.Y,
        MainMenuBackgroundTexture->GetSizeX(),
        MainMenuBackgroundTexture->GetSizeY());
    return true;
}

ADroneMenuGameMode::ADroneMenuGameMode()
{
    DefaultPawnClass = nullptr;
    HUDClass = nullptr;
}

void UDroneGameInstance::Init()
{
    Super::Init();
    FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UDroneGameInstance::OnPostLoadMap);
    InputProcessor = MakeShared<FDroneMenuInputProcessor>(this);
    if (FSlateApplication::IsInitialized())
    {
        FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor, 0);
    }
}

void UDroneGameInstance::OnStart()
{
    Super::OnStart();
    if (IsMainMenuWorld())
    {
        bGameplayStartedFromMainMenu = false;
        bStartupRedirectInProgress = false;
        SetupMenuPresentation(GetWorld());
        ShowMainMenu();
        UE_LOG(LogDroneMenu, Display, TEXT("Main Menu ready in %s"), *GetWorld()->GetMapName());
        return;
    }

    RedirectInitialGameplayWorldToMainMenu();
}

void UDroneGameInstance::Shutdown()
{
    FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
    if (FSlateApplication::IsInitialized() && InputProcessor.IsValid()) FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
    if (PendingLoadTicker.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(PendingLoadTicker);
    if (PresentationTicker.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(PresentationTicker);
    if (PlayerDeathMonitorTicker.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(PlayerDeathMonitorTicker);
    if (GameOverTicker.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(GameOverTicker);
    HideMissionBriefing();
    ShutdownMissionInteraction();
    PendingLoadTicker.Reset();
    PresentationTicker.Reset();
    PlayerDeathMonitorTicker.Reset();
    GameOverTicker.Reset();
    PendingLoad.Reset();
    StopGameplayMusic();
    if (IsValid(MenuMusicComponent.Get()))
    {
        MenuMusicComponent->Stop();
        MenuMusicComponent = nullptr;
    }
    Super::Shutdown();
}

bool UDroneGameInstance::IsMainMenuWorld() const
{
    return GetWorld() && (GetWorld()->GetMapName().Contains(TEXT("L_MainMenu")) || GetWorld()->GetMapName().Contains(TEXT("Entry")));
}

void UDroneGameInstance::OnPostLoadMap(UWorld* LoadedWorld)
{
    if (!LoadedWorld) return;
    bStartGamePending = false;
    StartGameDelayTimer.Invalidate();
    if (PendingLoadTicker.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(PendingLoadTicker);
        PendingLoadTicker.Reset();
    }
    if (LoadedWorld->GetMapName().Contains(TEXT("L_MainMenu")) || LoadedWorld->GetMapName().Contains(TEXT("Entry")))
    {
        StopGameplayMusic();
        bGameOverInProgress = false;
        ObservedPlayerPawn.Reset();
        bGameplayStartedFromMainMenu = false;
        bStartupRedirectInProgress = false;
        SetupMenuPresentation(LoadedWorld);
        ShowMainMenu();
        UE_LOG(LogDroneMenu, Display, TEXT("Main Menu ready in %s"), *LoadedWorld->GetMapName());
    }
    else
    {
        if (RedirectInitialGameplayWorldToMainMenu()) return;
        StartGameplayMusic(LoadedWorld);
        HideMenu(false);
        ObservePlayerPawn();
        InitializeMissionInteraction(LoadedWorld);
        if (bShowMissionBriefingAfterLoad) QueueMissionBriefing();
        UE_LOG(LogDroneMenu, Display, TEXT("Gameplay active in %s"), *LoadedWorld->GetMapName());
        if (PendingLoad.IsValid()) PendingLoadTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UDroneGameInstance::TryApplyPendingSave), 0.1f);
    }
}

void UDroneGameInstance::StartGameplayMusic(UWorld* World)
{
    if (!World || !World->GetMapName().Contains(TEXT("Level_1")))
    {
        StopGameplayMusic();
        return;
    }

    if (IsValid(MenuMusicComponent.Get()))
    {
        MenuMusicComponent->Stop();
        MenuMusicComponent = nullptr;
    }

    if (IsValid(GameplayMusicComponent.Get()))
    {
        GameplayMusicComponent->SetVolumeMultiplier(MusicVolume);
        if (!GameplayMusicComponent->IsPlaying()) GameplayMusicComponent->Play();
        return;
    }

    static const TCHAR* GameplayMusicPath = TEXT("/Game/ConceptArt/Epic_Space.Epic_Space");
    if (USoundWave* Music = LoadObject<USoundWave>(nullptr, GameplayMusicPath))
    {
        Music->bLooping = true;
        GameplayMusicComponent = UGameplayStatics::SpawnSound2D(this, Music, MusicVolume, 1.0f, 0.0f, nullptr, true, false);
        if (IsValid(GameplayMusicComponent.Get()))
        {
            GameplayMusicComponent->SetVolumeMultiplier(MusicVolume);
            UE_LOG(LogDroneMenu, Display, TEXT("Gameplay music started: %s at %.0f%%"), GameplayMusicPath, MusicVolume * 100.0f);
        }
    }
    else
    {
        UE_LOG(LogDroneMenu, Warning, TEXT("Gameplay music SoundWave could not be loaded: %s"), GameplayMusicPath);
    }
}

void UDroneGameInstance::StopGameplayMusic()
{
    if (IsValid(GameplayMusicComponent.Get()))
    {
        GameplayMusicComponent->Stop();
        GameplayMusicComponent = nullptr;
    }
}

bool UDroneGameInstance::RedirectInitialGameplayWorldToMainMenu()
{
    if (bGameplayStartedFromMainMenu || bStartupRedirectInProgress || IsMainMenuWorld()) return false;

    bStartupRedirectInProgress = true;
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        PC->SetIgnoreMoveInput(true);
        PC->SetIgnoreLookInput(true);
        if (APawn* Pawn = PC->GetPawn()) Pawn->DisableInput(PC);
        PC->SetShowMouseCursor(true);
        PC->SetInputMode(FInputModeUIOnly());
    }

    UE_LOG(LogDroneMenu, Display, TEXT("Initial startup in %s; redirecting to Main Menu"), GetWorld() ? *GetWorld()->GetMapName() : TEXT("unknown world"));
    UGameplayStatics::OpenLevel(this, FName(TEXT("/Engine/Maps/Entry")));
    return true;
}

void UDroneGameInstance::InstallMenu(TSharedRef<SWidget> Widget, bool bPauseWorld)
{
    if (!GEngine || !GEngine->GameViewport) return;
    if (ActiveMenu.IsValid()) GEngine->GameViewport->RemoveViewportWidgetContent(ActiveMenu.ToSharedRef());
    ActiveMenu = Widget;
    GEngine->GameViewport->AddViewportWidgetContent(Widget, 10000);
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this,0))
    {
        PC->SetShowMouseCursor(true); PC->SetInputMode(FInputModeUIOnly());
        if (bPauseWorld) PC->SetPause(true);
    }
}

void UDroneGameInstance::ShowMainMenu()
{
    bPauseMenuOpen = false;
    EnsureMainMenuBackgroundTexture();
    if (!IsValid(MenuMusicComponent.Get()))
    {
        static const TCHAR* MusicPath = TEXT("/Game/ConceptArt/SPaceMusic.SPaceMusic");
        if (USoundWave* Music = LoadObject<USoundWave>(nullptr, MusicPath))
        {
            Music->bLooping = true;
            MenuMusicComponent = UGameplayStatics::SpawnSound2D(this, Music, MusicVolume, 1.0f, 0.0f, nullptr, true, false);
            if (IsValid(MenuMusicComponent.Get())) MenuMusicComponent->SetVolumeMultiplier(MusicVolume);
        }
        else
        {
            UE_LOG(LogDroneMenu, Warning, TEXT("Menu music SoundWave could not be loaded: %s"), MusicPath);
        }
    }
    else if (!MenuMusicComponent->IsPlaying())
    {
        MenuMusicComponent->Play();
    }
    InstallMenu(SNew(SDroneMenuRoot).Owner(this).PauseMode(false), false);
}

void UDroneGameInstance::PlayMenuClick()
{
    static const TCHAR* ClickPath = TEXT("/Game/ConceptArt/interface-robot-click.interface-robot-click");
    if (USoundWave* Click = LoadObject<USoundWave>(nullptr, ClickPath))
        UGameplayStatics::PlaySound2D(this, Click, SFXVolume, 1.0f, 0.0f, nullptr, nullptr, true);
    else
        UE_LOG(LogDroneMenu, Warning, TEXT("Menu click SoundWave could not be loaded: %s"), ClickPath);
}

void UDroneGameInstance::SetMusicVolume(float Volume)
{
    MusicVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
    if (IsValid(MenuMusicComponent.Get())) MenuMusicComponent->SetVolumeMultiplier(MusicVolume);
    if (IsValid(GameplayMusicComponent.Get())) GameplayMusicComponent->SetVolumeMultiplier(MusicVolume);
}

void UDroneGameInstance::SetSFXVolume(float Volume)
{
    SFXVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
}
void UDroneGameInstance::ShowPauseMenu() { if (bPauseMenuOpen) return; bPauseMenuOpen=true; InstallMenu(SNew(SDroneMenuRoot).Owner(this).PauseMode(true), true); }
void UDroneGameInstance::ShowSettings(bool bReturnToPause) { TSharedRef<SDroneMenuRoot> W=SNew(SDroneMenuRoot).Owner(this).PauseMode(bReturnToPause); W->ShowSettingsPanel(bReturnToPause); InstallMenu(W,bReturnToPause); }
void UDroneGameInstance::ShowSaveDialog() { TSharedRef<SDroneMenuRoot> W=SNew(SDroneMenuRoot).Owner(this).PauseMode(true); W->ShowSave(); InstallMenu(W,true); }
void UDroneGameInstance::ShowLoadDialog(bool bReturnToPause) { TSharedRef<SDroneMenuRoot> W=SNew(SDroneMenuRoot).Owner(this).PauseMode(bReturnToPause); W->ShowLoad(bReturnToPause); InstallMenu(W,bReturnToPause); }

void UDroneGameInstance::HideMenu(bool bResumeGame)
{
    if (GEngine && GEngine->GameViewport && ActiveMenu.IsValid()) GEngine->GameViewport->RemoveViewportWidgetContent(ActiveMenu.ToSharedRef());
    ActiveMenu.Reset(); bPauseMenuOpen=false;
    if (APlayerController* PC=UGameplayStatics::GetPlayerController(this,0))
    {
        if (bResumeGame) PC->SetPause(false);
        PC->SetShowMouseCursor(false); PC->SetInputMode(FInputModeGameOnly());
    }
}

void UDroneGameInstance::HandleEscape()
{
    if (bGameOverInProgress || IsMainMenuWorld() || !bGameplayStartedFromMainMenu) return;
    bPauseMenuOpen ? HideMenu(true) : ShowPauseMenu();
}

void UDroneGameInstance::StartGame()
{
    if (bStartGamePending || !IsMainMenuWorld()) return;

    bStartGamePending = true;
    PlayMenuClick();

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(StartGameDelayTimer, this, &UDroneGameInstance::CompleteStartGame, 0.20f, false);
    }
    else
    {
        bStartGamePending = false;
    }
}

void UDroneGameInstance::CompleteStartGame()
{
    if (!bStartGamePending) return;
    bGameOverInProgress = false;
    bGameplayStartedFromMainMenu = true;
    bStartupRedirectInProgress = false;
    bStartupVoicePlayed = false;
    bShowMissionBriefingAfterLoad = true;
    if (IsValid(MenuMusicComponent.Get()))
    {
        MenuMusicComponent->Stop();
        MenuMusicComponent = nullptr;
    }
    HideMenu(false);
    UGameplayStatics::OpenLevel(this, FName(TEXT("/Game/Maps/Level_1")));
}

void UDroneGameInstance::ObservePlayerPawn()
{
    APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
    if (ObservedPlayerPawn.Get() != Pawn)
    {
        if (ObservedPlayerPawn.IsValid()) ObservedPlayerPawn->OnDestroyed.RemoveDynamic(this, &UDroneGameInstance::OnObservedPlayerDestroyed);
        ObservedPlayerPawn = Pawn;
        if (Pawn) Pawn->OnDestroyed.AddUniqueDynamic(this, &UDroneGameInstance::OnObservedPlayerDestroyed);
    }
    if (!PlayerDeathMonitorTicker.IsValid())
    {
        PlayerDeathMonitorTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UDroneGameInstance::TickPlayerDeathMonitor), 0.05f);
    }
}

bool UDroneGameInstance::TickPlayerDeathMonitor(float)
{
    if (bGameOverInProgress || IsMainMenuWorld() || !bGameplayStartedFromMainMenu) return true;
    ObservePlayerPawn();
    double Energy = 1.0;
    if (ObservedPlayerPawn.IsValid() && ReadEnergy(ObservedPlayerPawn.Get(), Energy) && Energy <= 0.0) BeginGameOver();
    return true;
}

void UDroneGameInstance::OnObservedPlayerDestroyed(AActor* DestroyedActor)
{
    BeginGameOverForActor(DestroyedActor);
}

void UDroneGameInstance::BeginGameOver()
{
    BeginGameOverForActor(ObservedPlayerPawn.Get());
}

void UDroneGameInstance::BeginGameOverForActor(AActor* PlayerActor)
{
    if (bGameOverInProgress || IsMainMenuWorld() || !bGameplayStartedFromMainMenu) return;
    bGameOverInProgress = true;
    bGameOverScreenVisible = false;
    ShutdownMissionInteraction();
    bPauseMenuOpen = false;

    const FVector DeathLocation = PlayerActor ? PlayerActor->GetActorLocation() : FVector::ZeroVector;
    const FRotator DeathRotation = PlayerActor ? PlayerActor->GetActorRotation() : FRotator::ZeroRotator;

    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        PC->SetIgnoreMoveInput(true);
        PC->SetIgnoreLookInput(true);
        if (APawn* Pawn = PC->GetPawn())
        {
            Pawn->DisableInput(PC);
            if (UPawnMovementComponent* Movement = Pawn->GetMovementComponent()) Movement->StopMovementImmediately();
        }
        PC->SetShowMouseCursor(false);
        PC->SetInputMode(FInputModeUIOnly());
    }

    static const TCHAR* ExplosionPath = TEXT("/Game/Assets/Effects/Projectiles/FireExplosion/NS_FireExplosion.NS_FireExplosion");
    if (UNiagaraSystem* Explosion = LoadObject<UNiagaraSystem>(nullptr, ExplosionPath))
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), Explosion, DeathLocation, DeathRotation);
    }
    else
    {
        UE_LOG(LogDroneMenu, Warning, TEXT("Player death explosion could not be loaded: %s"), ExplosionPath);
    }

    static const TCHAR* ExplosionSoundPath = TEXT("/Game/Assets/Sounds/Explode/sfx_FireBolt_Impact.sfx_FireBolt_Impact");
    if (USoundBase* ExplosionSound = LoadObject<USoundBase>(nullptr, ExplosionSoundPath))
    {
        UGameplayStatics::PlaySoundAtLocation(this, ExplosionSound, DeathLocation, SFXVolume);
    }
    else
    {
        UE_LOG(LogDroneMenu, Warning, TEXT("Player death explosion sound could not be loaded: %s"), ExplosionSoundPath);
    }

    if (PlayerActor)
    {
        PlayerActor->SetActorEnableCollision(false);
        PlayerActor->SetActorHiddenInGame(true);
        PlayerActor->SetActorTickEnabled(false);
    }

    // Use platform time/CoreTicker so both the pre-pause VFX delay and the
    // return-to-menu countdown continue independently of world pause.
    GameOverFreezeTime = FPlatformTime::Seconds() + 1.0;
    GameOverReturnTime = 0.0;
    if (GameOverTicker.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(GameOverTicker);
    GameOverTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UDroneGameInstance::TickGameOver), 0.05f);
    UE_LOG(LogDroneMenu, Display, TEXT("Player death confirmed; explosion spawned, freezing gameplay in 1.0 seconds"));
}

bool UDroneGameInstance::TickGameOver(float)
{
    if (!bGameOverInProgress) return false;

    const double Now = FPlatformTime::Seconds();
    if (!bGameOverScreenVisible)
    {
        if (Now < GameOverFreezeTime) return true;

        bGameOverScreenVisible = true;
        if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
        {
            PC->SetPause(true);
            PC->SetShowMouseCursor(false);
        }
        InstallMenu(SNew(SDroneGameOverRoot), false);
        if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0)) PC->SetShowMouseCursor(false);
        GameOverReturnTime = Now + 6.0;
        UE_LOG(LogDroneMenu, Display, TEXT("Gameplay frozen; showing GAME OVER for 6.0 seconds"));
        return true;
    }

    if (Now < GameOverReturnTime) return true;

    GameOverTicker.Reset();
    bGameplayStartedFromMainMenu = false;
    bStartupRedirectInProgress = false;
    bGameOverScreenVisible = false;
    ObservedPlayerPawn.Reset();
    UE_LOG(LogDroneMenu, Display, TEXT("GAME OVER complete; returning to startup Main Menu"));
    UGameplayStatics::OpenLevel(this, FName(TEXT("/Engine/Maps/Entry")));
    return false;
}

UDroneSaveIndex* UDroneGameInstance::LoadIndex() const
{
    if (USaveGame* S=UGameplayStatics::LoadGameFromSlot(DroneMenu::IndexSlot.ToString(),0)) return Cast<UDroneSaveIndex>(S);
    return Cast<UDroneSaveIndex>(UGameplayStatics::CreateSaveGameObject(UDroneSaveIndex::StaticClass()));
}
bool UDroneGameInstance::SaveIndex(UDroneSaveIndex* Index) const { return UGameplayStatics::SaveGameToSlot(Index,DroneMenu::IndexSlot.ToString(),0); }
TArray<FDroneSaveMeta> UDroneGameInstance::GetSaveMetas() const { if(UDroneSaveIndex* I=LoadIndex()) return I->Saves; return {}; }

bool UDroneGameInstance::ReadEnergy(APawn* Pawn, double& OutEnergy) const
{
    if (!Pawn) return false;
    FProperty* P=Pawn->GetClass()->FindPropertyByName(TEXT("Energy"));
    if (FDoubleProperty* D=CastField<FDoubleProperty>(P)){OutEnergy=D->GetPropertyValue_InContainer(Pawn);return true;}
    if (FFloatProperty* F=CastField<FFloatProperty>(P)){OutEnergy=F->GetPropertyValue_InContainer(Pawn);return true;}
    return false;
}
bool UDroneGameInstance::WriteEnergy(APawn* Pawn, double Energy) const
{
    if (!Pawn) return false;
    FProperty* P=Pawn->GetClass()->FindPropertyByName(TEXT("Energy"));
    if (FDoubleProperty* D=CastField<FDoubleProperty>(P)){D->SetPropertyValue_InContainer(Pawn,Energy);return true;}
    if (FFloatProperty* F=CastField<FFloatProperty>(P)){F->SetPropertyValue_InContainer(Pawn,(float)Energy);return true;}
    return false;
}

bool UDroneGameInstance::SaveGame(const FString& InDisplayName)
{
    APawn* Pawn=UGameplayStatics::GetPlayerPawn(this,0); if(!Pawn) return false;
    UDroneSaveIndex* Index=LoadIndex(); if(!Index) return false;
    const FString Slot=FString::Printf(TEXT("Save_%03d"),Index->NextSlotNumber++);
    UDroneSaveGame* Save=Cast<UDroneSaveGame>(UGameplayStatics::CreateSaveGameObject(UDroneSaveGame::StaticClass()));
    Save->DisplayName=InDisplayName.TrimStartAndEnd().IsEmpty()?Slot:InDisplayName.TrimStartAndEnd();
    Save->SlotName=Slot; Save->Timestamp=FDateTime::Now().ToString(TEXT("%d %b %Y - %H:%M"));
    Save->LevelName=UGameplayStatics::GetCurrentLevelName(this,true); Save->PlayerTransform=Pawn->GetActorTransform();
    Save->bHasEnergy=ReadEnergy(Pawn,Save->Energy);
    if(UGameplayStatics::SaveGameToSlot(Save,Slot,0))
    {
        FDroneSaveMeta Meta; Meta.DisplayName=Save->DisplayName; Meta.SlotName=Slot; Meta.LevelName=Save->LevelName; Meta.Timestamp=Save->Timestamp;
        Index->Saves.Insert(Meta,0);
        return SaveIndex(Index);
    }
    return false;
}

void UDroneGameInstance::LoadGame(const FString& SlotName)
{
    UDroneSaveGame* Save=Cast<UDroneSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName,0)); if(!Save)return;
    bGameplayStartedFromMainMenu = true;
    bStartupRedirectInProgress = false;
    bShowMissionBriefingAfterLoad = false;
    PendingLoad.Reset(Save); HideMenu(false); UGameplayStatics::OpenLevel(this,FName(*Save->LevelName));
}

void UDroneGameInstance::QueueMissionBriefing()
{
    if (MissionBriefingTicker.IsValid()) return;
    MissionBriefingStartTime = 0.0;
    MissionBriefingTicker = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &UDroneGameInstance::TickMissionBriefing), 0.05f);
}

bool UDroneGameInstance::TickMissionBriefing(float)
{
    if (IsMainMenuWorld() || bGameOverInProgress)
    {
        HideMissionBriefing();
        return false;
    }

    if (!MissionBriefing.IsValid())
    {
        APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
        if (!PC || !PC->GetPawn()) return true;
        if (!GEngine || !GEngine->GameViewport) return true;

        MissionBriefingStartTime = FPlatformTime::Seconds();
        TSharedRef<SDroneMissionBriefing> Briefing = SNew(SDroneMissionBriefing).StartTime(MissionBriefingStartTime);
        TSharedRef<SOverlay> BriefingLayer = SNew(SOverlay)
            + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(FMargin(24.f, 54.f, 24.f, 0.f))[Briefing];
        MissionBriefing = BriefingLayer;
        GEngine->GameViewport->AddViewportWidgetContent(BriefingLayer, 500);
        if (!bStartupVoicePlayed)
        {
            bStartupVoicePlayed = true;
            static const TCHAR* StartupVoicePath = TEXT("/Game/ConceptArt/Remote_link_initiali.Remote_link_initiali");
            USoundWave* StartupVoice = LoadObject<USoundWave>(nullptr, StartupVoicePath);
            if (StartupVoice)
            {
                UGameplayStatics::PlaySound2D(this, StartupVoice, SFXVolume, 1.0f, 0.0f, nullptr, nullptr, true);
                UE_LOG(LogDroneMenu, Display, TEXT("AIRA startup voice playback started: %s"), StartupVoicePath);
            }
            else
            {
                UE_LOG(LogDroneMenu, Warning, TEXT("AIRA VO Startup voice SoundWave could not be loaded: %s"), StartupVoicePath);
            }
        }
        bShowMissionBriefingAfterLoad = false;
        UE_LOG(LogDroneMenu, Display, TEXT("Startup mission briefing shown after player control became available"));
        return true;
    }

    if (FPlatformTime::Seconds() - MissionBriefingStartTime >= 20.0)
    {
        HideMissionBriefing();
        return false;
    }
    return true;
}

void UDroneGameInstance::HideMissionBriefing()
{
    const bool bCompletedNormally = MissionBriefingStartTime > 0.0 && FPlatformTime::Seconds() - MissionBriefingStartTime >= 19.9;
    if (GEngine && GEngine->GameViewport && MissionBriefing.IsValid())
        GEngine->GameViewport->RemoveViewportWidgetContent(MissionBriefing.ToSharedRef());
    MissionBriefing.Reset();
    if (MissionBriefingTicker.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(MissionBriefingTicker);
    MissionBriefingTicker.Reset();
    MissionBriefingStartTime = 0.0;
    if (bCompletedNormally && bMissionInteractionReady) ShowMissionObjectiveBriefing();
}

void UDroneGameInstance::InitializeMissionInteraction(UWorld* World)
{
    ShutdownMissionInteraction();
    if (!World || IsMainMenuWorld()) return;

    MissionReadySound = LoadObject<USoundBase>(nullptr, TEXT("/Game/ConceptArt/sci-fi_5123.sci-fi_5123"));
    MissionSuccessSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/ConceptArt/sci-fi-launch.sci-fi-launch"));

    const TCHAR* NetModeName = TEXT("Unknown");
    switch (World->GetNetMode())
    {
        case NM_Standalone: NetModeName = TEXT("Standalone"); break;
        case NM_DedicatedServer: NetModeName = TEXT("DedicatedServer"); break;
        case NM_ListenServer: NetModeName = TEXT("ListenServer"); break;
        case NM_Client: NetModeName = TEXT("Client"); break;
        default: break;
    }
    UE_LOG(LogDroneMenu, Display, TEXT("[MissionInteraction][BUILD CHECK] ANTEMMA OBSERVER + SISPEREDACHI UPLINK ACTIVE"));
    UE_LOG(LogDroneMenu, Display, TEXT("[MissionInteraction] World = %s"), *World->GetName());
    UE_LOG(LogDroneMenu, Display, TEXT("[MissionInteraction] Level = %s"),
        World->PersistentLevel ? *World->PersistentLevel->GetOutermost()->GetName() : TEXT("None"));
    UE_LOG(LogDroneMenu, Display, TEXT("[MissionInteraction] NetMode = %s"), NetModeName);

    const FString RelayClassPath(TEXT("/Game/Blueprints/Props/Antenna/Mesh/Antemma.Antemma_C"));
    const FString PrimaryClassPath(TEXT("/Game/Blueprints/Props/Antenna/Mesh/SistemaPeredachi/SistemaPeredachi/SisPeredachi.SisPeredachi_C"));
    // Prefer IsA() so placed child instances are tracked as well; retain the
    // generated-class path fallback for already-cooked Blueprint instances.
    UClass* RelayClass = LoadClass<AActor>(nullptr, *RelayClassPath);
    UClass* PrimaryClass = LoadClass<AActor>(nullptr, *PrimaryClassPath);
    int32 PrimaryActorCount = 0;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        const FString ClassPath = Actor->GetClass()->GetPathName();
        if ((RelayClass && Actor->IsA(RelayClass)) || ClassPath == RelayClassPath)
        {
            FDroneMissionDevice& Device = MissionRelays.AddDefaulted_GetRef();
            Device.Actor = Actor;
            Device.InteractionCenter = Actor->GetActorLocation();
            UE_LOG(LogDroneMenu, Display, TEXT("[MissionInteraction] Tracking Antemma unlock state: %s"), *Actor->GetName());
        }
        else if ((PrimaryClass && Actor->IsA(PrimaryClass)) || ClassPath == PrimaryClassPath)
        {
            MissionPrimary.Actor = Actor;
            MissionPrimary.InteractionCenter = Actor->GetActorLocation();
            MissionPrimary.InteractionRange = 1000.f;
            MissionPrimary.bPrimary = true;
            ++PrimaryActorCount;
            UE_LOG(LogDroneMenu, Display, TEXT("[MissionInteraction] Discovered primary transmission actor: %s | class: %s | center: %s | radius: %.1f"),
                *Actor->GetName(), *ClassPath, *MissionPrimary.InteractionCenter.ToCompactString(), MissionPrimary.InteractionRange);
        }
    }
    UE_LOG(LogDroneMenu, Display, TEXT("[MissionInteraction] Antemma found: %d"), MissionRelays.Num());
    UE_LOG(LogDroneMenu, Display, TEXT("[MissionInteraction] SisPeredachi found: %d"), PrimaryActorCount);
    UE_LOG(LogDroneMenu, Display, TEXT("[MissionInteraction] Primary transmission actors found: %d"), PrimaryActorCount);
    if (MissionRelays.IsEmpty() || !MissionPrimary.Actor.IsValid())
    {
        if (MissionRelays.IsEmpty()) UE_LOG(LogDroneMenu, Error, TEXT("[MissionInteraction][ERROR] No Antemma actors found (expected class %s)"), *RelayClassPath);
        if (!MissionPrimary.Actor.IsValid()) UE_LOG(LogDroneMenu, Error, TEXT("[MissionInteraction][ERROR] No SisPeredachi actor found (expected class %s)"), *PrimaryClassPath);
        return;
    }

    UnlockedRelayCount = 0;
    bMissionCompleted = false;
    bTransmissionFinalMessageShown = false;
    TransmissionCompleteMessageTime = 0.0;
    MissionTransmissionStage = 0;
    MissionSequenceNextTime = 0.0;
    MissionCompleteReturnTime = 0.0;
    bMissionInteractionReady = true;
    bMissionObjectiveStarted = false;
    bMissionPawnMissingLogged = false;
    bMissionHudMissingLogged = false;
    MissionDebugLogTime = 0.0;
    MissionControlledPawn.Reset();
    MissionInteractionTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UDroneGameInstance::TickMissionInteraction), 0.02f);
    UE_LOG(LogDroneMenu, Display, TEXT("Mission progression initialized with %d Antemma nodes and one SisPeredachi"), MissionRelays.Num());
}

void UDroneGameInstance::PlayMissionInteractionSound(USoundBase* Sound) const
{
    if (Sound)
    {
        UGameplayStatics::PlaySound2D(this, Sound, SFXVolume);
    }
}

void UDroneGameInstance::ShutdownMissionInteraction()
{
    if (MissionInteractionTicker.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(MissionInteractionTicker);
    MissionInteractionTicker.Reset();
    HideMissionOverlay();
    HideMissionComplete();
    MissionRelays.Reset();
    MissionPrimary = FDroneMissionDevice();
    ActiveMissionDevice = nullptr;
    bMissionInteractionReady = false;
    bMissionObjectiveStarted = false;
    MissionControlledPawn.Reset();
    MissionReadySound = nullptr;
    MissionSuccessSound = nullptr;
    bMissionPawnMissingLogged = false;
    bMissionHudMissingLogged = false;
    MissionDebugLogTime = 0.0;
}

void UDroneGameInstance::ShowMissionObjectiveBriefing()
{
    if (!bMissionInteractionReady || IsMainMenuWorld()) return;
    bMissionObjectiveStarted = true;
    MissionObjectiveBriefingEndTime = FPlatformTime::Seconds() + 18.0;
    if (!MissionOverlay.IsValid() && GEngine && GEngine->GameViewport)
    {
        TSharedRef<SWidget> Layer = SNew(SOverlay) + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(FMargin(24.f, 54.f))[SNew(SDroneMissionHUD).Owner(this)];
        MissionOverlay = Layer;
        GEngine->GameViewport->AddViewportWidgetContent(Layer, 510);
    }
}

void UDroneGameInstance::HideMissionOverlay()
{
    if (GEngine && GEngine->GameViewport && MissionOverlay.IsValid()) GEngine->GameViewport->RemoveViewportWidgetContent(MissionOverlay.ToSharedRef());
    MissionOverlay.Reset();
}

void UDroneGameInstance::ShowMissionComplete()
{
    HideMissionOverlay();
    if (!MissionCompleteOverlay.IsValid() && GEngine && GEngine->GameViewport)
    {
        TSharedRef<SWidget> Layer = SNew(SDroneMissionComplete);
        MissionCompleteOverlay = Layer;
        GEngine->GameViewport->AddViewportWidgetContent(Layer, 9500);
    }
}

void UDroneGameInstance::HideMissionComplete()
{
    if (GEngine && GEngine->GameViewport && MissionCompleteOverlay.IsValid())
        GEngine->GameViewport->RemoveViewportWidgetContent(MissionCompleteOverlay.ToSharedRef());
    MissionCompleteOverlay.Reset();
}

bool UDroneGameInstance::TickMissionInteraction(float DeltaTime)
{
#if 0 // Superseded authentication interaction retained only for source-history context; never compiled or run.
    if (!bMissionInteractionReady || IsMainMenuWorld() || bGameOverInProgress) return bMissionInteractionReady;
    APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
    if (!Pawn)
    {
        if (!bMissionPawnMissingLogged)
        {
            bMissionPawnMissingLogged = true;
            UE_LOG(LogDroneMenu, Error, TEXT("[MissionInteraction][ERROR] Player Pawn is null; waiting for possession and retrying"));
        }
        return true;
    }
    if (MissionControlledPawn.Get() != Pawn)
    {
        MissionControlledPawn = Pawn;
        bMissionPawnMissingLogged = false;
        UE_LOG(LogDroneMenu, Display, TEXT("[MissionInteraction] Controlled Pawn: %s | class: %s"), *Pawn->GetName(), *Pawn->GetClass()->GetPathName());
    }
    const double Now = FPlatformTime::Seconds();

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
    if (bDebugMissionInteraction && GetWorld())
    {
        for (const FDroneMissionDevice& Device : MissionRelays)
            if (Device.Actor.IsValid()) DrawDebugSphere(GetWorld(), Device.InteractionCenter, Device.InteractionRange, 64,
                (&Device == ActiveMissionDevice) ? FColor::Green : FColor::Cyan, false, 0.25f, 1, 8.f);
        if (MissionPrimary.Actor.IsValid()) DrawDebugSphere(GetWorld(), MissionPrimary.InteractionCenter, MissionPrimary.InteractionRange, 64,
            (&MissionPrimary == ActiveMissionDevice) ? FColor::Green : FColor::Cyan, false, 0.25f, 1, 8.f);
    }
#endif

    for (FDroneMissionDevice& Device : MissionRelays)
    {
        if (Device.State == EDroneMissionDeviceState::SecurityCooldown)
        {
            if (Now >= Device.CooldownEndTime) Device.State = (&Device == ActiveMissionDevice) ? EDroneMissionDeviceState::PlayerInRange : EDroneMissionDeviceState::Locked;
            else if (Now - Device.StateStartTime > 1.4) SetDeviceRing(Device, false);
        }
    }
    if (MissionPrimary.State == EDroneMissionDeviceState::SecurityCooldown)
    {
        if (Now >= MissionPrimary.CooldownEndTime) MissionPrimary.State = (&MissionPrimary == ActiveMissionDevice) ? EDroneMissionDeviceState::PlayerInRange : EDroneMissionDeviceState::Locked;
        else if (Now - MissionPrimary.StateStartTime > 1.4) SetDeviceRing(MissionPrimary, false);
    }

    FDroneMissionDevice* Nearest = nullptr;
    float BestDistanceSq = TNumericLimits<float>::Max();
    auto Consider = [&](FDroneMissionDevice& Device)
    {
        if (!Device.Actor.IsValid() || Device.State == EDroneMissionDeviceState::Unlocked || Device.State == EDroneMissionDeviceState::Authentication) return;
        const float DistanceSq = FVector::DistSquared(Pawn->GetActorLocation(), Device.InteractionCenter);
        if (DistanceSq <= FMath::Square(Device.InteractionRange) && DistanceSq < BestDistanceSq) { Nearest = &Device; BestDistanceSq = DistanceSq; }
    };
    for (FDroneMissionDevice& Device : MissionRelays) Consider(Device);
    if (!bMissionCompleted) Consider(MissionPrimary);

    if (bDebugMissionInteraction && Now >= MissionDebugLogTime)
    {
        MissionDebugLogTime = Now + 1.0;
        const FDroneMissionDevice* ClosestDevice = nullptr;
        float ClosestDistanceSq = TNumericLimits<float>::Max();
        for (const FDroneMissionDevice& Device : MissionRelays)
        {
            if (!Device.Actor.IsValid()) continue;
            const float DistanceSq = FVector::DistSquared(Pawn->GetActorLocation(), Device.InteractionCenter);
            if (DistanceSq < ClosestDistanceSq) { ClosestDistanceSq = DistanceSq; ClosestDevice = &Device; }
        }
        if (ClosestDevice)
        {
            UE_LOG(LogDroneMenu, Display, TEXT("[MissionInteraction][DEBUG] Pawn valid | pawn position = %s | nearest antenna = %s | antenna bounds center = %s | distance = %.1f | interaction radius = %.1f"),
                *Pawn->GetActorLocation().ToCompactString(), *ClosestDevice->Actor->GetName(),
                *ClosestDevice->InteractionCenter.ToCompactString(), FMath::Sqrt(ClosestDistanceSq), ClosestDevice->InteractionRange);
        }
    }

    if (!AuthenticationOverlay.IsValid() && ActiveMissionDevice != Nearest)
    {
        FDroneMissionDevice* Previous = ActiveMissionDevice;
        if (ActiveMissionDevice && ActiveMissionDevice->State == EDroneMissionDeviceState::Accessing)
        {
            ActiveMissionDevice->State = EDroneMissionDeviceState::Locked;
            ActiveMissionDevice->StateStartTime = 0.0;
            SetDeviceRing(*ActiveMissionDevice, false);
        }
        else if (ActiveMissionDevice && ActiveMissionDevice->State == EDroneMissionDeviceState::PlayerInRange)
        {
            ActiveMissionDevice->State = EDroneMissionDeviceState::Locked;
        }
        if (Previous && Previous->Actor.IsValid()) UE_LOG(LogDroneMenu, Display, TEXT("[MissionInteraction] EXIT range: %s"), *Previous->Actor->GetName());
        ActiveMissionDevice = Nearest;
        if (Nearest)
        {
            if (Nearest->State == EDroneMissionDeviceState::Locked) Nearest->State = EDroneMissionDeviceState::PlayerInRange;
            if (Nearest->Actor.IsValid()) UE_LOG(LogDroneMenu, Display, TEXT("[MissionInteraction] ENTER range: %s"), *Nearest->Actor->GetName());
        }
    }

    if (ActiveMissionDevice && ActiveMissionDevice->ScanRing.IsValid() && ActiveMissionDevice->ScanRing->IsVisible())
    {
        ActiveMissionDevice->ScanRing->AddLocalRotation(FRotator(0.f, 90.f * DeltaTime, 0.f));
        if (ActiveMissionDevice->ScanLight.IsValid()) ActiveMissionDevice->ScanLight->SetIntensity(1800.f + 700.f * FMath::Sin(static_cast<float>(Now * 8.0)));
    }
    if (ActiveMissionDevice && ActiveMissionDevice->State == EDroneMissionDeviceState::Accessing)
    {
        if (!bMissionInteractHeld)
        {
            ActiveMissionDevice->State = EDroneMissionDeviceState::PlayerInRange;
            SetDeviceRing(*ActiveMissionDevice, false);
        }
        else if (Now - ActiveMissionDevice->StateStartTime >= (ActiveMissionDevice->bPrimary ? 6.0 : 3.0)) CompleteCurrentAccess();
    }

    const bool bNeedsHud = Now < MissionObjectiveBriefingEndTime || Now < MissionStatusEndTime || ActiveMissionDevice != nullptr;
    if (bNeedsHud && !MissionOverlay.IsValid() && GEngine && GEngine->GameViewport)
    {
        TSharedRef<SWidget> Layer = SNew(SOverlay) + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(FMargin(24.f, 54.f))[SNew(SDroneMissionHUD).Owner(this)];
        MissionOverlay = Layer; GEngine->GameViewport->AddViewportWidgetContent(Layer, 510);
        UE_LOG(LogDroneMenu, Display, TEXT("[MissionInteraction] Interaction HUD added to local player viewport"));
    }
    else if (bNeedsHud && (!GEngine || !GEngine->GameViewport) && !bMissionHudMissingLogged)
    {
        bMissionHudMissingLogged = true;
        UE_LOG(LogDroneMenu, Error, TEXT("[MissionInteraction][ERROR] Interaction HUD unavailable: GameViewport is null"));
    }
    else if (!bNeedsHud) HideMissionOverlay();
    return true;
#else
#if 0 // Obsolete automatic/timed interaction. Kept only for source-history context.
    (void)DeltaTime;
    if (!bMissionInteractionReady || IsMainMenuWorld() || bGameOverInProgress) return bMissionInteractionReady;

    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
    APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
    if (!Pawn)
    {
        if (!bMissionPawnMissingLogged)
        {
            bMissionPawnMissingLogged = true;
            UE_LOG(LogDroneMenu, Error, TEXT("[MissionInteraction][ERROR] Controlled Pawn is null; waiting for possession"));
        }
        return true;
    }
    if (MissionControlledPawn.Get() != Pawn)
    {
        MissionControlledPawn = Pawn;
        bMissionPawnMissingLogged = false;
        UE_LOG(LogDroneMenu, Display, TEXT("[MissionInteraction] Controlled Pawn: %s | class: %s"), *Pawn->GetName(), *Pawn->GetClass()->GetPathName());
    }

    const double Now = FPlatformTime::Seconds();
    if (bMissionCompleted && !bTransmissionFinalMessageShown && Now >= TransmissionCompleteMessageTime)
    {
        bTransmissionFinalMessageShown = true;
        MissionStatusTitle = TEXT("TRANSMISSION COMPLETE");
        MissionStatusBody = TEXT("Station fault diagnostics successfully transmitted to Central Command.\n\nAI control failure and drone network anomaly data delivered for analysis.");
        MissionStatusEndTime = Now + 16.0;
    }

    FDroneMissionDevice* Nearest = nullptr;
    float BestDistanceSq = TNumericLimits<float>::Max();
    auto Consider = [&](FDroneMissionDevice& Device)
    {
        if (!Device.Actor.IsValid() || Device.State == EDroneMissionDeviceState::Unlocked) return;
        const float DistanceSq = FVector::DistSquared(Pawn->GetActorLocation(), Device.InteractionCenter);
        if (DistanceSq <= FMath::Square(Device.InteractionRange) && DistanceSq < BestDistanceSq)
        {
            Nearest = &Device;
            BestDistanceSq = DistanceSq;
        }
    };
    for (FDroneMissionDevice& Device : MissionRelays) Consider(Device);
    if (!bMissionCompleted) Consider(MissionPrimary);

    if (ActiveMissionDevice != Nearest)
    {
        FDroneMissionDevice* Previous = ActiveMissionDevice;
        if (Previous && Previous->State == EDroneMissionDeviceState::Unlocking)
        {
            Previous->State = EDroneMissionDeviceState::Locked;
            Previous->StateStartTime = 0.0;
            MissionStatusTitle = Previous->bPrimary ? TEXT("UPLINK INTERRUPTED") : TEXT("CONNECTION LOST");
            MissionStatusBody = Previous->bPrimary ? TEXT("Return to transmission range to restart the 10-second uplink.") : TEXT("Return to transmission range to restart unlocking from 5.");
            MissionStatusEndTime = Now + 2.0;
        }
        if (Previous && Previous->Actor.IsValid()) UE_LOG(LogDroneMenu, Display, TEXT("[MissionInteraction] EXIT range: %s"), *Previous->Actor->GetName());

        ActiveMissionDevice = Nearest;
        if (Nearest)
        {
            if (!Nearest->bPrimary || UnlockedRelayCount == MissionRelays.Num())
            {
                Nearest->State = EDroneMissionDeviceState::Unlocking;
                Nearest->StateStartTime = Now;
                UE_LOG(LogDroneMenu, Display, TEXT("[MissionInteraction] Automatic %s started: %s"), Nearest->bPrimary ? TEXT("uplink") : TEXT("unlock"), *Nearest->Actor->GetName());
            }
            UE_LOG(LogDroneMenu, Display, TEXT("[MissionInteraction] ENTER range: %s"), *Nearest->Actor->GetName());
        }
    }

    if (ActiveMissionDevice && ActiveMissionDevice->State == EDroneMissionDeviceState::Unlocking)
    {
        const double Duration = ActiveMissionDevice->bPrimary ? 10.0 : 5.0;
        if (Now - ActiveMissionDevice->StateStartTime >= Duration) CompleteCurrentAccess();
    }

    const bool bNeedsHud = Now < MissionObjectiveBriefingEndTime || Now < MissionStatusEndTime || ActiveMissionDevice != nullptr;
    if (bNeedsHud && !MissionOverlay.IsValid() && GEngine && GEngine->GameViewport)
    {
        TSharedRef<SWidget> Layer = SNew(SOverlay) + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(FMargin(24.f, 54.f))[SNew(SDroneMissionHUD).Owner(this)];
        MissionOverlay = Layer;
        GEngine->GameViewport->AddViewportWidgetContent(Layer, 510);
    }
    else if (!bNeedsHud) HideMissionOverlay();
    return true;
#endif

    (void)DeltaTime;
    if (!bMissionInteractionReady || IsMainMenuWorld() || bGameOverInProgress) return bMissionInteractionReady;

    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
    APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
    if (!Pawn) return true;

    const double Now = FPlatformTime::Seconds();
    auto ReadUnlocked = [](AActor* Actor)
    {
        if (!Actor) return false;
        const FBoolProperty* Property = FindFProperty<FBoolProperty>(Actor->GetClass(), TEXT("bUnlocked"));
        return Property && Property->GetPropertyValue_InContainer(Actor);
    };

    FDroneMissionDevice* RelayInRange = nullptr;
    float NearestRelayDistanceSq = TNumericLimits<float>::Max();
    for (FDroneMissionDevice& Relay : MissionRelays)
    {
        const bool bRelayUnlocked = ReadUnlocked(Relay.Actor.Get());
        bool bRelayInRange = false;
        if (AActor* RelayActor = Relay.Actor.Get())
        {
            if (const FBoolProperty* InRangeProperty = FindFProperty<FBoolProperty>(RelayActor->GetClass(), TEXT("bPlayerInRange")))
            {
                bRelayInRange = InRangeProperty->GetPropertyValue_InContainer(RelayActor);
            }
        }

        if (bRelayInRange && !Relay.bPlayerWasInRange && !bRelayUnlocked)
        {
            PlayMissionInteractionSound(MissionReadySound);
        }
        Relay.bPlayerWasInRange = bRelayInRange;
        if (bRelayInRange && !bRelayUnlocked && Relay.Actor.IsValid())
        {
            const float DistanceSq = FVector::DistSquared(Pawn->GetActorLocation(), Relay.Actor->GetActorLocation());
            if (DistanceSq < NearestRelayDistanceSq)
            {
                NearestRelayDistanceSq = DistanceSq;
                RelayInRange = &Relay;
            }
        }

        if (Relay.State != EDroneMissionDeviceState::Unlocked && bRelayUnlocked)
        {
            Relay.State = EDroneMissionDeviceState::Unlocked;
            if (bRelayInRange)
            {
                PlayMissionInteractionSound(MissionSuccessSound);
            }
            ++UnlockedRelayCount;
            MissionStatusTitle = TEXT("RELAY NODE ONLINE");
            MissionStatusBody = FString::Printf(TEXT("RELAY NODE ONLINE\n\nRELAY NETWORK: %d / %d ONLINE"), UnlockedRelayCount, MissionRelays.Num());
            MissionStatusEndTime = Now + 3.5;
            UE_LOG(LogDroneMenu, Display, TEXT("[MissionInteraction] Antemma online: %d / %d"), UnlockedRelayCount, MissionRelays.Num());

            if (UnlockedRelayCount == MissionRelays.Num())
            {
                MissionStatusTitle = TEXT("RELAY NETWORK RESTORED");
                MissionStatusBody = TEXT("RELAY NETWORK: 8 / 8 ONLINE\n\nRELAY NETWORK RESTORED\n\nNEW OBJECTIVE\nProceed to the Primary Transmission System.\nTransmit the station failure diagnostics to Central Command.");
                MissionStatusEndTime = Now + 10.0;
            }
        }
    }

    if (bMissionCompleted)
    {
        if (MissionTransmissionStage == 1 && Now >= MissionSequenceNextTime)
        {
            MissionStatusTitle = TEXT("PRIMARY UPLINK ESTABLISHED");
            MissionStatusBody = TEXT("TRANSMITTING STATION DIAGNOSTICS...");
            MissionStatusEndTime = Now + 1.5;
            MissionTransmissionStage = 2;
            MissionSequenceNextTime = Now + 1.5;
        }
        else if (MissionTransmissionStage == 2 && Now >= MissionSequenceNextTime)
        {
            MissionStatusTitle = TEXT("PRIMARY UPLINK ESTABLISHED");
            MissionStatusBody = TEXT("TRANSMITTING AI FAILURE DATA...");
            MissionStatusEndTime = Now + 1.5;
            MissionTransmissionStage = 3;
            MissionSequenceNextTime = Now + 1.5;
        }
        else if (MissionTransmissionStage == 3 && Now >= MissionSequenceNextTime)
        {
            MissionStatusTitle = TEXT("DATA TRANSFER COMPLETE");
            MissionStatusBody = TEXT("Station failure diagnostics successfully transmitted to Central Command.\n\nAI control anomaly and drone network failure data delivered for analysis.");
            MissionStatusEndTime = Now + 3.0;
            MissionTransmissionStage = 4;
            MissionSequenceNextTime = Now + 3.0;
        }
        else if (MissionTransmissionStage == 4 && Now >= MissionSequenceNextTime)
        {
            MissionTransmissionStage = 5;
            MissionCompleteReturnTime = Now + 5.0;
            ShowMissionComplete();
            UE_LOG(LogDroneMenu, Display, TEXT("Mission complete: diagnostic transmission delivered"));
        }
        else if (MissionTransmissionStage == 5 && Now >= MissionCompleteReturnTime)
        {
            HideMissionComplete();
            bGameplayStartedFromMainMenu = false;
            bStartupRedirectInProgress = false;
            bGameOverInProgress = false;
            // Tear down all per-level mission state before changing worlds so
            // stale actor references cannot affect the next session.
            ShutdownMissionInteraction();
            ObservedPlayerPawn.Reset();
            UE_LOG(LogDroneMenu, Display, TEXT("MISSION COMPLETE display finished; returning to startup Main Menu"));
            UGameplayStatics::OpenLevel(this, FName(TEXT("/Engine/Maps/Entry")));
            return false;
        }
    }
    else
    {
        const bool bPrimaryInRange = MissionPrimary.Actor.IsValid()
            && FVector::DistSquared(Pawn->GetActorLocation(), MissionPrimary.InteractionCenter) <= FMath::Square(MissionPrimary.InteractionRange);
        const bool bPrimaryReady = bPrimaryInRange && UnlockedRelayCount == MissionRelays.Num();
        if (bPrimaryReady && !MissionPrimary.bPlayerWasInRange)
        {
            PlayMissionInteractionSound(MissionReadySound);
        }
        MissionPrimary.bPlayerWasInRange = bPrimaryReady;
        ActiveMissionDevice = bPrimaryInRange ? &MissionPrimary : RelayInRange;

        if (bPrimaryReady
            && PlayerController->WasInputKeyJustPressed(EKeys::I))
        {
            MissionPrimary.State = EDroneMissionDeviceState::Unlocked;
            ActiveMissionDevice = nullptr;
            bMissionCompleted = true;
            MissionTransmissionStage = 1;
            MissionSequenceNextTime = Now + 1.25;
            MissionStatusTitle = TEXT("PRIMARY UPLINK ESTABLISHED");
            MissionStatusBody = TEXT("PRIMARY UPLINK ESTABLISHED");
            MissionStatusEndTime = Now + 1.5;
            PlayMissionInteractionSound(MissionSuccessSound);
        }
    }

    const bool bNeedsHud = !bMissionCompleted
        ? (Now < MissionObjectiveBriefingEndTime || Now < MissionStatusEndTime || ActiveMissionDevice != nullptr)
        : MissionTransmissionStage < 5;
    if (bNeedsHud && !MissionOverlay.IsValid() && GEngine && GEngine->GameViewport)
    {
        TSharedRef<SWidget> Layer = SNew(SOverlay)
            + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(FMargin(24.f, 54.f))
            [SNew(SDroneMissionHUD).Owner(this)];
        MissionOverlay = Layer;
        GEngine->GameViewport->AddViewportWidgetContent(Layer, 510);
    }
    else if (!bNeedsHud) HideMissionOverlay();
    return true;
#endif
}

#if 0
void UDroneGameInstance::CompleteCurrentAccess()
{
    if (!ActiveMissionDevice || AuthenticationOverlay.IsValid()) return;
    bMissionInteractHeld = false;
    ActiveMissionDevice->State = EDroneMissionDeviceState::Authentication;
    UE_LOG(LogDroneMenu, Display, TEXT("[MissionInteraction] Access complete"));
    UE_LOG(LogDroneMenu, Display, TEXT("[MissionInteraction] Access progress complete"));
    SetDeviceRing(*ActiveMissionDevice, false);
    ShowAuthentication(ActiveMissionDevice->bPrimary);
}

void UDroneGameInstance::ShowAuthentication(bool bPrimary)
{
    if (!GEngine || !GEngine->GameViewport)
    {
        UE_LOG(LogDroneMenu, Error, TEXT("[MissionInteraction][ERROR] Authentication UI unavailable: GameViewport is null"));
        return;
    }
    if (AuthenticationOverlay.IsValid()) return;
    UE_LOG(LogDroneMenu, Display, TEXT("[MissionInteraction] Opening authentication UI"));
    if (!bPrimary) { RelayQuestionA = FMath::RandRange(2, 9); RelayQuestionB = FMath::RandRange(2, 9); }
    TSharedRef<SWidget> Panel = bPrimary ? StaticCastSharedRef<SWidget>(SNew(SDronePrimaryAuthentication).Owner(this)) : StaticCastSharedRef<SWidget>(SNew(SDroneRelayAuthentication).Owner(this));
    TSharedRef<SWidget> Layer = SNew(SOverlay) + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(24.f)[SNew(SBox).WidthOverride(560.f)[Panel]];
    AuthenticationOverlay = Layer;
    GEngine->GameViewport->AddViewportWidgetContent(Layer, 9000);
    UE_LOG(LogDroneMenu, Display, TEXT("[MissionInteraction] Authentication UI opened"));
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        PC->SetIgnoreMoveInput(true); PC->SetIgnoreLookInput(true); PC->SetShowMouseCursor(bPrimary);
        PC->SetInputMode(FInputModeUIOnly());
    }
}

void UDroneGameInstance::CloseAuthentication()
{
    if (GEngine && GEngine->GameViewport && AuthenticationOverlay.IsValid()) GEngine->GameViewport->RemoveViewportWidgetContent(AuthenticationOverlay.ToSharedRef());
    AuthenticationOverlay.Reset();
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        PC->SetIgnoreMoveInput(false); PC->SetIgnoreLookInput(false); PC->SetShowMouseCursor(false); PC->SetInputMode(FInputModeGameOnly());
    }
}

void UDroneGameInstance::SubmitRelayAuthentication(const FString& Answer)
{
    if (!ActiveMissionDevice || ActiveMissionDevice->bPrimary || ActiveMissionDevice->State != EDroneMissionDeviceState::Authentication) return;
    int32 Value = 0;
    if (LexTryParseString(Value, *Answer.TrimStartAndEnd()) && Value == RelayQuestionA * RelayQuestionB) AcceptCurrentAuthentication();
    else FailCurrentAuthentication();
}

void UDroneGameInstance::SubmitPrimaryAuthentication(int32 AnswerIndex)
{
    if (!ActiveMissionDevice || !ActiveMissionDevice->bPrimary || ActiveMissionDevice->State != EDroneMissionDeviceState::Authentication) return;
    if (AnswerIndex == 0) AcceptCurrentAuthentication(); else FailCurrentAuthentication();
}

void UDroneGameInstance::FailCurrentAuthentication()
{
    if (!ActiveMissionDevice) return;
    CloseAuthentication();
    ActiveMissionDevice->State = EDroneMissionDeviceState::SecurityCooldown;
    ActiveMissionDevice->StateStartTime = FPlatformTime::Seconds();
    ActiveMissionDevice->CooldownEndTime = ActiveMissionDevice->StateStartTime + 12.0;
    SetDeviceRing(*ActiveMissionDevice, true, true);
    MissionStatusTitle = TEXT("AUTHENTICATION FAILED");
    MissionStatusBody = ActiveMissionDevice->bPrimary ? TEXT("PRIMARY UPLINK LOCKED") : TEXT("ACCESS DENIED");
    MissionStatusEndTime = FPlatformTime::Seconds() + 2.2;
}

void UDroneGameInstance::AcceptCurrentAuthentication()
{
    if (!ActiveMissionDevice || ActiveMissionDevice->State != EDroneMissionDeviceState::Authentication) return;
    FDroneMissionDevice* Accepted = ActiveMissionDevice;
    CloseAuthentication();
    Accepted->State = EDroneMissionDeviceState::Unlocked;
    SetDeviceRing(*Accepted, false);
    MissionStatusTitle = TEXT("AUTHENTICATION ACCEPTED");
    if (!Accepted->bPrimary)
    {
        ++UnlockedRelayCount;
        MissionStatusBody = FString::Printf(TEXT("RELAY NODE UNLOCKED\n\nRELAY NETWORK: %d / %d ONLINE"), UnlockedRelayCount, MissionRelays.Num());
        MissionStatusEndTime = FPlatformTime::Seconds() + 3.5;
        if (Accepted->Actor.IsValid())
        {
            UPointLightComponent* Light = NewObject<UPointLightComponent>(Accepted->Actor.Get(), NAME_None, RF_Transient);
            Light->SetLightColor(FLinearColor(0.02f, 1.f, 0.7f)); Light->SetIntensity(1500.f); Light->SetAttenuationRadius(500.f);
            Light->SetupAttachment(Accepted->Actor->GetRootComponent()); Accepted->Actor->AddInstanceComponent(Light); Light->RegisterComponent();
        }
        if (UnlockedRelayCount == MissionRelays.Num())
        {
            MissionStatusTitle = TEXT("RELAY NETWORK RESTORED");
            MissionStatusBody = TEXT("ALL LOCAL UPLINK NODES ONLINE\n\nNEW OBJECTIVE\nProceed to the Primary Transmission Array.\nEstablish a secure uplink and transmit the station diagnostic package to Central Command.");
            MissionStatusEndTime = FPlatformTime::Seconds() + 10.0;
        }
    }
    else
    {
        bMissionCompleted = true;
        MissionStatusBody = TEXT("PRIMARY UPLINK UNLOCKED\n\nTRANSMITTING STATION DIAGNOSTIC PACKAGE...\nUPLINK TO CENTRAL COMMAND ACTIVE\n\nTRANSMISSION COMPLETE\nStation fault diagnostics successfully transmitted to Central Command.\n\nAI control failure and drone network anomaly data have been delivered to the central processing system for analysis.");
        MissionStatusEndTime = FPlatformTime::Seconds() + 16.0;
        UE_LOG(LogDroneMenu, Display, TEXT("Mission complete: station diagnostics transmitted to Central Command"));
    }
    ActiveMissionDevice = nullptr;
}

void UDroneGameInstance::SetDeviceRing(FDroneMissionDevice& Device, bool bVisible, bool bFailure)
{
    if (!Device.ScanRing.IsValid()) return;
    Device.ScanRing->SetVisibility(bVisible);
    Device.ScanRing->SetVectorParameterValueOnMaterials(TEXT("Color"), bFailure ? FVector(1.f, 0.01f, 0.01f) : FVector(0.02f, 0.8f, 1.f));
    Device.ScanRing->SetVectorParameterValueOnMaterials(TEXT("EmissiveColor"), bFailure ? FVector(1.f, 0.01f, 0.01f) : FVector(0.02f, 0.8f, 1.f));
    if (Device.ScanLight.IsValid())
    {
        Device.ScanLight->SetLightColor(bFailure ? FLinearColor(1.f, 0.01f, 0.01f) : FLinearColor(0.02f, 0.8f, 1.f));
        Device.ScanLight->SetIntensity(bVisible ? 2200.f : 0.f);
        Device.ScanLight->SetVisibility(bVisible);
    }
}
#endif

void UDroneGameInstance::CompleteCurrentAccess()
{
    if (!ActiveMissionDevice || ActiveMissionDevice->State != EDroneMissionDeviceState::Unlocking) return;

    FDroneMissionDevice* Completed = ActiveMissionDevice;
    Completed->State = EDroneMissionDeviceState::Unlocked;
    Completed->StateStartTime = 0.0;
    ActiveMissionDevice = nullptr;
    const double Now = FPlatformTime::Seconds();

    if (!Completed->bPrimary)
    {
        ++UnlockedRelayCount;
        MissionStatusTitle = TEXT("RELAY NODE ONLINE");
        MissionStatusBody = FString::Printf(TEXT("RELAY NETWORK: %d / %d ONLINE"), UnlockedRelayCount, MissionRelays.Num());
        MissionStatusEndTime = Now + 3.5;
        UE_LOG(LogDroneMenu, Display, TEXT("[MissionInteraction] Relay online: %d / %d"), UnlockedRelayCount, MissionRelays.Num());

        if (UnlockedRelayCount == MissionRelays.Num())
        {
            MissionStatusTitle = TEXT("RELAY NETWORK RESTORED");
            MissionStatusBody = TEXT("NEW OBJECTIVE\n\nProceed to the Primary Transmission Array.\n\nUpload the station diagnostic package to Central Command.");
            MissionStatusEndTime = Now + 10.0;
        }
    }
    else
    {
        bMissionCompleted = true;
        MissionStatusTitle = TEXT("UPLINK ESTABLISHED");
        MissionStatusBody = TEXT("TRANSMITTING STATION DIAGNOSTICS...");
        MissionStatusEndTime = Now + 3.0;
        TransmissionCompleteMessageTime = Now + 2.5;
        UE_LOG(LogDroneMenu, Display, TEXT("Mission complete: primary uplink established; transmitting station diagnostics"));
    }
}

bool UDroneGameInstance::TryApplyPendingSave(float)
{
    if(!PendingLoad.IsValid())
    {
        PendingLoadTicker.Reset();
        return false;
    }
    APawn* Pawn=UGameplayStatics::GetPlayerPawn(this,0); if(!Pawn)return true;
    Pawn->SetActorTransform(PendingLoad->PlayerTransform,false,nullptr,ETeleportType::TeleportPhysics);
    if(PendingLoad->bHasEnergy) WriteEnergy(Pawn,PendingLoad->Energy);
    PendingLoad.Reset(); PendingLoadTicker.Reset(); HideMenu(false); return false;
}

void UDroneGameInstance::ApplySettings(float Master, float Music, float Sfx, const FString& WindowMode, const FString& Resolution)
{
    SetMusicVolume(Music);
    SetSFXVolume(Sfx);
    UGameUserSettings* S=GEngine?GEngine->GetGameUserSettings():nullptr; if(!S)return;
    FString L,R; if(Resolution.Split(TEXT("x"),&L,&R))S->SetScreenResolution(FIntPoint(FCString::Atoi(*L),FCString::Atoi(*R)));
    S->SetFullscreenMode(WindowMode==TEXT("FULLSCREEN")?EWindowMode::Fullscreen:(WindowMode==TEXT("WINDOWED")?EWindowMode::Windowed:EWindowMode::WindowedFullscreen));
    S->ApplySettings(false); S->SaveSettings();
    if (GEngine && GEngine->GetMainAudioDeviceRaw()) GEngine->GetMainAudioDeviceRaw()->SetTransientPrimaryVolume(Master);
}

void UDroneGameInstance::QuitGame()
{
    if(APlayerController* PC=UGameplayStatics::GetPlayerController(this,0))UKismetSystemLibrary::QuitGame(this,PC,EQuitPreference::Quit,false);
}

void UDroneGameInstance::SetupMenuPresentation(UWorld* World)
{
    if(!World)return;
    if (PresentationDrone.IsValid() && PresentationDrone->GetWorld() == World) return;
    if (PresentationTicker.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(PresentationTicker);
    AStaticMeshActor* Drone=World->SpawnActor<AStaticMeshActor>(FVector(0,0,80),FRotator(0,205,0));
    if(Drone)
    {
        UStaticMesh* Mesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Blueprints/Drone/Drone_Muslima_Mesh.Drone_Muslima_Mesh"));
        // This presentation actor is intentionally dynamic; set mobility before
        // assigning its mesh to avoid StaticMeshComponent mobility warnings.
        Drone->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
        Drone->GetStaticMeshComponent()->SetStaticMesh(Mesh); Drone->SetActorScale3D(FVector(1.8));
        Drone->GetRootComponent()->SetMobility(EComponentMobility::Movable);
        PresentationDrone = Drone;
        PresentationOrigin = Drone->GetActorLocation();
        PresentationTime = 0.f;
        PresentationTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UDroneGameInstance::TickMenuPresentation));
    }
    ACameraActor* Cam=World->SpawnActor<ACameraActor>(FVector(-460,160,150),FRotator(-5,-18,0));
    if(APlayerController* PC=UGameplayStatics::GetPlayerController(this,0))if(Cam)PC->SetViewTarget(Cam);
    for(const FVector Pos:{FVector(-80,220,260),FVector(100,-180,100)})
    {
        AActor* LightActor=World->SpawnActor<AActor>(Pos,FRotator::ZeroRotator);
        UPointLightComponent* Light=NewObject<UPointLightComponent>(LightActor); Light->RegisterComponent(); LightActor->SetRootComponent(Light);
        Light->SetIntensity(4500.f); Light->SetAttenuationRadius(900.f); Light->SetLightColor(Pos.Y > 0.f ? FLinearColor(0.05f, 0.55f, 1.f) : FLinearColor(0.1f, 0.2f, 0.5f));
    }

    UStaticMesh* StarMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    FRandomStream Stars(4187);
    for (int32 Index = 0; Index < 72; ++Index)
    {
        const FVector Position(Stars.FRandRange(450.f, 1350.f), Stars.FRandRange(-850.f, 850.f), Stars.FRandRange(-430.f, 620.f));
        AStaticMeshActor* Star = World->SpawnActor<AStaticMeshActor>(Position, FRotator::ZeroRotator);
        if (Star && StarMesh)
        {
            // Stars are runtime-spawned presentation actors, so make only these
            // components movable before replacing their default mesh.
            Star->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
            Star->GetStaticMeshComponent()->SetStaticMesh(StarMesh);
            Star->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Star->SetActorScale3D(FVector(Stars.FRandRange(0.008f, 0.025f)));
        }
    }
}

bool UDroneGameInstance::TickMenuPresentation(float DeltaTime)
{
    if (!PresentationDrone.IsValid() || !IsMainMenuWorld()) return false;
    PresentationTime += DeltaTime;
    AActor* Drone = PresentationDrone.Get();
    Drone->SetActorLocation(PresentationOrigin + FVector(0.f, 0.f, FMath::Sin(PresentationTime * 0.72f) * 12.f));
    FRotator Rotation = Drone->GetActorRotation();
    Rotation.Yaw += DeltaTime * 2.2f;
    Drone->SetActorRotation(Rotation);
    return true;
}
