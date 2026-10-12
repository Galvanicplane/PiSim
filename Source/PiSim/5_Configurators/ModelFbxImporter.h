// ModelFbxImporter.h
// FBX İthalatçısı: Binary FBX parser, mesh/UCX/sensör parçalama, APiSimVehicle aktörüne bağlama.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ModelFbxImporter.generated.h"

class APiSimVehicle;
class USkeletalMesh;

USTRUCT(BlueprintType)
struct FPiSimImporterMaterialSlot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Material")
    FLinearColor Color = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Material")
    float Metallic = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Material")
    float Roughness = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Material")
    float Specular = 0.5f;

    TArray<int32> Triangles;
};

USTRUCT(BlueprintType)
struct FPiSimMeshSection
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Mesh")
    FString MeshName = TEXT("");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Mesh")
    int32 ParentSectionIndex = -1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Mesh")
    FVector PivotPoint = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Mesh")
    FVector BoneDirection = FVector(0.0f, 1.0f, 0.0f);

    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FPiSimImporterMaterialSlot> MaterialSlots;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Mesh")
    FLinearColor MaterialColor = FLinearColor::White;

    float Metallic = 0.0f;
    float Roughness = 0.5f;
    float Specular = 0.5f;
};

USTRUCT(BlueprintType)
struct FPiSimSensorSection
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Sensor")
    FString SensorName = TEXT("");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Sensor")
    FVector PivotPoint = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Sensor")
    FRotator Rotation = FRotator::ZeroRotator;
};

struct FPiSimBoneHierarchyNode;

UCLASS(BlueprintType)
class PISIM_API UPiSimModelFbxImporter : public UObject
{
    GENERATED_BODY()

public:
    UPiSimModelFbxImporter();

    // @state: WIP - Diskteki Saved/Robots/Cache klasöründeki mevcut FBX dosyalarını listeler
    UFUNCTION(BlueprintCallable, Category = "PiSim|Importer")
    static TArray<FString> GetAvailableCacheModels();

    // @state: WIP - İkili FBX dosyasını bayt bayt okuyup görsel, UCX çarpışma ve sensör bölümlerine ayrıştırır
    static bool ParseBinaryFbxFile(
        const FString& FilePath,
        TArray<FPiSimMeshSection>& OutVisual,
        TArray<FPiSimMeshSection>& OutUCX,
        TArray<FPiSimSensorSection>& OutSensors,
        TArray<FString>& OutPureBones,
        TArray<struct FPiSimBoneHierarchyNode>& OutBoneHierarchy,
        float Scale = 1.0f,
        bool bEnableSmoothNormals = true
    );

    // @state: WIP - FBX dosyasını APiSimVehicle aktörüne yükler, parçaları oluşturur ve otomatik motor atamasını tetikler
    UFUNCTION(BlueprintCallable, Category = "PiSim|Importer")
    static bool ImportFbxFileToVehicle(
        APiSimVehicle* TargetVehicle,
        const FString& FilePath,
        float Scale = 1.0f,
        bool bEnableSmoothNormals = true
    );

    // @state: WIP - Hazır USkeletalMesh varlığını araca bağlar
    UFUNCTION(BlueprintCallable, Category = "PiSim|Importer")
    static bool ApplySkeletalMeshToVehicle(
        APiSimVehicle* TargetVehicle,
        USkeletalMesh* InSkeletalMesh,
        float InScaleMultiplier = 1.0f,
        FRotator InRotationOffset = FRotator::ZeroRotator
    );
};
