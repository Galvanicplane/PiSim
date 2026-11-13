// Wheel.cpp

#include "Wheel.h"
#include "Vehicle.h"
#include "PiSimTopicBridge.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "GameFramework/Actor.h"

// @state: WIP - Modül içi araç kimliğini, Zenoh Bridge ACK + kontrol topic aboneliğini ve UCX sürtünme materyalini başlatır
void UPiSimWheelComponent::InitializeModuleIdentity(APiSimVehicle* InVehicle)
{
    ParentVehicle = InVehicle;
    if (InVehicle)
    {
        ParentVehicleName = InVehicle->VehicleCode;
        VehicleIndex = InVehicle->VehicleId;
        ServoChannel = ArduPilotChannel;

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

            Bridge->OnControlTopicReceived.RemoveAll(this);
            Bridge->OnControlTopicReceived.AddWeakLambda(this, [this](const FString& InVehicleCode, const FString& TopicName, const FString& SourceType, const TSharedPtr<FJsonObject>& PayloadObj)
            {
                HandleControlTopic(InVehicleCode, TopicName, SourceType, PayloadObj);
            });
            bTopicBridgeBound = true;
        }
    }
    ApplyFrictionToCollisionMesh();
}

// @state: WIP - Tekerlek modülünün kendi içinden ArduPilot PWM (öncelikli) veya ROS kontrol topiğini dinlemesi
void UPiSimWheelComponent::HandleControlTopic(const FString& InVehicleCode, const FString& TopicName, const FString& SourceType, const TSharedPtr<FJsonObject>& PayloadObj)
{
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    const int32 MyCh = (ServoChannel > 0) ? ServoChannel : ArduPilotChannel;
    int32 PwmVal = 1500;
    if (UPiSimTopicBridge::ParseControlTopicForChannel(
        Now, ParentVehicleName, VehicleIndex, TargetBoneName, MyCh,
        InVehicleCode, TopicName, SourceType, PayloadObj,
        LastArduPilotPwmTime, LastControlTopicTime, PwmVal))
    {
        bManualOverride = false;
        SetFromPWM(PwmVal, false);
    }
}

// @state: WIP - Sürtünme katsayısını (FrictionCoefficient) çalışma anında UCX çarpışma mesh'ine PhysicalMaterial olarak aktarır
void UPiSimWheelComponent::ApplyFrictionToCollisionMesh()
{
    APiSimVehicle* Vehicle = ParentVehicle ? ParentVehicle : Cast<APiSimVehicle>(GetOwner());
    if (!Vehicle || TargetBoneName.IsNone()) return;

    UPrimitiveComponent* Prim = Vehicle->GetPrimitiveComponentForBone(TargetBoneName);
    if (!Prim) return;

    if (!DynamicWheelPhysMat)
    {
        DynamicWheelPhysMat = NewObject<UPhysicalMaterial>(this);
    }
    if (DynamicWheelPhysMat)
    {
        DynamicWheelPhysMat->Friction = FrictionCoefficient;
        DynamicWheelPhysMat->StaticFriction = FrictionCoefficient * 1.15f;
        DynamicWheelPhysMat->Restitution = 0.05f;
        Prim->SetPhysMaterialOverride(DynamicWheelPhysMat);
        LastAppliedFriction = FrictionCoefficient;
    }
}

// @state: WIP - Tekerlek bileşeni yapıcısı
UPiSimWheelComponent::UPiSimWheelComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

// @state: WIP - Her frame tekerlek topic aboneliğini, watchdog sıfırlamasını ve fizik durumunu günceller
void UPiSimWheelComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bTopicBridgeBound && GetOwner())
    {
        if (APiSimVehicle* Vehicle = ParentVehicle ? ParentVehicle : Cast<APiSimVehicle>(GetOwner()))
        {
            InitializeModuleIdentity(Vehicle);
        }
    }

    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    if (!bManualOverride && LastControlTopicTime > 0.0 && (Now - LastControlTopicTime) > TopicWatchdogTimeoutSec)
    {
        SetFromPWM(1500, false);
        LastControlTopicTime = -100.0;
    }

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

    if (bIsSteering)
    {
        TargetSteerAngleDeg = Norm * MaxSteerAngleDeg;
    }
    else
    {
        // 1000 = -1000 RPM (Geri), 1500 = 0 (Dur), 2000 = +1000 RPM (İleri)
        TargetRPM = Norm * 1200.0f;
    }
}

