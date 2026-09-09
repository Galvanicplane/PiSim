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
    // Tab Navigation & Top Actions
    FReply OnTabMotorsClicked();
    FReply OnTabTelemetryClicked();
    FReply OnTabSensorsClicked();
    FReply OnScale01Clicked();
    FReply OnScale10Clicked();
    FReply OnScale100Clicked();
    FReply OnReimportClicked();
    FReply OnTogglePhysicsClicked();

    // Motor Tab Actions
    FReply OnToggleDisplayModeClicked();
    FReply OnToggleAdvancedModeClicked();
    FReply OnSelectBoneClicked(int32 BoneIdx);
    FReply OnRemoveMotorClicked();
    FReply OnAssignRoleClicked(uint8 RoleEnumVal);
    void OnMotorTestSliderChanged(float NewValue);

    // Sensor Tab Actions
    FReply OnToggleSensorMarkersClicked();
    FReply OnSelectSensorClicked(int32 SensorIdx);
    FReply OnRemoveSensorClicked();
    FReply OnAddVirtualSensorClicked(uint8 SensorTypeEnumVal);

    // Slate Tab Container & View Switcher
    TSharedPtr<class SWidgetSwitcher> MainTabSwitcher;
    TSharedPtr<class SBorder> TabMotorsBtnBorder;
    TSharedPtr<class SBorder> TabTelemetryBtnBorder;
    TSharedPtr<class SBorder> TabSensorsBtnBorder;

    // Motor Tab Widgets
    TSharedPtr<class STextBlock> DisplayModeButtonText;
    TSharedPtr<class STextBlock> AdvancedModeButtonText;
    TSharedPtr<class SScrollBox> BoneListScrollBox;
    TSharedPtr<class SBorder> BoneInspectorBorder;
    TSharedPtr<class STextBlock> SelectedBoneTitleText;
    TSharedPtr<class STextBlock> SelectedBoneRoleBadgeText;
    TSharedPtr<class STextBlock> MotorDetailsText;
    TSharedPtr<class STextBlock> MotorTestSliderValueText;
    TSharedPtr<class SSlider> MotorTestSlider;
    TSharedPtr<class SBox> AssignMotorBox;
    TSharedPtr<class SBox> ExistingMotorBox;

    // Telemetry Tab Widgets
    TSharedPtr<class STextBlock> ConnectionBadgeText;
    TSharedPtr<class SBorder> ConnectionBadgeBorder;
    TSharedPtr<class STextBlock> ConnectionStagesText;
    TSharedPtr<class STextBlock> IncomingDataText;
    TSharedPtr<class STextBlock> OutgoingTelemetryText;
    TSharedPtr<class STextBlock> ModelCadStatusText;
    TSharedPtr<class STextBlock> ConnectionDebugLogText;
    TSharedPtr<class STextBlock> PhysicsButtonText;
    TSharedPtr<class SBorder> PhysicsButtonBorder;

    // Sensor Tab Widgets
    TSharedPtr<class SScrollBox> SensorListScrollBox;
    TSharedPtr<class SBorder> SensorInspectorBorder;
    TSharedPtr<class STextBlock> SelectedSensorTitleText;
    TSharedPtr<class STextBlock> SelectedSensorBadgeText;
    TSharedPtr<class STextBlock> SensorDetailsText;
    TSharedPtr<class STextBlock> SensorMarkerToggleText;
    TSharedPtr<class SBox> CameraPreviewBox;
    TSharedPtr<class SBorder> SensorCustomTelemetryBorder;
    TSharedPtr<class STextBlock> SensorCustomTelemetryText;

    // Mini Preview PiP Brush & Widgets
    FSlateBrush CameraPreviewBrush;
    TSharedPtr<class STextBlock> CameraPipInfoText;

    int32 LastRenderedBoneCount = -1;
    int32 LastRenderedSensorCount = -1;
    int32 CachedSelectedBone = -2;
    int32 CachedSelectedSensor = -2;
};
