// PiSimTopicBridge.cpp

#include "PiSimTopicBridge.h"
#include "Vehicle.h"
#include "IPiSimActuatorInterface.h"
#include "Common/UdpSocketBuilder.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"

UPiSimTopicBridge::UPiSimTopicBridge()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UPiSimTopicBridge::BeginPlay()
{
    Super::BeginPlay();

    CachedVehicle = Cast<APiSimVehicle>(GetOwner());
    if (CachedVehicle)
    {
        int32 VId = FMath::Max(1, CachedVehicle->VehicleId);
        BridgePort = 7410 + VId;
        TargetPort = 7510 + VId;

        FString CleanCode = CachedVehicle->VehicleCode.IsEmpty() ? FString::Printf(TEXT("rover_%02d"), VId) : CachedVehicle->VehicleCode;
        TopicPrefix = FString::Printf(TEXT("/%s"), *CleanCode);
    }

    InitSockets();
}

void UPiSimTopicBridge::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    CloseSockets();
    Super::EndPlay(EndPlayReason);
}

void UPiSimTopicBridge::InitSockets()
{
    CloseSockets();

    ListenSocket = FUdpSocketBuilder(TEXT("PiSimTopicBridgeRx"))
        .AsNonBlocking()
        .AsReusable()
        .BoundToPort(BridgePort)
        .WithReceiveBufferSize(1024 * 1024);

    SendSocket = FUdpSocketBuilder(TEXT("PiSimTopicBridgeTx"))
        .AsNonBlocking()
        .AsReusable()
        .WithSendBufferSize(1024 * 1024);
}

void UPiSimTopicBridge::CloseSockets()
{
    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (SocketSubsystem)
    {
        if (ListenSocket)
        {
            SocketSubsystem->DestroySocket(ListenSocket);
            ListenSocket = nullptr;
        }
        if (SendSocket)
        {
            SocketSubsystem->DestroySocket(SendSocket);
            SendSocket = nullptr;
        }
    }
}

void UPiSimTopicBridge::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    PollIncomingPackets();

    float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    float Interval = (SensorPublishRateHz > 0.0f) ? (1.0f / SensorPublishRateHz) : 0.05f;
    if (Now - LastSensorPublishTime >= Interval)
    {
        LastSensorPublishTime = Now;
        PublishSensors();
    }
}

void UPiSimTopicBridge::PollIncomingPackets()
{
    if (!ListenSocket) return;

    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem) return;

    TSharedRef<FInternetAddr> SenderAddr = SocketSubsystem->CreateInternetAddr();
    uint32 PendingSize = 0;

    while (ListenSocket->HasPendingData(PendingSize) && PendingSize > 0)
    {
        TArray<uint8> Buffer;
        Buffer.SetNumUninitialized(PendingSize + 1);
        int32 BytesRead = 0;

        if (ListenSocket->RecvFrom(Buffer.GetData(), PendingSize, BytesRead, *SenderAddr) && BytesRead > 0)
        {
            Buffer[BytesRead] = 0;
            FString JsonStr = UTF8_TO_TCHAR((const char*)Buffer.GetData());

            TSharedPtr<FJsonObject> JsonObj;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
            if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
            {
                FString Topic = JsonObj->HasField(TEXT("topic")) ? JsonObj->GetStringField(TEXT("topic")) : TEXT("");
                FString Cmd = JsonObj->HasField(TEXT("cmd")) ? JsonObj->GetStringField(TEXT("cmd")) : TEXT("");

                if (Topic.Contains(TEXT("ping")) || Cmd.Equals(TEXT("ping"), ESearchCase::IgnoreCase))
                {
                    HandlePingTest(JsonStr);
                }
                else if (Topic.Contains(TEXT("channels")) || Cmd.Equals(TEXT("channels"), ESearchCase::IgnoreCase))
                {
                    DispatchChannelsJson(JsonStr);
                }
                else if (Topic.Contains(TEXT("actuators")) || Cmd.Equals(TEXT("actuators"), ESearchCase::IgnoreCase))
                {
                    DispatchActuatorsJson(JsonStr);
                }
                else if (Topic.Contains(TEXT("cmd_vel")) || Cmd.Equals(TEXT("cmd_vel"), ESearchCase::IgnoreCase))
                {
                    float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
                    bool bPwmActive = (ControlMode == EPiSimTopicControlMode::PWMChannels) ||
                                     (ControlMode == EPiSimTopicControlMode::Auto && (Now - LastPwmReceivedTime) < PwmTimeoutSec);
                    if (!bPwmActive && CachedVehicle)
                    {
                        CachedVehicle->ProcessCommandJson(JsonStr);
                    }
                }
            }
        }
    }
}

