// PiSimHUD.h
// HUD Manager for PiSim Platform UI overlays and Garage Editor Widgets.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "PiSimGarageWidget.h"
#include "PiSimHUD.generated.h"

UCLASS()
class PISIM_API APiSimHUD : public AHUD
{
    GENERATED_BODY()

public:
    APiSimHUD();

protected:
    virtual void BeginPlay() override;

public:
    virtual void DrawHUD() override;

    /** Garage Editor User Widget Class */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|HUD")
    TSubclassOf<UPiSimGarageWidget> GarageWidgetClass;

    /** Active Instance of Garage Editor User Widget */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|HUD")
    UPiSimGarageWidget* ActiveGarageWidget = nullptr;

    /** Interactive Numeric Box State */
    UPROPERTY()
    int32 ActiveInputBoxID = -1;

    UPROPERTY()
    bool bIsDraggingBox = false;

    UPROPERTY()
    float DragStartMouseX = 0.0f;

    UPROPERTY()
    float DragStartValue = 0.0f;

    UPROPERTY()
    FString TypedInputString = TEXT("");

    void DrawInteractiveNumberBox(const FString& LabelText, const FString& UnitsText, float& Value, float MinVal, float MaxVal, float DragSensitivity, float BoxX, float BoxY, float BoxW, float BoxH, int32 BoxID, float MouseX, float MouseY, bool bJustPressed, class APlayerController* PC);
    void DrawInteractiveNumberBox(const FString& LabelText, const FString& UnitsText, double& Value, float MinVal, float MaxVal, float DragSensitivity, float BoxX, float BoxY, float BoxW, float BoxH, int32 BoxID, float MouseX, float MouseY, bool bJustPressed, class APlayerController* PC);
};


