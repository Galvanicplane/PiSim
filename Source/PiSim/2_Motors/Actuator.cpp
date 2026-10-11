// Actuator.cpp

#include "Actuator.h"
#include "GameFramework/Actor.h"

// @state: WIP - Lineer aktüatör yapıcısı
UPiSimLinearActuatorComponent::UPiSimLinearActuatorComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

// @state: WIP - Her frame pistonun hedef stroka doğru hareketini hesaplar
void UPiSimLinearActuatorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    float Step = StrokeSpeedCmPerSec * DeltaTime;
    float Diff = TargetStrokeCm - CurrentStrokeCm;
    if (FMath::Abs(Diff) <= Step)
    {
        CurrentStrokeCm = TargetStrokeCm;
    }
    else
    {
        CurrentStrokeCm += FMath::Sign(Diff) * Step;
    }
}

// @state: WIP - Otopark UI parametre listesini döner
void UPiSimLinearActuatorComponent::GetInspectableProperties(TArray<FPiSimPropertyDescriptor>& OutProperties)
{
    FPiSimPropertyDescriptor StrokeDesc;
    StrokeDesc.PropertyId = TEXT("TargetStrokeCm");
    StrokeDesc.DisplayName = TEXT("Piston Açılma Mesafesi (cm)");
    StrokeDesc.PropertyType = EPiSimPropertyType::Slider;
    StrokeDesc.MinValue = 0.0f;
    StrokeDesc.MaxValue = MaxStrokeCm;
    StrokeDesc.CurrentFloatValue = TargetStrokeCm;
    StrokeDesc.bContinuousTickUpdate = true;
    OutProperties.Add(StrokeDesc);

    FPiSimPropertyDescriptor SpeedDesc;
    SpeedDesc.PropertyId = TEXT("StrokeSpeedCmPerSec");
    SpeedDesc.DisplayName = TEXT("Strok Hızı (cm/s)");
    SpeedDesc.PropertyType = EPiSimPropertyType::Slider;
    SpeedDesc.MinValue = 1.0f;
    SpeedDesc.MaxValue = 50.0f;
    SpeedDesc.CurrentFloatValue = StrokeSpeedCmPerSec;
    SpeedDesc.bContinuousTickUpdate = false;
    OutProperties.Add(SpeedDesc);
}

// @state: WIP - UI sliderından gelen değeri uygular
void UPiSimLinearActuatorComponent::SetInspectablePropertyFloat(FName PropertyId, float NewValue)
{
    if (PropertyId == TEXT("TargetStrokeCm"))
    {
        SetTargetStroke(NewValue);
    }
    else if (PropertyId == TEXT("StrokeSpeedCmPerSec"))
    {
        StrokeSpeedCmPerSec = FMath::Clamp(NewValue, 0.1f, 100.0f);
    }
}

// @state: WIP - UI metin kutusu işlemi
void UPiSimLinearActuatorComponent::SetInspectablePropertyString(FName PropertyId, const FString& NewValue)
{
}

// @state: WIP - UI buton aksiyonu
void UPiSimLinearActuatorComponent::TriggerInspectableAction(FName ActionId)
{
    if (ActionId == TEXT("Retract"))
    {
        TargetStrokeCm = 0.0f;
    }
}
