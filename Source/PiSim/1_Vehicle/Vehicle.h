// Vehicle.h
// Ana Araç Pawn'ı: Garaj veya ithalat kodlarını İÇERMEZ. Saf iskelet (SkeletalMesh), kemik slotları ve temel fizik yönetimini üstlenir.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Vehicle.generated.h"

class USkeletalMeshComponent;
class USpringArmComponent;
class UCameraComponent;
class UPiSimTelemetryGateway;

UCLASS(ClassGroup=(PiSim), BlueprintType, Blueprintable)
class PISIM_API APiSimVehicle : public APawn
{
    GENERATED_BODY()

public:
    APiSimVehicle();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // Fiziği açıp kapatma fonksiyonu (Geçerli Physics Asset ve Body Setup kontrolü ile)
    UFUNCTION(BlueprintCallable, Category = "PiSim|Vehicle")
    void SetVehiclePhysicsEnabled(bool bEnable);

    // Fiziğin gerçekten aktif olup olmadığını döndürür
    UFUNCTION(BlueprintCallable, Category = "PiSim|Vehicle")
    bool IsVehiclePhysicsActive() const;

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

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Mesh")
    USkeletalMeshComponent* VehicleSkeletalMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Mesh")
    USkeletalMeshComponent* VisualSkeletalMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Camera")
    USpringArmComponent* CameraSpringArm;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Camera")
    UCameraComponent* OrbitCamera;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Gateway")
    UPiSimTelemetryGateway* TelemetryGateway;

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
};
