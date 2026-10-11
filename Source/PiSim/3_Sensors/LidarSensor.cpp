// LidarSensor.cpp

#include "LidarSensor.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "GameFramework/Actor.h"

// @state: WIP - LiDAR sensörü yapıcısı
UPiSimLidarSensor::UPiSimLidarSensor()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

// @state: WIP - Periyodik tarama döngüsünü yürütür
void UPiSimLidarSensor::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    ScanTimer += DeltaTime;
    if (ScanRateHz > 0.0f && ScanTimer >= (1.0f / ScanRateHz))
    {
        ScanTimer = 0.0f;
        PerformScan();
    }
}

// @state: WIP - Raycast ışın taraması yapar ve en yakın engeli tespit eder
void UPiSimLidarSensor::PerformScan()
{
    if (!GetWorld() || !GetOwner())
    {
        return;
    }

    FVector StartLoc = GetOwner()->GetActorLocation();
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(GetOwner());

    float MinDist = MaxRangeMeters;
    float AngleStep = 360.0f / (float)FMath::Max(1, Channels);

    for (int32 i = 0; i < Channels; ++i)
    {
        float AngleDeg = (float)i * AngleStep;
        FRotator Rot(0.0f, AngleDeg, 0.0f);
        FVector Dir = Rot.RotateVector(GetOwner()->GetActorForwardVector());
        FVector EndLoc = StartLoc + (Dir * (MaxRangeMeters * 100.0f));

        FHitResult Hit;
        if (GetWorld()->LineTraceSingleByChannel(Hit, StartLoc, EndLoc, ECC_Visibility, QueryParams))
        {
            float HitDistM = Hit.Distance / 100.0f;
            if (HitDistM < MinDist)
            {
                MinDist = HitDistM;
            }
        }
    }

    ClosestObstacleDistanceM = MinDist;
}

// @state: WIP - Otopark UI parametre listesini döner
void UPiSimLidarSensor::GetInspectableProperties(TArray<FPiSimPropertyDescriptor>& OutProperties)
{
    FPiSimPropertyDescriptor RangeDesc;
    RangeDesc.PropertyId = TEXT("MaxRangeMeters");
    RangeDesc.DisplayName = TEXT("Maksimum Menzil (m)");
    RangeDesc.PropertyType = EPiSimPropertyType::Slider;
    RangeDesc.MinValue = 5.0f;
    RangeDesc.MaxValue = 200.0f;
    RangeDesc.CurrentFloatValue = MaxRangeMeters;
    RangeDesc.bContinuousTickUpdate = false;
    OutProperties.Add(RangeDesc);

    FPiSimPropertyDescriptor ChDesc;
    ChDesc.PropertyId = TEXT("Channels");
    ChDesc.DisplayName = TEXT("Işın Kanal Sayısı");
    ChDesc.PropertyType = EPiSimPropertyType::Slider;
    ChDesc.MinValue = 4.0f;
    ChDesc.MaxValue = 64.0f;
    ChDesc.CurrentFloatValue = (float)Channels;
    ChDesc.bContinuousTickUpdate = false;
    OutProperties.Add(ChDesc);
}

// @state: WIP - UI sliderından gelen değeri uygular
void UPiSimLidarSensor::SetInspectablePropertyFloat(FName PropertyId, float NewValue)
{
    if (PropertyId == TEXT("MaxRangeMeters"))
    {
        MaxRangeMeters = FMath::Clamp(NewValue, 1.0f, 500.0f);
    }
    else if (PropertyId == TEXT("Channels"))
    {
        Channels = FMath::Clamp(FMath::RoundToInt(NewValue), 4, 128);
    }
}

// @state: WIP - UI metin kutusu işlemi
void UPiSimLidarSensor::SetInspectablePropertyString(FName PropertyId, const FString& NewValue)
{
}

// @state: WIP - UI aksiyon butonunu işler
void UPiSimLidarSensor::TriggerInspectableAction(FName ActionId)
{
}
