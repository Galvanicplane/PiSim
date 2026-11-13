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
    BridgePort = 7402;
    TargetPort = 7500;
    if (CachedVehicle)
    {
        int32 VId = FMath::Max(1, CachedVehicle->VehicleId);
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
        .WithBroadcast()
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

// @state: WIP - Tekil UDP 7402 soketinden gelen paketleri aktif filodaki TopicBridge bileşenlerine yönlendirir
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

            bool bDispatchedToFleet = false;
            {
                FScopeLock Lock(&APiSimVehicle::FleetLock);
                for (APiSimVehicle* Veh : APiSimVehicle::ActiveFleet)
                {
                    if (Veh && IsValid(Veh) && Veh->TopicBridge)
                    {
                        Veh->TopicBridge->ProcessIncomingPacketJson(JsonStr);
                        bDispatchedToFleet = true;
                    }
                }
            }
            if (!bDispatchedToFleet)
            {
                ProcessIncomingPacketJson(JsonStr);
            }
        }
    }
}

// @state: WIP - Gelen JSON kontrol topicini hedef araç eşleşmesiyle süzer ve hiçbir motora dışarıdan müdahale etmeden modüllere yayınlar
void UPiSimTopicBridge::ProcessIncomingPacketJson(const FString& JsonStr)
{
    TSharedPtr<FJsonObject> JsonObj;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
    if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
    {
        return;
    }

    FString Topic = JsonObj->HasField(TEXT("topic")) ? JsonObj->GetStringField(TEXT("topic")) : TEXT("");
    FString Cmd = JsonObj->HasField(TEXT("cmd")) ? JsonObj->GetStringField(TEXT("cmd")) : TEXT("");
    FString Source = JsonObj->HasField(TEXT("source")) ? JsonObj->GetStringField(TEXT("source")) : TEXT("");
    FString Target = JsonObj->HasField(TEXT("target")) ? JsonObj->GetStringField(TEXT("target")) : TEXT("");
    if (Target.IsEmpty() && JsonObj->HasField(TEXT("vehicle")))
    {
        Target = JsonObj->GetStringField(TEXT("vehicle"));
    }

    if (!Topic.IsEmpty())
    {
        TArray<FString> Parts;
        Topic.ParseIntoArray(Parts, TEXT("/"), true);
        if (Parts.Num() >= 2)
        {
            FString TopicTarget = (Parts[0].Equals(TEXT("pisim"), ESearchCase::IgnoreCase) && Parts.Num() >= 3) ? Parts[1] : Parts[0];
            if (Target.IsEmpty())
            {
                Target = TopicTarget;
            }
        }
    }
    if (Target.IsEmpty())
    {
        Target = TEXT("all");
    }

    if (CachedVehicle)
    {
        FString LegacyCode = FString::Printf(TEXT("rover_%02d"), CachedVehicle->VehicleId);
        bool bTargetMatch = Target.Equals(TEXT("all"), ESearchCase::IgnoreCase) ||
                            Target.Equals(CachedVehicle->VehicleCode, ESearchCase::IgnoreCase) ||
                            Target.Equals(CachedVehicle->VehicleName, ESearchCase::IgnoreCase) ||
                            Target.Equals(LegacyCode, ESearchCase::IgnoreCase) ||
                            CachedVehicle->VehicleCode.StartsWith(Target, ESearchCase::IgnoreCase) ||
                            Target.Equals(FString::FromInt(CachedVehicle->VehicleId));
        if (!bTargetMatch)
        {
            return;
        }
    }

    if (Topic.Contains(TEXT("ping")) || Cmd.Equals(TEXT("ping"), ESearchCase::IgnoreCase))
    {
        HandlePingTest(JsonStr);
        return;
    }
    if (Topic.Contains(TEXT("ack")) || Cmd.Equals(TEXT("ack"), ESearchCase::IgnoreCase))
    {
        DispatchAckJson(JsonStr);
        return;
    }

    // ROSBridge sarmalayıcısı ("msg": {...}) varsa iç alanları modüllerin kolay okuması için üst nesneye kopyala
    TSharedPtr<FJsonObject> DispatchObj = JsonObj;
    const TSharedPtr<FJsonObject>* MsgObjPtr = nullptr;
    if (JsonObj->TryGetObjectField(TEXT("msg"), MsgObjPtr) && MsgObjPtr && MsgObjPtr->IsValid())
    {
        for (const auto& Pair : (*MsgObjPtr)->Values)
        {
            if (!JsonObj->HasField(Pair.Key))
            {
                JsonObj->SetField(Pair.Key, Pair.Value);
            }
        }
    }

    const FString VehicleCode = CachedVehicle ? CachedVehicle->VehicleCode : Target;
    const bool bIsArduPilot = Source.Contains(TEXT("ardupilot"), ESearchCase::IgnoreCase) ||
                              Topic.Contains(TEXT("ardupilot"), ESearchCase::IgnoreCase) ||
                              Topic.EndsWith(TEXT("/pwm"), ESearchCase::IgnoreCase);

    float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    if (bIsArduPilot || Topic.Contains(TEXT("channels")) || Cmd.Equals(TEXT("channels"), ESearchCase::IgnoreCase) || JsonObj->HasField(TEXT("pwm")) || JsonObj->HasField(TEXT("channels")))
    {
        if (bIsArduPilot)
        {
            LastPwmReceivedTime = Now;
        }
        TopicAckStatusMap.Add(TopicPrefix + TEXT("/channels"), 2);
    }
    else if (Topic.Contains(TEXT("actuators")) || Cmd.Equals(TEXT("actuators"), ESearchCase::IgnoreCase))
    {
        TopicAckStatusMap.Add(TopicPrefix + TEXT("/actuators"), 2);
    }

    // Dışarıdan motor/kemik taraması YOK: Tüm modüller kendi içinden bu topiğin yayınını dinler
    OnControlTopicReceived.Broadcast(VehicleCode, Topic.IsEmpty() ? (TopicPrefix + TEXT("/") + (Cmd.IsEmpty() ? TEXT("channels") : Cmd)) : Topic, Source, DispatchObj);
}

