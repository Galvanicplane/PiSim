// LiftBody.cpp

#include "LiftBody.h"
#include "Vehicle.h"
#include "PiSimTopicBridge.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"

// @state: WIP - Modül içi araç kimliğini doldurur ve Zenoh Bridge ACK topic'ini doğrudan dinler
void UPiSimLiftBodyComponent::InitializeModuleIdentity(APiSimVehicle* InVehicle)
{
    ParentVehicle = InVehicle;
    if (InVehicle)
    {
        ParentVehicleName = InVehicle->VehicleCode;
        VehicleIndex = InVehicle->VehicleId;

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

// @state: WIP - Kaldırma gövdesi bileşeni yapıcısı
UPiSimLiftBodyComponent::UPiSimLiftBodyComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

// @state: WIP - Her frame akış hızına göre aerodinamik kuvvetleri uygular
void UPiSimLiftBodyComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!CachedSkeletalMesh && GetOwner())
    {
        CachedSkeletalMesh = GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
    }

    ApplyAerodynamicForces(DeltaTime);
}

// @state: WIP - Otopark UI parametre listesini döner
void UPiSimLiftBodyComponent::GetInspectableProperties(TArray<FPiSimPropertyDescriptor>& OutProperties)
{
    FPiSimPropertyDescriptor AreaDesc;
    AreaDesc.PropertyId = TEXT("WingAreaSquareMeters");
    AreaDesc.DisplayName = TEXT("Kanat Alanı (m²)");
    AreaDesc.PropertyType = EPiSimPropertyType::Slider;
    AreaDesc.MinValue = 0.05f;
    AreaDesc.MaxValue = 5.0f;
    AreaDesc.CurrentFloatValue = WingAreaSquareMeters;
    AreaDesc.bContinuousTickUpdate = false;
    OutProperties.Add(AreaDesc);

    FPiSimPropertyDescriptor SlopeDesc;
    SlopeDesc.PropertyId = TEXT("LiftSlope");
    SlopeDesc.DisplayName = TEXT("Kaldırma Eğimi (CL Alpha)");
    SlopeDesc.PropertyType = EPiSimPropertyType::Slider;
    SlopeDesc.MinValue = 1.0f;
    SlopeDesc.MaxValue = 10.0f;
    SlopeDesc.CurrentFloatValue = LiftSlope;
    SlopeDesc.bContinuousTickUpdate = false;
    OutProperties.Add(SlopeDesc);

    FPiSimPropertyDescriptor DeflDesc;
    DeflDesc.PropertyId = TEXT("ControlSurfaceDeflectionDeg");
    DeflDesc.DisplayName = TEXT("Kontrol Yüzeyi Sapması (°)");
    DeflDesc.PropertyType = EPiSimPropertyType::Slider;
    DeflDesc.MinValue = -30.0f;
    DeflDesc.MaxValue = 30.0f;
    DeflDesc.CurrentFloatValue = ControlSurfaceDeflectionDeg;
    DeflDesc.bContinuousTickUpdate = true;
    OutProperties.Add(DeflDesc);
}

// @state: WIP - UI sliderından gelen float değerini kanada yansıtır
void UPiSimLiftBodyComponent::SetInspectablePropertyFloat(FName PropertyId, float NewValue)
{
    if (PropertyId == TEXT("WingAreaSquareMeters"))
    {
        WingAreaSquareMeters = FMath::Max(0.001f, NewValue);
    }
    else if (PropertyId == TEXT("LiftSlope"))
    {
        LiftSlope = FMath::Max(0.1f, NewValue);
    }
    else if (PropertyId == TEXT("ControlSurfaceDeflectionDeg"))
    {
        ControlSurfaceDeflectionDeg = FMath::Clamp(NewValue, -45.0f, 45.0f);
    }
}

// @state: WIP - UI metin kutusundan gelen string değerini işler
void UPiSimLiftBodyComponent::SetInspectablePropertyString(FName PropertyId, const FString& NewValue)
{
}

// @state: WIP - UI tetikleyici butonunu işler
void UPiSimLiftBodyComponent::TriggerInspectableAction(FName ActionId)
{
}

