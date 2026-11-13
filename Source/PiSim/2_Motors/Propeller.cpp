// Propeller.cpp

#include "Propeller.h"
#include "Vehicle.h"
#include "Components/SkeletalMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "GameFramework/Actor.h"

// @state: WIP - Modül içi araç kimliğini doldurur
void UPiSimPropellerComponent::InitializeModuleIdentity(APiSimVehicle* InVehicle)
{
    ParentVehicle = InVehicle;
    if (InVehicle)
    {
        ParentVehicleName = InVehicle->VehicleCode;
        VehicleIndex = InVehicle->VehicleId;
        ServoChannel = ArduPilotChannel;
    }
}

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

    if (!TargetSceneComponent && GetOwner() && !TargetBoneName.IsNone())
    {
        APiSimVehicle* Vehicle = Cast<APiSimVehicle>(GetOwner());
        if (Vehicle)
        {
            TargetSceneComponent = Vehicle->GetSceneComponentForBone(TargetBoneName);
        }
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
        ChannelTestSlider = NewValue;
        SetThrottle(NewValue);
    }
    else if (PropertyId == TEXT("MaxThrustNewtons"))
    {
        MaxThrustNewtons = FMath::Max(1.0f, NewValue);
    }
    else if (PropertyId == TEXT("VisualSpinSpeedRPM"))
    {
        VisualSpinSpeedRPM = FMath::Clamp(NewValue, 0.0f, 30000.0f);
        MaxRPM = VisualSpinSpeedRPM;
    }
    else if (PropertyId == TEXT("bReverseThrust"))
    {
        bReverseThrust = (NewValue > 0.5f);
    }
    else if (PropertyId == TEXT("ArduPilotChannel"))
    {
        ArduPilotChannel = FMath::Clamp(FMath::RoundToInt(NewValue), 1, 16);
        ServoChannel = ArduPilotChannel;
    }

    // DEBUG BİLGİSİ: UI'dan gelen motor verisini konsola ve ekrana bas
    UE_LOG(LogTemp, Warning, TEXT("⚡ [PISIM MOTOR DEBUG] Kemik: '%s' | Pervane Parametresi: '%s' = %.2f | Bağlı Bileşen: %s"),
        *TargetBoneName.ToString(), *PropertyId.ToString(), NewValue,
        TargetSceneComponent ? *TargetSceneComponent->GetName() : TEXT("Bağlanmadı (Aranıyor...)"));

    APiSimVehicle* ParentVehicle = Cast<APiSimVehicle>(GetOwner());
    if (ParentVehicle && ParentVehicle->bShowDebugVisualsAndMessages && GEngine)
    {
        GEngine->AddOnScreenDebugMessage(930, 2.0f, FColor::Emerald,
            FString::Printf(TEXT("⚡ [PERVANE DEBUG] %s (%s) = %.2f"), *TargetBoneName.ToString(), *PropertyId.ToString(), NewValue));
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

// @state: WIP - İtki kuvvetini (normalize yön * Newton) hedef kemiğe ve görsel pervane dönüşünü uygular
void UPiSimPropellerComponent::ApplyThrustPhysics(float DeltaTime)
{
    if (Throttle <= 0.001f)
    {
        return;
    }

    FVector NormThrustDir = ThrustDirectionVector.IsNearlyZero() ? LocalThrustDirection.GetSafeNormal() : ThrustDirectionVector.GetSafeNormal();
    if (bReverseThrust) NormThrustDir = -NormThrustDir;

    FVector SpinAx = VisualSpinAxis.IsNearlyZero() ? NormThrustDir : VisualSpinAxis.GetSafeNormal();
    float ActiveVisualRPM = Throttle * VisualSpinSpeedRPM;

    // 1) Dinamik FBX Kemik Bileşeni Varsa Görsel Pervane Dönüşü + Fiziksel Kuvvet
    if (TargetSceneComponent)
    {
        float AngularSpeedDeg = ActiveVisualRPM * 6.0f;
        FQuat SpinQuat(SpinAx, FMath::DegreesToRadians(AngularSpeedDeg * DeltaTime));
        TargetSceneComponent->AddLocalRotation(SpinQuat);

        APiSimVehicle* Vehicle = ParentVehicle ? ParentVehicle : Cast<APiSimVehicle>(GetOwner());
        if (Vehicle && Vehicle->IsVehiclePhysicsActive())
        {
            UPrimitiveComponent* TargetBody = Vehicle->GetPrimitiveComponentForBone(TargetBoneName);
            if (!TargetBody || !TargetBody->IsSimulatingPhysics())
            {
                TargetBody = Cast<UPrimitiveComponent>(Vehicle->GetRootComponent());
            }
            if (TargetBody && TargetBody->IsSimulatingPhysics())
            {
                FTransform RefT = TargetSceneComponent->GetComponentTransform();
                FVector WorldThrust = RefT.TransformVectorNoScale(NormThrustDir).GetSafeNormal();
                TargetBody->AddForceAtLocation(WorldThrust * CurrentThrustNewtons * 100.0f, RefT.GetLocation());
            }
        }
    }

    // 2) SkeletalMesh Varsa Fiziksel İtki Tatbik Et
    if (!CachedSkeletalMesh)
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
