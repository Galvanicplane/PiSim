// Pi5Bridge.h
// Raspberry Pi 5 Köprüsü: İki asenkron iş parçacığı (Thread) ile tek dosyada çalışır:
// Thread 1: Port A üzerinden Canlı JPEG Video Yayını
// Thread 2: Port B üzerinden ROS 2 / JSON Sensör Telemetri Yayını

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Pi5Bridge.generated.h"

class UPiSimTelemetryGateway;

class FPi5WorkerThread : public FRunnable
{
public:
    FPi5WorkerThread(class UPiSimPi5Bridge* InOwner, bool bInIsVideoThread);
    virtual ~FPi5WorkerThread();

    virtual bool Init() override;
    virtual uint32 Run() override;
    virtual void Stop() override;
    virtual void Exit() override;

private:
    class UPiSimPi5Bridge* OwnerBridge;
    bool bIsVideoThread;
    FThreadSafeBool bRunning;
};

UCLASS(ClassGroup=(PiSim), meta=(BlueprintSpawnableComponent))
class PISIM_API UPiSimPi5Bridge : public UActorComponent
{
    GENERATED_BODY()

public:
    UPiSimPi5Bridge();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UFUNCTION(BlueprintCallable, Category = "PiSim|Pi5")
    bool StartStreaming(const FString& InTargetIP = TEXT("127.0.0.1"), int32 InTelemetryPort = 7400, int32 InVideoPort = 7401);

    UFUNCTION(BlueprintCallable, Category = "PiSim|Pi5")
    void StopStreaming();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Pi5")
    FString TargetIP = TEXT("127.0.0.1");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Pi5")
    int32 TelemetryPort = 7400;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Pi5")
    int32 VideoPort = 7401;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Pi5")
    float TelemetryRateHz = 50.0f;

    // İş parçacıkları tarafından çağrılır
    void ExecuteTelemetryLoop(FThreadSafeBool& bRunning);
    void ExecuteVideoLoop(FThreadSafeBool& bRunning);

protected:
    UPROPERTY()
    UPiSimTelemetryGateway* CachedGateway = nullptr;

    FSocket* TelemetrySocket = nullptr;
    FSocket* VideoSocket = nullptr;

    FPi5WorkerThread* TelemetryRunnable = nullptr;
    FRunnableThread* TelemetryThread = nullptr;

    FPi5WorkerThread* VideoRunnable = nullptr;
    FRunnableThread* VideoThread = nullptr;
};
