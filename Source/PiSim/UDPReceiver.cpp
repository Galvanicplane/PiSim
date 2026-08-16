// UDPReceiver.cpp

#include "UDPReceiver.h"
#include "Async/Async.h"
#include "Misc/ScopeLock.h"
#include "HAL/PlatformProcess.h"
#include "Engine/Engine.h"

UUDPReceiver::UUDPReceiver()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UUDPReceiver::BeginPlay()
{
    Super::BeginPlay();
    if (bAutoStart)
    {
        StartListening(ListenPort);
    }
}

void UUDPReceiver::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopListening();
    Super::EndPlay(EndPlayReason);
}

bool UUDPReceiver::StartListening(int32 Port)
{
    if (bRunning)
    {
        return false; // Already listening
    }

    ListenPort = Port;

    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem)
    {
        return false;
    }

    ListenerSocket = SocketSubsystem->CreateSocket(NAME_DGram, TEXT("UE_UDP_RECEIVER_BG"), false);
    if (!ListenerSocket)
    {
        return false;
    }

    TSharedRef<FInternetAddr> LocalAddr = SocketSubsystem->CreateInternetAddr();
    bool bOk = false;
    LocalAddr->SetIp(TEXT("0.0.0.0"), bOk);
    LocalAddr->SetPort(ListenPort);
    if (!bOk || !ListenerSocket->Bind(*LocalAddr))
    {
        SocketSubsystem->DestroySocket(ListenerSocket);
        ListenerSocket = nullptr;
        return false;
    }

    ListenerSocket->SetNonBlocking(true);
    bRunning = true;

    // Launch dedicated background thread for non-blocking socket polling
    ListenerFuture = Async(EAsyncExecution::Thread, [this]() {
        TArray<uint8> Buffer;
        Buffer.SetNumUninitialized(65507);
        ISocketSubsystem* SubSys = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

        while (bRunning && ListenerSocket)
        {
            int32 BytesRead = 0;
            TSharedRef<FInternetAddr> Sender = SubSys->CreateInternetAddr();
            bool bReceived = ListenerSocket->RecvFrom(Buffer.GetData(), Buffer.Num(), BytesRead, *Sender);
            if (bReceived && BytesRead > 0)
            {
                TArray<uint8> ReceivedBytes;
                ReceivedBytes.Append(Buffer.GetData(), BytesRead);

                FString Msg = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(ReceivedBytes.GetData())));

                // Dispatch events back to the Game Thread
                AsyncTask(ENamedThreads::GameThread, [this, ReceivedBytes, Msg]() {
                    if (ReceivedBytes.Num() < 4)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("[UDPReceiver WARNING] Received corrupted or invalid UDP packet (Size: %d bytes)"), ReceivedBytes.Num());
                        if (bPrintDebugToScreen && GEngine)
                        {
                            GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, FString::Printf(TEXT("[UDPReceiver WARNING] Invalid packet format! Size: %d bytes"), ReceivedBytes.Num()));
                        }
                    }
                    else
                    {
                        if (bPrintDebugToScreen && GEngine)
                        {
                            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, FString::Printf(TEXT("[UDPReceiver] Received packet (%d bytes)"), ReceivedBytes.Num()));
                        }
                    }

                    OnBytesReceived.Broadcast(ReceivedBytes);
                    OnStringReceived.Broadcast(Msg);
                });
            }
            // Sleep 5ms to yield CPU when no packets are present
            FPlatformProcess::Sleep(0.005f);
        }
    });

    return true;
}

void UUDPReceiver::StopListening()
{
    bRunning = false;
    if (ListenerFuture.IsValid())
    {
        ListenerFuture.Wait();
    }

    if (ListenerSocket)
    {
        ISocketSubsystem* Sub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
        if (Sub)
        {
            Sub->DestroySocket(ListenerSocket);
        }
        ListenerSocket = nullptr;
    }
}
