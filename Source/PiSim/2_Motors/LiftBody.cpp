// LiftBody.cpp

#include "LiftBody.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

// @state: WIP - Kaldırma gövdesi bileşeni yapıcısı
UPiSimLiftBodyComponent::UPiSimLiftBodyComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

// @state: WIP - Her frame akış hızına göre aerodinamik kuvvetleri uygular
void UPiSimLiftBodyComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!CachedSkeletalMesh && GetOwner())
    {
        CachedSkeletalMesh = GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
    }

    ApplyAerodynamicForces(DeltaTime);
}

// @state: WIP - Otopark UI parametre listesini döner
void UPiSimLiftBodyComponent::GetInspectableProperties(TArray<FPiSimPropertyDescriptor>& OutProperties)
{
    FPiSimPropertyDescriptor AreaDesc;
    AreaDesc.PropertyId = TEXT("WingAreaSquareMeters");
    AreaDesc.DisplayName = TEXT("Kanat Alanı (m²)");
    AreaDesc.PropertyType = EPiSimPropertyType::Slider;
    AreaDesc.MinValue = 0.05f;
    AreaDesc.MaxValue = 5.0f;
    AreaDesc.CurrentFloatValue = WingAreaSquareMeters;
    AreaDesc.bContinuousTickUpdate = false;
    OutProperties.Add(AreaDesc);

    FPiSimPropertyDescriptor SlopeDesc;
    SlopeDesc.PropertyId = TEXT("LiftSlope");
    SlopeDesc.DisplayName = TEXT("Kaldırma Eğimi (CL Alpha)");
    SlopeDesc.PropertyType = EPiSimPropertyType::Slider;
    SlopeDesc.MinValue = 1.0f;
    SlopeDesc.MaxValue = 10.0f;
    SlopeDesc.CurrentFloatValue = LiftSlope;
    SlopeDesc.bContinuousTickUpdate = false;
    OutProperties.Add(SlopeDesc);

    FPiSimPropertyDescriptor DeflDesc;
    DeflDesc.PropertyId = TEXT("ControlSurfaceDeflectionDeg");
    DeflDesc.DisplayName = TEXT("Kontrol Yüzeyi Sapması (°)");
    DeflDesc.PropertyType = EPiSimPropertyType::Slider;
    DeflDesc.MinValue = -30.0f;
    DeflDesc.MaxValue = 30.0f;
    DeflDesc.CurrentFloatValue = ControlSurfaceDeflectionDeg;
    DeflDesc.bContinuousTickUpdate = true;
    OutProperties.Add(DeflDesc);
}

// @state: WIP - UI sliderından gelen float değerini kanada yansıtır
void UPiSimLiftBodyComponent::SetInspectablePropertyFloat(FName PropertyId, float NewValue)
{
    if (PropertyId == TEXT("WingAreaSquareMeters"))
    {
        WingAreaSquareMeters = FMath::Max(0.001f, NewValue);
    }
    else if (PropertyId == TEXT("LiftSlope"))
    {
        LiftSlope = FMath::Max(0.1f, NewValue);
    }
    else if (PropertyId == TEXT("ControlSurfaceDeflectionDeg"))
    {
        ControlSurfaceDeflectionDeg = FMath::Clamp(NewValue, -45.0f, 45.0f);
    }
}

// @state: WIP - UI metin kutusundan gelen string değerini işler
void UPiSimLiftBodyComponent::SetInspectablePropertyString(FName PropertyId, const FString& NewValue)
{
}

// @state: WIP - UI tetikleyici butonunu işler
void UPiSimLiftBodyComponent::TriggerInspectableAction(FName ActionId)
{
}

