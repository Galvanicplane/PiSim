// SaveManager.h
// Kayıt Yöneticisi: Bake edilmiş araç konfigürasyonunu kullanıcıdan isim alarak diske (JSON) kaydeder ve geri yükler.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SaveManager.generated.h"

class APiSimVehicle;

UCLASS(BlueprintType)
class PISIM_API UPiSimSaveManager : public UObject
{
    GENERATED_BODY()

public:
    UPiSimSaveManager();

    // Aracı verilen isimle Saved/Vehicles/{ProfileName}.json olarak kaydeder
    UFUNCTION(BlueprintCallable, Category = "PiSim|Save")
    static bool SaveProfile(APiSimVehicle* TargetVehicle, const FString& ProfileName, FString& OutMessage);

    // Kayıtlı profili disktan okuyup araca yükler
    UFUNCTION(BlueprintCallable, Category = "PiSim|Save")
    static bool LoadProfile(APiSimVehicle* TargetVehicle, const FString& ProfileName, FString& OutMessage);

    // Mevcut tüm profil isimlerini listeler
    UFUNCTION(BlueprintCallable, Category = "PiSim|Save")
    static void GetSavedProfileNames(TArray<FString>& OutProfileNames);

    // Aracı standart robotik URDF (XML) formatında Saved/Vehicles/{ProfileName}.urdf olarak dışa aktarır
    UFUNCTION(BlueprintCallable, Category = "PiSim|Save")
    static bool ExportURDF(APiSimVehicle* TargetVehicle, const FString& ProfileName, FString& OutMessage);

    // Aracı ArduPilot SITL / Donanım için Saved/Vehicles/{ProfileName}.param olarak dışa aktarır
    UFUNCTION(BlueprintCallable, Category = "PiSim|Save")
    static bool ExportArduPilotParams(APiSimVehicle* TargetVehicle, const FString& ProfileName, FString& OutMessage);
};
