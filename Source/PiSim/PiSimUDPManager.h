// PiSimUDPManager.h
// Dual-port network manager for PiSim Bridge.
// Manages isolated Control/Telemetry (UDP 7400) and FPV Video Stream (UDP 5000) sockets.

#pragma once

#include "CoreMinimal.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "HAL/ThreadSafeBool.h"
#include "Async/Async.h"
#include "Templates/SharedPointer.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnControlPacketReceived, const TArray<uint8>& /*Bytes*/, const FString& /*SenderIP*/);

class PISIM_API FPiSimUDPManager
{
public:
    FPiSimUDPManager();
    ~FPiSimUDPManager();

    // Dual-Port Constants
    static constexpr int32 DEFAULT_CONTROL_PORT = 7400; // UE5 listens on 7400
    static constexpr int32 DEFAULT_TELEMETRY_PORT = 7401; // Python listens on 7401
    static constexpr int32 DEFAULT_VIDEO_PORT = 5000;    // Python listens on 5000

    /** Initialize Control & Telemetry UDP listener on specified port (Default: 7400) */
    bool StartControlListener(int32 ListenPort = DEFAULT_CONTROL_PORT);

    /** Initialize Video sender socket for target port (Default: 5000) */
    bool ReserveVideoSocket(int32 VideoPort = DEFAULT_VIDEO_PORT);

    /** Transmit control/telemetry payload to destination IP and Port (Default: 7401) */
    bool SendControlData(const TArray<uint8>& Data, const FString& TargetIP = TEXT("127.0.0.1"), int32 TargetPort = DEFAULT_TELEMETRY_PORT);

    /** Transmit live video stream frame payload to destination IP and Port (Default: 5000) */
    bool SendVideoData(const TArray<uint8>& Data, const FString& TargetIP = TEXT("127.0.0.1"), int32 TargetPort = DEFAULT_VIDEO_PORT);

    /** Stop all active sockets and threads */
    void Shutdown();

    /** Control Packet Delegate */
    FOnControlPacketReceived OnControlPacketReceived;

    bool IsControlSocketActive() const { return bIsControlRunning; }
    bool IsVideoSocketReserved() const { return VideoSocket != nullptr; }

private:
    FSocket* ControlSocket = nullptr;
    FSocket* VideoSocket = nullptr;

    FThreadSafeBool bIsControlRunning{false};
    TFuture<void> ControlListenerFuture;

    void ListenerLoop();
};
