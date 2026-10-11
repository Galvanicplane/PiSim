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

    if (!CachedGateway)
    {
        CachedGateway = GetOwner()->FindComponentByClass<UPiSimTelemetryGateway>();
    }

    FVector CurrentVelocity = GetOwner()->GetVelocity();
    if (!bHasPreviousVelocity)
    {
        PreviousVelocity = CurrentVelocity;
        bHasPreviousVelocity = true;
    }

    // Dünya İvmesi: a = dv / dt (cm/s^2 -> m/s^2)
    FVector WorldLinearAccel = (CurrentVelocity - PreviousVelocity) / (DeltaTime * 100.0f);
    PreviousVelocity = CurrentVelocity;

    // Yerçekimi reaksiyonu (Dikey yukarı +9.81 m/s^2 zemin tepkisi)
    FVector AccelWithGravity = WorldLinearAccel + FVector(0.0f, 0.0f, 9.81f);

    // Gövde (Body) eksenine çevir
    FQuat OwnerQuat = GetOwner()->GetActorQuat();
    FVector BodyAccel = OwnerQuat.UnrotateVector(AccelWithGravity);

    // Gürültü ekle
    if (NoiseIntensity > 0.0f)
    {
        BodyAccel += FVector(FMath::FRandRange(-NoiseIntensity, NoiseIntensity),
                             FMath::FRandRange(-NoiseIntensity, NoiseIntensity),
                             FMath::FRandRange(-NoiseIntensity, NoiseIntensity));
    }
    LinearAcceleration = BodyAccel;

    // Açısal Hız (Jiroskop)
    FVector WorldAngularVelDeg = FVector::ZeroVector;
    UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent());
    if (RootPrim && RootPrim->IsSimulatingPhysics())
    {
        WorldAngularVelDeg = RootPrim->GetPhysicsAngularVelocityInDegrees();
    }
    AngularVelocityDeg = WorldAngularVelDeg;

    // Gövde açısal hızı (rad/s)
    FVector BodyAngularVelDeg = OwnerQuat.UnrotateVector(WorldAngularVelDeg);
    FVector BodyAngularVelRad = FVector(
        FMath::DegreesToRadians(BodyAngularVelDeg.X),
        FMath::DegreesToRadians(BodyAngularVelDeg.Y),
        FMath::DegreesToRadians(BodyAngularVelDeg.Z)
    );

    // Manyetometre hesabı (Dünya Manyetik Alanı ~0.5 Gauss)
    FVector WorldMag(0.22f, 0.01f, 0.42f);
    FVector BodyMag = OwnerQuat.UnrotateVector(WorldMag);

    if (CachedGateway)
    {
        FPiSimIMUData ImuData;
        ImuData.LinearAcceleration = BodyAccel;
        ImuData.AngularVelocityRad = BodyAngularVelRad;
        ImuData.Orientation = OwnerQuat;
        ImuData.MagnetometerGauss = BodyMag;
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
