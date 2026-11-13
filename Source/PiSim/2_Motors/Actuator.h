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
    virtual void SetActuatorNormalized(float Value) override { NormalizedCommand = FMath::Clamp(Value, -1.0f, 1.0f); ChannelTestSlider = NormalizedCommand; SetTargetStroke(((NormalizedCommand + 1.0f) * 0.5f) * MaxStrokeCm); }
    virtual void SetActuatorDirect(float PhysicalValue) override { SetTargetStroke(PhysicalValue); }
    virtual void SetActuatorPWM(int32 InPWM) override { float Norm = FMath::Clamp((InPWM - 1000) / 1000.0f, 0.0f, 1.0f); SetTargetStroke(Norm * MaxStrokeCm); }
    virtual float GetActuatorNormalized() const override { return (MaxStrokeCm > 0.0f) ? (CurrentStrokeCm / MaxStrokeCm) : 0.0f; }
    virtual FName GetTargetBoneName() const override { return TargetBoneName; }
    virtual void SetTargetBoneName(FName InBoneName) override { TargetBoneName = InBoneName; }
    virtual FString GetActuatorType() const override { return TEXT("Actuator"); }
    virtual int32 GetArduPilotChannel() const override { return ArduPilotChannel; }
    virtual void SetArduPilotChannel(int32 InChannel) override { ArduPilotChannel = InChannel; ServoChannel = InChannel; }
    virtual FVector GetActuatorAxis() const override { return ExtensionAxis; }
    virtual void SetActuatorAxis(const FVector& InAxis) override { ExtensionAxis = InAxis; }
    virtual void InitializeModuleIdentity(class APiSimVehicle* InVehicle) override;
    virtual void HandleAckMessage(const FString& TopicName, int32 AckCode) override {}

    // @state: WIP - Modül içi araç kimliği ve kanal kontrol değişkenleri
    UPROPERTY()
    class APiSimVehicle* ParentVehicle = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Identity")
    FString ParentVehicleName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Identity")
    int32 VehicleIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Identity")
    int32 ServoChannel = 5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Identity")
    float ChannelTestSlider = 0.0f; // -1.0 .. +1.0

    UFUNCTION(BlueprintCallable, Category = "PiSim|Actuator")
    void SetTargetStroke(float InStrokeCm) { TargetStrokeCm = FMath::Clamp(InStrokeCm, 0.0f, MaxStrokeCm); }

    /** Hedef birincil veya _a kemik adı (LA_<isim>_a veya R_<isim>_a) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Actuator")
    FName TargetBoneName = NAME_None;

    /** İki kemikli çekiş çifti: BoneA (LA_..._a / R_..._a) ve BoneB (LA_..._b / R_..._b) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Actuator")
    FName BoneAName = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Actuator")
    FName BoneBName = NAME_None;

    /** True ise Halat (R_ ön ekli) modudur: Mesafeyi sabit tutar, uzayıp kısalmaz */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Actuator")
    bool bIsRopeVariant = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Actuator")
    float RestDistanceCm = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Actuator")
    float MinDistanceCm = 20.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Actuator")
    float MaxDistanceCm = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Actuator")
    float HoldStiffness = 500.0f;

    /** Harici hareket komutu (-1.0: Çek/Kısalt, 0.0: Sabit Tut, +1.0: İt/Uzat) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Actuator")
    float NormalizedCommand = 0.0f;

    bool bHasCapturedInitialRestDistance = false;

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
