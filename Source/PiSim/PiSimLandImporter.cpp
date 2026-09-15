// PiSimLandImporter.cpp

#include "PiSimLandImporter.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"

APiSimLandImporter::APiSimLandImporter()
{
    VehicleDomain = EVehicleDomain::Land;
}

void APiSimLandImporter::SetupDomainComponents()
{
    Super::SetupDomainComponents();

    if (VisualMeshComponents.Num() == 0 || !VisualMeshComponents[0]) return;

    UPrimitiveComponent* ChassisBody = VisualMeshComponents[0];
    ChassisBody->SetMassOverrideInKg(NAME_None, ChassisMassKg, true);
    ChassisBody->SetLinearDamping(0.8f);
    ChassisBody->SetAngularDamping(1.5f);

    UE_LOG(LogTemp, Warning, TEXT("🚗 [PISIM LAND IMPORTER] Şasi ve Tekerlek Fiziği Kuruldu. Kütle: %.1f kg"), ChassisMassKg);
}

void APiSimLandImporter::ApplyDomainPhysics(float DeltaTime)
{
    if (VisualMeshComponents.Num() == 0 || !VisualMeshComponents[0]) return;

    // =========================================================================================
    // 🔒 [DO NOT MODIFY - LAND IMPORTER WHEEL DRIVE LOCK]
    // Sürüş Tekerlekleri Roll Açısal Hız Güncellemesi
    // =========================================================================================
    for (int32 i = 1; i < VisualMeshComponents.Num(); ++i)
    {
        if (VisualMeshComponents[i] && VisualMeshComponents[i]->IsSimulatingPhysics())
        {
            FVector RelLoc = VisualMeshComponents[i]->GetRelativeLocation();
            float TotalRpm = (RelLoc.Y < 0.0f) ? LeftWheelsTargetRpm : RightWheelsTargetRpm;

            if (FMath::Abs(TotalRpm) > 0.001f)
            {
                float AngularSpeedDegPerSec = TotalRpm * 6.0f; // 1 RPM = 6 deg/sec
                FVector LocalAxle = FVector(1.0f, 0.0f, 0.0f); // Roll X axle
                FVector WorldAxle = VisualMeshComponents[i]->GetComponentTransform().TransformVectorNoScale(LocalAxle).GetSafeNormal();

                FVector CurrentAngVel = VisualMeshComponents[i]->GetPhysicsAngularVelocityInDegrees();
                FVector NonRollVel = CurrentAngVel - (WorldAxle * (CurrentAngVel | WorldAxle));
                FVector DesiredRollVel = WorldAxle * AngularSpeedDegPerSec;
                VisualMeshComponents[i]->SetPhysicsAngularVelocityInDegrees(NonRollVel + DesiredRollVel, false);
            }
        }
    }
}

void APiSimLandImporter::SetDifferentialDrive(float LeftRPM, float RightRPM)
{
    LeftWheelsTargetRpm = LeftRPM;
    RightWheelsTargetRpm = RightRPM;
}

void APiSimLandImporter::SetSteeringAngle(float InAngleDeg)
{
    SteerAngleDeg = FMath::Clamp(InAngleDeg, -45.0f, 45.0f);

    for (auto& Pair : SteerBones)
    {
        if (Pair.Value)
        {
            Pair.Value->SetRelativeRotation(FRotator(0.0f, SteerAngleDeg, 0.0f));
        }
    }
}
