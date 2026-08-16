// UDPReceiver.h
// An ActorComponent that starts a background UDP listener thread and fires Blueprint events when packets arrive.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "HAL/ThreadSafeBool.h"
#include "Templates/SharedPointer.h"
#include "Async/Async.h"
#include "UDPReceiver.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUDPBytesReceived, const TArray<uint8>&, Bytes);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUDPStringReceived, const FString&, Message);

/**
 * Background UDP Receiver Component.
 * Runs socket listener on a dedicated background thread without blocking the Game Thread.
 * Automatically broadcasts Blueprint events (OnBytesReceived) when packets arrive.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PISIM_API UUDPReceiver : public UActorComponent
{
    GENERATED_BODY()

public:
    UUDPReceiver();

    /** Port to listen on (Default: 5005 for telemetry) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Receiver")
    int32 ListenPort = 5005;

    /** Automatically start background listener on BeginPlay */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Receiver")
    bool bAutoStart = true;

    /** Print debug log messages to screen when packets are received or if a packet format is invalid */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Receiver")
    bool bPrintDebugToScreen = true;

    /** Blueprint event triggered on the Game Thread when binary UDP bytes arrive */
    UPROPERTY(BlueprintAssignable, Category = "UDP Receiver|Events")
    FOnUDPBytesReceived OnBytesReceived;

    /** Blueprint event triggered on the Game Thread when text UDP message arrives */
    UPROPERTY(BlueprintAssignable, Category = "UDP Receiver|Events")
    FOnUDPStringReceived OnStringReceived;

    /** Start background UDP socket listener */
    UFUNCTION(BlueprintCallable, Category = "UDP Receiver")
    bool StartListening(int32 Port);

    /** Stop background UDP socket listener */
    UFUNCTION(BlueprintCallable, Category = "UDP Receiver")
    void StopListening();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    FSocket* ListenerSocket = nullptr;
    FThreadSafeBool bRunning{false};
    TFuture<void> ListenerFuture;
};
