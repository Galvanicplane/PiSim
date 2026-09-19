// ModelFbxImporter.cpp

#include "ModelFbxImporter.h"
#include "Vehicle.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"

// @state: WIP - FBX Model İthalatçı nesne yapıcısı
UPiSimModelFbxImporter::UPiSimModelFbxImporter()
{
}

// @state: WIP - İskelet ağını araca bağlar, ölçek ve rotasyonu uygular
bool UPiSimModelFbxImporter::ApplySkeletalMeshToVehicle(APiSimVehicle* TargetVehicle, USkeletalMesh* InSkeletalMesh, float InScaleMultiplier, FRotator InRotationOffset)
{
    if (!TargetVehicle || !InSkeletalMesh)
    {
        return false;
    }

    if (TargetVehicle->VehicleSkeletalMesh)
    {
        TargetVehicle->VehicleSkeletalMesh->SetSkeletalMesh(InSkeletalMesh);
        TargetVehicle->VehicleSkeletalMesh->SetRelativeScale3D(FVector(InScaleMultiplier));
        TargetVehicle->VehicleSkeletalMesh->SetRelativeRotation(InRotationOffset);
        return true;
    }

    return false;
}
