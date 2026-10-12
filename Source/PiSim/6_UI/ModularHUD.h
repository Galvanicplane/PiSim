// ModularHUD.h
// PiSim HUD: TelemetryGateway'den araç verilerini okuyarak ekrana havacılık göstergelerini (Yapay Ufuk, Hız ve İrtifa Şeritleri, Pusula) çizer.
// Garaj / İthalat modunda ekranı kirletmemesi için varsayılan olarak kapalıdır.

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

    // @state: WIP - Uçuş HUD'ının görünürlüğünü ayarlar
    UFUNCTION(BlueprintCallable, Category = "PiSim|HUD")
    void SetAvionicsHUDVisible(bool bVisible) { bShowAvionicsHUD = bVisible; }

    // @state: WIP - Uçuş HUD'ını açar/kapatır (Örn. H tuşu ile)
    UFUNCTION(BlueprintCallable, Category = "PiSim|HUD")
    void ToggleAvionicsHUD() { bShowAvionicsHUD = !bShowAvionicsHUD; }

    // @state: WIP - Uçuş HUD'ının aktif olup olmadığını döner
    UFUNCTION(BlueprintPure, Category = "PiSim|HUD")
    bool IsAvionicsHUDVisible() const { return bShowAvionicsHUD; }

protected:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|HUD")
    bool bShowAvionicsHUD = false;

    UPROPERTY()
    UPiSimTelemetryGateway* CachedGateway = nullptr;

    void DrawAvionicsPFD();
    void DrawHorizonLadder(float CenterX, float CenterY, float PitchDeg, float RollDeg);
    void DrawSpeedTape(float CenterX, float CenterY, float SpeedKmh);
    void DrawAltitudeTape(float CenterX, float CenterY, float AltitudeM);
    void DrawHeadingTape(float CenterX, float CenterY, float HeadingDeg);
    void DrawStatusOverlay();
};
