// AutopilotBridge.h
// Otopilot Köprüsü: ArduPilot (Port 9002/9003) & PX4 (Port 14560) SITL protokolü ile konuşur.
// TelemetryGateway'den araç durumunu okur; gelen 16-kanal PWM sinyallerini araçtaki Servo, Pervane ve Tekerlek modüllerine dağıtır.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "AutopilotBridge.generated.h"

class UPiSimTelemetryGateway;
class APiSimVehicle;

#pragma pack(push, 1)
struct FArduPilotSitlServoPacket
{
    uint16 Magic;       // 18458
    uint16 FrameRate;
    uint32 FrameCount;
    uint16 PWM[16];     // 1000..2000 us
};
#pragma pack(pop)

class FAutopilotRxWorker : public FRunnable
{
public:
    FAutopilotRxWorker(class UPiSimModularAutopilotBridge* InOwner);
    virtual ~FAutopilotRxWorker();

    virtual bool Init() override { return true; }
    virtual uint32 Run() override;
    virtual void Stop() override { bRunning = false; }

private:
    class UPiSimModularAutopilotBridge* Owner;
    FThreadSafeBool bRunning;
};

UCLASS(ClassGroup=(PiSim), meta=(BlueprintSpawnableComponent))
class PISIM_API UPiSimModularAutopilotBridge : public UActorComponent
{
    GENERATED_BODY()

public:
    UPiSimModularAutopilotBridge();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UFUNCTION(BlueprintCallable, Category = "PiSim|Autopilot")
    bool StartSitl(int32 InListenPort = 9002, int32 InSendPort = 9003);

    UFUNCTION(BlueprintCallable, Category = "PiSim|Autopilot")
    void StopSitl();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Autopilot")
    FString TargetIP = TEXT("127.0.0.1");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Autopilot")
    int32 ListenPort = 9002;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Autopilot")
    int32 SendPort = 9003;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Autopilot")
    int32 VehicleId = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Autopilot")
    bool bIsConnected = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Autopilot")
    float PacketRateHz = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Autopilot")
    FString LastStatusMessage = TEXT("Beklemede");

    // Arka plan iş parçacığının okuma döngüsü
    void ExecuteRxLoop(FThreadSafeBool& bRunning);

    // En son alınan 16 kanal PWM
    uint16 LatestPWM[16];
    FCriticalSection PwmLock;
    bool bNewPwmAvailable = false;

protected:
    UPROPERTY()
    UPiSimTelemetryGateway* CachedGateway = nullptr;

    UPROPERTY()
    APiSimVehicle* CachedVehicle = nullptr;

    FSocket* ListenSocket = nullptr;
    FSocket* SendSocket = nullptr;

    FAutopilotRxWorker* RxWorker = nullptr;
    FRunnableThread* RxThread = nullptr;

    float LastPacketTime = 0.0f;
    int32 RxCountInWindow = 0;
    float RateTimer = 0.0f;

    void DispatchPwmToVehicleMotors();
};
