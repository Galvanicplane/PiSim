// Track.cpp

#include "Track.h"
#include "GameFramework/Actor.h"

// @state: WIP - Palet bileşeni yapıcısı
UPiSimTrackComponent::UPiSimTrackComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

// @state: WIP - Her frame palet sürtünme ve hızını günceller
void UPiSimTrackComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    CurrentSpeedMps = FMath::FInterpTo(CurrentSpeedMps, TargetSpeedMps, DeltaTime, 8.0f);
}

// @state: WIP - Otopark UI parametre listesini döner
void UPiSimTrackComponent::GetInspectableProperties(TArray<FPiSimPropertyDescriptor>& OutProperties)
{
    FPiSimPropertyDescriptor SpeedDesc;
    SpeedDesc.PropertyId = TEXT("TargetSpeedMps");
    SpeedDesc.DisplayName = TEXT("Palet Çizgisel Hızı (m/s)");
    SpeedDesc.PropertyType = EPiSimPropertyType::Slider;
    SpeedDesc.MinValue = -15.0f;
    SpeedDesc.MaxValue = 15.0f;
    SpeedDesc.CurrentFloatValue = TargetSpeedMps;
    SpeedDesc.bContinuousTickUpdate = true;
    OutProperties.Add(SpeedDesc);

    FPiSimPropertyDescriptor FricDesc;
    FricDesc.PropertyId = TEXT("FrictionCoefficient");
    FricDesc.DisplayName = TEXT("Zemin Sürtünmesi");
    FricDesc.PropertyType = EPiSimPropertyType::Slider;
    FricDesc.MinValue = 0.2f;
    FricDesc.MaxValue = 3.0f;
    FricDesc.CurrentFloatValue = FrictionCoefficient;
    FricDesc.bContinuousTickUpdate = false;
    OutProperties.Add(FricDesc);
}

// @state: WIP - UI sliderından gelen değeri uygular
void UPiSimTrackComponent::SetInspectablePropertyFloat(FName PropertyId, float NewValue)
{
    if (PropertyId == TEXT("TargetSpeedMps"))
    {
        TargetSpeedMps = NewValue;
    }
    else if (PropertyId == TEXT("FrictionCoefficient"))
    {
        FrictionCoefficient = FMath::Clamp(NewValue, 0.05f, 5.0f);
    }
}

// @state: WIP - UI metin kutusu işlemi
void UPiSimTrackComponent::SetInspectablePropertyString(FName PropertyId, const FString& NewValue)
{
}

// @state: WIP - UI buton aksiyonu
void UPiSimTrackComponent::TriggerInspectableAction(FName ActionId)
{
    if (ActionId == TEXT("StopTrack"))
    {
        TargetSpeedMps = 0.0f;
    }
}
