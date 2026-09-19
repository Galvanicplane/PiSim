// IMUSensor.cpp

#include "IMUSensor.h"
#include "TelemetryGateway.h"
#include "GameFramework/Actor.h"

// @state: WIP - IMU sensörü yapıcısı
UPiSimIMUSensor::UPiSimIMUSensor()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

// @state: WIP - Başlangıç hızını ve Gateway referansını bağlar
void UPiSimIMUSensor::BeginPlay()
{
    Super::BeginPlay();

    if (GetOwner())
    {
        CachedGateway = GetOwner()->FindComponentByClass<UPiSimTelemetryGateway>();
        PreviousVelocity = GetOwner()->GetVelocity();
        bHasPreviousVelocity = true;
    }
}

// @state: WIP - Her frame gerçekçi ivme, jiroskop ve yerçekimi vektörünü hesaplar
void UPiSimIMUSensor::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!GetOwner() || DeltaTime <= 0.0001f)
    {
        return;
    }

    FVector CurrentVelocity = GetOwner()->GetVelocity();
    if (!bHasPreviousVelocity)
    {
        PreviousVelocity = CurrentVelocity;
        bHasPreviousVelocity = true;
    }

    // Doğrusal İvme: a = dv / dt (cm/s^2 -> m/s^2)
    FVector RawAccel = (CurrentVelocity - PreviousVelocity) / (DeltaTime * 100.0f);
    PreviousVelocity = CurrentVelocity;

    // Yerçekimi ivmesi ekle (Sensör yerçekimini hisseder)
    FVector Gravity(0.0f, 0.0f, 9.81f);
    LinearAcceleration = RawAccel + Gravity;

    // Gürültü ekle (Sensör noise simülasyonu)
    if (NoiseIntensity > 0.0f)
    {
        LinearAcceleration += FVector(FMath::FRandRange(-NoiseIntensity, NoiseIntensity),
                                      FMath::FRandRange(-NoiseIntensity, NoiseIntensity),
                                      FMath::FRandRange(-NoiseIntensity, NoiseIntensity));
    }

    // Açısal Hız (Jiroskop)
    UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent());
    if (RootPrim && RootPrim->IsSimulatingPhysics())
    {
        AngularVelocityDeg = RootPrim->GetPhysicsAngularVelocityInDegrees();
    }
    else
    {
        AngularVelocityDeg = FVector::ZeroVector;
    }

    if (CachedGateway)
    {
        FPiSimIMUData ImuData;
        ImuData.LinearAcceleration = LinearAcceleration;
        ImuData.AngularVelocity = AngularVelocityDeg;
        ImuData.Orientation = GetOwner()->GetActorQuat();
        CachedGateway->FeedIMUData(ImuData);
    }
}

// @state: WIP - Otopark UI parametre listesini döner
void UPiSimIMUSensor::GetInspectableProperties(TArray<FPiSimPropertyDescriptor>& OutProperties)
{
    FPiSimPropertyDescriptor NoiseDesc;
    NoiseDesc.PropertyId = TEXT("NoiseIntensity");
    NoiseDesc.DisplayName = TEXT("Sensör Gürültüsü (Noise)");
    NoiseDesc.PropertyType = EPiSimPropertyType::Slider;
    NoiseDesc.MinValue = 0.0f;
    NoiseDesc.MaxValue = 0.5f;
    NoiseDesc.CurrentFloatValue = NoiseIntensity;
    NoiseDesc.bContinuousTickUpdate = false;
    OutProperties.Add(NoiseDesc);
}

// @state: WIP - UI sliderından gelen float değerini günceller
void UPiSimIMUSensor::SetInspectablePropertyFloat(FName PropertyId, float NewValue)
{
    if (PropertyId == TEXT("NoiseIntensity"))
    {
        NoiseIntensity = FMath::Clamp(NewValue, 0.0f, 2.0f);
    }
}

// @state: WIP - UI metin kutusundan gelen string değerini işler
void UPiSimIMUSensor::SetInspectablePropertyString(FName PropertyId, const FString& NewValue)
{
}

// @state: WIP - UI aksiyon butonunu işler
void UPiSimIMUSensor::TriggerInspectableAction(FName ActionId)
{
}