// @state: WIP - Tek birleşik Tekerlek modülü için Otopark UI parametre listesini döner (Drive/Free ayrımı kaldırıldı)
void UPiSimWheelComponent::GetInspectableProperties(TArray<FPiSimPropertyDescriptor>& OutProperties)
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
    ChDesc.DisplayName = TEXT("ArduPilot Kanalı (1-16)");
    ChDesc.PropertyType = EPiSimPropertyType::Slider;
    ChDesc.MinValue = 1.0f;
    ChDesc.MaxValue = 16.0f;
    ChDesc.CurrentFloatValue = (float)ServoChannel;
    ChDesc.bContinuousTickUpdate = false;
    OutProperties.Add(ChDesc);

    FPiSimPropertyDescriptor RevDesc;
    RevDesc.PropertyId = TEXT("bReverseDirection");
    RevDesc.DisplayName = TEXT("Dönme Yönünü Ters Çevir");
    RevDesc.PropertyType = EPiSimPropertyType::Toggle;
    RevDesc.CurrentFloatValue = bReverseDirection ? 1.0f : 0.0f;
    RevDesc.bContinuousTickUpdate = false;
    OutProperties.Add(RevDesc);

    FPiSimPropertyDescriptor TorqueDesc;
    TorqueDesc.PropertyId = TEXT("DriveTorque");
    TorqueDesc.DisplayName = TEXT("Tahrik Torku (N·m)");
    TorqueDesc.PropertyType = EPiSimPropertyType::Slider;
    TorqueDesc.MinValue = 5.0f;
    TorqueDesc.MaxValue = 1000.0f;
    TorqueDesc.CurrentFloatValue = DriveTorque;
    TorqueDesc.bContinuousTickUpdate = false;
    OutProperties.Add(TorqueDesc);

    FPiSimPropertyDescriptor MaxRpmDesc;
    MaxRpmDesc.PropertyId = TEXT("MaxRPM");
    MaxRpmDesc.DisplayName = TEXT("Azami Devir (Max RPM)");
    MaxRpmDesc.PropertyType = EPiSimPropertyType::Slider;
    MaxRpmDesc.MinValue = 50.0f;
    MaxRpmDesc.MaxValue = 3000.0f;
    MaxRpmDesc.CurrentFloatValue = MaxRPM;
    MaxRpmDesc.bContinuousTickUpdate = false;
    OutProperties.Add(MaxRpmDesc);

    FPiSimPropertyDescriptor RpmDesc;
    RpmDesc.PropertyId = TEXT("TargetRPM");
    RpmDesc.DisplayName = TEXT("Hedef Devir (RPM)");
    RpmDesc.PropertyType = EPiSimPropertyType::Slider;
    RpmDesc.MinValue = -MaxRPM;
    RpmDesc.MaxValue = MaxRPM;
    RpmDesc.CurrentFloatValue = TargetRPM;
    RpmDesc.bContinuousTickUpdate = true;
    OutProperties.Add(RpmDesc);

    FPiSimPropertyDescriptor FrictionDesc;
    FrictionDesc.PropertyId = TEXT("Friction");
    FrictionDesc.DisplayName = TEXT("UCX Sürtünme Katsayısı");
    FrictionDesc.PropertyType = EPiSimPropertyType::Slider;
    FrictionDesc.MinValue = 0.05f;
    FrictionDesc.MaxValue = 3.0f;
    FrictionDesc.CurrentFloatValue = FrictionCoefficient;
    FrictionDesc.bContinuousTickUpdate = false;
    OutProperties.Add(FrictionDesc);
}

// @state: WIP - UI sliderından gelen float değerini tekerleğe yansıtır
void UPiSimWheelComponent::SetInspectablePropertyFloat(FName PropertyId, float NewValue)
{
    bManualOverride = true;

    if (PropertyId == TEXT("ChannelTestSlider"))
    {
        ChannelTestSlider = FMath::Clamp(NewValue, -1.0f, 1.0f);
        SetActuatorNormalized(ChannelTestSlider);
    }
    else if (PropertyId == TEXT("bReverseDirection"))
    {
        bReverseDirection = (NewValue > 0.5f);
    }
    else if (PropertyId == TEXT("DriveTorque"))
    {
        DriveTorque = FMath::Max(1.0f, NewValue);
    }
    else if (PropertyId == TEXT("MaxRPM"))
    {
        MaxRPM = FMath::Clamp(NewValue, 10.0f, 5000.0f);
    }
    else if (PropertyId == TEXT("TargetRPM"))
    {
        SetTargetRPM(NewValue);
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
        ApplyFrictionToCollisionMesh();
    }
    else if (PropertyId == TEXT("ArduPilotChannel"))
    {
        ArduPilotChannel = FMath::Clamp(FMath::RoundToInt(NewValue), 1, 16);
        ServoChannel = ArduPilotChannel;
    }

    UE_LOG(LogTemp, Warning, TEXT("⚡ [PISIM MOTOR DEBUG] Araç: '%s' | Kemik: '%s' | Tekerlek '%s' = %.2f"),
        *ParentVehicleName, *TargetBoneName.ToString(), *PropertyId.ToString(), NewValue);

    APiSimVehicle* OwnerVehicle = ParentVehicle ? ParentVehicle : Cast<APiSimVehicle>(GetOwner());
    if (OwnerVehicle && OwnerVehicle->bShowDebugVisualsAndMessages && GEngine)
    {
        GEngine->AddOnScreenDebugMessage(920, 2.0f, FColor::Cyan,
            FString::Printf(TEXT("⚡ [TEKERLEK DEBUG] %s (%s) = %.2f"), *TargetBoneName.ToString(), *PropertyId.ToString(), NewValue));
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
        TargetSteerAngleDeg = 0.0f;
        bManualOverride = false;
    }
}

