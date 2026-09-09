#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class UAIRAFlightBoostComponent;

/** Non-interactive, component-owned Slate overlay; no HUD class or Widget Blueprint replacement. */
class SAIRAFlightBoostHUD : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SAIRAFlightBoostHUD) {}
        SLATE_ARGUMENT(UAIRAFlightBoostComponent*, BoostComponent)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args);

private:
    TWeakObjectPtr<UAIRAFlightBoostComponent> BoostComponent;
    TOptional<float> GetCharge() const;
    FSlateColor GetChargeColor() const;
    FText GetPercentage() const;
    FText GetStatus() const;
};
