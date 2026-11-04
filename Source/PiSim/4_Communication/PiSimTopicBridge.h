// PiSimTopicBridge.h
// Modern ROS 2 / Zenoh Topic Köprüsü:
// Her araç için dinamik olarak "/<vehicle_code>/imu", "/<vehicle_code>/gps" ve "/<vehicle_code>/joint_states"
// topic'lerini yayınlar; "/<vehicle_code>/actuators" ve "/<vehicle_code>/cmd_vel" topic'lerini dinler.
// Gelen komutları IPiSimActuatorInterface üzerinden araç motorlarına doğrudan iletir.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "IPiSimActuatorInterface.h"
#include "PiSimTopicBridge.generated.h"

class APiSimVehicle;

UCLASS(ClassGroup=(PiSim), meta=(BlueprintSpawnableComponent))
class PISIM_API UPiSimTopicBridge : public UActorComponent
{
    GENERATED_BODY()

public:
    UPiSimTopicBridge();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    /** Eyleyici topic'i üzerinden gelen motor komutunu araçtaki IPiSimActuatorInterface modüllerine dağıtır */
    UFUNCTION(BlueprintCallable, Category = "PiSim|ROS2")
    void DispatchActuatorsJson(const FString& InJsonStr);

    /** Eyleyici topic'i üzerinden gelen doğrudan ArduPilot PWM kanallarını (1..16) dağıtır */
    UFUNCTION(BlueprintCallable, Category = "PiSim|ROS2")
    void DispatchChannelsJson(const FString& InJsonStr);

    /** Belirtilen topic altına veri yayınlar (ROS 2 / Zenoh uyumlu) */
    UFUNCTION(BlueprintCallable, Category = "PiSim|ROS2")
    void PublishTopic(const FString& TopicSuffix, const FString& JsonPayload);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|ROS2")
    FString TopicPrefix; // Örn: "/rover_01"

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|ROS2")
    float SensorPublishRateHz = 20.0f; // 20 Hz ROS 2 sensör yayını

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|ROS2")
    int32 BridgePort = 7410; // Dinleme Portu (7410 + VehicleId)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|ROS2")
    FString TargetHost = TEXT("127.0.0.1");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|ROS2")
    int32 TargetPort = 7411; // Harici ROS 2 / Zenoh Bridge alıcı portu

protected:
    void InitSockets();
    void CloseSockets();
    void PollIncomingPackets();
    void PublishSensors();

    FSocket* ListenSocket = nullptr;
    FSocket* SendSocket = nullptr;
    float LastSensorPublishTime = 0.0f;

    UPROPERTY()
    APiSimVehicle* CachedVehicle = nullptr;
};
