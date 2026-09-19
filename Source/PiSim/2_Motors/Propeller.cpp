// Propeller.cpp

#include "Propeller.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

// @state: WIP - Pervane bileşeni yapıcısı
UPiSimPropellerComponent::UPiSimPropellerComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

// @state: WIP - Her frame itki kuvvetini ve devir sayısını hesaplar
void UPiSimPropellerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!CachedSkeletalMesh && GetOwner())
    {
        CachedSkeletalMesh = GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
    }

    ApplyThrustPhysics(DeltaTime);
}

// @state: WIP - Normalize gaz miktarını ayarlar (0..1)
void UPiSimPropellerComponent::SetThrottle(float InThrottleNormalized)
{
    Throttle = FMath::Clamp(InThrottleNormalized, 0.0f, 1.0f);
    CurrentRPM = Throttle * MaxRPM;
}

// @state: WIP - ArduPilot 1000..2000 us PWM sinyalini gaza çevirir
void UPiSimPropellerComponent::SetFromPWM(int32 InPWM)
{
    float ClampedPWM = FMath::Clamp((float)InPWM, 1000.0f, 2000.0f);
    SetThrottle((ClampedPWM - 1000.0f) / 1000.0f);
}

// @state: WIP - Otopark UI parametrelerini doldurur
void UPiSimPropellerComponent::GetInspectableProperties(TArray<FPiSimPropertyDescriptor>& OutProperties)
{
    FPiSimPropertyDescriptor ThrottleDesc;
    ThrottleDesc.PropertyId = TEXT("Throttle");
    ThrottleDesc.DisplayName = TEXT("Gaz / Güç (%)");
    ThrottleDesc.PropertyType = EPiSimPropertyType::Slider;
    ThrottleDesc.MinValue = 0.0f;
    ThrottleDesc.MaxValue = 1.0f;
    ThrottleDesc.CurrentFloatValue = Throttle;
    ThrottleDesc.bContinuousTickUpdate = true;
    OutProperties.Add(ThrottleDesc);

    FPiSimPropertyDescriptor ThrustDesc;
    ThrustDesc.PropertyId = TEXT("MaxThrustNewtons");
    ThrustDesc.DisplayName = TEXT("Maksimum İtki (Newton)");
    ThrustDesc.PropertyType = EPiSimPropertyType::Slider;
    ThrustDesc.MinValue = 1.0f;
    ThrustDesc.MaxValue = 500.0f;
    ThrustDesc.CurrentFloatValue = MaxThrustNewtons;
    ThrustDesc.bContinuousTickUpdate = false;
    OutProperties.Add(ThrustDesc);

    FPiSimPropertyDescriptor ChannelDesc;
    ChannelDesc.PropertyId = TEXT("ArduPilotChannel");
    ChannelDesc.DisplayName = TEXT("ArduPilot Gaz Kanalı (1-16)");
    ChannelDesc.PropertyType = EPiSimPropertyType::Slider;
    ChannelDesc.MinValue = 1.0f;
    ChannelDesc.MaxValue = 16.0f;
    ChannelDesc.CurrentFloatValue = (float)ArduPilotChannel;
    ChannelDesc.bContinuousTickUpdate = false;
    OutProperties.Add(ChannelDesc);
}

// @state: WIP - UI sliderından gelen float değerini pervaneye uygular
void UPiSimPropellerComponent::SetInspectablePropertyFloat(FName PropertyId, float NewValue)
{
    if (PropertyId == TEXT("Throttle"))
    {
        SetThrottle(NewValue);
    }
    else if (PropertyId == TEXT("MaxThrustNewtons"))
    {
        MaxThrustNewtons = FMath::Max(0.1f, NewValue);
    }
    else if (PropertyId == TEXT("ArduPilotChannel"))
    {
        ArduPilotChannel = FMath::Clamp((int32)NewValue, 1, 16);
    }
}

// @state: WIP - UI metin kutusundan gelen string değerini işler
void UPiSimPropellerComponent::SetInspectablePropertyString(FName PropertyId, const FString& NewValue)
{
}

// @state: WIP - Otopark UI aksiyon butonunu işler
void UPiSimPropellerComponent::TriggerInspectableAction(FName ActionId)
{
    if (ActionId == TEXT("KillEngine"))
    {
        SetThrottle(0.0f);
    }
}

// @state: WIP - Pervane itki kuvvetini gövdeye uygular
void UPiSimPropellerComponent::ApplyThrustPhysics(float DeltaTime)
{
    if (!CachedSkeletalMesh || Throttle <= 0.001f)
    {
        return;
    }

    float ForceMagnitude = Throttle * MaxThrustNewtons * 100.0f; // UE ölçeği (cm/s^2)
    FTransform BoneTransform = TargetBoneName.IsNone() ? CachedSkeletalMesh->GetComponentTransform() : CachedSkeletalMesh->GetSocketTransform(TargetBoneName);
    FVector WorldDirection = BoneTransform.TransformVectorNoScale(LocalThrustDirection).GetSafeNormal();

    CachedSkeletalMesh->AddForceAtLocation(WorldDirection * ForceMagnitude, BoneTransform.GetLocation(), TargetBoneName);
}
