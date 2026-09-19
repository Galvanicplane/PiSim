// MotorConfigurator.h
// Motor Yapılandırıcısı: İskeletteki kemik isimlerine göre otomatik motor rolü atar.
// UI seçimlerinde yeni motor/eyleyiciyi araca bağlar veya değiştirir. SADECE ATAMA YAPAR, KONTROL KODU İÇERMEZ.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "MotorConfigurator.generated.h"

class APiSimVehicle;
class UActorComponent;

UCLASS(BlueprintType)
class PISIM_API UPiSimMotorConfigurator : public UObject
{
    GENERATED_BODY()

public:
    UPiSimMotorConfigurator();

    // İskeletteki kemik isimlerine göre otomatik tarama ve varsayılan motor ataması yapar
    UFUNCTION(BlueprintCallable, Category = "PiSim|Configurator")
    static void AutoAssignMotorsByBoneNames(APiSimVehicle* TargetVehicle);

    // Kemiğe açıkça seçilen motor rolünü bağlar (Wheel, Servo, Propeller, LiftBody, None)
    UFUNCTION(BlueprintCallable, Category = "PiSim|Configurator")
    static UActorComponent* AssignMotorRoleToBone(APiSimVehicle* TargetVehicle, FName BoneName, const FString& RoleName);
};
