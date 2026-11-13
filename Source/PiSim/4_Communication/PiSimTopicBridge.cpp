// PiSimTopicBridge.cpp

#include "PiSimTopicBridge.h"
#include "Vehicle.h"
#include "CameraSensor.h"
#include "IPiSimActuatorInterface.h"
#include "Common/UdpSocketBuilder.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Misc/Base64.h"

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
                else if (Topic.Contains(TEXT("ack")) || Cmd.Equals(TEXT("ack"), ESearchCase::IgnoreCase))
                {
                    DispatchAckJson(JsonStr);
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
        // Gazebo / Doğrudan Aktüatör Modunu aktifleştir ve watchdog süresini kur
        CachedVehicle->bDirectActuatorControl = true;
        CachedVehicle->CommandWatchdogTimer = 0.8f;

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

        // 3. Aracı fiziksel olarak doğrudan tetikle
        CachedVehicle->ProcessCommandJson(InJsonStr);
    }
}

void UPiSimTopicBridge::DispatchChannelsJson(const FString& InJsonStr)
{
    if (!CachedVehicle) return;

    TSharedPtr<FJsonObject> JsonObj;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(InJsonStr);
    if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
    {
        TopicAckStatusMap.Add(TopicPrefix + TEXT("/channels"), 1); // 1 = Veri yapısı bozuk
        return;
    }

    const TSharedPtr<FJsonObject>* ChansObj = nullptr;
    if (!JsonObj->TryGetObjectField(TEXT("channels"), ChansObj) || !ChansObj || !ChansObj->IsValid())
    {
        const TSharedPtr<FJsonObject>* MsgObj = nullptr;
        if (JsonObj->TryGetObjectField(TEXT("msg"), MsgObj) && MsgObj && MsgObj->IsValid())
        {
            (*MsgObj)->TryGetObjectField(TEXT("channels"), ChansObj);
        }
    }

    if (ChansObj && ChansObj->IsValid())
    {
        LastPwmReceivedTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
        CachedVehicle->bDirectActuatorControl = true;
        CachedVehicle->CommandWatchdogTimer = 0.8f;
        TopicAckStatusMap.Add(TopicPrefix + TEXT("/channels"), 2); // 2 = Sağlıklı

        TArray<UActorComponent*> Comps;
        CachedVehicle->GetComponents(Comps);

        for (const auto& Pair : (*ChansObj)->Values)
        {
            FString KeyClean = Pair.Key;
            KeyClean.RemoveFromStart(TEXT("ch"), ESearchCase::IgnoreCase);
            int32 ChNum = FCString::Atoi(*KeyClean);
            if (ChNum <= 0) continue;

            float RawVal = (float)Pair.Value->AsNumber();
            float NormVal = 0.0f;
            int32 PwmVal = 1500;

            // Hem normalize (-1.0 .. +1.0) hem de ArduPilot PWM (1000 .. 2000) değerlerini otomatik destekle
            if (FMath::Abs(RawVal) <= 2.5f)
            {
                NormVal = FMath::Clamp(RawVal, -1.0f, 1.0f);
                PwmVal = FMath::RoundToInt(1500.0f + NormVal * 500.0f);
            }
            else
            {
                PwmVal = FMath::Clamp((int32)RawVal, 800, 2200);
                NormVal = FMath::Clamp((PwmVal - 1500.0f) / 500.0f, -1.0f, 1.0f);
            }

            bool bHandledByModule = false;
            // 1. Her motor modülü kendi ArduPilotChannel değişkenini kontrol eder ve kendi komutunu uygular
            //    Channel 1 -> Sürüş Tekerlekleri (İleri/Geri)
            //    Channel 2 -> Direksiyon Servoları (Sağ/Sol)
            for (UActorComponent* Comp : Comps)
            {
                if (IPiSimActuatorInterface* Actuator = Cast<IPiSimActuatorInterface>(Comp))
                {
                    if (Actuator->GetArduPilotChannel() == ChNum)
                    {
                        Actuator->SetActuatorPWM(PwmVal);
                        FName BoneName = Actuator->GetTargetBoneName();
                        if (!BoneName.IsNone())
                        {
                            CachedVehicle->SetMotorTestValue(BoneName, NormVal);
                        }
                        bHandledByModule = true;
                    }
                }
            }

            // 2. Kemik sahne bileşenleri için yedek eşleştirme (Channel 1 = Tekerlekler, Channel 2 = Steer Servolar)
            for (const auto& BonePair : CachedVehicle->BoneSceneComponents)
            {
                FString LowerName = BonePair.Key.ToString().ToLower();
                if (ChNum == 1 && (LowerName.Contains(TEXT("wheel")) || LowerName.Contains(TEXT("teker"))))
                {
                    CachedVehicle->SetMotorTestValue(BonePair.Key, NormVal);
                }
                else if (ChNum == 2 && (LowerName.Contains(TEXT("steer")) || LowerName.Contains(TEXT("servo"))))
                {
                    CachedVehicle->SetMotorTestValue(BonePair.Key, NormVal);
                }
            }
        }
    }
    else
    {
        TopicAckStatusMap.Add(TopicPrefix + TEXT("/channels"), 1); // 1 = İstenen yapıda değil
    }
}

