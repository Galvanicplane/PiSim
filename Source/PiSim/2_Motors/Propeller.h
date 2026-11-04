// Propeller.h
// İtki ve Pervane Mikro Modülü: Motor devri (RPM), tork ve ileri itki (Thrust Newton) fiziğini yönetir.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "IPiSimInspectableModule.h"
#include "IPiSimActuatorInterface.h"
#include "Propeller.generated.h"

UCLASS(ClassGroup=(PiSim), meta=(BlueprintSpawnableComponent))
class PISIM_API UPiSimPropellerComponent : public UActorComponent, public IPiSimInspectableModule, public IPiSimActuatorInterface
{
    GENERATED_BODY()

public:
    UPiSimPropellerComponent();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // IPiSimInspectableModule
    virtual void GetInspectableProperties(TArray<FPiSimPropertyDescriptor>& OutProperties) override;
    virtual void SetInspectablePropertyFloat(FName PropertyId, float NewValue) override;
    virtual void SetInspectablePropertyString(FName PropertyId, const FString& NewValue) override;
    virtual void TriggerInspectableAction(FName ActionId) override;

    // IPiSimActuatorInterface
    virtual void SetActuatorNormalized(float Value) override { SetThrottle(FMath::Clamp(Value, 0.0f, 1.0f)); }
    virtual void SetActuatorDirect(float PhysicalValue) override { SetThrottle(FMath::Clamp(PhysicalValue / FMath::Max(1.0f, MaxRPM), 0.0f, 1.0f)); }
    virtual void SetActuatorPWM(int32 InPWM) override { SetFromPWM(InPWM); }
    virtual float GetActuatorNormalized() const override { return Throttle; }
    virtual FName GetTargetBoneName() const override { return TargetBoneName; }
    virtual void SetTargetBoneName(FName InBoneName) override { TargetBoneName = InBoneName; }
    virtual FString GetActuatorType() const override { return TEXT("Propeller"); }
    virtual int32 GetArduPilotChannel() const override { return ArduPilotChannel; }
    virtual void SetArduPilotChannel(int32 InChannel) override { ArduPilotChannel = InChannel; }
    virtual FVector GetActuatorAxis() const override { return LocalThrustDirection; }
    virtual void SetActuatorAxis(const FVector& InAxis) override { LocalThrustDirection = InAxis; }

    UFUNCTION(BlueprintCallable, Category = "PiSim|Propeller")
    void SetThrottle(float InThrottleNormalized); // 0.0 .. 1.0

    UFUNCTION(BlueprintCallable, Category = "PiSim|Propeller")
    void SetFromPWM(int32 InPWM); // 1000 .. 2000 us

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Propeller")
    FName TargetBoneName = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Propeller")
    float Throttle = 0.0f; // 0.0 .. 1.0

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Propeller")
    float MaxThrustNewtons = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Propeller")
    float MaxRPM = 8000.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Propeller")
    float CurrentRPM = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Propeller")
    float CurrentThrustNewtons = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Propeller")
    int32 ArduPilotChannel = 3; // Varsayılan Gaz kanalı (CH3)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Propeller")
    FVector LocalThrustDirection = FVector(1.0f, 0.0f, 0.0f); // Varsayılan X ileri

    /** İtki yönünü tersine çevir (Pusher / Çekici motor dönüşümü) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Propeller")
    bool bReverseThrust = false;

    /** Manuel UI slider testi açıksa ağ paketleri değeri ezmez */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Propeller")
    bool bManualOverride = false;

protected:
    UPROPERTY()
    class USkeletalMeshComponent* CachedSkeletalMesh = nullptr;

    UPROPERTY()
    class USceneComponent* TargetSceneComponent = nullptr;

    void ApplyThrustPhysics(float DeltaTime);
};
