// Track.h
// Paletli Araç Sürtünme ve Tahrik Bileşeni: Palet plakası sürtünmesi, çizgisel hız ve palet torkunu yönetir.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "IPiSimInspectableModule.h"
#include "IPiSimActuatorInterface.h"
#include "Track.generated.h"

UCLASS(ClassGroup=(PiSim), meta=(BlueprintSpawnableComponent))
class PISIM_API UPiSimTrackComponent : public UActorComponent, public IPiSimInspectableModule, public IPiSimActuatorInterface
{
    GENERATED_BODY()

public:
    UPiSimTrackComponent();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // IPiSimInspectableModule
    virtual void GetInspectableProperties(TArray<FPiSimPropertyDescriptor>& OutProperties) override;
    virtual void SetInspectablePropertyFloat(FName PropertyId, float NewValue) override;
    virtual void SetInspectablePropertyString(FName PropertyId, const FString& NewValue) override;
    virtual void TriggerInspectableAction(FName ActionId) override;

    // IPiSimActuatorInterface
    virtual void SetActuatorNormalized(float Value) override { ChannelTestSlider = FMath::Clamp(Value, -1.0f, 1.0f); TargetSpeedMps = ChannelTestSlider * MaxTrackSpeedMps; }
    virtual void SetActuatorDirect(float PhysicalValue) override { SetTrackSpeed(PhysicalValue); }
    virtual void SetActuatorPWM(int32 InPWM) override { float N = FMath::Clamp((InPWM - 1500) / 500.0f, -1.0f, 1.0f); SetActuatorNormalized(N); }
    virtual float GetActuatorNormalized() const override { return (MaxTrackSpeedMps > 0.01f) ? (TargetSpeedMps / MaxTrackSpeedMps) : 0.0f; }
    virtual FName GetTargetBoneName() const override { return TargetBoneName; }
    virtual void SetTargetBoneName(FName InBoneName) override { TargetBoneName = InBoneName; }
    virtual FString GetActuatorType() const override { return TEXT("Track"); }
    virtual int32 GetArduPilotChannel() const override { return ArduPilotChannel; }
    virtual void SetArduPilotChannel(int32 InChannel) override { ArduPilotChannel = InChannel; ServoChannel = InChannel; }
    virtual FVector GetActuatorAxis() const override { return TrackForwardAxis; }
    virtual void SetActuatorAxis(const FVector& InAxis) override { TrackForwardAxis = InAxis; }
    virtual void InitializeModuleIdentity(class APiSimVehicle* InVehicle) override;
    virtual void HandleAckMessage(const FString& TopicName, int32 AckCode) override {}
    void HandleControlTopic(const FString& InVehicleCode, const FString& TopicName, const FString& SourceType, const TSharedPtr<class FJsonObject>& PayloadObj);

    // @state: WIP - Modül içi araç kimliği ve kanal testi değişkenleri
    UPROPERTY()
    class APiSimVehicle* ParentVehicle = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Identity")
    FString ParentVehicleName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Identity")
    int32 VehicleIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Identity")
    int32 ServoChannel = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Identity")
    float ChannelTestSlider = 0.0f;

    UFUNCTION(BlueprintCallable, Category = "PiSim|Track")
    void SetTrackSpeed(float InSpeedMps) { TargetSpeedMps = InSpeedMps; }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Track")
    FName TargetBoneName = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Track")
    bool bReverseDirection = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Track")
    float MaxTrackSpeedMps = 8.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Track")
    float ContactTraceDistanceCm = 35.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Track")
    bool bGroundContact = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Track")
    FVector TrackForwardAxis = FVector(1.0f, 0.0f, 0.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Track")
    float TargetSpeedMps = 0.0f; // m/s

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Track")
    float CurrentSpeedMps = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Track")
    float FrictionCoefficient = 1.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Track")
    float TrackWidthCm = 30.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Track")
    int32 ArduPilotChannel = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Track")
    bool bManualOverride = false;

protected:
    double LastArduPilotPwmTime = -100.0;
    double LastControlTopicTime = -100.0;
    float TopicWatchdogTimeoutSec = 0.6f;
    bool bTopicBridgeBound = false;
};
