// Servo.h
// Açısal Eylem & Kontrol Yüzeyi Servosu: Kemiklerin açısal limitli hareketini, ArduPilot PWM ve Elevon/Rudder mikserini yönetir.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "IPiSimInspectableModule.h"
#include "IPiSimActuatorInterface.h"
#include "Servo.generated.h"

UCLASS(ClassGroup=(PiSim), meta=(BlueprintSpawnableComponent))
class PISIM_API UPiSimServoComponent : public UActorComponent, public IPiSimInspectableModule, public IPiSimActuatorInterface
{
    GENERATED_BODY()

public:
    UPiSimServoComponent();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // IPiSimInspectableModule
    virtual void GetInspectableProperties(TArray<FPiSimPropertyDescriptor>& OutProperties) override;
    virtual void SetInspectablePropertyFloat(FName PropertyId, float NewValue) override;
    virtual void SetInspectablePropertyString(FName PropertyId, const FString& NewValue) override;
    virtual void TriggerInspectableAction(FName ActionId) override;

    // IPiSimActuatorInterface
    virtual void SetActuatorNormalized(float Value) override;
    virtual void SetActuatorDirect(float PhysicalValue) override { SetTargetAngle(PhysicalValue); }
    virtual void SetActuatorPWM(int32 InPWM) override { SetFromPWM(InPWM); }
    virtual float GetActuatorNormalized() const override { return (MaxAngleDeg > 0.0f) ? (CurrentAngleDeg / MaxAngleDeg) : 0.0f; }
    virtual FName GetTargetBoneName() const override { return TargetBoneName; }
    virtual void SetTargetBoneName(FName InBoneName) override { TargetBoneName = InBoneName; }
    virtual FString GetActuatorType() const override { return TEXT("Servo"); }
    virtual int32 GetArduPilotChannel() const override { return ArduPilotChannel; }
    virtual void SetArduPilotChannel(int32 InChannel) override { ArduPilotChannel = InChannel; }
    virtual FVector GetActuatorAxis() const override { return LocalRotationAxis; }
    virtual void SetActuatorAxis(const FVector& InAxis) override { LocalRotationAxis = InAxis; }

    UFUNCTION(BlueprintCallable, Category = "PiSim|Servo")
    void SetTargetAngle(float InAngleDeg);

    UFUNCTION(BlueprintCallable, Category = "PiSim|Servo")
    void SetFromPWM(int32 InPWM);

    UFUNCTION(BlueprintCallable, Category = "PiSim|Servo")
    void SetPitchRollMixing(float PitchNorm, float RollNorm);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Servo")
    FName TargetBoneName = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Servo")
    float MinAngleDeg = -45.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Servo")
    float MaxAngleDeg = 45.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Servo")
    float TargetAngleDeg = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Servo")
    float CurrentAngleDeg = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Servo")
    float SpeedDegPerSec = 240.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Servo")
    int32 ArduPilotChannel = 1; // 1 - 16

    /** Servo Rolü: Aileron, Elevator, Rudder, Elevon_Left, Elevon_Right, Flap, Steer */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Servo")
    FString ArduPilotFunction = TEXT("Aileron");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Servo")
    FVector LocalRotationAxis = FVector(0.0f, 0.0f, 1.0f); // Varsayılan Z Yaw ekseni

    /** Sürekli Açı Salınım Testi (MinAngleDeg ve MaxAngleDeg arasında durmaksızın kemiği döndürür) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Servo")
    bool bContinuousSweepTest = false; // [KULLANICI TALEBI] Otonom donusu bozmamasi icin varsayilan KAPALIDIR

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Servo")
    float SweepCycleSpeed = 1.0f;

    /** Manuel UI testi açıksa gelen PWM komutları değeri ezmez */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Servo")
    bool bManualOverride = false;

protected:
    UPROPERTY()
    class USkeletalMeshComponent* CachedSkeletalMesh = nullptr;

    UPROPERTY()
    class USceneComponent* TargetSceneComponent = nullptr;

    FRotator RestRotation = FRotator::ZeroRotator;
    bool bHasRestRotation = false;

    float SweepTestTimer = 0.0f;
    float LastSweepDebugLogTime = 0.0f;

    void ApplyServoPhysics(float DeltaTime);
};
