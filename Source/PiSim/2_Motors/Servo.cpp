// Servo.cpp

#include "Servo.h"
#include "Vehicle.h"
#include "PiSimTopicBridge.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "ProceduralMeshComponent.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "GameFramework/Actor.h"

// @state: WIP - Modül içi araç kimliğini doldurur ve Zenoh Bridge ACK topic'ini doğrudan dinler
void UPiSimServoComponent::InitializeModuleIdentity(APiSimVehicle* InVehicle)
{
    ParentVehicle = InVehicle;
    if (InVehicle)
    {
        ParentVehicleName = InVehicle->VehicleCode;
        VehicleIndex = InVehicle->VehicleId;
        ServoChannel = ArduPilotChannel;
        BoneId = InVehicle->FindVisualSectionForBone(TargetBoneName);

        if (UPiSimTopicBridge* Bridge = InVehicle->TopicBridge ? InVehicle->TopicBridge : InVehicle->FindComponentByClass<UPiSimTopicBridge>())
        {
            Bridge->OnAckReceived.RemoveAll(this);
            Bridge->OnAckReceived.AddWeakLambda(this, [this](const FString& InVehicleCode, const FString& TopicName, int32 AckCode)
            {
                if (ParentVehicleName.IsEmpty() || InVehicleCode.IsEmpty() || InVehicleCode == ParentVehicleName)
                {
                    HandleAckMessage(TopicName, AckCode);
                }
            });
        }
    }
}

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

        // Canlı Ekrana Debug Bilgisi Bas (Sadece bShowDebugVisualsAndMessages aktifse)
        APiSimVehicle* ParentVehicle = Cast<APiSimVehicle>(GetOwner());
        if (ParentVehicle && ParentVehicle->bShowDebugVisualsAndMessages && GEngine)
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

// @state: WIP - Otopark UI için servo parametrelerini döner (-1..+1 Test Slider & FQuat & Tork)
void UPiSimServoComponent::GetInspectableProperties(TArray<FPiSimPropertyDescriptor>& OutProperties)
{
    FPiSimPropertyDescriptor TestDesc;
    TestDesc.PropertyId = TEXT("ChannelTestSlider");
    TestDesc.DisplayName = TEXT("Canlı Kanal Testi (-1.0 .. +1.0)");
    TestDesc.PropertyType = EPiSimPropertyType::Slider;
    TestDesc.MinValue = -1.0f;
    TestDesc.MaxValue = 1.0f;
    TestDesc.CurrentFloatValue = ChannelTestSlider;
    TestDesc.bContinuousTickUpdate = true;
    OutProperties.Add(TestDesc);

    FPiSimPropertyDescriptor ChDesc;
    ChDesc.PropertyId = TEXT("ArduPilotChannel");
    ChDesc.DisplayName = TEXT("Servo Kanalı (1-16)");
    ChDesc.PropertyType = EPiSimPropertyType::Slider;
    ChDesc.MinValue = 1.0f;
    ChDesc.MaxValue = 16.0f;
    ChDesc.CurrentFloatValue = (float)ServoChannel;
    ChDesc.bContinuousTickUpdate = false;
    OutProperties.Add(ChDesc);

    FPiSimPropertyDescriptor FlipDesc;
    FlipDesc.PropertyId = TEXT("bFlipDirection");
    FlipDesc.DisplayName = TEXT("Yönü Ters Çevir (Flip)");
    FlipDesc.PropertyType = EPiSimPropertyType::Toggle;
    FlipDesc.CurrentFloatValue = bFlipDirection ? 1.0f : 0.0f;
    FlipDesc.bContinuousTickUpdate = false;
    OutProperties.Add(FlipDesc);

    FPiSimPropertyDescriptor TorqueDesc;
    TorqueDesc.PropertyId = TEXT("MaxTorqueNewtons");
    TorqueDesc.DisplayName = TEXT("Azami Servo Torku (N·m)");
    TorqueDesc.PropertyType = EPiSimPropertyType::Slider;
    TorqueDesc.MinValue = 1.0f;
    TorqueDesc.MaxValue = 500.0f;
    TorqueDesc.CurrentFloatValue = MaxTorqueNewtons;
    TorqueDesc.bContinuousTickUpdate = false;
    OutProperties.Add(TorqueDesc);

    FPiSimPropertyDescriptor ResDesc;
    ResDesc.PropertyId = TEXT("JointResistanceForce");
    ResDesc.DisplayName = TEXT("Eklem Direnç Kuvveti (N)");
    ResDesc.PropertyType = EPiSimPropertyType::Slider;
    ResDesc.MinValue = 0.0f;
    ResDesc.MaxValue = 50.0f;
    ResDesc.CurrentFloatValue = JointResistanceForce;
    ResDesc.bContinuousTickUpdate = false;
    OutProperties.Add(ResDesc);

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

    FPiSimPropertyDescriptor FuncDesc;
    FuncDesc.PropertyId = TEXT("ArduPilotFunction");
    FuncDesc.DisplayName = TEXT("Otopilot Fonksiyonu");
    FuncDesc.PropertyType = EPiSimPropertyType::Dropdown;
    FuncDesc.CurrentStringValue = ArduPilotFunction;
    FuncDesc.DropdownOptions = { TEXT("Aileron"), TEXT("Elevator"), TEXT("Rudder"), TEXT("Elevon_Left"), TEXT("Elevon_Right"), TEXT("Flap"), TEXT("Steer") };
    FuncDesc.bContinuousTickUpdate = false;
    OutProperties.Add(FuncDesc);
}

