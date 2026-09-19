// ModelFbxImporter.h
// FBX / Model İthalatçısı: Skeletal mesh modelini APiSimVehicle aktörüne bağlar; Scale, Pivot ve Yönelim dönüşümlerini yönetir.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ModelFbxImporter.generated.h"

class APiSimVehicle;
class USkeletalMesh;

UCLASS(BlueprintType)
class PISIM_API UPiSimModelFbxImporter : public UObject
{
    GENERATED_BODY()

public:
    UPiSimModelFbxImporter();

    UFUNCTION(BlueprintCallable, Category = "PiSim|Importer")
    static bool ApplySkeletalMeshToVehicle(APiSimVehicle* TargetVehicle, USkeletalMesh* InSkeletalMesh, float InScaleMultiplier = 1.0f, FRotator InRotationOffset = FRotator::ZeroRotator);
};