void UPiSimTopicBridge::HandlePingTest(const FString& InJsonStr)
{
    double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    FString CleanCode = CachedVehicle ? CachedVehicle->VehicleCode : TEXT("rover");
    FString PongMsg = FString::Printf(TEXT("{\"status\":\"ok\",\"msg\":\"Unreal Engine Zenoh Topic Bridge Alive!\",\"vehicle\":\"%s\",\"sim_time\":%.3f}"), *CleanCode, Now);
    PublishTopic(TEXT("pong"), PongMsg);
}

void UPiSimTopicBridge::DispatchActuatorsJson(const FString& InJsonStr)
{
    if (!CachedVehicle) return;

    float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    if (ControlMode == EPiSimTopicControlMode::PWMChannels)
    {
        return; // PWM modunda kilitli
    }
    if (ControlMode == EPiSimTopicControlMode::Auto && (Now - LastPwmReceivedTime) < PwmTimeoutSec)
    {
        return; // ArduPilot PWM akışı aktif, öncelik PWM'de!
    }

    TSharedPtr<FJsonObject> JsonObj;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(InJsonStr);
    if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid()) return;

    const TSharedPtr<FJsonObject>* MotorsObj = nullptr;
    if (JsonObj->TryGetObjectField(TEXT("motors"), MotorsObj) && MotorsObj && MotorsObj->IsValid())
    {
        TArray<UActorComponent*> Comps;
        CachedVehicle->GetComponents(Comps);

        for (const auto& Pair : (*MotorsObj)->Values)
        {
            FName BoneName = FName(*Pair.Key);
            float Value = (float)Pair.Value->AsNumber();

            // 1. Doğrudan ActorComponent seviyesindeki IPiSimActuatorInterface üzerinden sür (SIFIR CAST!)
            for (UActorComponent* Comp : Comps)
            {
                if (IPiSimActuatorInterface* Actuator = Cast<IPiSimActuatorInterface>(Comp))
                {
                    if (Actuator->GetTargetBoneName() == BoneName)
                    {
                        Actuator->SetActuatorNormalized(Value);
                    }
                }
            }

            // 2. Mafsal ve Chaos dinamik değer haritasını güncelle
            CachedVehicle->SetMotorTestValue(BoneName, Value);
        }
    }
}

void UPiSimTopicBridge::DispatchChannelsJson(const FString& InJsonStr)
{
    if (!CachedVehicle) return;

    LastPwmReceivedTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    TSharedPtr<FJsonObject> JsonObj;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(InJsonStr);
    if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid()) return;

    const TSharedPtr<FJsonObject>* ChansObj = nullptr;
    if (JsonObj->TryGetObjectField(TEXT("channels"), ChansObj) && ChansObj && ChansObj->IsValid())
    {
        TArray<UActorComponent*> Comps;
        CachedVehicle->GetComponents(Comps);

        for (const auto& Pair : (*ChansObj)->Values)
        {
            int32 ChNum = FCString::Atoi(*Pair.Key);
            int32 PwmVal = (int32)Pair.Value->AsNumber();

            for (UActorComponent* Comp : Comps)
            {
                if (IPiSimActuatorInterface* Actuator = Cast<IPiSimActuatorInterface>(Comp))
                {
                    if (Actuator->GetArduPilotChannel() == ChNum)
                    {
                        Actuator->SetActuatorPWM(PwmVal);
                    }
                }
            }
        }
    }
}

