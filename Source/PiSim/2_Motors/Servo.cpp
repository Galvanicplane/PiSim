// Servo.cpp

#include "Servo.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

// @state: WIP - Servo bileşeni yapıcısı
UPiSimServoComponent::UPiSimServoComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

// @state: WIP - Her frame servonun hedef açıya doğru interpolasyonunu hesaplar
void UPiSimServoComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!CachedSkeletalMesh && GetOwner())
    {
        CachedSkeletalMesh = GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
    }

    ApplyServoPhysics(DeltaTime);
}

// @state: WIP - Servonun hedef açısını limitler dahilinde belirler
void UPiSimServoComponent::SetTargetAngle(float InAngleDeg)
{
    TargetAngleDeg = FMath::Clamp(InAngleDeg, MinAngleDeg, MaxAngleDeg);
}

// @state: WIP - ArduPilot 1000..2000 us PWM sinyalini servo açısına çevirir
void UPiSimServoComponent::SetFromPWM(int32 InPWM)
{
    float ClampedPWM = FMath::Clamp((float)InPWM, 1000.0f, 2000.0f);
    float Normalized = (ClampedPWM - 1500.0f) / 500.0f; // -1.0 .. +1.0
    if (Normalized >= 0.0f)
    {
        SetTargetAngle(Normalized * MaxAngleDeg);
    }
    else
    {
        SetTargetAngle(FMath::Abs(Normalized) * MinAngleDeg);
    }
}

// @state: WIP - Otopark UI için servo parametrelerini döner
void UPiSimServoComponent::GetInspectableProperties(TArray<FPiSimPropertyDescriptor>& OutProperties)
{
    FPiSimPropertyDescriptor AngleDesc;
    AngleDesc.PropertyId = TEXT("TargetAngleDeg");
    AngleDesc.DisplayName = TEXT("Hedef Açı (Derece)");
    AngleDesc.PropertyType = EPiSimPropertyType::Slider;
    AngleDesc.MinValue = MinAngleDeg;
    AngleDesc.MaxValue = MaxAngleDeg;
    AngleDesc.CurrentFloatValue = TargetAngleDeg;
    AngleDesc.bContinuousTickUpdate = true;
    OutProperties.Add(AngleDesc);

    FPiSimPropertyDescriptor SpeedDesc;
    SpeedDesc.PropertyId = TEXT("SpeedDegPerSec");
    SpeedDesc.DisplayName = TEXT("Servo Hızı (Derece/sn)");
    SpeedDesc.PropertyType = EPiSimPropertyType::Slider;
    SpeedDesc.MinValue = 30.0f;
    SpeedDesc.MaxValue = 720.0f;
    SpeedDesc.CurrentFloatValue = SpeedDegPerSec;
    SpeedDesc.bContinuousTickUpdate = false;
    OutProperties.Add(SpeedDesc);

    FPiSimPropertyDescriptor ChannelDesc;
    ChannelDesc.PropertyId = TEXT("ArduPilotChannel");
    ChannelDesc.DisplayName = TEXT("ArduPilot PWM Kanalı (1-16)");
    ChannelDesc.PropertyType = EPiSimPropertyType::Slider;
    ChannelDesc.MinValue = 1.0f;
    ChannelDesc.MaxValue = 16.0f;
    ChannelDesc.CurrentFloatValue = (float)ArduPilotChannel;
    ChannelDesc.bContinuousTickUpdate = false;
    OutProperties.Add(ChannelDesc);

    FPiSimPropertyDescriptor FuncDesc;
    FuncDesc.PropertyId = TEXT("ArduPilotFunction");
    FuncDesc.DisplayName = TEXT("ArduPilot Rolü");
    FuncDesc.PropertyType = EPiSimPropertyType::Dropdown;
    FuncDesc.CurrentStringValue = ArduPilotFunction;
    FuncDesc.DropdownOptions = { TEXT("Aileron"), TEXT("Elevator"), TEXT("Rudder"), TEXT("ElevonLeft"), TEXT("ElevonRight"), TEXT("Flap"), TEXT("Steer") };
    FuncDesc.bContinuousTickUpdate = false;
    OutProperties.Add(FuncDesc);
}

// @state: WIP - UI sliderından gelen float değerini servoya uygular
void UPiSimServoComponent::SetInspectablePropertyFloat(FName PropertyId, float NewValue)
{
    if (PropertyId == TEXT("TargetAngleDeg"))
    {
        SetTargetAngle(NewValue);
    }
    else if (PropertyId == TEXT("SpeedDegPerSec"))
    {
        SpeedDegPerSec = FMath::Max(1.0f, NewValue);
    }
    else if (PropertyId == TEXT("ArduPilotChannel"))
    {
        ArduPilotChannel = FMath::Clamp((int32)NewValue, 1, 16);
    }
}

// @state: WIP - UI açılır listesinden gelen rol seçimini servoya uygular
void UPiSimServoComponent::SetInspectablePropertyString(FName PropertyId, const FString& NewValue)
{
    if (PropertyId == TEXT("ArduPilotFunction"))
    {
        ArduPilotFunction = NewValue;
    }
}

// @state: WIP - Otopark UI tetikleyici butonunu işler
void UPiSimServoComponent::TriggerInspectableAction(FName ActionId)
{
    if (ActionId == TEXT("CenterServo"))
    {
        SetTargetAngle(0.0f);
    }
}

// @state: WIP - Kemik veya bağlı bileşenin açısını yumuşakça hedefe doğru döndürür
void UPiSimServoComponent::ApplyServoPhysics(float DeltaTime)
{
    CurrentAngleDeg = FMath::FInterpConstantTo(CurrentAngleDeg, TargetAngleDeg, DeltaTime, SpeedDegPerSec);

    if (LinkedComponent)
    {
        FRotator TargetRot = FRotator(LocalRotationAxis.Y * CurrentAngleDeg, LocalRotationAxis.Z * CurrentAngleDeg, LocalRotationAxis.X * CurrentAngleDeg);
        LinkedComponent->SetRelativeRotation(TargetRot);
    }
}
