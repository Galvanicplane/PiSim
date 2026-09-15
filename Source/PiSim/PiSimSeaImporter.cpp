// PiSimSeaImporter.cpp

#include "PiSimSeaImporter.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"

APiSimSeaImporter::APiSimSeaImporter()
{
    VehicleDomain = EVehicleDomain::Sea;
}

void APiSimSeaImporter::SetupDomainComponents()
{
    Super::SetupDomainComponents();

    if (VisualMeshComponents.Num() == 0 || !VisualMeshComponents[0]) return;

    UPrimitiveComponent* HullBody = VisualMeshComponents[0];
    HullBody->SetLinearDamping(1.2f);
    HullBody->SetAngularDamping(2.0f);

    UE_LOG(LogTemp, Warning, TEXT("⚓ [PISIM SEA IMPORTER] Tekne/Gemi Gövdesi ('hull') ve Hidrodinamik Fiziği Kuruldu."));
}

void APiSimSeaImporter::ApplyDomainPhysics(float DeltaTime)
{
    if (VisualMeshComponents.Num() == 0 || !VisualMeshComponents[0]) return;
    UPrimitiveComponent* HullBody = VisualMeshComponents[0];
    if (!HullBody || !HullBody->IsSimulatingPhysics()) return;

    FVector HullLoc = HullBody->GetComponentLocation();

    // 1) Arşimet Kaldırma Kuvveti (Su seviyesinin altına indikçe yukarı itki)
    float SubmergedDepthCm = WaterPlaneZ - HullLoc.Z;
    if (SubmergedDepthCm > 0.0f)
    {
        float SubmergedFraction = FMath::Clamp(SubmergedDepthCm / 50.0f, 0.0f, 1.0f);
        float BuoyancyForceN = SubmergedFraction * DisplacementVolumeM3 * WaterDensityKgM3 * 9.81f;
        FVector BuoyancyUE = FVector(0.0f, 0.0f, BuoyancyForceN * 100.0f);

        HullBody->AddForce(BuoyancyUE);

        // Su direnci sönümleme
        FVector LinearVel = HullBody->GetPhysicsLinearVelocity();
        HullBody->AddForce(-LinearVel * LinearHydroDrag);

        FVector AngularVel = HullBody->GetPhysicsAngularVelocityInRadians();
        HullBody->AddTorqueInRadians(-AngularVel * AngularHydroDrag * 1000.0f);
    }

    // 2) Deniz İtki Pervanesi ve Dümen
    if (FMath::Abs(ThrusterCommand) > 0.001f)
    {
        FRotator ThrustRot = HullBody->GetComponentRotation() + FRotator(0.0f, RudderAngleDeg, 0.0f);
        FVector ForwardThrustDir = ThrustRot.Vector();
        float MaxThrustN = 50.0f; // 50 N deniz motoru itkisi
        FVector ThrustUE = ForwardThrustDir * (ThrusterCommand * MaxThrustN * 100.0f);

        HullBody->AddForce(ThrustUE);
    }
}

void APiSimSeaImporter::SetMarineThrottle(float InThrottle)
{
    ThrusterCommand = FMath::Clamp(InThrottle, -1.0f, 1.0f);
}

void APiSimSeaImporter::SetRudderAngle(float InAngleDeg)
{
    RudderAngleDeg = FMath::Clamp(InAngleDeg, -35.0f, 35.0f);
}
