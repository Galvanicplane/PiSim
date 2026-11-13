// MotorConfigurator.cpp

#include "MotorConfigurator.h"
#include "Vehicle.h"
#include "Wheel.h"
#include "Servo.h"
#include "Propeller.h"
#include "Actuator.h"
#include "Track.h"
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

        if (BoneStr.Contains(TEXT("wheel")) || BoneStr.Contains(TEXT("teker")))
        {
            // Caster / serbest dönen tekerlekler 0 modüllü kemik olarak bırakılır; motorlu tekerlekler birleşik Wheel modülü alır
            if (!BoneStr.Contains(TEXT("caster")) && !BoneStr.Contains(TEXT("sarhos")))
            {
                UActorComponent* Comp = AssignMotorRoleToBone(TargetVehicle, BoneName, TEXT("Wheel"));
                if (UPiSimWheelComponent* Wheel = Cast<UPiSimWheelComponent>(Comp))
                {
                    Wheel->ArduPilotChannel = 1;
                    Wheel->ServoChannel = 1;
                }
            }
        }
        else if (BoneStr.StartsWith(TEXT("la_")) && BoneStr.EndsWith(TEXT("_a")))
        {
            AssignMotorRoleToBone(TargetVehicle, BoneName, TEXT("Actuator"));
        }
        else if (BoneStr.StartsWith(TEXT("r_")) && BoneStr.EndsWith(TEXT("_a")))
        {
            UActorComponent* Comp = AssignMotorRoleToBone(TargetVehicle, BoneName, TEXT("Rope"));
            if (UPiSimLinearActuatorComponent* Rope = Cast<UPiSimLinearActuatorComponent>(Comp))
            {
                Rope->bIsRopeVariant = true;
            }
        }
        else if (BoneStr.Contains(TEXT("track")) || BoneStr.Contains(TEXT("palet")))
        {
            AssignMotorRoleToBone(TargetVehicle, BoneName, TEXT("Track"));
        }
        else if (BoneStr.Contains(TEXT("servo")) || BoneStr.Contains(TEXT("steer")) || BoneStr.Contains(TEXT("rudder")) || 
                 BoneStr.Contains(TEXT("aileron")) || BoneStr.Contains(TEXT("elevator")) || BoneStr.Contains(TEXT("elevon")) || BoneStr.Contains(TEXT("arm")))
        {
            UActorComponent* Comp = AssignMotorRoleToBone(TargetVehicle, BoneName, TEXT("Servo"));
            if (UPiSimServoComponent* Servo = Cast<UPiSimServoComponent>(Comp))
            {
                if (BoneStr.Contains(TEXT("steer")) || BoneStr.Contains(TEXT("servo")))
                {
                    Servo->ArduPilotFunction = TEXT("Steer");
                    Servo->ArduPilotChannel = 2; // Kanal 2: Direksiyon Servoları (Sağ/Sol)
                }
                else if (BoneStr.Contains(TEXT("elevon_l")) || BoneStr.Contains(TEXT("aileron_l")))
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
                else
                {
                    Servo->ArduPilotFunction = TEXT("Steer");
                    Servo->ArduPilotChannel = 2;
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
    else if (RoleName == TEXT("Actuator") || RoleName == TEXT("Rope"))
    {
        UPiSimLinearActuatorComponent* ActComp = NewObject<UPiSimLinearActuatorComponent>(TargetVehicle);
        if (ActComp)
        {
            ActComp->TargetBoneName = BoneName;
            ActComp->bIsRopeVariant = (RoleName == TEXT("Rope"));
            ActComp->RegisterComponent();
            NewComp = ActComp;
        }
    }
    else if (RoleName == TEXT("Track"))
    {
        UPiSimTrackComponent* TrackComp = NewObject<UPiSimTrackComponent>(TargetVehicle);
        if (TrackComp)
        {
            TrackComp->TargetBoneName = BoneName;
            TrackComp->RegisterComponent();
            NewComp = TrackComp;
        }
    }

    if (NewComp)
    {
        TargetVehicle->AssignModuleToBone(BoneName, NewComp);
    }

    return NewComp;
}
