// AutopilotBridge.cpp
// @state: WIP - Unreal Engine <-> ArduPilot SITL Köprüsü implementasyonu

#include "AutopilotBridge.h"
#include "Vehicle.h"
#include "PiSimTopicBridge.h"
#include "IPiSimActuatorInterface.h"
#include "Common/UdpSocketBuilder.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// @state: WIP - Yapıcı
UPiSimAutopilotBridge::UPiSimAutopilotBridge()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

// @state: WIP - Başlangıçta ParentVehicle referansını ve UDP soketlerini başlatır
void UPiSimAutopilotBridge::BeginPlay()
{
    Super::BeginPlay();
    ParentVehicle = Cast<APiSimVehicle>(GetOwner());
    if (bEnableSitlBridge)
    {
        InitSockets();
        SyncParametersWithSitl();
    }
}

// @state: WIP - Kapanışta soketleri kapatır
void UPiSimAutopilotBridge::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    CloseSockets();
    Super::EndPlay(EndPlayReason);
}

// @state: WIP - UDP soketlerini kurar
void UPiSimAutopilotBridge::InitSockets()
{
    CloseSockets();
    SendSocket = FUdpSocketBuilder(TEXT("PiSimSitlSendSocket"))
        .AsNonBlocking()
        .AsReusable()
        .WithSendBufferSize(65536);

    int32 Port = SitlPwmListenPort + (ParentVehicle ? FMath::Max(0, ParentVehicle->VehicleId - 1) * 10 : 0);
    ListenSocket = FUdpSocketBuilder(TEXT("PiSimSitlListenSocket"))
        .AsNonBlocking()
        .AsReusable()
        .BoundToPort(Port)
        .WithReceiveBufferSize(65536);
}

// @state: WIP - Soketleri temizler
void UPiSimAutopilotBridge::CloseSockets()
{
    ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (SendSocket) { if (SS) SS->DestroySocket(SendSocket); SendSocket = nullptr; }
    if (ListenSocket) { if (SS) SS->DestroySocket(ListenSocket); ListenSocket = nullptr; }
}

// @state: WIP - Her tick SITL sensör aktarımını ve gelen PWM yanıtlarını yürütür
void UPiSimAutopilotBridge::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!bEnableSitlBridge || !ParentVehicle) return;

    if (!SendSocket || !ListenSocket)
    {
        InitSockets();
    }

    PollSitlPwmResponses();

    double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    double Interval = (SitlSyncRateHz > 0.1f) ? (1.0 / SitlSyncRateHz) : 0.02;
    if ((Now - LastSyncTime) >= Interval)
    {
        LastSyncTime = Now;
        SendAggregatedSensorsToSitl();
    }
}

// @state: WIP - Aracın kanal -> motor modülü eşleştirme haritasını JSON olarak üretir
FString UPiSimAutopilotBridge::BuildChannelParameterJson() const
{
    TSharedPtr<FJsonObject> Root = MakeShareable(new FJsonObject());
    if (!ParentVehicle) return TEXT("{}");

    Root->SetStringField(TEXT("type"), TEXT("sitl_param_def"));
    Root->SetStringField(TEXT("vehicle"), ParentVehicle->VehicleCode);
    Root->SetNumberField(TEXT("vehicle_id"), ParentVehicle->VehicleId);
    Root->SetNumberField(TEXT("total_mass_kg"), ParentVehicle->TotalMassKg);

    TSharedPtr<FJsonObject> ChannelMap = MakeShareable(new FJsonObject());
    TArray<UActorComponent*> Comps;
    ParentVehicle->GetComponents(Comps);
    for (UActorComponent* Comp : Comps)
    {
        if (IPiSimActuatorInterface* Act = Cast<IPiSimActuatorInterface>(Comp))
        {
            int32 Ch = Act->GetArduPilotChannel();
            if (Ch >= 1 && Ch <= 16)
            {
                TSharedPtr<FJsonObject> Entry = MakeShareable(new FJsonObject());
                Entry->SetStringField(TEXT("module_type"), Act->GetActuatorType());
                Entry->SetStringField(TEXT("bone"), Act->GetTargetBoneName().ToString());
                int32 FuncId = 0;
                if (Act->GetActuatorType() == TEXT("Wheel") || Act->GetActuatorType() == TEXT("Track")) FuncId = 70;
                else if (Act->GetActuatorType() == TEXT("Servo")) FuncId = 26;
                else if (Act->GetActuatorType() == TEXT("Propeller")) FuncId = 33;
                Entry->SetNumberField(TEXT("servo_function"), FuncId);
                ChannelMap->SetObjectField(FString::FromInt(Ch), Entry);
            }
        }
    }
    Root->SetObjectField(TEXT("channel_map"), ChannelMap);

    FString OutStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutStr);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
    return OutStr;
}

