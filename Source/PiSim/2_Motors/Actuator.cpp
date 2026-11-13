// Actuator.cpp

#include "Actuator.h"
#include "Vehicle.h"
#include "PiSimTopicBridge.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"

// @state: WIP - Araç kemik listesinden LA_<isim>_a/_b veya R_<isim>_a/_b ikili kemiklerini modül içinde çözer
void UPiSimLinearActuatorComponent::ResolvePartnerBones(APiSimVehicle* InVehicle)
{
    if (!InVehicle) return;

    TArray<FName> AllBones;
    InVehicle->GetVehicleBoneNames(AllBones);

    const FString TargetStr = TargetBoneName.ToString();
    if (TargetStr.StartsWith(TEXT("R_"), ESearchCase::IgnoreCase))
    {
        bIsRopeVariant = true;
    }
    else if (TargetStr.StartsWith(TEXT("LA_"), ESearchCase::IgnoreCase))
    {
        bIsRopeVariant = false;
    }

    if (!TargetBoneName.IsNone())
    {
        FString Stem = TargetStr;
        if (Stem.EndsWith(TEXT("_a"), ESearchCase::IgnoreCase) || Stem.EndsWith(TEXT("_b"), ESearchCase::IgnoreCase))
        {
            Stem = Stem.LeftChop(2);
        }

        FName FoundA = NAME_None;
        FName FoundB = NAME_None;
        const FString ExpectedA = Stem + TEXT("_a");
        const FString ExpectedB = Stem + TEXT("_b");

        for (const FName& BName : AllBones)
        {
            const FString BStr = BName.ToString();
            if (BStr.Equals(ExpectedA, ESearchCase::IgnoreCase))
            {
                FoundA = BName;
            }
            else if (BStr.Equals(ExpectedB, ESearchCase::IgnoreCase))
            {
                FoundB = BName;
            }
        }

        if (!FoundA.IsNone() && !FoundB.IsNone())
        {
            BoneAName = FoundA;
            BoneBName = FoundB;
            bIsRopeVariant = Stem.StartsWith(TEXT("R_"), ESearchCase::IgnoreCase);
            return;
        }

        if (TargetStr.EndsWith(TEXT("_a"), ESearchCase::IgnoreCase) || TargetStr.EndsWith(TEXT("_b"), ESearchCase::IgnoreCase))
        {
            BoneAName = FName(*ExpectedA);
            BoneBName = FName(*ExpectedB);
            return;
        }
    }

    if (BoneAName.IsNone() || BoneBName.IsNone())
    {
        TMap<FString, TPair<FName, FName>> CandidatePairs;
        for (const FName& BName : AllBones)
        {
            const FString BStr = BName.ToString();
            const bool bIsLA = BStr.StartsWith(TEXT("LA_"), ESearchCase::IgnoreCase);
            const bool bIsR = BStr.StartsWith(TEXT("R_"), ESearchCase::IgnoreCase);
            if ((bIsLA || bIsR) && (BStr.EndsWith(TEXT("_a"), ESearchCase::IgnoreCase) || BStr.EndsWith(TEXT("_b"), ESearchCase::IgnoreCase)))
            {
                const FString Stem = BStr.LeftChop(2);
                TPair<FName, FName>& Pair = CandidatePairs.FindOrAdd(Stem);
                if (BStr.EndsWith(TEXT("_a"), ESearchCase::IgnoreCase))
                {
                    Pair.Key = BName;
                }
                else
                {
                    Pair.Value = BName;
                }
            }
        }

        const FString CompName = GetName();
        FName FallbackA = NAME_None;
        FName FallbackB = NAME_None;
        bool bFallbackIsRope = false;

        for (const auto& Entry : CandidatePairs)
        {
            if (!Entry.Value.Key.IsNone() && !Entry.Value.Value.IsNone())
            {
                const bool bEntryRope = Entry.Key.StartsWith(TEXT("R_"), ESearchCase::IgnoreCase);
                if (CompName.Contains(Entry.Key, ESearchCase::IgnoreCase))
                {
                    BoneAName = Entry.Value.Key;
                    BoneBName = Entry.Value.Value;
                    bIsRopeVariant = bEntryRope;
                    return;
                }
                if (FallbackA.IsNone())
                {
                    FallbackA = Entry.Value.Key;
                    FallbackB = Entry.Value.Value;
                    bFallbackIsRope = bEntryRope;
                }
            }
        }

        if (TargetBoneName.IsNone() && !FallbackA.IsNone() && !FallbackB.IsNone())
        {
            BoneAName = FallbackA;
            BoneBName = FallbackB;
            bIsRopeVariant = bFallbackIsRope;
        }
    }
}

// @state: WIP - Modül içi araç kimliğini doldurur, LA_/R_ kemik çiftini çözer ve Zenoh Bridge ACK + kontrol topic'ini dinler
void UPiSimLinearActuatorComponent::InitializeModuleIdentity(APiSimVehicle* InVehicle)
{
    ParentVehicle = InVehicle;
    if (InVehicle)
    {
        ParentVehicleName = InVehicle->VehicleCode;
        VehicleIndex = InVehicle->VehicleId;
        ServoChannel = ArduPilotChannel;

        ResolvePartnerBones(InVehicle);

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
}

// @state: WIP - Aktüatör modülünün kendi içinden ArduPilot PWM (öncelikli) veya ROS kontrol topiğini dinlemesi
void UPiSimLinearActuatorComponent::HandleControlTopic(const FString& InVehicleCode, const FString& TopicName, const FString& SourceType, const TSharedPtr<FJsonObject>& PayloadObj)
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
        SetActuatorPWM(PwmVal);
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

    if (!bTopicBridgeBound)
    {
        InitializeModuleIdentity(Vehicle);
    }

    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    if (!bManualOverride && LastControlTopicTime > 0.0 && (Now - LastControlTopicTime) > TopicWatchdogTimeoutSec)
    {
        SetActuatorPWM(1500);
        LastControlTopicTime = -100.0;
    }

    if ((BoneAName.IsNone() || BoneBName.IsNone()) && !TargetBoneName.IsNone())
    {
        ResolvePartnerBones(Vehicle);
    }

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
        bManualOverride = (FMath::Abs(ChannelTestSlider) > 0.001f);
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
