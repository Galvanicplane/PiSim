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

// @state: WIP - PWM sinyalinden (1000..2000 us) RPM veya Direksiyon açısı ayarlar
void UPiSimWheelComponent::SetFromPWM(int32 InPWM, bool bIsSteering)
{
    if (bManualOverride)
    {
        return;
    }

    float Norm = FMath::Clamp((InPWM - 1500) / 500.0f, -1.0f, 1.0f); // -1.0 .. +1.0

    if (bIsSteering || WheelMode == EPiSimWheelMode::SteeredWheel)
    {
        TargetSteerAngleDeg = Norm * MaxSteerAngleDeg;
    }
    else
    {
        // 1000 = -1000 RPM (Geri), 1500 = 0 (Dur), 2000 = +1000 RPM (İleri)
        TargetRPM = Norm * 1200.0f;
    }
}

// @state: WIP - Otopark UI için tekerlek parametre listesini döner
void UPiSimWheelComponent::GetInspectableProperties(TArray<FPiSimPropertyDescriptor>& OutProperties)
{
    FPiSimPropertyDescriptor ModeDesc;
    ModeDesc.PropertyId = TEXT("WheelMode");
    ModeDesc.DisplayName = TEXT("Tekerlek Modu");
    ModeDesc.PropertyType = EPiSimPropertyType::Dropdown;
    ModeDesc.CurrentStringValue = (WheelMode == EPiSimWheelMode::DriveWheel) ? TEXT("DriveWheel") :
                                 (WheelMode == EPiSimWheelMode::SteeredWheel) ? TEXT("SteeredWheel") : TEXT("FreeWheel");
    ModeDesc.DropdownOptions = { TEXT("DriveWheel"), TEXT("SteeredWheel"), TEXT("FreeWheel") };
    ModeDesc.bContinuousTickUpdate = false;
    OutProperties.Add(ModeDesc);

    FPiSimPropertyDescriptor RpmDesc;
    RpmDesc.PropertyId = TEXT("TargetRPM");
    RpmDesc.DisplayName = TEXT("Hedef Devir (RPM)");
    RpmDesc.PropertyType = EPiSimPropertyType::Slider;
    RpmDesc.MinValue = -2500.0f;
    RpmDesc.MaxValue = 2500.0f;
    RpmDesc.CurrentFloatValue = TargetRPM;
    RpmDesc.bContinuousTickUpdate = true;
    OutProperties.Add(RpmDesc);

    if (WheelMode == EPiSimWheelMode::SteeredWheel)
    {
        FPiSimPropertyDescriptor SteerDesc;
        SteerDesc.PropertyId = TEXT("TargetSteerAngleDeg");
        SteerDesc.DisplayName = TEXT("Direksiyon Açısı (°)");
        SteerDesc.PropertyType = EPiSimPropertyType::Slider;
        SteerDesc.MinValue = -MaxSteerAngleDeg;
        SteerDesc.MaxValue = MaxSteerAngleDeg;
        SteerDesc.CurrentFloatValue = TargetSteerAngleDeg;
        SteerDesc.bContinuousTickUpdate = true;
        OutProperties.Add(SteerDesc);
    }

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

    FPiSimPropertyDescriptor ChDesc;
    ChDesc.PropertyId = TEXT("ArduPilotChannel");
    ChDesc.DisplayName = TEXT("ArduPilot Kanalı (1-16)");
    ChDesc.PropertyType = EPiSimPropertyType::Slider;
    ChDesc.MinValue = 1.0f;
    ChDesc.MaxValue = 16.0f;
    ChDesc.CurrentFloatValue = (float)ArduPilotChannel;
    ChDesc.bContinuousTickUpdate = false;
    OutProperties.Add(ChDesc);
}

// @state: WIP - UI sliderından gelen float değerini tekerleğe yansıtır
void UPiSimWheelComponent::SetInspectablePropertyFloat(FName PropertyId, float NewValue)
{
    bManualOverride = true;

    if (PropertyId == TEXT("TargetRPM"))
    {
        TargetRPM = NewValue;
    }
    else if (PropertyId == TEXT("TargetSteerAngleDeg"))
    {
        TargetSteerAngleDeg = FMath::Clamp(NewValue, -MaxSteerAngleDeg, MaxSteerAngleDeg);
    }
    else if (PropertyId == TEXT("RadiusCm"))
    {
        RadiusCm = FMath::Clamp(NewValue, 1.0f, 200.0f);
    }
    else if (PropertyId == TEXT("Friction"))
    {
        FrictionCoefficient = FMath::Clamp(NewValue, 0.01f, 10.0f);
    }
    else if (PropertyId == TEXT("ArduPilotChannel"))
    {
        ArduPilotChannel = FMath::Clamp(FMath::RoundToInt(NewValue), 1, 16);
    }
}

// @state: WIP - UI metin kutusundan gelen string değerini işler
void UPiSimWheelComponent::SetInspectablePropertyString(FName PropertyId, const FString& NewValue)
{
    if (PropertyId == TEXT("WheelMode"))
    {
        if (NewValue == TEXT("SteeredWheel"))
        {
            WheelMode = EPiSimWheelMode::SteeredWheel;
        }
        else if (NewValue == TEXT("FreeWheel"))
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
        TargetSteerAngleDeg = 0.0f;
        bManualOverride = false;
    }
}

// @state: WIP - Tekerlek dönme ve direksiyon fiziğini uygular
void UPiSimWheelComponent::ApplyWheelPhysics(float DeltaTime)
{
    if (!CachedSkeletalMesh || TargetBoneName.IsNone())
    {
        return;
    }

    int32 BoneIndex = CachedSkeletalMesh->GetBoneIndex(TargetBoneName);
    if (BoneIndex == INDEX_NONE)
    {
        return;
    }

    // Direksiyon açısı enterpolasyonu
    CurrentSteerAngleDeg = FMath::FInterpTo(CurrentSteerAngleDeg, TargetSteerAngleDeg, DeltaTime, 12.0f);

    if (CachedSkeletalMesh->IsSimulatingPhysics(TargetBoneName))
    {
        if (WheelMode == EPiSimWheelMode::DriveWheel || WheelMode == EPiSimWheelMode::SteeredWheel)
        {
            // 1 RPM = 6 deg/sec
            float DesiredAngularVelocityDeg = TargetRPM * 6.0f;
            FTransform BoneTransform = CachedSkeletalMesh->GetBoneTransform(BoneIndex);

            // Direksiyon açısı varsa dönme eksenini yaw kadar saptır
            FVector ActiveSpinAxis = LocalSpinAxis;
            if (WheelMode == EPiSimWheelMode::SteeredWheel && FMath::Abs(CurrentSteerAngleDeg) > 0.01f)
            {
                FRotator SteerRot(0.0f, CurrentSteerAngleDeg, 0.0f);
                ActiveSpinAxis = SteerRot.RotateVector(LocalSpinAxis);
            }

            FVector WorldSpinAxis = BoneTransform.TransformVectorNoScale(ActiveSpinAxis).GetSafeNormal();

            FVector CurrentAngVel = CachedSkeletalMesh->GetPhysicsAngularVelocityInDegrees(TargetBoneName);
            FVector NonRollVel = CurrentAngVel - (WorldSpinAxis * (CurrentAngVel | WorldSpinAxis));
            FVector TargetSpinVel = WorldSpinAxis * DesiredAngularVelocityDeg;

            CachedSkeletalMesh->SetPhysicsAngularVelocityInDegrees(NonRollVel + TargetSpinVel, false, TargetBoneName);
            CurrentRPM = TargetRPM;
        }
    }
}
