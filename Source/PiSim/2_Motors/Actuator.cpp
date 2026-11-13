// Actuator.cpp

#include "Actuator.h"
#include "Vehicle.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"

// @state: WIP - Modül içi araç kimliğini ve LA_a/LA_b veya R_a/R_b kemik çiftlerini çözer
void UPiSimLinearActuatorComponent::InitializeModuleIdentity(APiSimVehicle* InVehicle)
{
    ParentVehicle = InVehicle;
    if (InVehicle)
    {
        ParentVehicleName = InVehicle->VehicleCode;
        VehicleIndex = InVehicle->VehicleId;
        ServoChannel = ArduPilotChannel;

        FString BoneStr = TargetBoneName.ToString();
        if (BoneStr.StartsWith(TEXT("R_"), ESearchCase::IgnoreCase))
        {
            bIsRopeVariant = true;
        }
        for (const FPiSimLinkedBonePair& Pair : InVehicle->LinkedBonePairs)
        {
            if (Pair.BoneAName == TargetBoneName || Pair.BoneBName == TargetBoneName || BoneStr.Contains(Pair.PairBaseName))
            {
                BoneAName = Pair.BoneAName;
                BoneBName = Pair.BoneBName;
                bIsRopeVariant = Pair.bIsRope;
                break;
            }
        }
        if (BoneAName.IsNone() && (BoneStr.EndsWith(TEXT("_a"), ESearchCase::IgnoreCase) || BoneStr.EndsWith(TEXT("_b"), ESearchCase::IgnoreCase)))
        {
            FString Base = BoneStr.LeftChop(2);
            BoneAName = FName(*(Base + TEXT("_a")));
            BoneBName = FName(*(Base + TEXT("_b")));
        }
    }
}

// @state: WIP - Lineer aktüatör yapıcısı
UPiSimLinearActuatorComponent::UPiSimLinearActuatorComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

// @state: WIP - İki kemik arasında doğrusal 0.5*Force itme/çekme veya dururken sabit mesafe koruma fiziği uygular
void UPiSimLinearActuatorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    APiSimVehicle* Vehicle = ParentVehicle ? ParentVehicle : Cast<APiSimVehicle>(GetOwner());
    if (!Vehicle) return;

    USceneComponent* CompA = Vehicle->GetSceneComponentForBone(BoneAName.IsNone() ? TargetBoneName : BoneAName);
    USceneComponent* CompB = !BoneBName.IsNone() ? Vehicle->GetSceneComponentForBone(BoneBName) : nullptr;

    if (CompA && CompB)
    {
        FVector LocA = CompA->GetComponentLocation();
        FVector LocB = CompB->GetComponentLocation();
        FVector DeltaVec = LocB - LocA;
        float CurrentDist = DeltaVec.Size();
        FVector LineDir = DeltaVec.GetSafeNormal();

        if (!bHasCapturedInitialRestDistance && CurrentDist > 0.1f)
        {
            RestDistanceCm = CurrentDist;
            bHasCapturedInitialRestDistance = true;
        }

        // Halat (R_) değilse ve hareket komutu varsa hedef mesafeyi güncelle
        if (!bIsRopeVariant && FMath::Abs(NormalizedCommand) > 0.01f)
        {
            RestDistanceCm = FMath::Clamp(RestDistanceCm + (NormalizedCommand * StrokeSpeedCmPerSec * DeltaTime), MinDistanceCm, MaxDistanceCm);
        }

        CurrentStrokeCm = FMath::Clamp(RestDistanceCm - MinDistanceCm, 0.0f, MaxStrokeCm);

        // Fizik aktifken iki kemiğe yarımşar (0.5 * Force) kuvvet uygula
        if (Vehicle->IsVehiclePhysicsActive())
        {
            UPrimitiveComponent* PrimA = Vehicle->GetPrimitiveComponentForBone(BoneAName);
            UPrimitiveComponent* PrimB = Vehicle->GetPrimitiveComponentForBone(BoneBName);

            // Dururken mesafeyi koruma veya hareket anında doğrusal hat boyunca itme/çekme
            float DistError = CurrentDist - RestDistanceCm; // + ise birbirinden uzaklaşmış, - ise yaklaşmış
            float RequiredForceN = FMath::Clamp(-DistError * HoldStiffness, -MaxPushForceNewtons, MaxPushForceNewtons);
            if (!bIsRopeVariant && FMath::Abs(NormalizedCommand) > 0.01f)
            {
                RequiredForceN += NormalizedCommand * MaxPushForceNewtons;
                RequiredForceN = FMath::Clamp(RequiredForceN, -MaxPushForceNewtons, MaxPushForceNewtons);
            }

            // İki kemiğe eşit yarı kuvvet (0.5 * Force) dağılımı (1 Newton = 100 Unreal unit)
            FVector HalfForceVec = LineDir * (0.5f * RequiredForceN * 100.0f);
            if (PrimA && PrimA->IsSimulatingPhysics())
            {
                PrimA->AddForceAtLocation(-HalfForceVec, LocA);
            }
            if (PrimB && PrimB->IsSimulatingPhysics())
            {
                PrimB->AddForceAtLocation(HalfForceVec, LocB);
            }
        }
        return;
    }

    // Tek kemikli klasik strok enterpolasyonu
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

    FPiSimPropertyDescriptor RopeDesc;
    RopeDesc.PropertyId = TEXT("bIsRopeVariant");
    RopeDesc.DisplayName = TEXT("Halat Modu (R_ Sabit Uzunluk)");
    RopeDesc.PropertyType = EPiSimPropertyType::Toggle;
    RopeDesc.CurrentFloatValue = bIsRopeVariant ? 1.0f : 0.0f;
    RopeDesc.bContinuousTickUpdate = false;
    OutProperties.Add(RopeDesc);

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
    if (PropertyId == TEXT("ChannelTestSlider"))
    {
        ChannelTestSlider = FMath::Clamp(NewValue, -1.0f, 1.0f);
        NormalizedCommand = ChannelTestSlider;
    }
    else if (PropertyId == TEXT("ArduPilotChannel"))
    {
        ArduPilotChannel = FMath::Clamp(FMath::RoundToInt(NewValue), 1, 16);
        ServoChannel = ArduPilotChannel;
    }
    else if (PropertyId == TEXT("bIsRopeVariant"))
    {
        bIsRopeVariant = (NewValue > 0.5f);
    }
    else if (PropertyId == TEXT("TargetStrokeCm"))
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
