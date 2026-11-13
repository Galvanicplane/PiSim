// Track.cpp

#include "Track.h"
#include "Vehicle.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "GameFramework/Actor.h"

// @state: WIP - Modül içi araç kimliğini doldurur
void UPiSimTrackComponent::InitializeModuleIdentity(APiSimVehicle* InVehicle)
{
    ParentVehicle = InVehicle;
    if (InVehicle)
    {
        ParentVehicleName = InVehicle->VehicleCode;
        VehicleIndex = InVehicle->VehicleId;
        ServoChannel = ArduPilotChannel;
    }
}

// @state: WIP - Palet bileşeni yapıcısı
UPiSimTrackComponent::UPiSimTrackComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

// @state: WIP - Temas plakası zemine değiyorsa aracı palet doğrultusunda ileri iter
void UPiSimTrackComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    float EffectiveTarget = bReverseDirection ? -TargetSpeedMps : TargetSpeedMps;
    CurrentSpeedMps = FMath::FInterpTo(CurrentSpeedMps, EffectiveTarget, DeltaTime, 8.0f);

    APiSimVehicle* Vehicle = ParentVehicle ? ParentVehicle : Cast<APiSimVehicle>(GetOwner());
    UWorld* World = GetWorld();
    if (!Vehicle || !World) return;

    USceneComponent* BoneComp = Vehicle->GetSceneComponentForBone(TargetBoneName);
    if (!BoneComp) BoneComp = Vehicle->GetRootComponent();
    if (!BoneComp) return;

    FTransform BoneT = BoneComp->GetComponentTransform();
    FVector Start = BoneT.GetLocation();
    FVector End = Start - FVector(0.0f, 0.0f, ContactTraceDistanceCm);

    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(PiSimTrackContact), false, Vehicle);
    Params.AddIgnoredActor(Vehicle);
    bGroundContact = World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params);

    if (bGroundContact && Vehicle->IsVehiclePhysicsActive() && FMath::Abs(CurrentSpeedMps) > 0.01f)
    {
        UPrimitiveComponent* TargetBody = Vehicle->GetPrimitiveComponentForBone(TargetBoneName);
        if (!TargetBody || !TargetBody->IsSimulatingPhysics())
        {
            TargetBody = Cast<UPrimitiveComponent>(Vehicle->GetRootComponent());
        }
        if (TargetBody && TargetBody->IsSimulatingPhysics())
        {
            FVector WorldFwd = BoneT.TransformVectorNoScale(TrackForwardAxis).GetSafeNormal();
            FVector TangentFwd = FVector::VectorPlaneProject(WorldFwd, Hit.ImpactNormal).GetSafeNormal();
            float CurrentFwdSpeedMs = (TargetBody->GetPhysicsLinearVelocity() / 100.0f) | TangentFwd;
            float SpeedError = CurrentSpeedMps - CurrentFwdSpeedMs;
            float PropForceN = FMath::Clamp(SpeedError * FrictionCoefficient * 250.0f, -1500.0f, 1500.0f);
            TargetBody->AddForceAtLocation(TangentFwd * PropForceN * 100.0f, Start);
        }
    }
}

// @state: WIP - Otopark UI parametre listesini döner
void UPiSimTrackComponent::GetInspectableProperties(TArray<FPiSimPropertyDescriptor>& OutProperties)
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
    RevDesc.DisplayName = TEXT("Palet Yönünü Ters Çevir");
    RevDesc.PropertyType = EPiSimPropertyType::Toggle;
    RevDesc.CurrentFloatValue = bReverseDirection ? 1.0f : 0.0f;
    RevDesc.bContinuousTickUpdate = false;
    OutProperties.Add(RevDesc);

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
    if (PropertyId == TEXT("ChannelTestSlider"))
    {
        SetActuatorNormalized(NewValue);
    }
    else if (PropertyId == TEXT("ArduPilotChannel"))
    {
        ArduPilotChannel = FMath::Clamp(FMath::RoundToInt(NewValue), 1, 16);
        ServoChannel = ArduPilotChannel;
    }
    else if (PropertyId == TEXT("bReverseDirection"))
    {
        bReverseDirection = (NewValue > 0.5f);
    }
    else if (PropertyId == TEXT("TargetSpeedMps"))
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
