#include "AIRAFlightBoostHUD.h"
#include "AIRAFlightBoostComponent.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

void SAIRAFlightBoostHUD::Construct(const FArguments& Args)
{
    BoostComponent = Args._BoostComponent;
    SetVisibility(EVisibility::HitTestInvisible);
    ChildSlot
    [
        SNew(SOverlay)
        + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Center).Padding(FMargin(0, 0, 50, 0))
        [
            SNew(SBox).WidthOverride(136)
            [
                SNew(SBorder)
                .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                .BorderBackgroundColor(FLinearColor(0.005f, 0.018f, 0.035f, 0.94f))
                .Padding(12)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(STextBlock).Text(NSLOCTEXT("AIRAFlightBoost", "Label", "BOOST"))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
                        .ColorAndOpacity(this, &SAIRAFlightBoostHUD::GetChargeColor)
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 8)
                    [
                        SNew(SBox).HeightOverride(12)
                        [
                            SNew(SProgressBar).Percent(this, &SAIRAFlightBoostHUD::GetCharge)
                            .FillColorAndOpacity(this, &SAIRAFlightBoostHUD::GetChargeColor)
                        ]
                    ]
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(STextBlock).Text(this, &SAIRAFlightBoostHUD::GetPercentage)
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 18))
                        .ColorAndOpacity(FLinearColor(0.72f, 0.92f, 1.0f))
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 5, 0, 0)
                    [
                        SNew(STextBlock).Text(this, &SAIRAFlightBoostHUD::GetStatus)
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                        .ColorAndOpacity(this, &SAIRAFlightBoostHUD::GetChargeColor)
                    ]
                ]
            ]
        ]
    ];
}

TOptional<float> SAIRAFlightBoostHUD::GetCharge() const
{
    return BoostComponent.IsValid() ? BoostComponent->GetBoostEnergyPercent() : 0.0f;
}

FSlateColor SAIRAFlightBoostHUD::GetChargeColor() const
{
    const float Charge = GetCharge().Get(0.0f);
    if (Charge < 0.10f) return FLinearColor(1.0f, 0.18f, 0.12f);
    if (Charge < 0.25f) return FLinearColor(1.0f, 0.65f, 0.12f);
    return FLinearColor(0.04f, 0.78f, 1.0f);
}

FText SAIRAFlightBoostHUD::GetPercentage() const
{
    // Do not display 0% while a small usable charge remains.
    const int32 Percent = FMath::CeilToInt(GetCharge().Get(0.0f) * 100.0f);
    return FText::Format(NSLOCTEXT("AIRAFlightBoost", "Percent", "{0}%"), FText::AsNumber(Percent));
}

FText SAIRAFlightBoostHUD::GetStatus() const
{
    if (BoostComponent.IsValid())
    {
        if (BoostComponent->GetBoostEnergy() <= 0.0f) return NSLOCTEXT("AIRAFlightBoost", "Depleted", "BOOST DEPLETED");
        if (BoostComponent->IsBoostActive()) return NSLOCTEXT("AIRAFlightBoost", "Active", "BOOST ACTIVE");
        if (BoostComponent->IsRecharging()) return NSLOCTEXT("AIRAFlightBoost", "Recharge", "RECHARGING");
    }
    return NSLOCTEXT("AIRAFlightBoost", "Ready", "BOOST READY");
}
