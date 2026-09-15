// PiSimAirImporter.cpp

#include "PiSimAirImporter.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"

APiSimAirImporter::APiSimAirImporter()
{
    VehicleDomain = EVehicleDomain::Air;
}

void APiSimAirImporter::SetupDomainComponents()
{
    Super::SetupDomainComponents();

    if (VisualMeshComponents.Num() == 0 || !VisualMeshComponents[0])
    {
        return;
    }

    UPrimitiveComponent* RootBody = VisualMeshComponents[0];

    // 1) Kanat Kaldırma Gövdelerini (Aero Wing Components) Soket Noktalarına Tak
    // Sağ ve Sol Kanat için 2 ayrı aerodinamik kuvvet gövdesi
    if (!LeftWingComponent)
    {
        LeftWingComponent = NewObject<UPiSimAeroWingComponent>(this, TEXT("LeftWingAeroBody"));
        LeftWingComponent->WingName = TEXT("LeftWing");
        LeftWingComponent->bIsRightWing = false;
        LeftWingComponent->WingArea = FlightConfig.WingArea * 0.5f;
        LeftWingComponent->Wingspan = FlightConfig.Wingspan * 0.5f;
        LeftWingComponent->CL0 = FlightConfig.CL0;
        LeftWingComponent->CLAlpha = FlightConfig.CLAlpha;
        LeftWingComponent->CD0 = FlightConfig.CD0;
        LeftWingComponent->StallAngleDeg = FlightConfig.StallAngleDeg;
        LeftWingComponent->ElevonEffectiveness = FlightConfig.ElevonEffectiveness;

        LeftWingComponent->SetupAttachment(RootBody);

        // Sol kanat kemik ofseti (Örn: Y < 0)
        FVector LeftOffset = FVector(0.0f, -FlightConfig.Wingspan * 25.0f, 0.0f);
        LeftWingComponent->SetRelativeLocation(LeftOffset);
        LeftWingComponent->RegisterComponent();
        AttachedWingBodies.Add(LeftWingComponent);
    }

    if (!RightWingComponent)
    {
        RightWingComponent = NewObject<UPiSimAeroWingComponent>(this, TEXT("RightWingAeroBody"));
        RightWingComponent->WingName = TEXT("RightWing");
        RightWingComponent->bIsRightWing = true;
        RightWingComponent->WingArea = FlightConfig.WingArea * 0.5f;
        RightWingComponent->Wingspan = FlightConfig.Wingspan * 0.5f;
        RightWingComponent->CL0 = FlightConfig.CL0;
        RightWingComponent->CLAlpha = FlightConfig.CLAlpha;
        RightWingComponent->CD0 = FlightConfig.CD0;
        RightWingComponent->StallAngleDeg = FlightConfig.StallAngleDeg;
        RightWingComponent->ElevonEffectiveness = FlightConfig.ElevonEffectiveness;

        RightWingComponent->SetupAttachment(RootBody);

        // Sağ kanat kemik ofseti (Örn: Y > 0)
        FVector RightOffset = FVector(0.0f, FlightConfig.Wingspan * 25.0f, 0.0f);
        RightWingComponent->SetRelativeLocation(RightOffset);
        RightWingComponent->RegisterComponent();
        AttachedWingBodies.Add(RightWingComponent);
    }

    // 2) Motorları ve İtki Aktüatörlerini Tak (BLDC / Thruster)
    for (int32 i = 0; i < VisualSections.Num(); ++i)
    {
        FString LowerName = VisualSections[i].MeshName.ToLower();
        if (LowerName.Contains(TEXT("thrust")) || LowerName.Contains(TEXT("prop")) || LowerName.Contains(TEXT("motor")))
        {
            FName ActName = *FString::Printf(TEXT("Actuator_Thruster_%d"), i);
            MainThrusterActuator = NewObject<UPiSimActuatorComponent>(this, ActName);
            MainThrusterActuator->BoneIndex = i;
            MainThrusterActuator->ActuatorName = VisualSections[i].MeshName;
            MainThrusterActuator->Role = EPiSimMotorRole::Thruster;
            MainThrusterActuator->MotorType = EPiSimMotorType::BLDC_ESC;
            MainThrusterActuator->Ros2Topic = TEXT("/actuator/thrust_cmd");
            MainThrusterActuator->BoneDirection = VisualSections[i].BoneDirection;
            MainThrusterActuator->MaxTorqueNm = 18.0f; // 18 N net itki
            MainThrusterActuator->MaxVelocityRPM = 10000.0f;

            if (VisualMeshComponents.IsValidIndex(i))
            {
                MainThrusterActuator->LinkedVisualMesh = VisualMeshComponents[i];
            }

            MainThrusterActuator->SetupAttachment(RootBody);
            FVector PropOffset = VisualSections[i].PivotPoint - VisualSections[0].PivotPoint;
            MainThrusterActuator->SetRelativeLocation(PropOffset);
            MainThrusterActuator->RegisterComponent();
            AttachedActuators.Add(MainThrusterActuator);
        }
        else if (LowerName.Contains(TEXT("elevon")) || LowerName.Contains(TEXT("aileron")))
        {
            FName ActName = *FString::Printf(TEXT("Actuator_Elevon_%d"), i);
            UPiSimActuatorComponent* ElevonActuator = NewObject<UPiSimActuatorComponent>(this, ActName);
            ElevonActuator->BoneIndex = i;
            ElevonActuator->ActuatorName = VisualSections[i].MeshName;
            ElevonActuator->Role = EPiSimMotorRole::ServoJoint;
            ElevonActuator->MotorType = EPiSimMotorType::Servo_Position;
            ElevonActuator->MinLimitDeg = -20.0f;
            ElevonActuator->MaxLimitDeg = +20.0f;

            if (VisualMeshComponents.IsValidIndex(i))
            {
                ElevonActuator->LinkedVisualMesh = VisualMeshComponents[i];
            }

            ElevonActuator->SetupAttachment(RootBody);
            FVector ElevonOffset = VisualSections[i].PivotPoint - VisualSections[0].PivotPoint;
            ElevonActuator->SetRelativeLocation(ElevonOffset);
            ElevonActuator->RegisterComponent();
            AttachedActuators.Add(ElevonActuator);

            // Sağ / Sol eşleştirmesi
            if (ElevonOffset.Y < 0.0f)
            {
                LeftElevonActuator = ElevonActuator;
                ElevonActuator->Ros2Topic = TEXT("/actuator/elevon_left");
            }
            else
            {
                RightElevonActuator = ElevonActuator;
                ElevonActuator->Ros2Topic = TEXT("/actuator/elevon_right");
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("✈️ [PISIM AIR IMPORTER] %d Kanat Gövdesi ve %d Aktüatör Başarıyla Soketlere Takıldı!"),
        AttachedWingBodies.Num(), AttachedActuators.Num());
}

void APiSimAirImporter::ApplyDomainPhysics(float DeltaTime)
{
    if (VisualMeshComponents.Num() == 0 || !VisualMeshComponents[0]) return;
    UPrimitiveComponent* RootBody = VisualMeshComponents[0];
    if (!RootBody || !RootBody->IsSimulatingPhysics()) return;

    // 1) Elevon Mikseri: Pitch + Roll girdi kombinasyonu
    float TargetLeftAngle = FMath::Clamp(CommandPitchDeg + CommandRollDeg, -20.0f, 20.0f);
    float TargetRightAngle = FMath::Clamp(CommandPitchDeg - CommandRollDeg, -20.0f, 20.0f);

    if (LeftElevonActuator)
    {
        LeftElevonActuator->SetTargetAngle(TargetLeftAngle);
    }
    if (RightElevonActuator)
    {
        RightElevonActuator->SetTargetAngle(TargetRightAngle);
    }

    if (LeftWingComponent)
    {
        LeftWingComponent->ControlSurfaceDeflectionDeg = TargetLeftAngle;
        LeftWingComponent->CalculateAndApplyAeroForces(DeltaTime, RootBody);
    }
    if (RightWingComponent)
    {
        RightWingComponent->ControlSurfaceDeflectionDeg = TargetRightAngle;
        RightWingComponent->CalculateAndApplyAeroForces(DeltaTime, RootBody);
    }

    // 2) İtki Aktüatörü (BLDC Pervane)
    if (MainThrusterActuator)
    {
        MainThrusterActuator->SetNormalizedCommand(CommandThrottle);
        MainThrusterActuator->ApplyActuatorPhysics(DeltaTime, RootBody);
        TotalThrustForceN = MainThrusterActuator->CurrentOutputForceOrTorque;
    }

    // 3) Uçuş Telemetrisi Toplama
    TotalLiftForceN = 0.0f;
    TotalDragForceN = 0.0f;
    if (LeftWingComponent && RightWingComponent)
    {
        TotalLiftForceN = LeftWingComponent->CurrentLiftN + RightWingComponent->CurrentLiftN;
        TotalDragForceN = LeftWingComponent->CurrentDragN + RightWingComponent->CurrentDragN;
        AirspeedKmh = (LeftWingComponent->AirspeedKmh + RightWingComponent->AirspeedKmh) * 0.5f;
        AirspeedMs = (LeftWingComponent->AirspeedMs + RightWingComponent->AirspeedMs) * 0.5f;
        AngleOfAttackDeg = (LeftWingComponent->AngleOfAttackDeg + RightWingComponent->AngleOfAttackDeg) * 0.5f;
    }

    // G-Force: (Lift / Weight)
    float WeightN = RootBody->GetMass() * 9.81f;
    GForce = (WeightN > 0.1f) ? (TotalLiftForceN / WeightN) : 1.0f;
}

void APiSimAirImporter::SetThrottleCommand(float InThrottle)
{
    CommandThrottle = FMath::Clamp(InThrottle, 0.0f, 1.0f);
}

void APiSimAirImporter::SetElevonMixCommand(float InPitchDeg, float InRollDeg)
{
    CommandPitchDeg = FMath::Clamp(InPitchDeg, -20.0f, 20.0f);
    CommandRollDeg = FMath::Clamp(InRollDeg, -20.0f, 20.0f);
}

void APiSimAirImporter::HandleRos2ControlPacket(const FString& Topic, float Value)
{
    if (Topic.Contains(TEXT("thrust")) || Topic.Contains(TEXT("throttle")))
    {
        SetThrottleCommand(Value);
    }
    else if (Topic.Contains(TEXT("pitch")))
    {
        CommandPitchDeg = Value;
    }
    else if (Topic.Contains(TEXT("roll")))
    {
        CommandRollDeg = Value;
    }
    else if (Topic.Contains(TEXT("elevon_left")) && LeftElevonActuator)
    {
        LeftElevonActuator->SetTargetAngle(Value);
    }
    else if (Topic.Contains(TEXT("elevon_right")) && RightElevonActuator)
    {
        RightElevonActuator->SetTargetAngle(Value);
    }
}
