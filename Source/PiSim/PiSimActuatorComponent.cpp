// PiSimActuatorComponent.cpp

#include "PiSimActuatorComponent.h"
#include "ProceduralMeshComponent.h"

UPiSimActuatorComponent::UPiSimActuatorComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    bWantsInitializeComponent = true;
}

void UPiSimActuatorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Live update of associated visual meshes
    if (Role == EPiSimMotorRole::Thruster)
    {
        // Pervanenin görsel dönüşü
        CurrentRPM = TargetNormalizedValue * MaxVelocityRPM;
        if (LinkedVisualMesh && FMath::Abs(CurrentRPM) > 0.1f)
        {
            float AngularSpeedDegPerSec = CurrentRPM * 6.0f; // 1 RPM = 6 deg/sec
            FVector LocalThrustAxis = BoneDirection.IsNearlyZero() ? FVector(0.0f, 1.0f, 0.0f) : BoneDirection;
            FQuat SpinQuat(LocalThrustAxis, FMath::DegreesToRadians(AngularSpeedDegPerSec * DeltaTime));
            LinkedVisualMesh->AddLocalRotation(SpinQuat);
        }
    }
    else if (Role == EPiSimMotorRole::ServoJoint)
    {
        // Servo mafsal (Elevon, dümen, aileron açısı)
        CurrentAngleDeg = FMath::FInterpTo(CurrentAngleDeg, TargetAngleDeg, DeltaTime, 15.0f);
        if (LinkedVisualMesh)
        {
            // Pitch ekseni (X ekseni etrafında kanatçık sapması)
            LinkedVisualMesh->SetRelativeRotation(FRotator(CurrentAngleDeg, 0.0f, 0.0f));
        }
    }
}

void UPiSimActuatorComponent::SetNormalizedCommand(float InValue)
{
    TargetNormalizedValue = FMath::Clamp(InValue, -1.0f, 1.0f);

    if (Role == EPiSimMotorRole::Thruster)
    {
        TargetRPM = TargetNormalizedValue * MaxVelocityRPM;
    }
    else if (Role == EPiSimMotorRole::ServoJoint || Role == EPiSimMotorRole::SteeredWheel)
    {
        TargetAngleDeg = FMath::Lerp(MinLimitDeg, MaxLimitDeg, (TargetNormalizedValue + 1.0f) * 0.5f);
    }
    else if (Role == EPiSimMotorRole::DriveWheel)
    {
        TargetRPM = TargetNormalizedValue * MaxVelocityRPM;
    }
}

void UPiSimActuatorComponent::SetTargetAngle(float InAngleDeg)
{
    TargetAngleDeg = FMath::Clamp(InAngleDeg, MinLimitDeg, MaxLimitDeg);
    TargetNormalizedValue = (MaxLimitDeg != MinLimitDeg) ? 
        ((TargetAngleDeg - MinLimitDeg) / (MaxLimitDeg - MinLimitDeg) * 2.0f - 1.0f) : 0.0f;
}

void UPiSimActuatorComponent::SetTargetVelocity(float InRPM)
{
    TargetRPM = FMath::Clamp(InRPM, -MaxVelocityRPM, MaxVelocityRPM);
    TargetNormalizedValue = (MaxVelocityRPM > 0.01f) ? (TargetRPM / MaxVelocityRPM) : 0.0f;
}

void UPiSimActuatorComponent::ApplyActuatorPhysics(float DeltaTime, UPrimitiveComponent* RootBody)
{
    if (!RootBody || !RootBody->IsSimulatingPhysics()) return;

    if (Role == EPiSimMotorRole::Thruster)
    {
        if (FMath::Abs(TargetNormalizedValue) > 0.001f)
        {
            float MaxThrustN = (MaxTorqueNm > 0.1f) ? MaxTorqueNm : 18.0f;
            float CommandedThrustN = TargetNormalizedValue * MaxThrustN;

            FVector LocalThrustDir = BoneDirection.IsNearlyZero() ? FVector(0.0f, 1.0f, 0.0f) : BoneDirection;
            if (bReverseDirection)
            {
                LocalThrustDir = -LocalThrustDir;
            }

            FVector WorldThrustDir = GetComponentTransform().TransformVectorNoScale(LocalThrustDir).GetSafeNormal();
            FVector ForceVecN = WorldThrustDir * CommandedThrustN;
            FVector ThrustForceUE = ForceVecN * 100.0f; // 1 N = 100 kg*cm/s² in Unreal units

            RootBody->AddForceAtLocation(ThrustForceUE, GetComponentLocation());
            CurrentOutputForceOrTorque = CommandedThrustN;
        }
        else
        {
            CurrentOutputForceOrTorque = 0.0f;
        }
    }
}
