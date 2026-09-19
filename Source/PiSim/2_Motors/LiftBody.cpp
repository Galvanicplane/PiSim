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
    AreaDesc.DisplayName = TEXT("Yüzey Alanı (m^2)");
    AreaDesc.PropertyType = EPiSimPropertyType::Slider;
    AreaDesc.MinValue = 0.01f;
    AreaDesc.MaxValue = 10.0f;
    AreaDesc.CurrentFloatValue = WingAreaSquareMeters;
    AreaDesc.bContinuousTickUpdate = false;
    OutProperties.Add(AreaDesc);

    FPiSimPropertyDescriptor SlopeDesc;
    SlopeDesc.PropertyId = TEXT("LiftSlope");
    SlopeDesc.DisplayName = TEXT("Kaldırma Katsayısı Eğimi (CL Alpha)");
    SlopeDesc.PropertyType = EPiSimPropertyType::Slider;
    SlopeDesc.MinValue = 1.0f;
    SlopeDesc.MaxValue = 10.0f;
    SlopeDesc.CurrentFloatValue = LiftSlope;
    SlopeDesc.bContinuousTickUpdate = false;
    OutProperties.Add(SlopeDesc);
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
}

// @state: WIP - UI metin kutusundan gelen string değerini işler
void UPiSimLiftBodyComponent::SetInspectablePropertyString(FName PropertyId, const FString& NewValue)
{
}

// @state: WIP - UI tetikleyici butonunu işler
void UPiSimLiftBodyComponent::TriggerInspectableAction(FName ActionId)
{
}

// @state: WIP - Hücum açısı (AoA) ve dinamik basınca göre Lift/Drag hesaplar
void UPiSimLiftBodyComponent::ApplyAerodynamicForces(float DeltaTime)
{
    if (!CachedSkeletalMesh)
    {
        return;
    }

    FTransform BoneTransform = TargetBoneName.IsNone() ? CachedSkeletalMesh->GetComponentTransform() : CachedSkeletalMesh->GetSocketTransform(TargetBoneName);
    FVector BoneVelocity = CachedSkeletalMesh->GetPhysicsLinearVelocity(TargetBoneName); // cm/s
    float SpeedMetersPerSec = BoneVelocity.Size() / 100.0f;

    if (SpeedMetersPerSec < 1.0f)
    {
        return;
    }

    FVector ForwardDir = BoneTransform.GetUnitAxis(EAxis::X);
    FVector UpDir = BoneTransform.GetUnitAxis(EAxis::Z);
    FVector AirflowDir = -BoneVelocity.GetSafeNormal();

    // Dinamik Basınç: q = 0.5 * rho * v^2
    float DynamicPressure = 0.5f * FluidDensity * (SpeedMetersPerSec * SpeedMetersPerSec);

    // Basit Hücum Açısı (Angle of Attack)
    float AoA = FMath::DegreesToRadians(FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(ForwardDir | AirflowDir, -1.0f, 1.0f))) - 180.0f);
    float CL = FMath::Clamp(LiftSlope * AoA, -1.5f, 1.5f);
    float CD = DragCoefficientZero + (CL * CL) / (PI * 6.0f);

    float LiftForceN = CL * DynamicPressure * WingAreaSquareMeters;
    float DragForceN = CD * DynamicPressure * WingAreaSquareMeters;

    FVector LiftVec = UpDir * LiftForceN * 100.0f;
    FVector DragVec = AirflowDir * DragForceN * 100.0f;

    CachedSkeletalMesh->AddForceAtLocation(LiftVec + DragVec, BoneTransform.GetLocation(), TargetBoneName);
}
