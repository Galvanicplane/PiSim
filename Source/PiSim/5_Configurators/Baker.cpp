// Baker.cpp

#include "Baker.h"
#include "Vehicle.h"
#include "Wheel.h"
#include "Servo.h"
#include "Propeller.h"
#include "LiftBody.h"
#include "Components/SkeletalMeshComponent.h"

// @state: WIP - Baker nesne yapıcısı
UPiSimBaker::UPiSimBaker()
{
}

// @state: WIP - Araç üzerindeki tüm modülleri tarar, fizik durumunu kilitler ve özet çıkarır
bool UPiSimBaker::BakeVehicle(APiSimVehicle* TargetVehicle, FString& OutBakeSummary)
{
    if (!TargetVehicle || !TargetVehicle->VehicleSkeletalMesh)
    {
        OutBakeSummary = TEXT("Hata: Geçerli bir araç veya iskelet bulunamadı.");
        return false;
    }

    int32 WheelCount = 0;
    int32 ServoCount = 0;
    int32 PropCount = 0;
    int32 LiftCount = 0;

    TArray<UActorComponent*> AllComps;
    TargetVehicle->GetComponents(AllComps);

    for (UActorComponent* Comp : AllComps)
    {
        if (Cast<UPiSimWheelComponent>(Comp)) WheelCount++;
        else if (Cast<UPiSimServoComponent>(Comp)) ServoCount++;
        else if (Cast<UPiSimPropellerComponent>(Comp)) PropCount++;
        else if (Cast<UPiSimLiftBodyComponent>(Comp)) LiftCount++;
    }

    // Fiziği başlatmaya hazır hale getir
    TargetVehicle->SetVehiclePhysicsEnabled(true);

    OutBakeSummary = FString::Printf(TEXT("BAKE BAŞARILI: %d Tekerlek, %d Servo, %d Pervane, %d Kanat/Gövde gömüldü. Araç simülasyona hazır."),
        WheelCount, ServoCount, PropCount, LiftCount);

    return true;
}