// @state: WIP - UI sliderından gelen değeri servoya uygular
void UPiSimServoComponent::SetInspectablePropertyFloat(FName PropertyId, float NewValue)
{
    if (PropertyId == TEXT("ChannelTestSlider"))
    {
        ChannelTestSlider = FMath::Clamp(NewValue, -1.0f, 1.0f);
        bContinuousSweepTest = false;
        SetActuatorNormalized(ChannelTestSlider);
    }
    else if (PropertyId == TEXT("bFlipDirection"))
    {
        bFlipDirection = (NewValue > 0.5f);
    }
    else if (PropertyId == TEXT("MaxTorqueNewtons"))
    {
        MaxTorqueNewtons = FMath::Max(0.5f, NewValue);
    }
    else if (PropertyId == TEXT("JointResistanceForce"))
    {
        JointResistanceForce = FMath::Max(0.0f, NewValue);
    }
    else if (PropertyId == TEXT("ContinuousSweepTest"))
    {
        bContinuousSweepTest = (NewValue > 0.5f);
        if (!bContinuousSweepTest)
        {
            bManualOverride = true;
        }
    }
    else if (PropertyId == TEXT("TargetAngleDeg"))
    {
        bContinuousSweepTest = false;
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
        ServoChannel = ArduPilotChannel;
    }

    // Min/MaxRotationQuat sınırlarını derece açılarıyla senkron tut
    FVector Axis = LocalRotationAxis.GetSafeNormal();
    if (!Axis.IsNearlyZero())
    {
        MinRotationQuat = FQuat(Axis, FMath::DegreesToRadians(MinAngleDeg));
        MaxRotationQuat = FQuat(Axis, FMath::DegreesToRadians(MaxAngleDeg));
    }

    UE_LOG(LogTemp, Warning, TEXT("⚡ [PISIM MOTOR DEBUG] Araç: '%s' | Kemik: '%s' | Servo '%s' = %.2f"),
        *ParentVehicleName, *TargetBoneName.ToString(), *PropertyId.ToString(), NewValue);

    APiSimVehicle* ParentVehicle = Cast<APiSimVehicle>(GetOwner());
    if (ParentVehicle && ParentVehicle->bShowDebugVisualsAndMessages && GEngine)
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

// @state: WIP - Fizik aktifken SetRelativeRotation KESİNLİKLE kullanılmaz! FQuat Slerp + Fiziksel Tork / Açısal Sürücü uygular
void UPiSimServoComponent::ApplyServoPhysics(float DeltaTime)
{
    float MaxStep = SpeedDegPerSec * DeltaTime;
    float EffectiveTargetDeg = bFlipDirection ? -TargetAngleDeg : TargetAngleDeg;
    float AngleDiff = EffectiveTargetDeg - CurrentAngleDeg;
    if (FMath::Abs(AngleDiff) <= MaxStep)
    {
        CurrentAngleDeg = EffectiveTargetDeg;
    }
    else
    {
        CurrentAngleDeg += FMath::Sign(AngleDiff) * MaxStep;
    }

    // FQuat Slerp hedef hesabı (-1..+1 aralığından [0..1] alfa)
    float Span = FMath::Max(1.0f, MaxAngleDeg - MinAngleDeg);
    float Alpha = FMath::Clamp((CurrentAngleDeg - MinAngleDeg) / Span, 0.0f, 1.0f);
    FQuat TargetLocalQuat = FQuat::Slerp(MinRotationQuat, MaxRotationQuat, Alpha).GetNormalized();

    APiSimVehicle* Vehicle = ParentVehicle ? ParentVehicle : Cast<APiSimVehicle>(GetOwner());
    bool bPhysicsActive = Vehicle && Vehicle->IsVehiclePhysicsActive();

    if (bPhysicsActive && Vehicle)
    {
        // 1) Açısal fiziksel eklem sürücüsü (Constraint Angular Motor Drive)
        UPhysicsConstraintComponent* Constraint = Vehicle->FindConstraintForBone(TargetBoneName);
        if (!Constraint)
        {
            int32 TargetSection = Vehicle->FindVisualSectionForBone(TargetBoneName);
            Constraint = Vehicle->FindConstraintForSection(TargetSection);
        }
        if (Constraint)
        {
            Constraint->SetAngularDriveMode(EAngularDriveMode::SLERP);
            Constraint->SetOrientationDriveSLERP(true);
            Constraint->SetAngularOrientationTarget(TargetLocalQuat.Rotator());
            float EffectiveForceLimit = FMath::Max(0.0f, (MaxTorqueNewtons - JointResistanceForce) * 10000.0f);
            Constraint->SetAngularDriveParams(500.0f, 50.0f, EffectiveForceLimit);
        }

        // 2) UCX fizik gövdesine doğrudan tork uygulaması (statik/kinetik eklem direncini yenerek)
        if (UPrimitiveComponent* Prim = Vehicle->GetPrimitiveComponentForBone(TargetBoneName))
        {
            if (Prim->IsSimulatingPhysics())
            {
                FQuat CurrentWorldQuat = Prim->GetComponentQuat();
                FQuat DesiredWorldQuat = TargetSceneComponent ? (TargetSceneComponent->GetComponentQuat() * TargetLocalQuat) : TargetLocalQuat;
                FQuat ErrorQuat = DesiredWorldQuat * CurrentWorldQuat.Inverse();
                ErrorQuat.EnforceShortestArcWith(FQuat::Identity);
                FVector ErrAxis;
                float ErrAngleRad = 0.0f;
                ErrorQuat.ToAxisAndAngle(ErrAxis, ErrAngleRad);

                float NetTorque = FMath::Clamp(ErrAngleRad * MaxTorqueNewtons * 10.0f, -MaxTorqueNewtons, MaxTorqueNewtons);
                if (FMath::Abs(NetTorque) > JointResistanceForce)
                {
                    float DrivenTorque = (NetTorque - FMath::Sign(NetTorque) * JointResistanceForce) * 10000.0f;
                    Prim->AddTorqueInRadians(ErrAxis * DrivenTorque, NAME_None, false);
                }
            }
        }
        return;
    }

    // Statik Garaj Modu (Fizik KAPALI iken görsel önizleme)
    if (TargetSceneComponent && !bPhysicsActive)
    {
        if (!bHasRestRotation)
        {
            RestRotation = TargetSceneComponent->GetRelativeRotation();
            bHasRestRotation = true;
        }
        FQuat RestQ = RestRotation.Quaternion();
        TargetSceneComponent->SetRelativeRotation((RestQ * TargetLocalQuat).Rotator());
    }
}

void UPiSimServoComponent::SetActuatorNormalized(float Value)
{
    float EffectiveVal = FMath::Clamp(Value, -1.0f, 1.0f);
    ChannelTestSlider = EffectiveVal;
    if (bFlipDirection) EffectiveVal = -EffectiveVal;
    bManualOverride = (FMath::Abs(EffectiveVal) > 0.001f);
    float TargetDeg = (EffectiveVal >= 0.0f) ? (EffectiveVal * MaxAngleDeg) : (-EffectiveVal * MinAngleDeg);
    SetTargetAngle(TargetDeg);
}