// @state: WIP - Modül içi kontrol topic çözümleyicisi: ArduPilot PWM aktifse öncelikli alır, yoksa ROS /channels veya /cmd_vel veya /actuators okur
bool UPiSimTopicBridge::ParseControlTopicForChannel(
    double NowSeconds,
    const FString& ModuleVehicleCode,
    int32 ModuleVehicleId,
    FName TargetBoneName,
    int32 MyChannel,
    const FString& InVehicleCode,
    const FString& TopicName,
    const FString& SourceType,
    const TSharedPtr<FJsonObject>& PayloadObj,
    double& InOutLastArduPilotPwmTime,
    double& InOutLastControlTopicTime,
    int32& OutPwmVal)
{
    if (!PayloadObj.IsValid() || MyChannel <= 0)
    {
        return false;
    }

    if (!InVehicleCode.IsEmpty() && !InVehicleCode.Equals(TEXT("all"), ESearchCase::IgnoreCase))
    {
        FString LegacyCode = FString::Printf(TEXT("rover_%02d"), ModuleVehicleId);
        bool bMatch = ModuleVehicleCode.IsEmpty() ||
                      InVehicleCode.Equals(ModuleVehicleCode, ESearchCase::IgnoreCase) ||
                      InVehicleCode.Equals(LegacyCode, ESearchCase::IgnoreCase) ||
                      ModuleVehicleCode.StartsWith(InVehicleCode, ESearchCase::IgnoreCase) ||
                      InVehicleCode.Equals(FString::FromInt(ModuleVehicleId));
        if (!bMatch)
        {
            return false;
        }
    }

    const bool bIsArduPilot = SourceType.Contains(TEXT("ardupilot"), ESearchCase::IgnoreCase) ||
                              TopicName.Contains(TEXT("ardupilot"), ESearchCase::IgnoreCase) ||
                              TopicName.EndsWith(TEXT("/pwm"), ESearchCase::IgnoreCase);

    // ArduPilot kontrol topiği aktifse (son 0.5 sn içinde geldiyse) standart ROS topikleri yok sayılır
    if (!bIsArduPilot && InOutLastArduPilotPwmTime > 0.0 && (NowSeconds - InOutLastArduPilotPwmTime) < 0.5)
    {
        return false;
    }

    bool bMatched = false;

    // 1. ArduPilot Ham PWM Dizisi (1000..1500..2000 us) - Kanal indeksi MyChannel (1 tabanlı)
    const TArray<TSharedPtr<FJsonValue>>* PwmArray = nullptr;
    if (PayloadObj->TryGetArrayField(TEXT("pwm"), PwmArray) && PwmArray && PwmArray->IsValidIndex(MyChannel - 1))
    {
        float RawPwm = (float)(*PwmArray)[MyChannel - 1]->AsNumber();
        OutPwmVal = FMath::Clamp(FMath::RoundToInt(RawPwm), 800, 2200);
        bMatched = true;
    }

    // 2. Kanallar Haritası (/ardupilot/pwm veya /channels -> {"1": 1500, "2": 1800} veya normalize [-1..+1])
    if (!bMatched)
    {
        const TSharedPtr<FJsonObject>* ChansObj = nullptr;
        if (PayloadObj->TryGetObjectField(TEXT("channels"), ChansObj) && ChansObj && ChansObj->IsValid())
        {
            const FString KeyPlain = FString::FromInt(MyChannel);
            const FString KeyLower = TEXT("ch") + KeyPlain;
            const FString KeyUpper = TEXT("CH") + KeyPlain;
            FString MatchedKey;
            if ((*ChansObj)->HasField(KeyPlain)) MatchedKey = KeyPlain;
            else if ((*ChansObj)->HasField(KeyLower)) MatchedKey = KeyLower;
            else if ((*ChansObj)->HasField(KeyUpper)) MatchedKey = KeyUpper;

            if (!MatchedKey.IsEmpty())
            {
                float RawVal = (float)(*ChansObj)->GetNumberField(MatchedKey);
                if (RawVal > 500.0f)
                {
                    OutPwmVal = FMath::Clamp(FMath::RoundToInt(RawVal), 800, 2200);
                }
                else
                {
                    float NormVal = FMath::Clamp(RawVal, -1.0f, 1.0f);
                    OutPwmVal = FMath::RoundToInt(1500.0f + NormVal * 500.0f);
                }
                bMatched = true;
            }
        }
    }

    // 3. Doğrudan Aktüatör Topiği (/actuators -> {"motors": {"<BoneName>": val}}) - Sadece kendi kemiğini okur
    if (!bMatched && !bIsArduPilot && !TargetBoneName.IsNone())
    {
        const TSharedPtr<FJsonObject>* MotorsObj = nullptr;
        if (PayloadObj->TryGetObjectField(TEXT("motors"), MotorsObj) && MotorsObj && MotorsObj->IsValid())
        {
            FString BoneStr = TargetBoneName.ToString();
            if ((*MotorsObj)->HasField(BoneStr))
            {
                float RawVal = (float)(*MotorsObj)->GetNumberField(BoneStr);
                if (RawVal > 500.0f)
                {
                    OutPwmVal = FMath::Clamp(FMath::RoundToInt(RawVal), 800, 2200);
                }
                else
                {
                    OutPwmVal = FMath::RoundToInt(1500.0f + FMath::Clamp(RawVal, -1.0f, 1.0f) * 500.0f);
                }
                bMatched = true;
            }
        }
    }

    // 4. Standart ROS Kontrol Topiği (/cmd_vel -> linear.x / angular.z) - Kanal 1: İleri/Geri, Kanal 2: Sağ/Sol
    if (!bMatched && !bIsArduPilot)
    {
        const TSharedPtr<FJsonObject>* LinearObj = nullptr;
        const TSharedPtr<FJsonObject>* AngularObj = nullptr;
        bool bHasTwist = PayloadObj->TryGetObjectField(TEXT("linear"), LinearObj) ||
                         PayloadObj->TryGetObjectField(TEXT("angular"), AngularObj) ||
                         PayloadObj->HasField(TEXT("linear_x")) ||
                         PayloadObj->HasField(TEXT("angular_z"));
        if (bHasTwist)
        {
            if (MyChannel == 1)
            {
                float LinearX = PayloadObj->HasField(TEXT("linear_x")) ? (float)PayloadObj->GetNumberField(TEXT("linear_x")) : 0.0f;
                if (LinearObj && LinearObj->IsValid() && (*LinearObj)->HasField(TEXT("x")))
                {
                    LinearX = (float)(*LinearObj)->GetNumberField(TEXT("x"));
                }
                float Norm = FMath::Clamp(LinearX / 3.0f, -1.0f, 1.0f);
                OutPwmVal = FMath::RoundToInt(1500.0f + Norm * 500.0f);
                bMatched = true;
            }
            else if (MyChannel == 2)
            {
                float AngularZ = PayloadObj->HasField(TEXT("angular_z")) ? (float)PayloadObj->GetNumberField(TEXT("angular_z")) : 0.0f;
                if (AngularObj && AngularObj->IsValid() && (*AngularObj)->HasField(TEXT("z")))
                {
                    AngularZ = (float)(*AngularObj)->GetNumberField(TEXT("z"));
                }
                float Norm = FMath::Clamp(AngularZ / 1.5f, -1.0f, 1.0f);
                OutPwmVal = FMath::RoundToInt(1500.0f + Norm * 500.0f);
                bMatched = true;
            }
        }
    }

    if (bMatched)
    {
        InOutLastControlTopicTime = NowSeconds;
        if (bIsArduPilot)
        {
            InOutLastArduPilotPwmTime = NowSeconds;
        }
    }
    return bMatched;
}

