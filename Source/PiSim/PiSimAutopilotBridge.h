// PiSimAutopilotBridge.h
// Multi-Backend Autopilot Simulation Bridge for PiSim (UE5).
// Supports:
// 1) ArduPilot SITL JSON & Binary Servo Protocol (UDP Port 9002/9003)
// 2) PX4 SITL / MAVLink HIL Protocol (UDP Port 14560)
// 3) Companion Computer (Raspberry Pi 5) Passthrough

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "HAL/ThreadSafeBool.h"
#include "Async/Async.h"
#include "PiSimVirtualSensorSuite.h"
#include "PiSimAutopilotBridge.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(
    FOnAutopilotCommandReceived,
    float, Throttle,
    float, Roll,
    float, Pitch,
    float, Yaw,
    const TArray<float>&, RawChannels
);

#pragma pack(push, 1)
// ArduPilot SITL Binary Servo Output Packet (16 Channels)
struct FArduPilotServoPacket16
{
    uint16 Magic;       // 18458 (Standard ArduPilot SITL Magic)
    uint16 FrameRate;   // Simulation rate in Hz
    uint32 FrameCount;  // Monotonic frame index
    uint16 PWM[16];     // 16 PWM channel values (typically 1000..2000 µs)
};
#pragma pack(pop)

UCLASS(ClassGroup=(PiSim), meta=(BlueprintSpawnableComponent))
class PISIM_API UPiSimAutopilotBridge : public UActorComponent
{
    GENERATED_BODY()

public:
    UPiSimAutopilotBridge();
    virtual ~UPiSimAutopilotBridge();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /** Start ArduPilot SITL UDP Socket (Default listen: 9002, reply target: 9002 or 9003) */
    UFUNCTION(BlueprintCallable, Category = "PiSim|Autopilot")
    bool StartArduPilotSitl(int32 InListenPort = 9002);

    /** Start PX4 SITL UDP Socket (Default port: 14560) */
    UFUNCTION(BlueprintCallable, Category = "PiSim|Autopilot")
    bool StartPx4Sitl(int32 InListenPort = 14560);

    /** Transmit JSON State string to ArduPilot SITL */
    UFUNCTION(BlueprintCallable, Category = "PiSim|Autopilot")
    bool SendArduPilotStateJson(const FString& JsonPayload, const FString& TargetIP = TEXT("127.0.0.1"), int32 TargetPort = 9002);

    /** Transmit simulated sensors to PX4 HITL / SITL via MAVLink over UDP */
    void SendPx4Sensors(const FPiSimImuSensorData& Imu, const FPiSimBaroSensorData& Baro, const FPiSimGpsSensorData& Gps, const FString& TargetIP = TEXT("127.0.0.1"), int32 TargetPort = 14560);

    /** Shutdown active sockets and worker threads */
    UFUNCTION(BlueprintCallable, Category = "PiSim|Autopilot")
    void Shutdown();

    // Event broadcast when motor/servo commands arrive from otopilot
    UPROPERTY(BlueprintAssignable, Category = "PiSim|Autopilot")
    FOnAutopilotCommandReceived OnAutopilotCommandReceived;

    // Telemetry & Live Status
    UPROPERTY(BlueprintReadOnly, Category = "PiSim|Autopilot")
    bool bIsConnected = false;

    UPROPERTY(BlueprintReadOnly, Category = "PiSim|Autopilot")
    FString ConnectedIP = TEXT("127.0.0.1");

    UPROPERTY(BlueprintReadOnly, Category = "PiSim|Autopilot")
    int32 ConnectedPort = 9002;

    UPROPERTY(BlueprintReadOnly, Category = "PiSim|Autopilot")
    int32 TotalPacketsReceived = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PiSim|Autopilot")
    int32 TotalPacketsSent = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PiSim|Autopilot")
    float PacketRateHz = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "PiSim|Autopilot")
    float NormalizedThrottle = 0.0f; // 0.0 to 1.0

    UPROPERTY(BlueprintReadOnly, Category = "PiSim|Autopilot")
    float NormalizedRoll = 0.0f;     // -1.0 to +1.0 (Aileron / Elevon L)

    UPROPERTY(BlueprintReadOnly, Category = "PiSim|Autopilot")
    float NormalizedPitch = 0.0f;    // -1.0 to +1.0 (Elevator)

    UPROPERTY(BlueprintReadOnly, Category = "PiSim|Autopilot")
    float NormalizedYaw = 0.0f;      // -1.0 to +1.0 (Rudder)

    UPROPERTY(BlueprintReadOnly, Category = "PiSim|Autopilot")
    TArray<float> ChannelPwmValues;

    UPROPERTY(BlueprintReadOnly, Category = "PiSim|Autopilot")
    FString LastStatusMessage = TEXT("SITL Bekleniyor...");

    /** Port ArduPilot listens on for physics state (--sim-port-in). Default: 9003. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Autopilot")
    int32 ArduPilotStatePort = 9003;

    /** Returns true if the SITL socket is open and ready to send/receive. */
    bool IsSocketOpen() const { return SitlSocket != nullptr; }

private:
    FSocket* SitlSocket = nullptr;
    FThreadSafeBool bIsRunning{false};
    TFuture<void> ListenerFuture;

    void ListenerLoop();
    void ProcessIncomingPacket(const TArray<uint8>& PacketBytes, const FString& SenderIP, int32 SenderPort);

    // Rate calculation
    int32 RxCountInWindow = 0;
    float RateCalcTimer = 0.0f;
    float LastPacketTime = 0.0f;

    uint8 MavlinkSeq = 0;
};
