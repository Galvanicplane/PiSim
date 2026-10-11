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

// @state: WIP - Her frame itki kuvvetini hesaplar ve fizik gövdesine uygular
void UPiSimPropellerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!CachedSkeletalMesh && GetOwner())
    {
        CachedSkeletalMesh = GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
    }

    ApplyThrustPhysics(DeltaTime);
}

// @state: WIP - Gaz kolu değerini (0.0 - 1.0) günceller
void UPiSimPropellerComponent::SetThrottle(float InThrottleNormalized)
{
    Throttle = FMath::Clamp(InThrottleNormalized, 0.0f, 1.0f);
    CurrentRPM = Throttle * MaxRPM;
    CurrentThrustNewtons = (Throttle * Throttle) * MaxThrustNewtons;
}

// @state: WIP - ArduPilot 1000..2000 us PWM sinyalini gaza çevirir
void UPiSimPropellerComponent::SetFromPWM(int32 InPWM)
{
    if (bManualOverride)
    {
        return;
    }

    float Norm = FMath::Clamp((InPWM - 1000) / 1000.0f, 0.0f, 1.0f);
    SetThrottle(Norm);
}

// @state: WIP - Otopark UI için pervane parametrelerini döner
void UPiSimPropellerComponent::GetInspectableProperties(TArray<FPiSimPropertyDescriptor>& OutProperties)
{
    FPiSimPropertyDescriptor ThrDesc;
    ThrDesc.PropertyId = TEXT("Throttle");
    ThrDesc.DisplayName = TEXT("Gaz / İtki (0 - 100%)");
    ThrDesc.PropertyType = EPiSimPropertyType::Slider;
    ThrDesc.MinValue = 0.0f;
    ThrDesc.MaxValue = 1.0f;
    ThrDesc.CurrentFloatValue = Throttle;
    ThrDesc.bContinuousTickUpdate = true;
    OutProperties.Add(ThrDesc);

    FPiSimPropertyDescriptor ThrustDesc;
    ThrustDesc.PropertyId = TEXT("MaxThrustNewtons");
    ThrustDesc.DisplayName = TEXT("Maksimum İtki (Newton)");
    ThrustDesc.PropertyType = EPiSimPropertyType::Slider;
    ThrustDesc.MinValue = 5.0f;
    ThrustDesc.MaxValue = 500.0f;
    ThrustDesc.CurrentFloatValue = MaxThrustNewtons;
    ThrustDesc.bContinuousTickUpdate = false;
    OutProperties.Add(ThrustDesc);

    FPiSimPropertyDescriptor RevDesc;
    RevDesc.PropertyId = TEXT("bReverseThrust");
    RevDesc.DisplayName = TEXT("İtki Yönünü Ters Çevir");
    RevDesc.PropertyType = EPiSimPropertyType::Toggle;
    RevDesc.CurrentFloatValue = bReverseThrust ? 1.0f : 0.0f;
    RevDesc.bContinuousTickUpdate = false;
    OutProperties.Add(RevDesc);

    FPiSimPropertyDescriptor ChDesc;
    ChDesc.PropertyId = TEXT("ArduPilotChannel");
    ChDesc.DisplayName = TEXT("ArduPilot Kanalı");
    ChDesc.PropertyType = EPiSimPropertyType::Slider;
    ChDesc.MinValue = 1.0f;
    ChDesc.MaxValue = 16.0f;
    ChDesc.CurrentFloatValue = (float)ArduPilotChannel;
    ChDesc.bContinuousTickUpdate = false;
    OutProperties.Add(ChDesc);
}

// @state: WIP - UI sliderından gelen float değerini pervaneye uygular
void UPiSimPropellerComponent::SetInspectablePropertyFloat(FName PropertyId, float NewValue)
{
    bManualOverride = true;

    if (PropertyId == TEXT("Throttle"))
    {
        SetThrottle(NewValue);
    }
    else if (PropertyId == TEXT("MaxThrustNewtons"))
    {
        MaxThrustNewtons = FMath::Max(1.0f, NewValue);
    }
    else if (PropertyId == TEXT("bReverseThrust"))
    {
        bReverseThrust = (NewValue > 0.5f);
    }
    else if (PropertyId == TEXT("ArduPilotChannel"))
    {
        ArduPilotChannel = FMath::Clamp(FMath::RoundToInt(NewValue), 1, 16);
    }
}

// @state: WIP - UI metin kutusundan gelen string değerini işler
void UPiSimPropellerComponent::SetInspectablePropertyString(FName PropertyId, const FString& NewValue)
{
}

// @state: WIP - UI aksiyon butonunu işler
void UPiSimPropellerComponent::TriggerInspectableAction(FName ActionId)
{
    if (ActionId == TEXT("CutThrottle"))
    {
        SetThrottle(0.0f);
        bManualOverride = false;
    }
}

// @state: WIP - İtki kuvvetini (Newton * 100 = dyn/cm) iskelet kemiği konumunda uygular
void UPiSimPropellerComponent::ApplyThrustPhysics(float DeltaTime)
{
    if (!CachedSkeletalMesh || Throttle <= 0.001f)
    {
        return;
    }

    FTransform BoneTransform = TargetBoneName.IsNone() ? CachedSkeletalMesh->GetComponentTransform() : CachedSkeletalMesh->GetSocketTransform(TargetBoneName);

    FVector LocalDir = LocalThrustDirection.GetSafeNormal();
    if (bReverseThrust)
    {
        LocalDir = -LocalDir;
    }

    FVector WorldThrustDir = BoneTransform.TransformVectorNoScale(LocalDir).GetSafeNormal();
    CurrentThrustNewtons = (Throttle * Throttle) * MaxThrustNewtons;

    // UE kuvvet birimi: 1 Newton = 100 N*cm/s^2 (veya AddForce için Newton * 100)
    FVector ForceVector = WorldThrustDir * CurrentThrustNewtons * 100.0f;

    CachedSkeletalMesh->AddForceAtLocation(ForceVector, BoneTransform.GetLocation(), TargetBoneName);
}
