// Vehicle.cpp

#include "Vehicle.h"
#include "TelemetryGateway.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"

// @state: WIP - Araç Pawn yapıcısı, iskelet ve kamera bileşenlerini kurar
APiSimVehicle::APiSimVehicle()
{
    PrimaryActorTick.bCanEverTick = true;

    VehicleSkeletalMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("VehicleSkeletalMesh"));
    SetRootComponent(VehicleSkeletalMesh);
    VehicleSkeletalMesh->SetSimulatePhysics(false);
    VehicleSkeletalMesh->SetCollisionProfileName(TEXT("BlockAll"));
    VehicleSkeletalMesh->bApplyImpulseOnDamage = false;

    VisualSkeletalMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("VisualSkeletalMesh"));
    VisualSkeletalMesh->SetupAttachment(VehicleSkeletalMesh);
    VisualSkeletalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    CameraSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraSpringArm"));
    CameraSpringArm->SetupAttachment(VehicleSkeletalMesh);
    CameraSpringArm->TargetArmLength = 350.0f;
    CameraSpringArm->bUsePawnControlRotation = false;
    CameraSpringArm->bDoCollisionTest = false;

    OrbitCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("OrbitCamera"));
    OrbitCamera->SetupAttachment(CameraSpringArm);

    TelemetryGateway = CreateDefaultSubobject<UPiSimTelemetryGateway>(TEXT("TelemetryGateway"));
}

// @state: WIP - Başlangıçta fizik durumunu ayarlar
void APiSimVehicle::BeginPlay()
{
    Super::BeginPlay();

    if (bStartWithPhysics)
    {
        SetVehiclePhysicsEnabled(true);
    }
}

// @state: WIP - Her frame araç durumunu günceller
void APiSimVehicle::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

// @state: WIP - Fizik durumunu doğrular ve güvenle açıp kapatır
void APiSimVehicle::SetVehiclePhysicsEnabled(bool bEnable)
{
    if (!VehicleSkeletalMesh)
    {
        return;
    }

    if (bEnable)
    {
        // 1. İskelet ve Physics Asset kontrolü
        if (!VehicleSkeletalMesh->GetSkeletalMeshAsset())
        {
            UE_LOG(LogTemp, Warning, TEXT("[APiSimVehicle] Fizik açılamadı: SkeletalMesh atanmamış!"));
            return;
        }

        UPhysicsAsset* PhysAsset = VehicleSkeletalMesh->GetPhysicsAsset();
        if (!PhysAsset)
        {
            UE_LOG(LogTemp, Warning, TEXT("[APiSimVehicle] Fizik açılamadı: İskelete ait Physics Asset bulunamadı!"));
            return;
        }

        // 2. Fiziği aktifleştir
        VehicleSkeletalMesh->SetSimulatePhysics(true);

        // 3. Gerçekten fizik simülasyonu başlayıp başlamadığını teyit et
        if (VehicleSkeletalMesh->IsSimulatingPhysics())
        {
            // Kütleyi uygula
            VehicleSkeletalMesh->SetMassOverrideInKg(NAME_None, FMath::Max(0.1f, TotalMassKg), true);

            // Ağırlık merkezi ofseti (CoG)
            if (!CenterOfMassOffset.IsNearlyZero())
            {
                VehicleSkeletalMesh->SetCenterOfMass(CenterOfMassOffset, NAME_None);
            }

            // Sönümlemeler
            VehicleSkeletalMesh->SetLinearDamping(LinearDamping);
            VehicleSkeletalMesh->SetAngularDamping(AngularDamping);

            UE_LOG(LogTemp, Log, TEXT("[APiSimVehicle] Fizik simülasyonu başarıyla aktif edildi. Kütle: %.1f kg"), TotalMassKg);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("[APiSimVehicle] SetSimulatePhysics(true) çağrıldı ancak Chaos fizik gövdesi başlatılamadı!"));
        }
    }
    else
    {
        VehicleSkeletalMesh->SetSimulatePhysics(false);
        UE_LOG(LogTemp, Log, TEXT("[APiSimVehicle] Fizik simülasyonu kapatıldı."));
    }
}

// @state: WIP - Fiziğin aktif olup olmadığını döner
bool APiSimVehicle::IsVehiclePhysicsActive() const
{
    return VehicleSkeletalMesh && VehicleSkeletalMesh->IsSimulatingPhysics();
}

// @state: WIP - Belirtilen iskelet kemiğine mikro-modül (Wheel, Servo vb.) atar
bool APiSimVehicle::AssignModuleToBone(FName BoneName, UActorComponent* ModuleComponent)
{
    if (!ModuleComponent || BoneName.IsNone())
    {
        return false;
    }

    // Eski modülü temizle veya yenisiyle değiştir
    if (BoneSlotRegistry.Contains(BoneName))
    {
        UActorComponent* OldComp = BoneSlotRegistry[BoneName];
        if (OldComp && OldComp != ModuleComponent)
        {
            OldComp->DestroyComponent();
        }
    }

    BoneSlotRegistry.Add(BoneName, ModuleComponent);
    return true;
}

// @state: WIP - Kemiğe atanmış mikro modülü döndürür
UActorComponent* APiSimVehicle::GetModuleAtBone(FName BoneName) const
{
    return BoneSlotRegistry.FindRef(BoneName);
}

// @state: WIP - UI Otopark için iskeletteki tüm kemik isimlerini toplar
void APiSimVehicle::GetVehicleBoneNames(TArray<FName>& OutBoneNames) const
{
    OutBoneNames.Empty();
    if (VehicleSkeletalMesh && VehicleSkeletalMesh->GetSkeletalMeshAsset())
    {
        int32 NumBones = VehicleSkeletalMesh->GetNumBones();
        for (int32 i = 0; i < NumBones; ++i)
        {
            OutBoneNames.Add(VehicleSkeletalMesh->GetBoneName(i));
        }
    }
}
