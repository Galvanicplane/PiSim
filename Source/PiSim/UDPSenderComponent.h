// UDPSenderComponent.h
// An ActorComponent that maintains a persistent UDP socket and automatically streams byte payloads at a fixed FPS rate when bSend is true.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "UDPMessage.h"
#include "AirplaneUDPMessage.h"
#include "LandVehicleUDPMessage.h"
#include "UDPSenderComponent.generated.h"

/**
 * High-performance, persistent UDP Sender Component.
 * Automatically streams BytesToSend payload at a set SendRate (FPS) when bSend is enabled.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PISIM_API UUDPSenderComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UUDPSenderComponent();

    /** Target IP address (e.g. Raspberry Pi 5 IP or 127.0.0.1 for local testing) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Sender")
    FString TargetIP = TEXT("127.0.0.1");

    /** Target UDP port (Default: 5005 for telemetry) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Sender")
    int32 TargetPort = 5005;

    /** Enable automatic background streaming of BytesToSend */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Sender")
    bool bSend = true;

    /** Automatic streaming rate in FPS (Default: 30.0 FPS) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Sender")
    float SendRate = 30.0f;

    /** The binary byte payload buffer to send automatically on timer ticks */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Sender")
    TArray<uint8> BytesToSend;

    /** Start automatic background streaming timer */
    UFUNCTION(BlueprintCallable, Category = "UDP Sender")
    void StartAutoSending();

    /** Stop automatic background streaming timer */
    UFUNCTION(BlueprintCallable, Category = "UDP Sender")
    void StopAutoSending();

    /** Set the BytesToSend buffer directly from Blueprint */
    UFUNCTION(BlueprintCallable, Category = "UDP Sender")
    void SetBytesToSend(const TArray<uint8>& InBytes);

    /** Set BytesToSend buffer from an FLandVehicleUDPMessage struct */
    UFUNCTION(BlueprintCallable, Category = "UDP Sender")
    void SetLandVehicleMessage(const FLandVehicleUDPMessage& Message);

    /** Set BytesToSend buffer from an FAirplaneUDPMessage struct */
    UFUNCTION(BlueprintCallable, Category = "UDP Sender")
    void SetAirplaneMessage(const FAirplaneUDPMessage& Message);

    /** Send raw binary byte array immediately over UDP */
    UFUNCTION(BlueprintCallable, Category = "UDP Sender")
    bool SendBytes(const TArray<uint8>& Bytes);

    /** Send UTF-8 text string immediately over UDP */
    UFUNCTION(BlueprintCallable, Category = "UDP Sender")
    bool SendString(const FString& Message);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void OnSendTimerTick();
    bool InitializeSocket(const FString& InIPAddress, int32 InPort);
    void CloseSocket();

    FSocket* SenderSocket = nullptr;
    TSharedPtr<FInternetAddr> RemoteAddr;
    FTimerHandle SendTimerHandle;
};
