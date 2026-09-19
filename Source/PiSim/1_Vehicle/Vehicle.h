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

    // Fiziği açıp kapatma fonksiyonu
    UFUNCTION(BlueprintCallable, Category = "PiSim|Vehicle")
    void SetVehiclePhysicsEnabled(bool bEnable);

    // Kemiğe modül atama fonksiyonu (Wheel, Servo, Propeller vb.)
    UFUNCTION(BlueprintCallable, Category = "PiSim|Vehicle")
    bool AssignModuleToBone(FName BoneName, UActorComponent* ModuleComponent);

    // Kemiğe atanmış modülü alma
    UFUNCTION(BlueprintCallable, Category = "PiSim|Vehicle")
    UActorComponent* GetModuleAtBone(FName BoneName) const;

    // İskeletteki tüm kemik isimlerini listeler (UI Otopark için)
    UFUNCTION(BlueprintCallable, Category = "PiSim|Vehicle")
    void GetVehicleBoneNames(TArray<FName>& OutBoneNames) const;

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

protected:
    UPROPERTY()
    TMap<FName, UActorComponent*> BoneSlotRegistry;
};
