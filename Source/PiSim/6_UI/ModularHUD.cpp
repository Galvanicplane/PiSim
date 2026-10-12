// ModularHUD.cpp

#include "ModularHUD.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
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

    if (!Canvas)
    {
        return;
    }

    if (!CachedGateway)
    {
        APawn* PlayerPawn = GetOwningPawn();
        if (PlayerPawn)
        {
            CachedGateway = PlayerPawn->FindComponentByClass<UPiSimTelemetryGateway>();
        }
    }

    // Sadece uçuş HUD'ı açıkken PFD göstergelerini çiz (Garajda ekranı temiz tut)
    if (bShowAvionicsHUD)
    {
        DrawAvionicsPFD();
        DrawStatusOverlay();
    }
}

// @state: WIP - Birincil Uçuş Ekranı (PFD) avionik göstergelerini çizer
void APiSimModularHUD::DrawAvionicsPFD()
{
    if (!Canvas || !CachedGateway)
    {
        return;
    }

    float CenterX = Canvas->SizeX * 0.5f;
    float CenterY = Canvas->SizeY * 0.5f;

    FPiSimIMUData IMU;
    CachedGateway->GetLatestIMUData(IMU);
    FRotator Rot = IMU.Orientation.Rotator();

    FPiSimGPSData GPS;
    CachedGateway->GetLatestGPSData(GPS);

    FPiSimBaroData Baro;
    CachedGateway->GetLatestBaroData(Baro);

    // 1. Yapay Ufuk ve Yunuslama Merdiveni (Pitch & Roll)
    DrawHorizonLadder(CenterX, CenterY, Rot.Pitch, Rot.Roll);

    // 2. Sol Hız Şeridi (Speed Tape)
    DrawSpeedTape(CenterX - 220.0f, CenterY, GPS.SpeedKmh);

    // 3. Sağ İrtifa Şeridi (Altitude Tape)
    DrawAltitudeTape(CenterX + 220.0f, CenterY, Baro.PressureAltitudeM > 0.0f ? Baro.PressureAltitudeM : GPS.AltitudeMeters);

    // 4. Üst Pusula Şeridi (Heading Tape)
    float Heading = Rot.Yaw < 0.0f ? Rot.Yaw + 360.0f : Rot.Yaw;
    DrawHeadingTape(CenterX, CenterY - 180.0f, Heading);
}

// @state: WIP - Yapay ufuk çizgisi ve merkez hedef artı işareti
void APiSimModularHUD::DrawHorizonLadder(float CenterX, float CenterY, float PitchDeg, float RollDeg)
{
    FLinearColor GreenHud(0.0f, 1.0f, 0.45f, 0.85f);

    // Merkez uçak referans işareti: [ --- o --- ]
    Canvas->K2_DrawLine(FVector2D(CenterX - 35.0f, CenterY), FVector2D(CenterX - 10.0f, CenterY), 2.0f, GreenHud);
    Canvas->K2_DrawLine(FVector2D(CenterX + 10.0f, CenterY), FVector2D(CenterX + 35.0f, CenterY), 2.0f, GreenHud);
    Canvas->K2_DrawLine(FVector2D(CenterX - 10.0f, CenterY), FVector2D(CenterX - 10.0f, CenterY + 8.0f), 2.0f, GreenHud);
    Canvas->K2_DrawLine(FVector2D(CenterX + 10.0f, CenterY), FVector2D(CenterX + 10.0f, CenterY + 8.0f), 2.0f, GreenHud);

    // Ufuk çizgisi (Pitch ofsetli, Roll açılı)
    float PitchPixelOffset = PitchDeg * 4.0f;
    float AngleRad = FMath::DegreesToRadians(-RollDeg);
    float CosA = FMath::Cos(AngleRad);
    float SinA = FMath::Sin(AngleRad);

    float HalfLen = 120.0f;
    FVector2D LeftP(CenterX - (HalfLen * CosA), CenterY + PitchPixelOffset - (HalfLen * SinA));
    FVector2D RightP(CenterX + (HalfLen * CosA), CenterY + PitchPixelOffset + (HalfLen * SinA));

    Canvas->K2_DrawLine(LeftP, RightP, 1.5f, GreenHud);
}

// @state: WIP - Sol hız kutusu ve sayısal gösterge
void APiSimModularHUD::DrawSpeedTape(float X, float Y, float SpeedKmh)
{
    FLinearColor CyanHud(0.0f, 0.88f, 1.0f, 0.9f);
    Canvas->K2_DrawBox(FVector2D(X - 45.0f, Y - 25.0f), FVector2D(90.0f, 50.0f), 1.5f, CyanHud);

    FString SpdStr = FString::Printf(TEXT("%.0f"), SpeedKmh);
    Canvas->DrawText(GEngine->GetLargeFont(), SpdStr, X - 20.0f, Y - 18.0f, 1.0f, 1.0f, FFontRenderInfo());

    Canvas->DrawText(GEngine->GetSmallFont(), TEXT("KM/H"), X - 18.0f, Y + 10.0f, 0.8f, 0.8f);
}

// @state: WIP - Sağ irtifa kutusu ve sayısal gösterge
void APiSimModularHUD::DrawAltitudeTape(float X, float Y, float AltitudeM)
{
    FLinearColor CyanHud(0.0f, 0.88f, 1.0f, 0.9f);
    Canvas->K2_DrawBox(FVector2D(X - 45.0f, Y - 25.0f), FVector2D(90.0f, 50.0f), 1.5f, CyanHud);

    FString AltStr = FString::Printf(TEXT("%.0f"), AltitudeM);
    Canvas->DrawText(GEngine->GetLargeFont(), AltStr, X - 22.0f, Y - 18.0f, 1.0f, 1.0f, FFontRenderInfo());

    Canvas->DrawText(GEngine->GetSmallFont(), TEXT("METRE"), X - 18.0f, Y + 10.0f, 0.8f, 0.8f);
}

// @state: WIP - Üst pusula istikamet şeridi
void APiSimModularHUD::DrawHeadingTape(float X, float Y, float HeadingDeg)
{
    FLinearColor GreenHud(0.0f, 1.0f, 0.45f, 0.9f);
    Canvas->K2_DrawBox(FVector2D(X - 35.0f, Y - 15.0f), FVector2D(70.0f, 30.0f), 1.5f, GreenHud);

    FString HdgStr = FString::Printf(TEXT("%03.0f°"), HeadingDeg);
    Canvas->DrawText(GEngine->GetMediumFont(), HdgStr, X - 20.0f, Y - 10.0f, 1.0f, 1.0f);
}

// @state: WIP - Sol alt durum ve telemetri çubuğu
void APiSimModularHUD::DrawStatusOverlay()
{
    if (!Canvas || !CachedGateway) return;

    FPiSimGPSData GPS;
    CachedGateway->GetLatestGPSData(GPS);

    FString StatusStr = FString::Printf(
        TEXT("PiSim Avionics PFD | GPS: %.5f, %.5f | Uydu: %d | İrtifa: %.1fm | Hız: %.1f km/h"),
        GPS.Latitude, GPS.Longitude, GPS.Satellites, GPS.AltitudeMeters, GPS.SpeedKmh
    );

    Canvas->SetDrawColor(FColor(0, 220, 255));
    Canvas->DrawText(GEngine->GetSmallFont(), StatusStr, 25.0f, Canvas->SizeY - 35.0f, 1.0f, 1.0f);
}
