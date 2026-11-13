// AutopilotBridge.h
// @state: WIP - Unreal Engine <-> ArduPilot SITL Köprüsü
// Aracın kanal-motor tanım dosyasını (.param / JSON) ArduPilot SITL'e gönderir,
// sensör durumlarını (IMU, GPS, Baro, Lidar) SITL'e aktarır ve PWM çıkışlarını modüllere dağıtır.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "AutopilotBridge.generated.h"

class APiSimVehicle;

UCLASS(ClassGroup=(PiSim), meta=(BlueprintSpawnableComponent))
class PISIM_API UPiSimAutopilotBridge : public UActorComponent
{
    GENERATED_BODY()

public:
    UPiSimAutopilotBridge();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // @state: WIP - Aracın kanal-motor eşleştirme tanımını JSON olarak üretir
    UFUNCTION(BlueprintCallable, Category = "PiSim|Autopilot")
    FString BuildChannelParameterJson() const;

    // @state: WIP - Kanal-motor tanımını ve .param eşleşmesini ArduPilot SITL köprüsüne gönderir
    UFUNCTION(BlueprintCallable, Category = "PiSim|Autopilot")
    void SyncParametersWithSitl();

    // @state: WIP - Tüm sensör verilerini birleştirip SITL durum paketine dönüştürerek gönderir
    UFUNCTION(BlueprintCallable, Category = "PiSim|Autopilot")
    void SendAggregatedSensorsToSitl();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Autopilot")
    FString SitlBridgeHost = TEXT("127.0.0.1");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Autopilot")
    int32 SitlSensorBridgePort = 7505;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Autopilot")
    int32 SitlPwmListenPort = 9003;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Autopilot")
    float SitlSyncRateHz = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Autopilot")
    bool bEnableSitlBridge = false;

protected:
    UPROPERTY()
    APiSimVehicle* ParentVehicle = nullptr;

    FSocket* SendSocket = nullptr;
    FSocket* ListenSocket = nullptr;
    double LastSyncTime = 0.0;

    void InitSockets();
    void CloseSockets();
    void PollSitlPwmResponses();
};

