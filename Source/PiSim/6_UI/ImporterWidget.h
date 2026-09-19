// ImporterWidget.h
// Model İthalat Arayüzü: Dosya seçimi, scale çarpanı ve otomatik motor atama tetikleyicisini barındırır.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ImporterWidget.generated.h"

class APiSimVehicle;
class USkeletalMesh;

UCLASS()
class PISIM_API UPiSimModularImporterWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "PiSim|Importer")
    bool ExecuteImport(APiSimVehicle* TargetVehicle, USkeletalMesh* InMesh, float ScaleMultiplier = 1.0f);
};
