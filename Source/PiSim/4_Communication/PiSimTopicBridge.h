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
#include "Dom/JsonObject.h"
#include "IPiSimActuatorInterface.h"
#include "PiSimTopicBridge.generated.h"

class APiSimVehicle;

/** @state: WIP - Modüllerin Unreal Zenoh Bridge üzerinden gelen ACK mesajlarını doğrudan dinlemesi için multicast delege */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnPiSimBridgeAckReceived, const FString& /*VehicleCode*/, const FString& /*TopicName*/, int32 /*AckCode*/);

/** @state: WIP - Her motor modülünün kendi içinden kontrol topiclerini (ArduPilot PWM öncelikli, yoksa ROS kontrol topiği) dinlemesi için delege */
DECLARE_MULTICAST_DELEGATE_FourParams(FOnPiSimControlTopicReceived, const FString& /*VehicleCode*/, const FString& /*TopicName*/, const FString& /*SourceType*/, const TSharedPtr<FJsonObject>& /*PayloadObj*/);

UENUM(BlueprintType)
enum class EPiSimTopicControlMode : uint8
{
    Auto            UMETA(DisplayName = "Otomatik (PWM Öncelikli)"),
    DirectActuator  UMETA(DisplayName = "Doğrudan Aktüatör (-1.0 .. +1.0)"),
    PWMChannels     UMETA(DisplayName = "ArduPilot PWM (1000..2000 us)"),
    CmdVel          UMETA(DisplayName = "Kinematik Twist (cmd_vel)")
};

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

    /** Eyleyici topic'i üzerinden gelen doğrudan ArduPilot PWM/Joystick kanallarını (1: Tekerlek, 2: Steer Servo...) dağıtır */
    UFUNCTION(BlueprintCallable, Category = "PiSim|ROS2")
    void DispatchChannelsJson(const FString& InJsonStr);

    /** Karşı taraftan (Pi 5 / Dashboard) gelen ACK ve Kamera Tetikleme (3=Aç, 4=Standby Mavi Ekran) mesajlarını işler */
    UFUNCTION(BlueprintCallable, Category = "PiSim|ROS2")
    void DispatchAckJson(const FString& InJsonStr);

    /** Aracın kendi topic sağlık ve ACK tablosunu (/ack) yayınlar (0=Yok, 1=Bozuk, 2=Sağlıklı, 3=Trigger, 4=Standby) */
    UFUNCTION(BlueprintCallable, Category = "PiSim|ROS2")
    void PublishAckStatus();

    /** Topic bazlı anlık ACK durum haritası (Debug ve UI gösterimi için) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|ROS2")
    TMap<FString, int32> TopicAckStatusMap;

    /** Her modülün kendi içinde Bridge ACK yayınını doğrudan dinleyebilmesi için event akışı */
    FOnPiSimBridgeAckReceived OnAckReceived;

    /** Her motor modülünün kendi içinden kontrol topic'lerini (ArduPilot PWM / ROS) dinlemesi için event akışı */
    FOnPiSimControlTopicReceived OnControlTopicReceived;

    /** Gelen ham kontrol topic JSON paketini ayrıştırır ve araç içi modüllere broadcast eder */
    void ProcessIncomingPacketJson(const FString& JsonStr);

    /** @state: WIP - Her motor modülünün kendi içinden ArduPilot PWM öncelikli (yoksa standart ROS) topic kontrolünü çözmesi için yardımcı */
    static bool ParseControlTopicForChannel(
        double NowSeconds,
        const FString& ModuleVehicleCode,
        int32 ModuleVehicleId,
        FName TargetBoneName,
        int32 MyChannel,
        const FString& InVehicleCode,
        const FString& TopicName,
        const FString& SourceType,
        const TSharedPtr<FJsonObject>& PayloadObj,
        double& InOutLastArduPilotPwmTime,
        double& InOutLastControlTopicTime,
        int32& OutPwmVal);

    /** Belirtilen topic altına veri yayınlar (ROS 2 / Zenoh uyumlu) */
    UFUNCTION(BlueprintCallable, Category = "PiSim|ROS2")
    void PublishTopic(const FString& TopicSuffix, const FString& JsonPayload);

    /** Ağ doğrulama için Ping/Echo test yanıtlayıcısı */
    UFUNCTION(BlueprintCallable, Category = "PiSim|ROS2")
    void HandlePingTest(const FString& InJsonStr);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Control")
    EPiSimTopicControlMode ControlMode = EPiSimTopicControlMode::Auto;

    /** Son ArduPilot PWM paketinin alındığı simülasyon zamanı */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Control")
    float LastPwmReceivedTime = -100.0f;

    /** PWM öncelik geçerlilik süresi (saniye) - bu süre boyunca PWM akarsa diğer modlar kilitlenir */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Control")
    float PwmTimeoutSec = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|ROS2")
    FString TopicPrefix; // Örn: "/rover_01"

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|ROS2")
    float SensorPublishRateHz = 20.0f; // 20 Hz ROS 2 sensör yayını

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|ROS2")
    int32 BridgePort = 7402; // Tekil RX Dinleme Portu (Zenoh -> Unreal: 7402)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|ROS2")
    FString TargetHost = TEXT("127.0.0.1");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|ROS2")
    int32 TargetPort = 7500; // Tekil TX Gönderme Portu (Unreal -> Zenoh: 7500)

protected:
    void InitSockets();
    void CloseSockets();
    void PollIncomingPackets();
    void PublishSensors();

    FSocket* ListenSocket = nullptr;
    FSocket* SendSocket = nullptr;
    float LastSensorPublishTime = 0.0f;
    double LastCameraPublishTime = 0.0;
    double LastAckPublishTime = 0.0;

    UPROPERTY()
    APiSimVehicle* CachedVehicle = nullptr;
};

