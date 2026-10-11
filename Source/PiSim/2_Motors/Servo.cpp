// Servo.cpp

#include "Servo.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

// @state: WIP - Servo eyleyici yapıcısı
UPiSimServoComponent::UPiSimServoComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

// @state: WIP - Her frame servonun hedef açıya doğru hareketini uygular
void UPiSimServoComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!CachedSkeletalMesh && GetOwner())
    {
        CachedSkeletalMesh = GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
    }

    ApplyServoPhysics(DeltaTime);
}

// @state: WIP - Hedef açıyı limitler dahilinde ayarlar
void UPiSimServoComponent::SetTargetAngle(float InAngleDeg)
{
    TargetAngleDeg = FMath::Clamp(InAngleDeg, MinAngleDeg, MaxAngleDeg);
}

// @state: WIP - ArduPilot 1000..2000 us PWM sinyalini açıya çevirir
void UPiSimServoComponent::SetFromPWM(int32 InPWM)
{
    if (bManualOverride)
    {
        return;
    }

    float Norm = FMath::Clamp((InPWM - 1500) / 500.0f, -1.0f, 1.0f); // -1.0 .. +1.0
    if (Norm >= 0.0f)
    {
        TargetAngleDeg = Norm * MaxAngleDeg;
    }
    else
    {
        TargetAngleDeg = -Norm * MinAngleDeg;
    }
}

// @state: WIP - Pitch ve Roll bileşenlerini Elevon mimarisinde karıştırır
void UPiSimServoComponent::SetPitchRollMixing(float PitchNorm, float RollNorm)
{
    if (bManualOverride)
    {
        return;
    }

    FString FuncLower = ArduPilotFunction.ToLower();
    if (FuncLower.Contains(TEXT("elevon_l")) || FuncLower.Contains(TEXT("left")))
    {
        // Sol Elevon = Pitch + Roll
        float Angle = (PitchNorm + RollNorm) * MaxAngleDeg;
        SetTargetAngle(Angle);
    }
    else if (FuncLower.Contains(TEXT("elevon_r")) || FuncLower.Contains(TEXT("right")))
    {
        // Sağ Elevon = Pitch - Roll
        float Angle = (PitchNorm - RollNorm) * MaxAngleDeg;
        SetTargetAngle(Angle);
    }
    else if (FuncLower.Contains(TEXT("elevator")))
    {
        SetTargetAngle(PitchNorm * MaxAngleDeg);
    }
    else if (FuncLower.Contains(TEXT("aileron")))
    {
        SetTargetAngle(RollNorm * MaxAngleDeg);
    }
}

// @state: WIP - Otopark UI için servo parametrelerini döner
void UPiSimServoComponent::GetInspectableProperties(TArray<FPiSimPropertyDescriptor>& OutProperties)
{
    FPiSimPropertyDescriptor AngleDesc;
    AngleDesc.PropertyId = TEXT("TargetAngleDeg");
    AngleDesc.DisplayName = TEXT("Hedef Açı (°)");
    AngleDesc.PropertyType = EPiSimPropertyType::Slider;
    AngleDesc.MinValue = MinAngleDeg;
    AngleDesc.MaxValue = MaxAngleDeg;
    AngleDesc.CurrentFloatValue = TargetAngleDeg;
    AngleDesc.bContinuousTickUpdate = true;
    OutProperties.Add(AngleDesc);

    FPiSimPropertyDescriptor SpeedDesc;
    SpeedDesc.PropertyId = TEXT("SpeedDegPerSec");
    SpeedDesc.DisplayName = TEXT("Servo Hızı (°/s)");
    SpeedDesc.PropertyType = EPiSimPropertyType::Slider;
    SpeedDesc.MinValue = 30.0f;
    SpeedDesc.MaxValue = 720.0f;
    SpeedDesc.CurrentFloatValue = SpeedDegPerSec;
    SpeedDesc.bContinuousTickUpdate = false;
    OutProperties.Add(SpeedDesc);

    FPiSimPropertyDescriptor FuncDesc;
    FuncDesc.PropertyId = TEXT("ArduPilotFunction");
    FuncDesc.DisplayName = TEXT("Otopilot Fonksiyonu");
    FuncDesc.PropertyType = EPiSimPropertyType::Dropdown;
    FuncDesc.CurrentStringValue = ArduPilotFunction;
    FuncDesc.DropdownOptions = { TEXT("Aileron"), TEXT("Elevator"), TEXT("Rudder"), TEXT("Elevon_Left"), TEXT("Elevon_Right"), TEXT("Flap"), TEXT("Steer") };
    FuncDesc.bContinuousTickUpdate = false;
    OutProperties.Add(FuncDesc);

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

// @state: WIP - UI sliderından gelen değeri servoya uygular
void UPiSimServoComponent::SetInspectablePropertyFloat(FName PropertyId, float NewValue)
{
    bManualOverride = true;

    if (PropertyId == TEXT("TargetAngleDeg"))
    {
        SetTargetAngle(NewValue);
    }
    else if (PropertyId == TEXT("SpeedDegPerSec"))
    {
        SpeedDegPerSec = FMath::Clamp(NewValue, 10.0f, 1500.0f);
    }
    else if (PropertyId == TEXT("ArduPilotChannel"))
    {
        ArduPilotChannel = FMath::Clamp(FMath::RoundToInt(NewValue), 1, 16);
    }
}

// @state: WIP - UI fonksiyon seçimini uygular
void UPiSimServoComponent::SetInspectablePropertyString(FName PropertyId, const FString& NewValue)
{
    if (PropertyId == TEXT("ArduPilotFunction"))
    {
        ArduPilotFunction = NewValue;
    }
}

// @state: WIP - UI buton aksiyonu
void UPiSimServoComponent::TriggerInspectableAction(FName ActionId)
{
    if (ActionId == TEXT("ZeroServo"))
    {
        TargetAngleDeg = 0.0f;
        bManualOverride = false;
    }
}

// @state: WIP - Kemik oryantasyonunu servo açısına göre günceller
void UPiSimServoComponent::ApplyServoPhysics(float DeltaTime)
{
    if (!CachedSkeletalMesh || TargetBoneName.IsNone())
    {
        return;
    }

    // Açı limitli enterpolasyon
    float MaxStep = SpeedDegPerSec * DeltaTime;
    float AngleDiff = TargetAngleDeg - CurrentAngleDeg;
    if (FMath::Abs(AngleDiff) <= MaxStep)
    {
        CurrentAngleDeg = TargetAngleDeg;
    }
    else
    {
        CurrentAngleDeg += FMath::Sign(AngleDiff) * MaxStep;
    }

    // İskeletteki kemiği döndür
    int32 BoneIndex = CachedSkeletalMesh->GetBoneIndex(TargetBoneName);
    if (BoneIndex != INDEX_NONE)
    {
        FQuat DeltaQuat(LocalRotationAxis.GetSafeNormal(), FMath::DegreesToRadians(CurrentAngleDeg));
        // Fizik simüle ediliyorsa açısal hedef veya constraint ile kilitler
        if (CachedSkeletalMesh->IsSimulatingPhysics(TargetBoneName))
        {
            // Bone physical angular rotation
        }
    }
}
