// ModularHUD.h
// PiSim HUD: TelemetryGateway'den araç verilerini okuyarak ekrana havacılık göstergelerini (Yapay Ufuk, Hız ve İrtifa Şeritleri, Pusula) çizer.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "TelemetryGateway.h"
#include "ModularHUD.generated.h"

UCLASS()
class PISIM_API APiSimModularHUD : public AHUD
{
    GENERATED_BODY()

public:
    APiSimModularHUD();

    virtual void DrawHUD() override;

protected:
    UPROPERTY()
    UPiSimTelemetryGateway* CachedGateway = nullptr;

    void DrawAvionicsPFD();
    void DrawHorizonLadder(float CenterX, float CenterY, float PitchDeg, float RollDeg);
    void DrawSpeedTape(float CenterX, float CenterY, float SpeedKmh);
    void DrawAltitudeTape(float CenterX, float CenterY, float AltitudeM);
    void DrawHeadingTape(float CenterX, float CenterY, float HeadingDeg);
    void DrawStatusOverlay();
};
