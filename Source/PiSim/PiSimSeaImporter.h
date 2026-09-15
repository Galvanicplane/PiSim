// PiSimSeaImporter.h
// Specialized Marine / Sea Vehicle Importer and Hydrodynamics Controller.
// Root Bone: 'hull', 'boat_hull', 'keel', 'usv'.
// Encapsulates Archimedes Buoyancy, Hydrodynamic Drag, Marine Thrusters, and Rudder.

#pragma once

#include "CoreMinimal.h"
#include "PiSimModelImporterBase.h"
#include "PiSimSeaImporter.generated.h"

UCLASS()
class PISIM_API APiSimSeaImporter : public APiSimModelImporterBase
{
    GENERATED_BODY()

public:
    APiSimSeaImporter();

    virtual void SetupDomainComponents() override;
    virtual void ApplyDomainPhysics(float DeltaTime) override;

    // --- Marine Hydrodynamics Configuration ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Sea")
    float WaterPlaneZ = 0.0f; // Su yüzeyi Z koordinatı (Unreal cm)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Sea")
    float DisplacementVolumeM3 = 0.05f; // Batma hacmi (m³)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Sea")
    float WaterDensityKgM3 = 1000.0f; // Su yoğunluğu (1000 kg/m³)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Sea")
    float LinearHydroDrag = 150.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Sea")
    float AngularHydroDrag = 250.0f;

    // --- Control Inputs ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Sea")
    float ThrusterCommand = 0.0f; // [-1.0 - +1.0]

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Sea")
    float RudderAngleDeg = 0.0f; // [-35° - +35°]

    // --- Control Methods ---
    UFUNCTION(BlueprintCallable, Category = "PiSim|Sea")
    void SetMarineThrottle(float InThrottle);

    UFUNCTION(BlueprintCallable, Category = "PiSim|Sea")
    void SetRudderAngle(float InAngleDeg);
};
