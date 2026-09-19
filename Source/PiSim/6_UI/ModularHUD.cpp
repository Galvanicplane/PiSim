// ModularHUD.cpp

#include "ModularHUD.h"
#include "Engine/Canvas.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

// @state: WIP - HUD yapıcısı
APiSimModularHUD::APiSimModularHUD()
{
}

// @state: WIP - Her frame HUD çizimini gerçekleştirir
void APiSimModularHUD::DrawHUD()
{
    Super::DrawHUD();

    if (!CachedGateway)
    {
        APawn* PlayerPawn = GetOwningPawn();
        if (PlayerPawn)
        {
            CachedGateway = PlayerPawn->FindComponentByClass<UPiSimTelemetryGateway>();
        }
    }

    DrawTelemetryOverlay();
}

// @state: WIP - Ekrana canlı telemetri değerlerini (Hız, İrtifa vb.) çizer
void APiSimModularHUD::DrawTelemetryOverlay()
{
    if (!Canvas || !CachedGateway)
    {
        return;
    }

    FPiSimGPSData GPS;
    CachedGateway->GetLatestGPSData(GPS);

    FPiSimIMUData IMU;
    CachedGateway->GetLatestIMUData(IMU);

    FString TelemetryText = FString::Printf(TEXT("HIZ: %.1f km/h | İRTİFA: %.1f m | ENLEM: %.5f | BOYLAM: %.5f"),
        GPS.SpeedKmh, GPS.AltitudeMeters, GPS.Latitude, GPS.Longitude);

    Canvas->SetDrawColor(FColor::Green);
    Canvas->DrawText(GEngine->GetMediumFont(), TelemetryText, 50.0f, 50.0f, 1.2f, 1.2f);
}
