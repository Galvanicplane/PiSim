// PiSimUDPManager.cpp
#include "PiSimUDPManager.h"
#include "IPAddress.h"
#include "Common/TcpSocketBuilder.h"
#include "Async/Async.h"

FPiSimUDPManager::FPiSimUDPManager()
{
}

FPiSimUDPManager::~FPiSimUDPManager()
{
    Shutdown();
}

bool FPiSimUDPManager::StartControlListener(int32 ListenPort)
{
    if (bIsControlRunning)
    {
        return true;
    }

    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("[PiSimUDPManager] SocketSubsystem unavailable!"));
        return false;
    }

    TSharedRef<FInternetAddr> LocalAddr = SocketSubsystem->CreateInternetAddr();
    LocalAddr->SetAnyAddress();
    LocalAddr->SetPort(ListenPort);

    ControlSocket = SocketSubsystem->CreateSocket(NAME_DGram, TEXT("PiSimControlSocket"), true);
    if (!ControlSocket)
    {
        UE_LOG(LogTemp, Error, TEXT("[PiSimUDPManager] Failed to create Control socket on port %d!"), ListenPort);
        return false;
    }

    ControlSocket->SetReuseAddr(true);
    ControlSocket->SetNonBlocking(true);

    int32 BufferSize = 2 * 1024 * 1024; // 2MB buffer
    ControlSocket->SetReceiveBufferSize(BufferSize, BufferSize);
    ControlSocket->SetSendBufferSize(BufferSize, BufferSize);

    if (!ControlSocket->Bind(*LocalAddr))
    {
        UE_LOG(LogTemp, Error, TEXT("[PiSimUDPManager] Failed to bind Control socket to port %d!"), ListenPort);
        SocketSubsystem->DestroySocket(ControlSocket);
        ControlSocket = nullptr;
        return false;
    }

    bIsControlRunning = true;
    ControlListenerFuture = Async(EAsyncExecution::Thread, [this]()
    {
        ListenerLoop();
    });

    UE_LOG(LogTemp, Log, TEXT("[PiSimUDPManager] Bound Control socket successfully on UDP Port %d."), ListenPort);
    return true;
}

bool FPiSimUDPManager::ReserveVideoSocket(int32 VideoPort)
{
    if (VideoSocket)
    {
        return true;
    }

    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem)
    {
        return false;
    }

    VideoSocket = SocketSubsystem->CreateSocket(NAME_DGram, TEXT("PiSimVideoSocket"), true);
    if (!VideoSocket)
    {
        UE_LOG(LogTemp, Error, TEXT("[PiSimUDPManager] Failed to create Video stream sender socket!"));
        return false;
    }

    VideoSocket->SetReuseAddr(true);
    VideoSocket->SetNonBlocking(true);
    int32 VideoBufferSize = 8 * 1024 * 1024; // 8MB buffer for video frames
    VideoSocket->SetReceiveBufferSize(VideoBufferSize, VideoBufferSize);
    VideoSocket->SetSendBufferSize(VideoBufferSize, VideoBufferSize);

    UE_LOG(LogTemp, Log, TEXT("[PiSimUDPManager] Prepared FPV Video sender socket for UDP Port %d."), VideoPort);
    return true;
}

bool FPiSimUDPManager::SendControlData(const TArray<uint8>& Data, const FString& TargetIP, int32 TargetPort)
{
    if (!ControlSocket)
    {
        return false;
    }

    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem)
    {
        return false;
    }

    TSharedRef<FInternetAddr> TargetAddr = SocketSubsystem->CreateInternetAddr();
    bool bIsValid = false;
    TargetAddr->SetIp(*TargetIP, bIsValid);
    TargetAddr->SetPort(TargetPort);

    if (!bIsValid)
    {
        UE_LOG(LogTemp, Error, TEXT("[PiSimUDPManager] Invalid Target IP: %s"), *TargetIP);
        return false;
    }

    int32 BytesSent = 0;
    return ControlSocket->SendTo(Data.GetData(), Data.Num(), BytesSent, *TargetAddr);
}

bool FPiSimUDPManager::SendVideoData(const TArray<uint8>& Data, const FString& TargetIP, int32 TargetPort)
{
    if (!VideoSocket)
    {
        return false;
    }

    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem)
    {
        return false;
    }

    TSharedRef<FInternetAddr> TargetAddr = SocketSubsystem->CreateInternetAddr();
    bool bIsValid = false;
    TargetAddr->SetIp(*TargetIP, bIsValid);
    TargetAddr->SetPort(TargetPort);

    if (!bIsValid)
    {
        return false;
    }

    int32 BytesSent = 0;
    return VideoSocket->SendTo(Data.GetData(), Data.Num(), BytesSent, *TargetAddr);
}

void FPiSimUDPManager::ListenerLoop()
{
    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    TArray<uint8> ReceivedBytes;
    ReceivedBytes.SetNumUninitialized(4096);

    while (bIsControlRunning)
    {
        if (!ControlSocket)
        {
            FPlatformProcess::Sleep(0.01f);
            continue;
        }

        uint32 PendingSize = 0;
        if (ControlSocket->HasPendingData(PendingSize) && PendingSize > 0)
        {
            TSharedRef<FInternetAddr> SenderAddr = SocketSubsystem->CreateInternetAddr();
            int32 ReadBytes = 0;

            if (ControlSocket->RecvFrom(ReceivedBytes.GetData(), ReceivedBytes.Num(), ReadBytes, *SenderAddr))
            {
                if (ReadBytes > 0)
                {
                    TArray<uint8> PacketData;
                    PacketData.Append(ReceivedBytes.GetData(), ReadBytes);
                    FString SenderIP = SenderAddr->ToString(false);

                    // Dispatch safely to GameThread for Unreal Engine thread-safety
                    AsyncTask(ENamedThreads::GameThread, [this, PacketData, SenderIP]()
                    {
                        OnControlPacketReceived.Broadcast(PacketData, SenderIP);
                    });
                }
            }
        }
        else
        {
            FPlatformProcess::Sleep(0.001f); // 1ms sleep to prevent CPU thread starvation
        }
    }
}

void FPiSimUDPManager::Shutdown()
{
    bIsControlRunning = false;

    if (ControlListenerFuture.IsValid())
    {
        ControlListenerFuture.Wait();
    }

    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (SocketSubsystem)
    {
        if (ControlSocket)
        {
            ControlSocket->Close();
            SocketSubsystem->DestroySocket(ControlSocket);
            ControlSocket = nullptr;
        }
        if (VideoSocket)
        {
            VideoSocket->Close();
            SocketSubsystem->DestroySocket(VideoSocket);
            VideoSocket = nullptr;
        }
    }
}
