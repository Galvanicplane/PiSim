// PiSimLandImporter.h
// Specialized Land Vehicle Importer and Ground Physics Controller.
// Encapsulates Locked Wheel Constraints, 1-DOF Roll, Steer Yaw, and Differential Drive.

#pragma once

#include "CoreMinimal.h"
#include "PiSimModelImporterBase.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "PiSimLandImporter.generated.h"

UCLASS()
class PISIM_API APiSimLandImporter : public APiSimModelImporterBase
{
    GENERATED_BODY()

public:
    APiSimLandImporter();

    virtual void SetupDomainComponents() override;
    virtual void ApplyDomainPhysics(float DeltaTime) override;

    // --- Ground Vehicle Configuration ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Land")
    float ChassisMassKg = 30.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Land")
    float LeftWheelsTargetRpm = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Land")
    float RightWheelsTargetRpm = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Land")
    float SteerAngleDeg = 0.0f;

    // --- Constraints & Sockets ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Land")
    TArray<UPhysicsConstraintComponent*> WheelJointConstraints;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Land")
    TMap<FString, USceneComponent*> SteerBones;

    // --- Control Methods ---
    UFUNCTION(BlueprintCallable, Category = "PiSim|Land")
    void SetDifferentialDrive(float LeftRPM, float RightRPM);

    UFUNCTION(BlueprintCallable, Category = "PiSim|Land")
    void SetSteeringAngle(float InAngleDeg);
};
