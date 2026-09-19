// Baker.h
// PiSim Baker: Yapılan motor, mafsal ve ArduPilot kanal eşleşmelerini kalıcı hale getirip simülasyona gömer.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Baker.generated.h"

class APiSimVehicle;

UCLASS(BlueprintType)
class PISIM_API UPiSimBaker : public UObject
{
    GENERATED_BODY()

public:
    UPiSimBaker();

    // Aracın tüm kemik, motor ve kanal ayarlarını simülasyona hazır şekilde gömer
    UFUNCTION(BlueprintCallable, Category = "PiSim|Baker")
    static bool BakeVehicle(APiSimVehicle* TargetVehicle, FString& OutBakeSummary);
};