// @state: WIP - Tekerlek dönme, tork ve UCX PhysicalMaterial sürtünme fiziğini uygular
void UPiSimWheelComponent::ApplyWheelPhysics(float DeltaTime)
{
    if (!FMath::IsNearlyEqual(LastAppliedFriction, FrictionCoefficient, 0.005f))
    {
        ApplyFrictionToCollisionMesh();
    }

    float DirSign = bReverseDirection ? -1.0f : 1.0f;
    float EffectiveRPM = FMath::Clamp(TargetRPM * DirSign, -MaxRPM, MaxRPM);

    if (TargetSceneComponent)
    {
        float DesiredAngularVelocityDeg = EffectiveRPM * 6.0f;
        float DeltaRotationDeg = DesiredAngularVelocityDeg * DeltaTime;

        APiSimVehicle* Vehicle = ParentVehicle ? ParentVehicle : Cast<APiSimVehicle>(GetOwner());
        bool bPhysicsActive = Vehicle && Vehicle->IsVehiclePhysicsActive();

        // Fizik KAPALIYKEN statik garaj önizlemesi
        if (!bPhysicsActive && FMath::Abs(DeltaRotationDeg) > 0.001f)
        {
            FQuat SpinQuat(LocalSpinAxis.GetSafeNormal(), FMath::DegreesToRadians(DeltaRotationDeg));
            TargetSceneComponent->AddLocalRotation(SpinQuat);
            CurrentRPM = EffectiveRPM;
        }

        // Canlı Fizik Sürüşü: UCX çarpışma gövdesine tork ve RPM uygula
        if (bPhysicsActive && Vehicle)
        {
            if (UPrimitiveComponent* Prim = Vehicle->GetPrimitiveComponentForBone(TargetBoneName))
            {
                if (Prim->IsSimulatingPhysics())
                {
                    FVector LocalAxle = LocalSpinAxis.GetSafeNormal();
                    FVector WorldAxle = Prim->GetComponentTransform().TransformVectorNoScale(LocalAxle).GetSafeNormal();
                    FVector CurrentAngVel = Prim->GetPhysicsAngularVelocityInDegrees();
                    float CurrentRollDeg = (CurrentAngVel | WorldAxle);
                    FVector NonRollVel = CurrentAngVel - (WorldAxle * CurrentRollDeg);
                    Prim->SetPhysicsAngularVelocityInDegrees(NonRollVel + WorldAxle * DesiredAngularVelocityDeg, false);

                    // Tork sınırlı fiziksel sürüş
                    float ErrorDeg = DesiredAngularVelocityDeg - CurrentRollDeg;
                    float TorqueScale = FMath::Clamp(ErrorDeg / 360.0f, -1.0f, 1.0f) * DriveTorque * 10000.0f;
                    Prim->AddTorqueInRadians(WorldAxle * TorqueScale, NAME_None, false);
                    CurrentRPM = EffectiveRPM;
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
    if (BoneIndex == INDEX_NONE)
    {
        return;
    }

    if (CachedSkeletalMesh->IsSimulatingPhysics(TargetBoneName))
    {
        if (WheelMode == EPiSimWheelMode::DriveWheel)
        {
            float DesiredAngularVelocityDeg = TargetRPM * 6.0f;
            FTransform BoneTransform = CachedSkeletalMesh->GetBoneTransform(BoneIndex);

            FVector ActiveSpinAxis = LocalSpinAxis;

            FVector WorldSpinAxis = BoneTransform.TransformVectorNoScale(ActiveSpinAxis).GetSafeNormal();

            FVector CurrentAngVel = CachedSkeletalMesh->GetPhysicsAngularVelocityInDegrees(TargetBoneName);
            FVector NonRollVel = CurrentAngVel - (WorldSpinAxis * (CurrentAngVel | WorldSpinAxis));
            FVector TargetSpinVel = WorldSpinAxis * DesiredAngularVelocityDeg;

            CachedSkeletalMesh->SetPhysicsAngularVelocityInDegrees(NonRollVel + TargetSpinVel, false, TargetBoneName);
            CurrentRPM = TargetRPM;
        }
    }
}

void UPiSimWheelComponent::SetActuatorNormalized(float Value)
{
    ChannelTestSlider = FMath::Clamp(Value, -1.0f, 1.0f);
    SetTargetRPM(ChannelTestSlider * MaxRPM);
}
