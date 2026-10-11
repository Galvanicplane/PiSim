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
            UActorComponent* Comp = AssignMotorRoleToBone(TargetVehicle, BoneName, TEXT("Wheel"));
            if (UPiSimWheelComponent* Wheel = Cast<UPiSimWheelComponent>(Comp))
            {
                if (BoneStr.Contains(TEXT("caster")) || BoneStr.Contains(TEXT("sarhos")))
                {
                    Wheel->SetWheelMode(EPiSimWheelMode::FreeWheel);
                }
                else if (BoneStr.Contains(TEXT("front")) || BoneStr.Contains(TEXT("steer")) || BoneStr.Contains(TEXT("on_")))
                {
                    Wheel->SetWheelMode(EPiSimWheelMode::SteeredWheel);
                    Wheel->ArduPilotChannel = 1;
                }
                else
                {
                    Wheel->SetWheelMode(EPiSimWheelMode::DriveWheel);
                    Wheel->ArduPilotChannel = 3;
                }
            }
        }
        else if (BoneStr.Contains(TEXT("servo")) || BoneStr.Contains(TEXT("steer")) || BoneStr.Contains(TEXT("rudder")) || 
                 BoneStr.Contains(TEXT("aileron")) || BoneStr.Contains(TEXT("elevator")) || BoneStr.Contains(TEXT("elevon")) || BoneStr.Contains(TEXT("arm")))
        {
            UActorComponent* Comp = AssignMotorRoleToBone(TargetVehicle, BoneName, TEXT("Servo"));
            if (UPiSimServoComponent* Servo = Cast<UPiSimServoComponent>(Comp))
            {
                if (BoneStr.Contains(TEXT("elevon_l")) || BoneStr.Contains(TEXT("aileron_l")))
                {
                    Servo->ArduPilotFunction = TEXT("Elevon_Left");
                    Servo->ArduPilotChannel = 1;
                }
                else if (BoneStr.Contains(TEXT("elevon_r")) || BoneStr.Contains(TEXT("aileron_r")))
                {
                    Servo->ArduPilotFunction = TEXT("Elevon_Right");
                    Servo->ArduPilotChannel = 2;
                }
                else if (BoneStr.Contains(TEXT("elevator")))
                {
                    Servo->ArduPilotFunction = TEXT("Elevator");
                    Servo->ArduPilotChannel = 2;
                }
                else if (BoneStr.Contains(TEXT("rudder")))
                {
                    Servo->ArduPilotFunction = TEXT("Rudder");
                    Servo->ArduPilotChannel = 4;
                }
            }
        }
        else if (BoneStr.Contains(TEXT("prop")) || BoneStr.Contains(TEXT("motor")) || BoneStr.Contains(TEXT("thrust")) || BoneStr.Contains(TEXT("pervane")))
        {
            UActorComponent* Comp = AssignMotorRoleToBone(TargetVehicle, BoneName, TEXT("Propeller"));
            if (UPiSimPropellerComponent* Prop = Cast<UPiSimPropellerComponent>(Comp))
            {
                Prop->ArduPilotChannel = 3;
            }
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
