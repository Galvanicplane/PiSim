// MotorConfigurator.cpp

#include "MotorConfigurator.h"
#include "Vehicle.h"
#include "Wheel.h"
#include "Servo.h"
#include "Propeller.h"
#include "LiftBody.h"

// @state: WIP - Motor yapılandırıcı nesne yapıcısı
UPiSimMotorConfigurator::UPiSimMotorConfigurator()
{
}

// @state: WIP - Kemik isimlerine göre otomatik motor/eyleyici ataması yapar
void UPiSimMotorConfigurator::AutoAssignMotorsByBoneNames(APiSimVehicle* TargetVehicle)
{
    if (!TargetVehicle)
    {
        return;
    }

    TArray<FName> BoneNames;
    TargetVehicle->GetVehicleBoneNames(BoneNames);

    for (const FName& BoneName : BoneNames)
    {
        FString BoneStr = BoneName.ToString().ToLower();

        if (BoneStr.Contains(TEXT("wheel")) || BoneStr.Contains(TEXT("teker")) || BoneStr.Contains(TEXT("caster")))
        {
            AssignMotorRoleToBone(TargetVehicle, BoneName, TEXT("Wheel"));
        }
        else if (BoneStr.Contains(TEXT("servo")) || BoneStr.Contains(TEXT("steer")) || BoneStr.Contains(TEXT("rudder")) || 
                 BoneStr.Contains(TEXT("aileron")) || BoneStr.Contains(TEXT("elevator")) || BoneStr.Contains(TEXT("arm")))
        {
            AssignMotorRoleToBone(TargetVehicle, BoneName, TEXT("Servo"));
        }
        else if (BoneStr.Contains(TEXT("prop")) || BoneStr.Contains(TEXT("motor")) || BoneStr.Contains(TEXT("thrust")) || BoneStr.Contains(TEXT("pervane")))
        {
            AssignMotorRoleToBone(TargetVehicle, BoneName, TEXT("Propeller"));
        }
        else if (BoneStr.Contains(TEXT("wing")) || BoneStr.Contains(TEXT("kanat")) || BoneStr.Contains(TEXT("lift")) || BoneStr.Contains(TEXT("flap")))
        {
            AssignMotorRoleToBone(TargetVehicle, BoneName, TEXT("LiftBody"));
        }
    }
}

// @state: WIP - Belirtilen kemiğe istenen mikro modül bileşenini dinamik olarak bağlar
UActorComponent* UPiSimMotorConfigurator::AssignMotorRoleToBone(APiSimVehicle* TargetVehicle, FName BoneName, const FString& RoleName)
{
    if (!TargetVehicle || BoneName.IsNone())
    {
        return nullptr;
    }

    UActorComponent* NewComp = nullptr;

    if (RoleName == TEXT("Wheel"))
    {
        UPiSimWheelComponent* WheelComp = NewObject<UPiSimWheelComponent>(TargetVehicle);
        if (WheelComp)
        {
            WheelComp->TargetBoneName = BoneName;
            WheelComp->RegisterComponent();
            NewComp = WheelComp;
        }
    }
    else if (RoleName == TEXT("Servo"))
    {
        UPiSimServoComponent* ServoComp = NewObject<UPiSimServoComponent>(TargetVehicle);
        if (ServoComp)
        {
            ServoComp->TargetBoneName = BoneName;
            ServoComp->RegisterComponent();
            NewComp = ServoComp;
        }
    }
    else if (RoleName == TEXT("Propeller"))
    {
        UPiSimPropellerComponent* PropComp = NewObject<UPiSimPropellerComponent>(TargetVehicle);
        if (PropComp)
        {
            PropComp->TargetBoneName = BoneName;
            PropComp->RegisterComponent();
            NewComp = PropComp;
        }
    }
    else if (RoleName == TEXT("LiftBody"))
    {
        UPiSimLiftBodyComponent* LiftComp = NewObject<UPiSimLiftBodyComponent>(TargetVehicle);
        if (LiftComp)
        {
            LiftComp->TargetBoneName = BoneName;
            LiftComp->RegisterComponent();
            NewComp = LiftComp;
        }
    }

    if (NewComp)
    {
        TargetVehicle->AssignModuleToBone(BoneName, NewComp);
    }

    return NewComp;
}
