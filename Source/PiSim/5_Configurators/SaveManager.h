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
};
