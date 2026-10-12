// Servo.cpp

#include "Servo.h"
#include "Vehicle.h"
#include "Components/SkeletalMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
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

    if (!TargetSceneComponent && GetOwner() && !TargetBoneName.IsNone())
    {
        APiSimVehicle* Vehicle = Cast<APiSimVehicle>(GetOwner());
        if (Vehicle)
        {
            TargetSceneComponent = Vehicle->GetSceneComponentForBone(TargetBoneName);
        }
    }

    // 1) SÜREKLİ SALINIM TESTİ (Kullanıcı Talebi 1: Servoya özel, kemiği range içinde durmaksızın döndüren test)
    if (bContinuousSweepTest)
    {
        SweepTestTimer += DeltaTime * FMath::Max(0.1f, SweepCycleSpeed);

        // Sinüs dalgası: [-1.0 .. +1.0] -> [0.0 .. 1.0]
        float SinVal = FMath::Sin(SweepTestTimer * 2.5f);
        float Alpha = (SinVal + 1.0f) * 0.5f;

        // MinAngleDeg ile MaxAngleDeg arasında kesintisiz salın
        TargetAngleDeg = FMath::Lerp(MinAngleDeg, MaxAngleDeg, Alpha);

        // Canlı Ekrana Debug Bilgisi Bas
        if (GEngine)
        {
            int32 MsgKey = 910 + (GetUniqueID() % 50);
            FString BoneStr = TargetBoneName.ToString();
            FString CompStr = TargetSceneComponent ? TargetSceneComponent->GetName() : TEXT("❌ Kemik Yok");
            GEngine->AddOnScreenDebugMessage(MsgKey, 0.1f, FColor::Cyan,
                FString::Printf(TEXT("⚡ [SERVO TEST] Kemik: '%s' | Açı: %+.1f° [%.0f° .. %.0f°] | Dönen Kemik: %s"),
                    *BoneStr, CurrentAngleDeg, MinAngleDeg, MaxAngleDeg, *CompStr));
        }

        // Konsol Logu (Saniyede 2 kez)
        float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : SweepTestTimer;
        if (CurrentTime - LastSweepDebugLogTime >= 0.5f)
        {
            LastSweepDebugLogTime = CurrentTime;
            UE_LOG(LogTemp, Warning, TEXT("⚡ [SERVO TEST] Kemik: '%s' | Hedef: %+.1f° | Mevcut: %+.1f° (Aralık: [%.1f°, %.1f°]) | Dönen Kemik: %s"),
                *TargetBoneName.ToString(), TargetAngleDeg, CurrentAngleDeg, MinAngleDeg, MaxAngleDeg,
                TargetSceneComponent ? *TargetSceneComponent->GetName() : TEXT("Bağlanmadı"));
        }
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
    if (bManualOverride || bContinuousSweepTest)
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
    if (bManualOverride || bContinuousSweepTest)
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
    FPiSimPropertyDescriptor SweepDesc;
    SweepDesc.PropertyId = TEXT("ContinuousSweepTest");
    SweepDesc.DisplayName = TEXT("Sürekli Salınım Testi (0=Kapalı, 1=Açık)");
    SweepDesc.PropertyType = EPiSimPropertyType::Slider;
    SweepDesc.MinValue = 0.0f;
    SweepDesc.MaxValue = 1.0f;
    SweepDesc.CurrentFloatValue = bContinuousSweepTest ? 1.0f : 0.0f;
    SweepDesc.bContinuousTickUpdate = false;
    OutProperties.Add(SweepDesc);

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
    if (PropertyId == TEXT("ContinuousSweepTest"))
    {
        bContinuousSweepTest = (NewValue > 0.5f);
        if (!bContinuousSweepTest)
        {
            bManualOverride = true;
        }
    }
    else if (PropertyId == TEXT("TargetAngleDeg"))
    {
        bContinuousSweepTest = false; // Slider elle tutulunca otomatik testi durdur
        bManualOverride = true;
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

    // DEBUG BİLGİSİ: UI'dan gelen motor verisini ekrana ve konsola bas
    UE_LOG(LogTemp, Warning, TEXT("⚡ [PISIM MOTOR DEBUG] Kemik: '%s' | Servo Parametresi: '%s' = %.2f | Bağlı Bileşen: %s"),
        *TargetBoneName.ToString(), *PropertyId.ToString(), NewValue,
        TargetSceneComponent ? *TargetSceneComponent->GetName() : TEXT("Bağlanmadı (Aranıyor...)"));

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(910, 2.0f, FColor::Yellow,
            FString::Printf(TEXT("⚡ [SERVO DEBUG] %s (%s) = %.2f°"), *TargetBoneName.ToString(), *PropertyId.ToString(), NewValue));
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
        bContinuousSweepTest = false;
    }
}

// @state: WIP - Kemik oryantasyonunu servo açısına göre günceller
void UPiSimServoComponent::ApplyServoPhysics(float DeltaTime)
{
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

    // 🛑 KESİNLİKLE VE SADECE KEMİK SAHNE BİLEŞENİ (TargetSceneComponent) DÖNDÜRÜLÜR!
    // Görsel meshlere ASLA el sürülmez! Çünkü meshler bu kemiğe bağlı çocuklardır ve onunla beraber döner!
    if (TargetSceneComponent)
    {
        if (!bHasRestRotation)
        {
            RestRotation = TargetSceneComponent->GetRelativeRotation();
            bHasRestRotation = true;
        }

        FRotator NewRot = RestRotation;
        NewRot.Yaw = FRotator::NormalizeAxis(RestRotation.Yaw + CurrentAngleDeg);
        TargetSceneComponent->SetRelativeRotation(NewRot);

        if (APiSimVehicle* Vehicle = Cast<APiSimVehicle>(GetOwner()))
        {
            if (Vehicle->IsVehiclePhysicsActive())
            {
                UPhysicsConstraintComponent* Constraint = Vehicle->FindConstraintForBone(TargetBoneName);
                if (!Constraint)
                {
                    int32 TargetSection = Vehicle->FindVisualSectionForBone(TargetBoneName);
                    Constraint = Vehicle->FindConstraintForSection(TargetSection);
                }
                if (Constraint)
                {
                    FTransform SteerPose(NewRot, Constraint->GetRelativeLocation());
                    Constraint->SetConstraintReferenceFrame(EConstraintFrame::Frame1, SteerPose);
                }
            }
        }
        return;
    }

    // 2) SkeletalMesh Varsa
    if (!CachedSkeletalMesh || TargetBoneName.IsNone())
    {
        return;
    }

    int32 BoneIndex = CachedSkeletalMesh->GetBoneIndex(TargetBoneName);
    if (BoneIndex != INDEX_NONE)
    {
        FQuat DeltaQuat(LocalRotationAxis.GetSafeNormal(), FMath::DegreesToRadians(CurrentAngleDeg));
        if (CachedSkeletalMesh->IsSimulatingPhysics(TargetBoneName))
        {
            // Bone physical angular rotation
        }
    }
}