// @state: WIP - Hücum açısı (AoA), kullanıcı UpVector/ForwardVector ayarı ve manuel parametrelerle Lift/Drag kuvvetlerini uygular
void UPiSimLiftBodyComponent::ApplyAerodynamicForces(float DeltaTime)
{
    APiSimVehicle* Vehicle = ParentVehicle ? ParentVehicle : Cast<APiSimVehicle>(GetOwner());
    FTransform BoneTransform = FTransform::Identity;
    FVector BoneVelocity = FVector::ZeroVector;
    UPrimitiveComponent* TargetPrim = nullptr;

    if (Vehicle && Vehicle->IsVehiclePhysicsActive())
    {
        USceneComponent* BoneComp = Vehicle->GetSceneComponentForBone(TargetBoneName);
        if (!BoneComp) BoneComp = Vehicle->GetRootComponent();
        if (BoneComp) BoneTransform = BoneComp->GetComponentTransform();

        TargetPrim = Vehicle->GetPrimitiveComponentForBone(TargetBoneName);
        if (!TargetPrim || !TargetPrim->IsSimulatingPhysics())
        {
            TargetPrim = Cast<UPrimitiveComponent>(Vehicle->GetRootComponent());
        }
        if (TargetPrim && TargetPrim->IsSimulatingPhysics())
        {
            BoneVelocity = TargetPrim->GetPhysicsLinearVelocity();
        }
    }
    else if (CachedSkeletalMesh)
    {
        BoneTransform = TargetBoneName.IsNone() ? CachedSkeletalMesh->GetComponentTransform() : CachedSkeletalMesh->GetSocketTransform(TargetBoneName);
        BoneVelocity = CachedSkeletalMesh->GetPhysicsLinearVelocity(TargetBoneName);
    }
    else
    {
        return;
    }

    float SpeedMs = BoneVelocity.Size() / 100.0f;
    CurrentAirspeedKmh = SpeedMs * 3.6f;

    if (SpeedMs < 1.5f)
    {
        CurrentLiftNewtons = 0.0f;
        CurrentDragNewtons = 0.0f;
        CurrentAoADeg = 0.0f;
        return;
    }

    FVector ForwardDir = BoneTransform.TransformVectorNoScale(ForwardVector).GetSafeNormal();
    FVector UpDir = BoneTransform.TransformVectorNoScale(UpVector).GetSafeNormal();
    FVector RightDir = (UpDir ^ ForwardDir).GetSafeNormal();
    FVector AirflowDir = -BoneVelocity.GetSafeNormal();

    float DynamicPressure = 0.5f * FluidDensity * (SpeedMs * SpeedMs);

    float DotForward = FMath::Clamp(ForwardDir | AirflowDir, -1.0f, 1.0f);
    float AoARad = FMath::Acos(DotForward);
    if ((AirflowDir | UpDir) < 0.0f)
    {
        AoARad = -AoARad;
    }
    CurrentAoADeg = FMath::RadiansToDegrees(AoARad);

    float DeflectionRad = FMath::DegreesToRadians(ControlSurfaceDeflectionDeg);
    float DeltaCL_Control = ElevonEffectiveness * DeflectionRad;
    float CL = CL0 + (LiftSlope * AoARad) + DeltaCL_Control;

    float StallRad = FMath::DegreesToRadians(StallAngleDeg);
    if (FMath::Abs(AoARad) > StallRad)
    {
        float Excess = FMath::Abs(AoARad) - StallRad;
        CL *= FMath::Max(0.2f, 1.0f - (Excess * 2.0f));
    }
    CL = FMath::Clamp(CL, -1.8f, 1.8f);

    float AspectRatio = (WingspanMeters * WingspanMeters) / FMath::Max(0.01f, WingAreaSquareMeters);
    float InducedDrag = (CL * CL) / (PI * AspectRatio * 0.85f);
    float CD = FMath::Clamp(DragCoefficientZero + InducedDrag, 0.01f, 1.5f);

    CurrentLiftNewtons = CL * DynamicPressure * WingAreaSquareMeters;
    CurrentDragNewtons = CD * DynamicPressure * WingAreaSquareMeters;

    FVector LiftDir = (AirflowDir ^ RightDir).GetSafeNormal();
    if ((LiftDir | UpDir) < 0.0f) LiftDir = -LiftDir;
    FVector DragDir = AirflowDir;

    FVector TotalAeroForce = (LiftDir * CurrentLiftNewtons + DragDir * CurrentDragNewtons) * 100.0f;

    if (TargetPrim && TargetPrim->IsSimulatingPhysics())
    {
        TargetPrim->AddForceAtLocation(TotalAeroForce, BoneTransform.GetLocation());
    }
    else if (CachedSkeletalMesh)
    {
        CachedSkeletalMesh->AddForceAtLocation(TotalAeroForce, BoneTransform.GetLocation(), TargetBoneName);
    }
}
