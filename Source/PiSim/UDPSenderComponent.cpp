// UDPSenderComponent.cpp

#include "UDPSenderComponent.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

UUDPSenderComponent::UUDPSenderComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UUDPSenderComponent::BeginPlay()
{
    Super::BeginPlay();
    InitializeSocket(TargetIP, TargetPort);
    StartAutoSending();
}

void UUDPSenderComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopAutoSending();
    CloseSocket();
    Super::EndPlay(EndPlayReason);
}

void UUDPSenderComponent::StartAutoSending()
{
    StopAutoSending();
    float Interval = (SendRate > 0.0f) ? (1.0f / SendRate) : 0.033f;

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(SendTimerHandle, this, &UUDPSenderComponent::OnSendTimerTick, Interval, true);
    }
}

void UUDPSenderComponent::StopAutoSending()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(SendTimerHandle);
    }
}

void UUDPSenderComponent::OnSendTimerTick()
{
    if (bSend && BytesToSend.Num() > 0)
    {
        SendBytes(BytesToSend);
    }
}

bool UUDPSenderComponent::InitializeSocket(const FString& InIPAddress, int32 InPort)
{
    CloseSocket();

    TargetIP = InIPAddress;
    TargetPort = InPort;

    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem)
    {
        return false;
    }

    RemoteAddr = SocketSubsystem->CreateInternetAddr();
    bool bIsValid = false;
    RemoteAddr->SetIp(*TargetIP, bIsValid);
    RemoteAddr->SetPort(TargetPort);
    if (!bIsValid)
    {
        UE_LOG(LogTemp, Error, TEXT("[UDPSenderComponent] Invalid IP Address: %s"), *TargetIP);
        return false;
    }

    SenderSocket = SocketSubsystem->CreateSocket(NAME_DGram, TEXT("UE_UDP_SENDER_COMP"), false);
    if (!SenderSocket)
    {
        UE_LOG(LogTemp, Error, TEXT("[UDPSenderComponent] Failed to create UDP socket!"));
        return false;
    }

    SenderSocket->SetNonBlocking(true);
    SenderSocket->SetReuseAddr(true);
    return true;
}

void UUDPSenderComponent::CloseSocket()
{
    if (SenderSocket)
    {
        ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
        if (SocketSubsystem)
        {
            SenderSocket->Close();
            SocketSubsystem->DestroySocket(SenderSocket);
        }
        SenderSocket = nullptr;
    }
    RemoteAddr.Reset();
}

void UUDPSenderComponent::SetBytesToSend(const TArray<uint8>& InBytes)
{
    BytesToSend = InBytes;
}

void UUDPSenderComponent::SetLandVehicleMessage(const FLandVehicleUDPMessage& Message)
{
    Message.ToBinary(BytesToSend);
}

void UUDPSenderComponent::SetAirplaneMessage(const FAirplaneUDPMessage& Message)
{
    Message.ToBinary(BytesToSend);
}

bool UUDPSenderComponent::SendBytes(const TArray<uint8>& Bytes)
{
    if (Bytes.Num() == 0)
    {
        return false;
    }

    if (!SenderSocket || !RemoteAddr.IsValid())
    {
        if (!InitializeSocket(TargetIP, TargetPort))
        {
            return false;
        }
    }

    int32 BytesSent = 0;
    bool bOk = SenderSocket->SendTo(Bytes.GetData(), Bytes.Num(), BytesSent, *RemoteAddr);
    return bOk && BytesSent == Bytes.Num();
}

bool UUDPSenderComponent::SendString(const FString& Message)
{
    FTCHARToUTF8 Converter(*Message);
    TArray<uint8> Bytes;
    Bytes.Append(reinterpret_cast<const uint8*>(Converter.Get()), Converter.Length());
    return SendBytes(Bytes);
}
