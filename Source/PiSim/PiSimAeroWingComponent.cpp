// PiSimAeroWingComponent.cpp

#include "PiSimAeroWingComponent.h"
#include "Components/PrimitiveComponent.h"

UPiSimAeroWingComponent::UPiSimAeroWingComponent()
{
    PrimaryComponentTick.bCanEverTick = false; // Forces are driven in coordinated vehicle physics tick
}

void UPiSimAeroWingComponent::CalculateAndApplyAeroForces(float DeltaTime, UPrimitiveComponent* RootBody, float AirDensityKgM3)
{
    if (!RootBody || !RootBody->IsSimulatingPhysics()) return;

    FVector WingWorldLoc = GetComponentLocation();
    FVector WorldVelCmS = RootBody->GetPhysicsLinearVelocityAtPoint(WingWorldLoc);
    FVector WorldVelMs = WorldVelCmS * 0.01f; // cm/s -> m/s

    AirspeedMs = WorldVelMs.Size();
    AirspeedKmh = AirspeedMs * 3.6f;

    if (AirspeedMs < 0.2f)
    {
        CurrentLiftN = 0.0f;
        CurrentDragN = 0.0f;
        AngleOfAttackDeg = 0.0f;
        LastAppliedForceWorld = FVector::ZeroVector;
        return;
    }

    // Gövdeye veya kanada göre yerel hız vektörü
    FTransform WingXform = GetComponentTransform();
    FVector ForwardDir = WingXform.GetUnitAxis(EAxis::X); // İleri
    FVector RightDir   = WingXform.GetUnitAxis(EAxis::Y); // Sağ
    FVector UpDir      = WingXform.GetUnitAxis(EAxis::Z); // Yukarı

    float ForwardSpeed = WorldVelMs | ForwardDir;
    float VerticalSpeed = WorldVelMs | UpDir;

    // Hücum açısı (Angle of Attack, Alpha): İleri hız ile dikey hız arasındaki açı
    float AlphaRad = 0.0f;
    if (ForwardSpeed > 0.1f)
    {
        AlphaRad = FMath::Atan2(-VerticalSpeed, ForwardSpeed);
    }
    else if (ForwardSpeed < -0.1f)
    {
        AlphaRad = FMath::Atan2(-VerticalSpeed, -ForwardSpeed);
    }
    AngleOfAttackDeg = FMath::RadiansToDegrees(AlphaRad);

    // Dinamik Basınç: q = 0.5 * rho * V^2
    float DynPressure = 0.5f * AirDensityKgM3 * AirspeedMs * AirspeedMs;

    // Khan & Nahon Aerodinamik Lift Katsayısı C_L:
    float StallRad = FMath::DegreesToRadians(StallAngleDeg);
    float SafeAlpha = FMath::Clamp(AlphaRad, -StallRad, StallRad);

    // Elevon etkisi (derece -> radyan)
    float ElevonRad = FMath::DegreesToRadians(ControlSurfaceDeflectionDeg);
    float DeltaCL_Elevon = ElevonEffectiveness * ElevonRad;

    float CL = CL0 + (CLAlpha * SafeAlpha) + DeltaCL_Elevon;

    // Stall bölgesinde lift sönümlenmesi
    if (FMath::Abs(AlphaRad) > StallRad)
    {
        float StallFactor = FMath::Max(0.0f, 1.0f - (FMath::Abs(AlphaRad) - StallRad) * 2.0f);
        CL *= StallFactor;
    }

    // Sürükleme Katsayısı C_D (İndüklenmiş sürükleme dahil):
    float AspectRatio = (Wingspan > 0.01f && WingArea > 0.001f) ? (Wingspan * Wingspan / WingArea) : 5.0f;
    float OswaldEfficiency = 0.85f;
    float CD_Induced = (CL * CL) / (PI * OswaldEfficiency * AspectRatio);
    float CD = CD0 + CD_Induced;

    // Kuvvet Büyüklükleri (Newton)
    float LiftN = DynPressure * WingArea * CL;
    float DragN = DynPressure * WingArea * CD;

    // Taşıma (Lift) yönü: Hız vektörüne dik ve yukarı yönde
    FVector VelNorm = WorldVelMs.GetSafeNormal();
    FVector LiftDir = (UpDir - (VelNorm * (UpDir | VelNorm))).GetSafeNormal();
    if (LiftDir.IsNearlyZero()) LiftDir = UpDir;

    // Sürükleme (Drag) yönü: Hız vektörünün tersine
    FVector DragDir = -VelNorm;

    FVector TotalForceWorldN = (LiftDir * LiftN) + (DragDir * DragN);
    FVector TotalForceUE = TotalForceWorldN * 100.0f; // 1 N = 100 kg*cm/s²

    RootBody->AddForceAtLocation(TotalForceUE, WingWorldLoc);

    CurrentLiftN = LiftN;
    CurrentDragN = DragN;
    LastAppliedForceWorld = TotalForceWorldN;
}
