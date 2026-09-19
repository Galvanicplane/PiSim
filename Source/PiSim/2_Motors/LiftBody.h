// LiftBody.h
// Aerodinamik / Hidrodinamik Kaldırma Gövdesi: Kanat, flap, rudder ve gövdelerin akış hızına göre kaldırma (Lift) ve sürükleme (Drag) kuvvetlerini hesaplar.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "IPiSimInspectableModule.h"
#include "LiftBody.generated.h"

UCLASS(ClassGroup=(PiSim), meta=(BlueprintSpawnableComponent))
class PISIM_API UPiSimLiftBodyComponent : public UActorComponent, public IPiSimInspectableModule
{
    GENERATED_BODY()

public:
    UPiSimLiftBodyComponent();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // IPiSimInspectableModule
    virtual void GetInspectableProperties(TArray<FPiSimPropertyDescriptor>& OutProperties) override;
    virtual void SetInspectablePropertyFloat(FName PropertyId, float NewValue) override;
    virtual void SetInspectablePropertyString(FName PropertyId, const FString& NewValue) override;
    virtual void TriggerInspectableAction(FName ActionId) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Lift")
    FName TargetBoneName = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Lift")
    float WingAreaSquareMeters = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Lift")
    float LiftSlope = 5.73f; // CL alpha (per radian)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Lift")
    float DragCoefficientZero = 0.02f; // CD0

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Lift")
    float FluidDensity = 1.225f; // kg/m^3 (Hava: 1.225, Su: 1000)

protected:
    UPROPERTY()
    class USkeletalMeshComponent* CachedSkeletalMesh = nullptr;

    void ApplyAerodynamicForces(float DeltaTime);
};
