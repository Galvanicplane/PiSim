// PiSimAutopilotManager.h
// Switchable control backends: Direct ROS 2, ArduPilot SITL (JSON/UDP), PX4 SITL (MAVLink TCP/UDP), PX4 HITL (serial).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PiSimMavlink.h"
#include "PiSimSerialPort.h"
#include "PiSimAutopilotManager.generated.h"

class UPiSimVirtualSensorSuite;
class APiSimModelImporter;

UENUM(BlueprintType)
enum class EPiSimControlMode : uint8
{
    DirectROS2     UMETA(DisplayName = "Direkt ROS 2 (Pi 5 /cmd_vel)"),
    ArduPilotSITL  UMETA(DisplayName = "ArduPilot SITL (JSON UDP 9002)"),
    PX4SITL        UMETA(DisplayName = "PX4 SITL (MAVLink TCP 4560)"),
    PX4HITL        UMETA(DisplayName = "PX4 HITL (USB COM)")
};

USTRUCT(BlueprintType)
struct PISIM_API FPiSimAutopilotTelemetry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "PiSim|Autopilot")
    bool bLinkUp = false;

    UPROPERTY(BlueprintReadOnly, Category = "PiSim|Autopilot")
    bool bReceivingActuators = false;

    UPROPERTY(BlueprintReadOnly, Category = "PiSim|Autopilot")
    bool bArmed = false;

    UPROPERTY(BlueprintReadOnly, Category = "PiSim|Autopilot")
    FString StatusLine = TEXT("Direkt ROS 2");

    UPROPERTY(BlueprintReadOnly, Category = "PiSim|Autopilot")
    float SensorTxHz = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "PiSim|Autopilot")
    float ActuatorRxHz = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "PiSim|Autopilot")
    float RollCmd = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "PiSim|Autopilot")
    float PitchCmd = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "PiSim|Autopilot")
    float YawCmd = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "PiSim|Autopilot")
    float ThrottleCmd = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "PiSim|Autopilot")
    TArray<int32> MotorPwmUs;
};

UCLASS(ClassGroup=(PiSim), meta=(BlueprintSpawnableComponent))
class PISIM_API UPiSimAutopilotManager : public UActorComponent
{
    GENERATED_BODY()

public:
    UPiSimAutopilotManager();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UFUNCTION(BlueprintCallable, Category = "PiSim|Autopilot")
    void SetControlMode(EPiSimControlMode NewMode);

    UFUNCTION(BlueprintCallable, Category = "PiSim|Autopilot")
    bool Connect();

    UFUNCTION(BlueprintCallable, Category = "PiSim|Autopilot")
    void Disconnect();

    UFUNCTION(BlueprintCallable, Category = "PiSim|Autopilot")
    void CycleSerialPort();

    bool ShouldDriveActuators() const;
    void RefreshAvailableSerialPorts();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Autopilot")
    EPiSimControlMode ControlMode = EPiSimControlMode::DirectROS2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Autopilot")
    FString ArduPilotBindIP = TEXT("0.0.0.0");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Autopilot")
    int32 ArduPilotListenPort = 9002;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Autopilot")
    FString Px4SitlHost = TEXT("127.0.0.1");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Autopilot")
    int32 Px4SitlTcpPort = 4560;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Autopilot")
    int32 Px4SitlUdpPort = 14560;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Autopilot")
    FString SerialPortName = TEXT("COM3");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Autopilot")
    int32 SerialBaudRate = 921600;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Autopilot")
    TArray<FString> AvailableSerialPorts;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Autopilot")
    FPiSimAutopilotTelemetry Telemetry;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Autopilot")
    int32 SerialPortListIndex = 0;

private:
    void TickDirectRos();
    void TickArduPilotSitl(float DeltaTime);
    void TickPx4Mavlink(float DeltaTime, bool bUseSerial);
    void SendVirtualSensors(double UnixSec, bool bForceGps);
    void HandleHilActuators(const FPiSimHilActuatorControls& Act);
    void HandleArduPilotPwm(const uint16* Pwm, int32 Count);
    void ApplyMixerToOwner();
    bool OpenDatagramSocket(class FSocket*& OutSocket, int32 BindPort);
    void DestroySocket(class FSocket*& Socket);
    bool SendBytes(class FSocket* Socket, const TArray<uint8>& Data, const FString& TargetIP, int32 TargetPort);
    void DrainUdp(class FSocket* Socket, TFunction<void(const TArray<uint8>&, const FString&, int32)> Handler);
    bool ConnectPx4Tcp();
    void DrainTcp();
    void DrainSerial();

    TUniquePtr<FPiSimSerialPort> SerialPort;
    class FSocket* ArduSocket = nullptr;
    class FSocket* Px4UdpSocket = nullptr;
    class FSocket* Px4TcpSocket = nullptr;
    TArray<uint8> MavParseBuffer;
    uint8 MavSeq = 0;
    uint8 SimSysId = 142;
    uint8 SimCompId = 1;

    FString LastArduSenderIP = TEXT("127.0.0.1");
    int32 LastArduSenderPort = 9003;

    float HilSensorTimer = 0.0f;
    float HilGpsTimer = 0.0f;
    float HeartbeatTimer = 0.0f;
    float RateWindowTimer = 0.0f;
    int32 SensorTxInWindow = 0;
    int32 ActuatorRxInWindow = 0;
    float LastActuatorTime = -100.0f;

    float MixRoll = 0.0f;
    float MixPitch = 0.0f;
    float MixYaw = 0.0f;
    float MixThrottle = 0.0f;
    uint16 LastPwm[16] = {};
    float LastRawControls[16] = {};
};