// @state: WIP - Kanal tanımını ArduPilot SITL köprüsüne UDP üzerinden gönderir
void UPiSimAutopilotBridge::SyncParametersWithSitl()
{
    if (!SendSocket || !ParentVehicle) return;
    FString ParamJson = BuildChannelParameterJson();
    FTCHARToUTF8 Utf8(*ParamJson);

    ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SS) return;
    TSharedRef<FInternetAddr> DestAddr = SS->CreateInternetAddr();
    bool bValid = false;
    DestAddr->SetIp(*SitlBridgeHost, bValid);
    DestAddr->SetPort(SitlSensorBridgePort);
    if (bValid)
    {
        int32 Sent = 0;
        SendSocket->SendTo((const uint8*)Utf8.Get(), Utf8.Length(), Sent, *DestAddr);
    }
}

// @state: WIP - Araç sensör verilerini (IMU + GPS + Baro) tek pakette birleştirip SITL köprüsüne atar
void UPiSimAutopilotBridge::SendAggregatedSensorsToSitl()
{
    if (!SendSocket || !ParentVehicle) return;

    FPiSimIMUData Imu;
    FPiSimGPSData Gps;
    FPiSimBaroData Baro;
    ParentVehicle->GetLatestIMUData(Imu);
    ParentVehicle->GetLatestGPSData(Gps);
    ParentVehicle->GetLatestBaroData(Baro);

    TSharedPtr<FJsonObject> Root = MakeShareable(new FJsonObject());
    Root->SetStringField(TEXT("type"), TEXT("sitl_sensor_state"));
    Root->SetStringField(TEXT("vehicle"), ParentVehicle->VehicleCode);
    Root->SetNumberField(TEXT("vehicle_id"), ParentVehicle->VehicleId);
    Root->SetNumberField(TEXT("timestamp"), GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0);

    TArray<TSharedPtr<FJsonValue>> ImuAccel = {
        MakeShareable(new FJsonValueNumber(Imu.LinearAcceleration.X)),
        MakeShareable(new FJsonValueNumber(Imu.LinearAcceleration.Y)),
        MakeShareable(new FJsonValueNumber(Imu.LinearAcceleration.Z))
    };
    TArray<TSharedPtr<FJsonValue>> ImuGyro = {
        MakeShareable(new FJsonValueNumber(Imu.AngularVelocityRad.X)),
        MakeShareable(new FJsonValueNumber(Imu.AngularVelocityRad.Y)),
        MakeShareable(new FJsonValueNumber(Imu.AngularVelocityRad.Z))
    };
    FRotator Euler = Imu.Orientation.Rotator();
    Root->SetArrayField(TEXT("imu_accel"), ImuAccel);
    Root->SetArrayField(TEXT("imu_gyro"), ImuGyro);
    Root->SetNumberField(TEXT("roll_deg"), Euler.Roll);
    Root->SetNumberField(TEXT("pitch_deg"), Euler.Pitch);
    Root->SetNumberField(TEXT("yaw_deg"), Euler.Yaw);

    Root->SetNumberField(TEXT("lat"), Gps.Latitude);
    Root->SetNumberField(TEXT("lon"), Gps.Longitude);
    Root->SetNumberField(TEXT("alt"), Gps.AltitudeMeters);
    Root->SetNumberField(TEXT("speed_kmh"), Gps.SpeedKmh);
    Root->SetNumberField(TEXT("baro_hpa"), Baro.AbsPressureHPa);

    FString OutStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutStr);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

    FTCHARToUTF8 Utf8(*OutStr);
    ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SS) return;
    TSharedRef<FInternetAddr> DestAddr = SS->CreateInternetAddr();
    bool bValid = false;
    DestAddr->SetIp(*SitlBridgeHost, bValid);
    DestAddr->SetPort(SitlSensorBridgePort);
    if (bValid)
    {
        int32 Sent = 0;
        SendSocket->SendTo((const uint8*)Utf8.Get(), Utf8.Length(), Sent, *DestAddr);
    }
}

// @state: WIP - ArduPilot SITL'den dönen PWM/kanal çıkışlarını okuyup aracın TopicBridge'ine iletir
void UPiSimAutopilotBridge::PollSitlPwmResponses()
{
    if (!ListenSocket || !ParentVehicle || !ParentVehicle->TopicBridge) return;
    ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SS) return;

    uint32 Pending = 0;
    while (ListenSocket->HasPendingData(Pending))
    {
        TArray<uint8> Buf;
        Buf.SetNumUninitialized(FMath::Min(Pending, 4096u));
        TSharedRef<FInternetAddr> Sender = SS->CreateInternetAddr();
        int32 BytesRead = 0;
        if (ListenSocket->RecvFrom(Buf.GetData(), Buf.Num(), BytesRead, *Sender) && BytesRead > 0)
        {
            Buf.Add(0);
            FString JsonStr = UTF8_TO_TCHAR((const char*)Buf.GetData());
            ParentVehicle->TopicBridge->DispatchChannelsJson(JsonStr);
        }
    }
}

