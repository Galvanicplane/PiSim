// Track.h
// Paletli Araç Sürtünme ve Tahrik Bileşeni (Taslak)

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

    // IPiSimInspectableModule
    virtual void GetInspectableProperties(TArray<FPiSimPropertyDescriptor>& OutProperties) override {}
    virtual void SetInspectablePropertyFloat(FName PropertyId, float NewValue) override {}
    virtual void SetInspectablePropertyString(FName PropertyId, const FString& NewValue) override {}
    virtual void TriggerInspectableAction(FName ActionId) override {}

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Track")
    FName TargetBoneName = NAME_None;
};