void UPiSimTopicBridge::HandlePingTest(const FString& InJsonStr)
{
    double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    FString CleanCode = CachedVehicle ? CachedVehicle->VehicleCode : TEXT("rover");
    FString PongMsg = FString::Printf(TEXT("{\"status\":\"ok\",\"msg\":\"Unreal Engine Zenoh Topic Bridge Alive!\",\"vehicle\":\"%s\",\"sim_time\":%.3f}"), *CleanCode, Now);
    PublishTopic(TEXT("pong"), PongMsg);
}

// @state: WIP - Dış aktüatör taraması kaldırıldı; gelen aktüatör JSON'ını yalnızca OnControlTopicReceived ile modüllere yayınlar
void UPiSimTopicBridge::DispatchActuatorsJson(const FString& InJsonStr)
{
    ProcessIncomingPacketJson(InJsonStr);
}

// @state: WIP - Dış kemik/modül müdahalesi kaldırıldı; gelen kanal/PWM JSON'ını yalnızca OnControlTopicReceived ile modüllere yayınlar
void UPiSimTopicBridge::DispatchChannelsJson(const FString& InJsonStr)
{
    ProcessIncomingPacketJson(InJsonStr);
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
    if (bValid)
    {
        SendSocket->SendTo((const uint8*)Utf8.Get(), Utf8.Length(), BytesSent, *TargetAddr);
    }

    // Hedef başka bir IP ise ayrıca yerel 127.0.0.1:7500 Zenoh dinleyicisine de tekil yayın yap
    if (!TargetHost.Equals(TEXT("127.0.0.1")) && !TargetHost.Equals(TEXT("localhost"), ESearchCase::IgnoreCase))
    {
        TSharedRef<FInternetAddr> LocalAddr = SocketSubsystem->CreateInternetAddr();
        LocalAddr->SetIp(TEXT("127.0.0.1"), bValid);
        LocalAddr->SetPort(7500);
        SendSocket->SendTo((const uint8*)Utf8.Get(), Utf8.Length(), BytesSent, *LocalAddr);
    }
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

