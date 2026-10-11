// Track.h
// Paletli Araç Sürtünme ve Tahrik Bileşeni: Palet plakası sürtünmesi, çizgisel hız ve palet torkunu yönetir.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "IPiSimInspectableModule.h"
#include "Track.generated.h"

UCLASS(ClassGroup=(PiSim), meta=(BlueprintSpawnableComponent))
class PISIM_API UPiSimTrackComponent : public UActorComponent, public IPiSimInspectableModule
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

    UFUNCTION(BlueprintCallable, Category = "PiSim|Track")
    void SetTrackSpeed(float InSpeedMps) { TargetSpeedMps = InSpeedMps; }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Track")
    FName TargetBoneName = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Track")
    float TargetSpeedMps = 0.0f; // m/s

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Track")
    float CurrentSpeedMps = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Track")
    float FrictionCoefficient = 1.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Track")
    float TrackWidthCm = 30.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Track")
    int32 ArduPilotChannel = 3;
};