// @state: WIP - Gelen ACK mesajını topic haritasına yazar ve Bridge dinleyici delegesiyle doğrudan dinleyen modüllere sunar
void UPiSimTopicBridge::DispatchAckJson(const FString& InJsonStr)
{
    if (!CachedVehicle) return;

    TSharedPtr<FJsonObject> JsonObj;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(InJsonStr);
    if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid()) return;

    const TSharedPtr<FJsonObject>* TopicsObj = nullptr;
    if (!JsonObj->TryGetObjectField(TEXT("topics"), TopicsObj) || !TopicsObj || !TopicsObj->IsValid())
    {
        const TSharedPtr<FJsonObject>* MsgObj = nullptr;
        if (JsonObj->TryGetObjectField(TEXT("msg"), MsgObj) && MsgObj && MsgObj->IsValid())
        {
            (*MsgObj)->TryGetObjectField(TEXT("topics"), TopicsObj);
        }
    }

    if (TopicsObj && TopicsObj->IsValid())
    {
        const FString VehicleCode = CachedVehicle ? CachedVehicle->VehicleCode : TEXT("");
        for (const auto& Pair : (*TopicsObj)->Values)
        {
            FString TopicKey = Pair.Key;
            int32 StatusCode = (int32)Pair.Value->AsNumber();
            TopicAckStatusMap.Add(TopicKey, StatusCode);

            // Modüller kendi içlerinde bu olaya abone olup kendi araç/topic filtrelemesini yapar
            OnAckReceived.Broadcast(VehicleCode, TopicKey, StatusCode);
        }
    }
}

void UPiSimTopicBridge::PublishAckStatus()
{
    if (!CachedVehicle) return;

    float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    FString CleanCode = CachedVehicle->VehicleCode.IsEmpty() ? TEXT("rover_01") : CachedVehicle->VehicleCode;

    int32 ImuCode = 2; // 2 = Sağlıklı
    int32 GpsCode = 2; // 2 = Sağlıklı
    int32 CamCode = (CachedVehicle->CameraSensor && CachedVehicle->CameraSensor->bLiveH264StreamActive) ? 3 : 4; // 3=Canlı H264, 4=Standby Mavi Ekran
    int32 ChCode = ((Now - LastPwmReceivedTime) < 3.0f) ? 2 : TopicAckStatusMap.FindRef(TopicPrefix + TEXT("/channels"));
    int32 ActCode = (CachedVehicle->CommandWatchdogTimer > 0.0f) ? 2 : TopicAckStatusMap.FindRef(TopicPrefix + TEXT("/actuators"));

    FString AckPayload = FString::Printf(
        TEXT("{\"vehicle_code\":\"%s\",\"source\":\"unreal\",\"topics\":{"
             "\"%s/imu\":%d,"
             "\"%s/gps\":%d,"
             "\"%s/camera/h264\":%d,"
             "\"%s/channels\":%d,"
             "\"%s/actuators\":%d}}"),
        *CleanCode,
        *TopicPrefix, ImuCode,
        *TopicPrefix, GpsCode,
        *TopicPrefix, CamCode,
        *TopicPrefix, ChCode,
        *TopicPrefix, ActCode
    );
    PublishTopic(TEXT("ack"), AckPayload);
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
    FPiSimIMUData Imu;
    CachedVehicle->GetLatestIMUData(Imu);
    FString ImuPayload = FString::Printf(
        TEXT("{\"header\":{\"stamp\":{\"sec\":%lld,\"nanosec\":%u},\"frame_id\":\"base_link\"},"
             "\"orientation\":{\"x\":%.5f,\"y\":%.5f,\"z\":%.5f,\"w\":%.5f},"
             "\"angular_velocity\":{\"x\":%.4f,\"y\":%.4f,\"z\":%.4f},"
             "\"linear_acceleration\":{\"x\":%.4f,\"y\":%.4f,\"z\":%.4f}}"),
        Sec, NanoSec, Imu.Orientation.X, Imu.Orientation.Y, Imu.Orientation.Z, Imu.Orientation.W,
        Imu.AngularVelocityRad.X, Imu.AngularVelocityRad.Y, Imu.AngularVelocityRad.Z,
        Imu.LinearAcceleration.X, Imu.LinearAcceleration.Y, Imu.LinearAcceleration.Z
    );
    PublishTopic(TEXT("imu"), ImuPayload);

    // 2. ROS 2 sensor_msgs/msg/NavSatFix (GPS)
    FPiSimGPSData Gps;
    CachedVehicle->GetLatestGPSData(Gps);
    FString GpsPayload = FString::Printf(
        TEXT("{\"header\":{\"stamp\":{\"sec\":%lld,\"nanosec\":%u},\"frame_id\":\"gps_link\"},"
             "\"status\":{\"status\":%d,\"service\":1},"
             "\"latitude\":%.7f,\"longitude\":%.7f,\"altitude\":%.2f}"),
        Sec, NanoSec, Gps.FixType, Gps.Latitude, Gps.Longitude, Gps.AltitudeMeters
    );
    PublishTopic(TEXT("gps"), GpsPayload);

    // 3. Araca Özel ACK Sağlık ve Kamera Durum Tablosu (2 Hz: /<vehicle_code>/ack)
    if (NowSec - LastAckPublishTime >= 0.50)
    {
        LastAckPublishTime = NowSec;
        PublishAckStatus();
    }
}