void UPiSimTopicBridge::PublishTopic(const FString& TopicSuffix, const FString& JsonPayload)
{
    if (!SendSocket) return;

    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem) return;

    FString FullTopic = FString::Printf(TEXT("%s/%s"), *TopicPrefix, *TopicSuffix);
    FString Enveloped = FString::Printf(TEXT("{\"topic\":\"%s\",\"msg\":%s}\n"), *FullTopic, *JsonPayload);

    FTCHARToUTF8 Utf8(*Enveloped);
    int32 BytesSent = 0;

    TSharedRef<FInternetAddr> TargetAddr = SocketSubsystem->CreateInternetAddr();
    bool bValid = false;
    TargetAddr->SetIp(*TargetHost, bValid);
    TargetAddr->SetPort(TargetPort);
    SendSocket->SendTo((const uint8*)Utf8.Get(), Utf8.Length(), BytesSent, *TargetAddr);

    // Aynı makinedeki yerel dinleyiciler için Loopback (127.0.0.1:7500)
    TSharedRef<FInternetAddr> LocalAddr = SocketSubsystem->CreateInternetAddr();
    LocalAddr->SetIp(TEXT("127.0.0.1"), bValid);
    LocalAddr->SetPort(7500);
    SendSocket->SendTo((const uint8*)Utf8.Get(), Utf8.Length(), BytesSent, *LocalAddr);
}

void UPiSimTopicBridge::PublishSensors()
{
    if (!CachedVehicle) return;

    double NowSec = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    int64 Sec = (int64)NowSec;
    uint32 NanoSec = (uint32)((NowSec - (double)Sec) * 1e9);

    // 1. ROS 2 sensor_msgs/msg/Imu
    FQuat Rot = CachedVehicle->GetActorQuat();
    FVector LinAcc = CachedVehicle->GetVelocity(); // Basitleştirilmiş ivme
    FVector AngVel = CachedVehicle->GetRootComponent() ? CachedVehicle->GetRootComponent()->GetComponentVelocity() : FVector::ZeroVector;

    FString ImuPayload = FString::Printf(
        TEXT("{\"header\":{\"stamp\":{\"sec\":%lld,\"nanosec\":%u},\"frame_id\":\"base_link\"},"
             "\"orientation\":{\"x\":%.5f,\"y\":%.5f,\"z\":%.5f,\"w\":%.5f},"
             "\"angular_velocity\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},"
             "\"linear_acceleration\":{\"x\":%.4f,\"y\":%.4f,\"z\":%.4f}}"),
        Sec, NanoSec, Rot.X, Rot.Y, Rot.Z, Rot.W, LinAcc.X * 0.01f, LinAcc.Y * 0.01f, LinAcc.Z * 0.01f
    );
    PublishTopic(TEXT("imu"), ImuPayload);

    // 2. ROS 2 sensor_msgs/msg/NavSatFix (GPS)
    FVector Pos = CachedVehicle->GetActorLocation();
    const double OriginLat = 39.9334;
    const double OriginLon = 32.8597;
    double Lat = OriginLat + ((Pos.X * 0.01) / 111320.0);
    double Lon = OriginLon + ((Pos.Y * 0.01) / (111320.0 * FMath::Cos(FMath::DegreesToRadians(OriginLat))));
    double Alt = (Pos.Z * 0.01) + 850.0;

    FString GpsPayload = FString::Printf(
        TEXT("{\"header\":{\"stamp\":{\"sec\":%lld,\"nanosec\":%u},\"frame_id\":\"gps_link\"},"
             "\"status\":{\"status\":0,\"service\":1},"
             "\"latitude\":%.7f,\"longitude\":%.7f,\"altitude\":%.2f}"),
        Sec, NanoSec, Lat, Lon, Alt
    );
    PublishTopic(TEXT("gps"), GpsPayload);
}

