// LiftBody.h
// Aerodinamik Kaldırma Gövdesi: Kanat, elevon, flap ve gövdelerin akış hızına göre kaldırma (Lift), sürükleme (Drag) ve yunuslama momenti (Pitching Moment) kuvvetlerini hesaplar.

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

    UFUNCTION(BlueprintCallable, Category = "PiSim|Lift")
    void SetControlSurfaceDeflection(float InDeflectionDeg) { ControlSurfaceDeflectionDeg = InDeflectionDeg; }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Lift")
    FName TargetBoneName = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Lift")
    float WingAreaSquareMeters = 0.26f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Lift")
    float WingspanMeters = 1.15f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Lift")
    float CL0 = 0.18f; // Sıfır hücum açısı kaldırma katsayısı

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Lift")
    float LiftSlope = 5.5f; // CL alpha (radyan başına)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Lift")
    float DragCoefficientZero = 0.020f; // CD0

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Lift")
    float StallAngleDeg = 15.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Lift")
    float ElevonEffectiveness = 0.50f; // Elevon kontrol etkinliği (tau)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Lift")
    float FluidDensity = 1.225f; // kg/m^3

    /** Bağlı kontrol yüzeyinin anlık sapma açısı (derece) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Lift")
    float ControlSurfaceDeflectionDeg = 0.0f;

    // Canlı Telemetri Çıktıları
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Lift|State")
    float CurrentLiftNewtons = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Lift|State")
    float CurrentDragNewtons = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Lift|State")
    float CurrentAirspeedKmh = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Lift|State")
    float CurrentAoADeg = 0.0f;

protected:
    UPROPERTY()
    class USkeletalMeshComponent* CachedSkeletalMesh = nullptr;

    void ApplyAerodynamicForces(float DeltaTime);
};
