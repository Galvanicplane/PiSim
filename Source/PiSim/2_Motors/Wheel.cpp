// Wheel.cpp

#include "Wheel.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

// @state: WIP - Tekerlek bileşeni yapıcısı
UPiSimWheelComponent::UPiSimWheelComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

// @state: WIP - Her frame tekerlek fizik durumunu günceller
void UPiSimWheelComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!CachedSkeletalMesh && GetOwner())
    {
        CachedSkeletalMesh = GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
    }

    ApplyWheelPhysics(DeltaTime);
}

// @state: WIP - Otopark UI için tekerlek parametre listesini döner
void UPiSimWheelComponent::GetInspectableProperties(TArray<FPiSimPropertyDescriptor>& OutProperties)
{
    FPiSimPropertyDescriptor RpmDesc;
    RpmDesc.PropertyId = TEXT("TargetRPM");
    RpmDesc.DisplayName = TEXT("Hedef Devir (RPM)");
    RpmDesc.PropertyType = EPiSimPropertyType::Slider;
    RpmDesc.MinValue = -3000.0f;
    RpmDesc.MaxValue = 3000.0f;
    RpmDesc.CurrentFloatValue = TargetRPM;
    RpmDesc.bContinuousTickUpdate = true;
    OutProperties.Add(RpmDesc);

    FPiSimPropertyDescriptor RadiusDesc;
    RadiusDesc.PropertyId = TEXT("RadiusCm");
    RadiusDesc.DisplayName = TEXT("Tekerlek Yarıçapı (cm)");
    RadiusDesc.PropertyType = EPiSimPropertyType::Slider;
    RadiusDesc.MinValue = 5.0f;
    RadiusDesc.MaxValue = 100.0f;
    RadiusDesc.CurrentFloatValue = RadiusCm;
    RadiusDesc.bContinuousTickUpdate = false;
    OutProperties.Add(RadiusDesc);

    FPiSimPropertyDescriptor FrictionDesc;
    FrictionDesc.PropertyId = TEXT("Friction");
    FrictionDesc.DisplayName = TEXT("Sürtünme Katsayısı");
    FrictionDesc.PropertyType = EPiSimPropertyType::Slider;
    FrictionDesc.MinValue = 0.1f;
    FrictionDesc.MaxValue = 3.0f;
    FrictionDesc.CurrentFloatValue = FrictionCoefficient;
    FrictionDesc.bContinuousTickUpdate = false;
    OutProperties.Add(FrictionDesc);
}

// @state: WIP - UI sliderından gelen float değerini tekerleğe yansıtır
void UPiSimWheelComponent::SetInspectablePropertyFloat(FName PropertyId, float NewValue)
{
    if (PropertyId == TEXT("TargetRPM"))
    {
        TargetRPM = NewValue;
    }
    else if (PropertyId == TEXT("RadiusCm"))
    {
        RadiusCm = FMath::Clamp(NewValue, 1.0f, 200.0f);
    }
    else if (PropertyId == TEXT("Friction"))
    {
        FrictionCoefficient = FMath::Clamp(NewValue, 0.01f, 10.0f);
    }
}

// @state: WIP - UI metin kutusundan gelen string değerini işler
void UPiSimWheelComponent::SetInspectablePropertyString(FName PropertyId, const FString& NewValue)
{
    if (PropertyId == TEXT("WheelMode"))
    {
        if (NewValue == TEXT("FreeWheel"))
        {
            WheelMode = EPiSimWheelMode::FreeWheel;
        }
        else
        {
            WheelMode = EPiSimWheelMode::DriveWheel;
        }
    }
}

// @state: WIP - Otopark UI tetikleyici butonunu işler
void UPiSimWheelComponent::TriggerInspectableAction(FName ActionId)
{
    if (ActionId == TEXT("ResetRPM"))
    {
        TargetRPM = 0.0f;
    }
}

// @state: WIP - Tekerlek dönme fiziğini ve açısal hızını uygular
void UPiSimWheelComponent::ApplyWheelPhysics(float DeltaTime)
{
    if (!CachedSkeletalMesh || !CachedSkeletalMesh->IsSimulatingPhysics(TargetBoneName))
    {
        return;
    }

    if (WheelMode == EPiSimWheelMode::DriveWheel)
    {
        // 1 RPM = 6 deg/sec
        float DesiredAngularVelocityDeg = TargetRPM * 6.0f;
        FTransform BoneTransform = CachedSkeletalMesh->GetBoneTransform(CachedSkeletalMesh->GetBoneIndex(TargetBoneName));
        FVector WorldSpinAxis = BoneTransform.TransformVectorNoScale(LocalSpinAxis).GetSafeNormal();

        FVector CurrentAngVel = CachedSkeletalMesh->GetPhysicsAngularVelocityInDegrees(TargetBoneName);
        FVector NonRollVel = CurrentAngVel - (WorldSpinAxis * (CurrentAngVel | WorldSpinAxis));
        FVector TargetSpinVel = WorldSpinAxis * DesiredAngularVelocityDeg;

        CachedSkeletalMesh->SetPhysicsAngularVelocityInDegrees(NonRollVel + TargetSpinVel, false, TargetBoneName);
        CurrentRPM = TargetRPM;
    }
}
