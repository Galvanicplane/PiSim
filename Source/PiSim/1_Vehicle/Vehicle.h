// Vehicle.h
// Ana Araç Pawn'ı: Saf iskelet (SkeletalMesh) ve dinamik parçalar, kemik slotları ve temel fizik yönetimini üstlenir.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "ModelFbxImporter.h"
#include "Vehicle.generated.h"

class USkeletalMeshComponent;
class UProceduralMeshComponent;
class USpringArmComponent;
class UCameraComponent;
class UPiSimTelemetryGateway;
class UPhysicsConstraintComponent;

USTRUCT(BlueprintType)
struct FPiSimBoneHierarchyNode
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Bones")
    FName BoneName = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Bones")
    FName ParentBoneName = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Bones")
    int32 IndentLevel = 0;
};

UCLASS(ClassGroup=(PiSim), BlueprintType, Blueprintable)
class PISIM_API APiSimVehicle : public APawn
{
    GENERATED_BODY()

public:
    APiSimVehicle();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // @state: WIP - Fiziği açıp kapatma fonksiyonu (Chaos parametreleri ve kütle uygulaması)
    UFUNCTION(BlueprintCallable, Category = "PiSim|Vehicle")
    void SetVehiclePhysicsEnabled(bool bEnable);

    // @state: WIP - Fiziğin gerçekten aktif olup olmadığını döndürür
    UFUNCTION(BlueprintCallable, Category = "PiSim|Vehicle")
    bool IsVehiclePhysicsActive() const;

    // @state: WIP - Görsel model ile UCX çarpışma zırhı arasındaki görünürlüğü değiştirir
    UFUNCTION(BlueprintCallable, Category = "PiSim|Vehicle")
    void SetCollisionMeshViewEnabled(bool bShowUCX);

    // @state: WIP - Çarpışma zırhı görünürlüğünün durumunu döner
    UFUNCTION(BlueprintCallable, Category = "PiSim|Vehicle")
    bool IsCollisionMeshViewEnabled() const { return bShowCollisionView; }

    // @state: WIP - Mevcut modeli, alt parçaları ve kemik kayıtlarını temizler
    UFUNCTION(BlueprintCallable, Category = "PiSim|Vehicle")
    void ClearVehicleModel();

    // @state: WIP - İthal edilen kemik listesini kaydeder
    void RegisterImportedBones(const TArray<FName>& InBoneNames);

    // @state: WIP - Hiyerarşik kemik ağacını kaydeder
    void RegisterImportedBoneHierarchy(const TArray<FPiSimBoneHierarchyNode>& InHierarchy);

    // @state: WIP - Hiyerarşik kemik ağacını döner
    const TArray<FPiSimBoneHierarchyNode>& GetVehicleBoneHierarchy() const { return VehicleBoneHierarchy; }

    // @state: WIP - Kemik ile fiziksel/görsel sahne bileşenini eşleştirir
    void RegisterBoneSceneComponent(FName BoneName, USceneComponent* SceneComp);

    // @state: WIP - Kemiğe bağlı sahne bileşenini döner (Servo/Tekerlek rotasyonu için)
    USceneComponent* GetSceneComponentForBone(FName BoneName) const;

    // Kemiğe bağlı veya çocuğundaki fiziksel ilkel bileşeni döner (Dinamik tekerlek fizik itkisi için)
    UPrimitiveComponent* GetPrimitiveComponentForBone(FName BoneName) const;

    // Kemiğe modül atama fonksiyonu (Wheel, Servo, Propeller vb.)
    UFUNCTION(BlueprintCallable, Category = "PiSim|Vehicle")
    bool AssignModuleToBone(FName BoneName, UActorComponent* ModuleComponent);

    // Kemiğe atanmış modülü alma
    UFUNCTION(BlueprintCallable, Category = "PiSim|Vehicle")
    UActorComponent* GetModuleAtBone(FName BoneName) const;

    // İskeletteki tüm kemik isimlerini listeler (UI Otopark için)
    UFUNCTION(BlueprintCallable, Category = "PiSim|Vehicle")
    void GetVehicleBoneNames(TArray<FName>& OutBoneNames) const;

    // Atanmış tüm modülleri harita olarak döner
    const TMap<FName, UActorComponent*>& GetBoneSlotRegistry() const { return BoneSlotRegistry; }

    // Canlı motor test kontrolü (UI Otopark Slider entegrasyonu)
    UFUNCTION(BlueprintCallable, Category = "PiSim|Motors")
    void SetMotorTestValue(FName BoneName, float Value);

    UFUNCTION(BlueprintCallable, Category = "PiSim|Motors")
    float GetMotorTestValue(FName BoneName) const;

