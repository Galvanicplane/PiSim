// ModularHUD.h
// PiSim HUD: TelemetryGateway'den araç verilerini okuyarak ekrana canlı telemetri çizimi yapar.

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

    void DrawTelemetryOverlay();
};
