// Actuator.h
// Lineer Hidrolik / Elektrikli Piston Eyleyici Bileşeni: Piston strok mesafesi (cm), itme kuvveti (N) ve konum kontrolünü yönetir.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "IPiSimInspectableModule.h"
#include "IPiSimActuatorInterface.h"
#include "Actuator.generated.h"

UCLASS(ClassGroup=(PiSim), meta=(BlueprintSpawnableComponent))
class PISIM_API UPiSimLinearActuatorComponent : public UActorComponent, public IPiSimInspectableModule, public IPiSimActuatorInterface
{
    GENERATED_BODY()

public:
    UPiSimLinearActuatorComponent();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // IPiSimInspectableModule
    virtual void GetInspectableProperties(TArray<FPiSimPropertyDescriptor>& OutProperties) override;
    virtual void SetInspectablePropertyFloat(FName PropertyId, float NewValue) override;
    virtual void SetInspectablePropertyString(FName PropertyId, const FString& NewValue) override;
    virtual void TriggerInspectableAction(FName ActionId) override;

    // IPiSimActuatorInterface
    virtual void SetActuatorNormalized(float Value) override { SetTargetStroke(FMath::Clamp(Value, 0.0f, 1.0f) * MaxStrokeCm); }
    virtual void SetActuatorDirect(float PhysicalValue) override { SetTargetStroke(PhysicalValue); }
    virtual void SetActuatorPWM(int32 InPWM) override { float Norm = FMath::Clamp((InPWM - 1000) / 1000.0f, 0.0f, 1.0f); SetTargetStroke(Norm * MaxStrokeCm); }
    virtual float GetActuatorNormalized() const override { return (MaxStrokeCm > 0.0f) ? (CurrentStrokeCm / MaxStrokeCm) : 0.0f; }
    virtual FName GetTargetBoneName() const override { return TargetBoneName; }
    virtual void SetTargetBoneName(FName InBoneName) override { TargetBoneName = InBoneName; }
    virtual FString GetActuatorType() const override { return TEXT("Actuator"); }
    virtual int32 GetArduPilotChannel() const override { return ArduPilotChannel; }
    virtual void SetArduPilotChannel(int32 InChannel) override { ArduPilotChannel = InChannel; }
    virtual FVector GetActuatorAxis() const override { return ExtensionAxis; }
    virtual void SetActuatorAxis(const FVector& InAxis) override { ExtensionAxis = InAxis; }

    UFUNCTION(BlueprintCallable, Category = "PiSim|Actuator")
    void SetTargetStroke(float InStrokeCm) { TargetStrokeCm = FMath::Clamp(InStrokeCm, 0.0f, MaxStrokeCm); }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Actuator")
    FName TargetBoneName = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Actuator")
    float MaxStrokeCm = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Actuator")
    float TargetStrokeCm = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Actuator")
    float CurrentStrokeCm = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Actuator")
    float StrokeSpeedCmPerSec = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Actuator")
    float MaxPushForceNewtons = 500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Actuator")
    int32 ArduPilotChannel = 5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Actuator")
    FVector ExtensionAxis = FVector(0.0f, 0.0f, 1.0f); // Varsayılan Z ekseni
};
