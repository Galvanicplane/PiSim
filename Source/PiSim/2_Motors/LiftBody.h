// LiftBody.h
// Aerodinamik Kaldırma Gövdesi: Kanat, elevon, flap ve gövdelerin akış hızına göre kaldırma (Lift), sürükleme (Drag) ve yunuslama momenti (Pitching Moment) kuvvetlerini hesaplar.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "IPiSimInspectableModule.h"
#include "IPiSimActuatorInterface.h"
#include "LiftBody.generated.h"

UCLASS(ClassGroup=(PiSim), meta=(BlueprintSpawnableComponent))
class PISIM_API UPiSimLiftBodyComponent : public UActorComponent, public IPiSimInspectableModule, public IPiSimActuatorInterface
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

    // IPiSimActuatorInterface (Temel modül kimlik sözleşmesi)
    virtual void SetActuatorNormalized(float Value) override { SetControlSurfaceDeflection(Value * 30.0f); }
    virtual void SetActuatorDirect(float PhysicalValue) override { SetControlSurfaceDeflection(PhysicalValue); }
    virtual void SetActuatorPWM(int32 InPWM) override { SetControlSurfaceDeflection(((InPWM - 1500) / 500.0f) * 30.0f); }
    virtual float GetActuatorNormalized() const override { return ControlSurfaceDeflectionDeg / 30.0f; }
    virtual FName GetTargetBoneName() const override { return TargetBoneName; }
    virtual void SetTargetBoneName(FName InBoneName) override { TargetBoneName = InBoneName; }
    virtual FString GetActuatorType() const override { return TEXT("LiftBody"); }
    virtual int32 GetArduPilotChannel() const override { return ServoChannel; }
    virtual void SetArduPilotChannel(int32 InChannel) override { ServoChannel = InChannel; }
    virtual FVector GetActuatorAxis() const override { return UpVector; }
    virtual void SetActuatorAxis(const FVector& InAxis) override { UpVector = InAxis; }
    virtual void InitializeModuleIdentity(class APiSimVehicle* InVehicle) override;
    virtual void HandleAckMessage(const FString& TopicName, int32 AckCode) override {}

    // @state: WIP - Modül içi araç kimliği
    UPROPERTY()
    class APiSimVehicle* ParentVehicle = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Identity")
    FString ParentVehicleName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Identity")
    int32 VehicleIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Identity")
    int32 ServoChannel = 0;

    UFUNCTION(BlueprintCallable, Category = "PiSim|Lift")
    void SetControlSurfaceDeflection(float InDeflectionDeg) { ControlSurfaceDeflectionDeg = InDeflectionDeg; }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Lift")
    FName TargetBoneName = NAME_None;

    /** Kullanıcı tarafından girilen kanat yukarı vektörü (UpVector) ve burun vektörü (ForwardVector) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Lift")
    FVector UpVector = FVector(0.0f, 0.0f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Lift")
    FVector ForwardVector = FVector(1.0f, 0.0f, 0.0f);

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
