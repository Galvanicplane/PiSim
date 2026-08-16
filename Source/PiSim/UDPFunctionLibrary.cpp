// Fill out your copyright notice in the Description page of Project Settings.

#include "UDPFunctionLibrary.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Misc/Timespan.h"
#include "HAL/PlatformProcess.h"

bool UUDPFunctionLibrary::SendUDPBytes(const FString& IPAddress, int32 Port, const TArray<uint8>& Bytes)
{
    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem)
    {
        return false;
    }

    TSharedRef<FInternetAddr> RemoteAddr = SocketSubsystem->CreateInternetAddr();
    bool bIsValid = false;
    RemoteAddr->SetIp(*IPAddress, bIsValid);
    RemoteAddr->SetPort(Port);
    if (!bIsValid)
    {
        return false;
    }

    FSocket* Socket = SocketSubsystem->CreateSocket(NAME_DGram, TEXT("UE_UDP_SENDER"), false);
    if (!Socket)
    {
        return false;
    }

    Socket->SetNonBlocking(true);
    Socket->SetReuseAddr(true);

    int32 BytesSent = 0;
    bool bOk = Socket->SendTo(Bytes.GetData(), Bytes.Num(), BytesSent, *RemoteAddr);

    Socket->Close();
    SocketSubsystem->DestroySocket(Socket);

    return bOk && BytesSent == Bytes.Num();
}

bool UUDPFunctionLibrary::ReceiveUDPBytes(int32 LocalPort, float TimeoutSeconds, TArray<uint8>& OutBytes)
{
    OutBytes.Empty();

    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem)
    {
        return false;
    }

    FSocket* Socket = SocketSubsystem->CreateSocket(NAME_DGram, TEXT("UE_UDP_RECEIVER"), false);
    if (!Socket)
    {
        return false;
    }

    TSharedRef<FInternetAddr> LocalAddr = SocketSubsystem->CreateInternetAddr();
    bool bLocalOk = false;
    LocalAddr->SetIp(TEXT("0.0.0.0"), bLocalOk);
    LocalAddr->SetPort(LocalPort);
    if (!bLocalOk)
    {
        Socket->Close();
        SocketSubsystem->DestroySocket(Socket);
        return false;
    }

    if (!Socket->Bind(*LocalAddr))
    {
        Socket->Close();
        SocketSubsystem->DestroySocket(Socket);
        return false;
    }

    Socket->SetNonBlocking(false);
    FTimespan WaitTime = FTimespan::FromSeconds(FMath::Max(0.0f, TimeoutSeconds));
    bool bReady = Socket->Wait(ESocketWaitConditions::WaitForRead, WaitTime);
    if (!bReady)
    {
        Socket->Close();
        SocketSubsystem->DestroySocket(Socket);
        return false;
    }

    OutBytes.SetNumUninitialized(65507);
    int32 BytesRead = 0;
    TSharedRef<FInternetAddr> Sender = SocketSubsystem->CreateInternetAddr();
    bool bRecv = Socket->RecvFrom(OutBytes.GetData(), OutBytes.Num(), BytesRead, *Sender);

    if (bRecv && BytesRead > 0)
    {
        OutBytes.SetNum(BytesRead);
        Socket->Close();
        SocketSubsystem->DestroySocket(Socket);
        return true;
    }

    Socket->Close();
    SocketSubsystem->DestroySocket(Socket);
    return false;
}

bool UUDPFunctionLibrary::SendUDPMessage(const FString& IPAddress, int32 Port, const FString& Message)
{
    FTCHARToUTF8 Converter(*Message);
    TArray<uint8> Bytes;
    Bytes.Append(reinterpret_cast<const uint8*>(Converter.Get()), Converter.Length());
    return SendUDPBytes(IPAddress, Port, Bytes);
}

bool UUDPFunctionLibrary::ReceiveUDPMessage(int32 LocalPort, float TimeoutSeconds, FString& OutMessage)
{
    OutMessage = TEXT("");
    TArray<uint8> Bytes;
    if (!ReceiveUDPBytes(LocalPort, TimeoutSeconds, Bytes))
    {
        return false;
    }

    OutMessage = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(Bytes.GetData())));
    return true;
}

bool UUDPFunctionLibrary::GetUDPMessageType(const TArray<uint8>& Bytes, FString& OutMessageType)
{
    OutMessageType = TEXT("");
    if (Bytes.Num() < static_cast<int32>(sizeof(uint32)))
    {
        return false;
    }

    const uint8* Data = Bytes.GetData();
    int32 Remaining = Bytes.Num();

    uint32 Length = 0;
    FMemory::Memcpy(&Length, Data, sizeof(uint32));
    Data += sizeof(uint32);
    Remaining -= sizeof(uint32);

    if (Remaining < static_cast<int32>(Length))
    {
        return false;
    }

    if (Length > 0)
    {
        const ANSICHAR* Utf8Data = reinterpret_cast<const ANSICHAR*>(Data);
        FUTF8ToTCHAR Converter(Utf8Data, Length);
        OutMessageType = FString(Converter.Get(), Converter.Length());
    }
    return true;
}

bool UUDPFunctionLibrary::AirplaneMessageToBytes(const FAirplaneUDPMessage& Message, TArray<uint8>& OutBytes)
{
    return Message.ToBinary(OutBytes);
}

bool UUDPFunctionLibrary::BytesToAirplaneMessage(const TArray<uint8>& Bytes, FAirplaneUDPMessage& OutMessage)
{
    return FAirplaneUDPMessage::FromBinary(Bytes, OutMessage);
}

bool UUDPFunctionLibrary::LandVehicleMessageToBytes(const FLandVehicleUDPMessage& Message, TArray<uint8>& OutBytes)
{
    return Message.ToBinary(OutBytes);
}

bool UUDPFunctionLibrary::BytesToLandVehicleMessage(const TArray<uint8>& Bytes, FLandVehicleUDPMessage& OutMessage)
{
    return FLandVehicleUDPMessage::FromBinary(Bytes, OutMessage);
}