// @state: WIP - Hücum açısı (AoA), elevon sapması ve akış hızına göre Khan & Nahon Lift/Drag modelini hesaplar
void UPiSimLiftBodyComponent::ApplyAerodynamicForces(float DeltaTime)
{
    if (!CachedSkeletalMesh)
    {
        return;
    }

    FTransform BoneTransform = TargetBoneName.IsNone() ? CachedSkeletalMesh->GetComponentTransform() : CachedSkeletalMesh->GetSocketTransform(TargetBoneName);
    FVector BoneVelocity = CachedSkeletalMesh->GetPhysicsLinearVelocity(TargetBoneName); // cm/s
    float SpeedMs = BoneVelocity.Size() / 100.0f;
    CurrentAirspeedKmh = SpeedMs * 3.6f;

    // Minimum akış hızı (1.5 m/s altı aero ihmal edilir)
    if (SpeedMs < 1.5f)
    {
        CurrentLiftNewtons = 0.0f;
        CurrentDragNewtons = 0.0f;
        CurrentAoADeg = 0.0f;
        return;
    }

    FVector ForwardDir = BoneTransform.GetUnitAxis(EAxis::X); // Burun yönü
    FVector UpDir = BoneTransform.GetUnitAxis(EAxis::Z);      // Üst yön
    FVector AirflowDir = -BoneVelocity.GetSafeNormal();       // Göreceli rüzgar

    // Dinamik Basınç: q = 0.5 * rho * v^2
    float DynamicPressure = 0.5f * FluidDensity * (SpeedMs * SpeedMs);

    // Hücum Açısı (Angle of Attack, radyan)
    float DotForward = FMath::Clamp(ForwardDir | AirflowDir, -1.0f, 1.0f);
    float AoARad = FMath::Acos(DotForward);
    // Yön işareti: Rüzgar alttan geliyorsa pozitif AoA
    if ((AirflowDir | UpDir) < 0.0f)
    {
        AoARad = -AoARad;
    }
    CurrentAoADeg = FMath::RadiansToDegrees(AoARad);

    // Kontrol yüzeyi sapmasının etkinliği (delta CL)
    float DeflectionRad = FMath::DegreesToRadians(ControlSurfaceDeflectionDeg);
    float DeltaCL_Control = ElevonEffectiveness * DeflectionRad;

    // Taşıma Katsayısı (CL)
    float CL = CL0 + (LiftSlope * AoARad) + DeltaCL_Control;

    // Perdövites (Stall) sınırlaması
    float StallRad = FMath::DegreesToRadians(StallAngleDeg);
    if (FMath::Abs(AoARad) > StallRad)
    {
        float Excess = FMath::Abs(AoARad) - StallRad;
        CL *= FMath::Max(0.2f, 1.0f - (Excess * 2.0f));
    }
    CL = FMath::Clamp(CL, -1.8f, 1.8f);

    // İndüklenmiş Sürükleme Katsayısı (CD = CD0 + CL^2 / (pi * AR * e))
    float AspectRatio = (WingspanMeters * WingspanMeters) / FMath::Max(0.01f, WingAreaSquareMeters);
    float OswaldEff = 0.85f;
    float InducedDrag = (CL * CL) / (PI * AspectRatio * OswaldEff);
    float CD = DragCoefficientZero + InducedDrag;
    CD = FMath::Clamp(CD, 0.01f, 1.5f);

    // Kuvvet Büyüklükleri (Newton)
    CurrentLiftNewtons = CL * DynamicPressure * WingAreaSquareMeters;
    CurrentDragNewtons = CD * DynamicPressure * WingAreaSquareMeters;

    // Kaldırma Kuvveti Rüzgara Dik, Sürükleme Rüzgar Boyuncadır
    FVector LiftDir = (AirflowDir ^ BoneTransform.GetUnitAxis(EAxis::Y)).GetSafeNormal();
    FVector DragDir = AirflowDir;

    // UE kuvvet ölçeği: Newton * 100
    FVector TotalAeroForce = (LiftDir * CurrentLiftNewtons + DragDir * CurrentDragNewtons) * 100.0f;

    CachedSkeletalMesh->AddForceAtLocation(TotalAeroForce, BoneTransform.GetLocation(), TargetBoneName);
}
