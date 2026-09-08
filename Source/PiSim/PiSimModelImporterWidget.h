// PiSimModelImporterWidget.h
// Clean, Dedicated Slate-Powered HUD Widget for PiSimModelImporter.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "PiSimModelImporterWidget.generated.h"

class APiSimModelImporter;

UCLASS()
class PISIM_API UPiSimModelImporterWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    UPROPERTY(BlueprintReadOnly, Category = "PiSim|UI")
    APiSimModelImporter* TargetImporter = nullptr;

private:
    FReply OnScale01Clicked();
    FReply OnScale10Clicked();
    FReply OnScale100Clicked();
    FReply OnReimportClicked();
    FReply OnTogglePhysicsClicked();

    FSlateBrush CameraPreviewBrush;
    TSharedPtr<class STextBlock> CameraPipInfoText;
    TSharedPtr<class STextBlock> ConnectionBadgeText;
    TSharedPtr<class SBorder> ConnectionBadgeBorder;
    TSharedPtr<class STextBlock> ConnectionStagesText;
    TSharedPtr<class STextBlock> IncomingDataText;
    TSharedPtr<class STextBlock> OutgoingTelemetryText;
    TSharedPtr<class STextBlock> ModelCadStatusText;
    TSharedPtr<class STextBlock> ConnectionDebugLogText;
    TSharedPtr<class STextBlock> PhysicsButtonText;
    TSharedPtr<class SBorder> PhysicsButtonBorder;
};