    UFUNCTION(BlueprintCallable, Category = "PiSim|Motors")
    void ResetAllMotorTestValues();

    // Tekerlek 1-DOF serbestlik derecesi (DOF) yapılandırması
    void ConfigureConstraintDof(UPhysicsConstraintComponent* Constraint, int32 SectionIndex = 0);

    // Kesite ait kısıtlama bileşenini bulur (PiSimModelImporter pattern)
    UPhysicsConstraintComponent* FindConstraintForSection(int32 SectionIndex);

    // Kemiğe ait kısıtlama bileşenini bulur (Steer ve Tekerlek kemikleri)
    UPhysicsConstraintComponent* FindConstraintForBone(FName BoneName);

    // Kemiğe karşılık gelen görsel parça indeksini bulur (PiSimModelImporter pattern)
    UFUNCTION(BlueprintCallable, Category = "PiSim|Hierarchy")
    int32 FindVisualSectionForBone(FName BoneName) const;

    // Tüm tekerlek kısıtlamalarının serbestlik derecelerini (DOF) günceller (PiSimModelImporter pattern)
    UFUNCTION(BlueprintCallable, Category = "PiSim|Physics")
    void UpdateAllConstraintDofs();

    // Kayıtlı FBX parça kesitleri (Chaos ve UCX Convex zırhı için)
    UPROPERTY()
    TArray<FPiSimMeshSection> StoredVisualSections;

    UPROPERTY()
    TArray<FPiSimMeshSection> StoredUCXSections;

    // Canlı motor test değerleri: KemikAdı -> TestDeğeri (-1.0f .. +1.0f)
    UPROPERTY()
    TMap<FName, float> MotorTestValues;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Mesh")
    USkeletalMeshComponent* VehicleSkeletalMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Mesh")
    USkeletalMeshComponent* VisualSkeletalMesh;

    // Dinamik FBX görsel alt parçaları
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Mesh")
    TArray<UProceduralMeshComponent*> VisualMeshComponents;

    // Dinamik FBX UCX çarpışma zırhı parçaları (Görsel inceleme için)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Mesh")
    TArray<UProceduralMeshComponent*> CollisionMeshComponents;

    // Tekerlek eklem kısıtlamaları (Physics Constraints)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Physics")
    TArray<UPhysicsConstraintComponent*> JointConstraints;

    // Kesit İndeksi -> Kısıtlama Haritası (PiSimModelImporter pattern)
    UPROPERTY()
    TMap<int32, UPhysicsConstraintComponent*> SectionConstraintMap;

    // Kemik Adı -> Kısıtlama Haritası (PiSimModelImporter pattern)
    UPROPERTY()
    TMap<FName, UPhysicsConstraintComponent*> BoneConstraintMap;

    // Tekerlek İndeksi -> Steer Kemik Adı Eşleştirmesi (PiSimModelImporter pattern)
    UPROPERTY()
    TMap<int32, FString> WheelToSteerBoneMap;

    // Kemik Adı -> Sahne Bileşeni Haritası
    UPROPERTY()
    TMap<FName, USceneComponent*> BoneSceneComponents;

    // Sensör yerel montaj transform haritası
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Sensors")
    TMap<FString, FTransform> SensorSocketTransforms;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Camera")
    USpringArmComponent* CameraSpringArm;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Camera")
    UCameraComponent* OrbitCamera;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Gateway")
    UPiSimTelemetryGateway* TelemetryGateway;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Debug")
    class UPiSimMotorDebugComponent* MotorDebugComponent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Physics")
    float TotalMassKg = 25.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Physics")
    bool bStartWithPhysics = false;

    /** Chaos eylemsizlik momenti çarpanı (Küçük modellerin fazla hassas dönmesini engeller) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Physics", meta = (ClampMin = "0.1", ClampMax = "50.0"))
    float InertiaTensorScale = 1.0f;

    /** Ağırlık merkezi (CoG) ofseti (cm) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Physics")
    FVector CenterOfMassOffset = FVector::ZeroVector;

    /** Doğrusal sönümleme (Linear Damping) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Physics")
    float LinearDamping = 0.05f;

    /** Açısal sönümleme (Angular Damping) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Physics")
    float AngularDamping = 0.1f;

protected:
    UPROPERTY()
    TMap<FName, UActorComponent*> BoneSlotRegistry;

    UPROPERTY()
    TArray<FName> VehicleBoneNames;

    UPROPERTY()
    TArray<FPiSimBoneHierarchyNode> VehicleBoneHierarchy;

    UPROPERTY()
    bool bShowCollisionView = false;

    UPROPERTY()
    bool bIsPhysicsSimulating = false;
};
