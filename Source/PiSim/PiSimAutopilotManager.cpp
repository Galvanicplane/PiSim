#include "PiSimAutopilotManager.h"
#include "PiSimModelImporter.h"
#include "PiSimVirtualSensorSuite.h"
#include "PiSimUDPManager.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "Common/TcpSocketBuilder.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Templates/Function.h"

namespace
{
    constexpr uint16 ArduJsonMagic = 18458;

    float PwmToNorm(uint16 PwmUs, bool bThrottle)
    {
        if (bThrottle)
        {
            return FMath::Clamp((static_cast<float>(PwmUs) - 1000.0f) / 1000.0f, 0.0f, 1.0f);
        }
        return FMath::Clamp((static_cast<float>(PwmUs) - 1500.0f) / 400.0f, -1.0f, 1.0f);
    }

    uint64 UnixMicros()
    {
        return static_cast<uint64>(FDateTime::UtcNow().ToUnixTimestamp() * 1000000LL)
            + static_cast<uint64>(FDateTime::UtcNow().GetMillisecond() * 1000);
    }
}

UPiSimAutopilotManager::UPiSimAutopilotManager()
{
    PrimaryComponentTick.bCanEverTick = true;
    SerialPort = MakeUnique<FPiSimSerialPort>();
}

void UPiSimAutopilotManager::BeginPlay()
{
    Super::BeginPlay();
    RefreshAvailableSerialPorts();
    TickDirectRos();
}

void UPiSimAutopilotManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Disconnect();
    Super::EndPlay(EndPlayReason);
}

void UPiSimAutopilotManager::RefreshAvailableSerialPorts()
{
    AvailableSerialPorts = FPiSimSerialPort::ListAvailablePorts();
    if (AvailableSerialPorts.Num() > 0 && !AvailableSerialPorts.Contains(SerialPortName))
    {
        SerialPortName = AvailableSerialPorts[0];
        SerialPortListIndex = 0;
    }
}

void UPiSimAutopilotManager::CycleSerialPort()
{
    RefreshAvailableSerialPorts();
    if (AvailableSerialPorts.Num() == 0)
    {
        return;
    }
    SerialPortListIndex = (SerialPortListIndex + 1) % AvailableSerialPorts.Num();
    SerialPortName = AvailableSerialPorts[SerialPortListIndex];
}

void UPiSimAutopilotManager::SetControlMode(EPiSimControlMode NewMode)
{
    if (ControlMode == NewMode)
    {
        return;
    }
    Disconnect();
    ControlMode = NewMode;
    if (ControlMode == EPiSimControlMode::DirectROS2)
    {
        TickDirectRos();
        return;
    }
    Connect();
}

bool UPiSimAutopilotManager::Connect()
{
    Disconnect();

    switch (ControlMode)
    {
    case EPiSimControlMode::DirectROS2:
        TickDirectRos();
        return true;

    case EPiSimControlMode::ArduPilotSITL:
        if (!OpenDatagramSocket(ArduSocket, ArduPilotListenPort))
        {
            Telemetry.StatusLine = TEXT("ArduPilot SITL: UDP 9002 açılamadı");
            return false;
        }
        Telemetry.StatusLine = FString::Printf(TEXT("ArduPilot SITL: 0.0.0.0:%d dinleniyor"), ArduPilotListenPort);
        Telemetry.bLinkUp = true;
        return true;

    case EPiSimControlMode::PX4SITL:
        if (ConnectPx4Tcp())
        {
            Telemetry.StatusLine = FString::Printf(TEXT("PX4 SITL TCP %s:%d bağlı"), *Px4SitlHost, Px4SitlTcpPort);
            Telemetry.bLinkUp = true;
            return true;
        }
        if (OpenDatagramSocket(Px4UdpSocket, 0))
        {
            Telemetry.StatusLine = FString::Printf(TEXT("PX4 SITL UDP %s:%d (TCP yok, UDP yedek)"), *Px4SitlHost, Px4SitlUdpPort);
            Telemetry.bLinkUp = true;
            return true;
        }
        Telemetry.StatusLine = TEXT("PX4 SITL: TCP/UDP bağlanamadı");
        return false;

    case EPiSimControlMode::PX4HITL:
        RefreshAvailableSerialPorts();
        if (SerialPort->Open(SerialPortName, SerialBaudRate))
        {
            Telemetry.StatusLine = FString::Printf(TEXT("PX4 HITL: %s @ %d bağlı"), *SerialPortName, SerialBaudRate);
            Telemetry.bLinkUp = true;
            return true;
        }
        Telemetry.StatusLine = FString::Printf(TEXT("PX4 HITL: %s açılamadı"), *SerialPortName);
        return false;
    }

    return false;
}

void UPiSimAutopilotManager::Disconnect()
{
    DestroySocket(ArduSocket);
    DestroySocket(Px4UdpSocket);
    DestroySocket(Px4TcpSocket);
    if (SerialPort)
    {
        SerialPort->Close();
    }
    MavParseBuffer.Reset();
    Telemetry.bLinkUp = false;
    Telemetry.bReceivingActuators = false;
    Telemetry.bArmed = false;
    LastActuatorTime = -100.0f;
}

bool UPiSimAutopilotManager::ShouldDriveActuators() const
{
    return ControlMode != EPiSimControlMode::DirectROS2 && Telemetry.bReceivingActuators;
}

void UPiSimAutopilotManager::TickDirectRos()
{
    Telemetry.bLinkUp = false;
    Telemetry.bReceivingActuators = false;
    Telemetry.StatusLine = TEXT("Direkt ROS 2 (Pi 5 /cmd_vel + IMU 7400/7401)");
}

void UPiSimAutopilotManager::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    APiSimModelImporter* OwnerPawn = Cast<APiSimModelImporter>(GetOwner());
    if (!OwnerPawn)
    {
        return;
    }

    UPiSimVirtualSensorSuite* Sensors = OwnerPawn->VirtualSensors;
    if (Sensors && OwnerPawn->VisualMeshComponents.IsValidIndex(0) && OwnerPawn->VisualMeshComponents[0])
    {
        UProceduralMeshComponent* Chassis = OwnerPawn->VisualMeshComponents[0];
        Sensors->UpdateSensors(
            DeltaTime,
            Chassis->GetComponentLocation(),
            Chassis->GetPhysicsLinearVelocity(),
            OwnerPawn->CurrentLinearAccel,
            Chassis->GetPhysicsAngularVelocityInDegrees(),
            Chassis->GetComponentQuat(),
            OwnerPawn->AeroConfig.CurrentAirspeedKmh);
    }

    switch (ControlMode)
    {
    case EPiSimControlMode::DirectROS2:
        break;
    case EPiSimControlMode::ArduPilotSITL:
        TickArduPilotSitl(DeltaTime);
        break;
    case EPiSimControlMode::PX4SITL:
        TickPx4Mavlink(DeltaTime, false);
        break;
    case EPiSimControlMode::PX4HITL:
        TickPx4Mavlink(DeltaTime, true);
        break;
    }

    RateWindowTimer += DeltaTime;
    if (RateWindowTimer >= 0.5f)
    {
        Telemetry.SensorTxHz = SensorTxInWindow / RateWindowTimer;
        Telemetry.ActuatorRxHz = ActuatorRxInWindow / RateWindowTimer;
        SensorTxInWindow = 0;
        ActuatorRxInWindow = 0;
        RateWindowTimer = 0.0f;
    }

    const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    Telemetry.bReceivingActuators = (Now - LastActuatorTime) < 1.0f;
    if (!Telemetry.bReceivingActuators)
    {
        Telemetry.bArmed = false;
    }
}

void UPiSimAutopilotManager::TickArduPilotSitl(float DeltaTime)
{
    if (!ArduSocket)
    {
        return;
    }

    DrainUdp(ArduSocket, [this](const TArray<uint8>& Bytes, const FString& SenderIP, int32 SenderPort)
    {
        LastArduSenderIP = SenderIP;
        LastArduSenderPort = SenderPort;
        Telemetry.bLinkUp = true;

        if (Bytes.Num() >= 40)
        {
            uint16 Magic = static_cast<uint16>(Bytes[0] | (Bytes[1] << 8));
            if (Magic == ArduJsonMagic)
            {
                uint16 Pwm[16] = {};
                for (int32 i = 0; i < 16; ++i)
                {
                    const int32 Off = 8 + i * 2;
                    if (Off + 1 < Bytes.Num())
                    {
                        Pwm[i] = static_cast<uint16>(Bytes[Off] | (Bytes[Off + 1] << 8));
                    }
                }
                HandleArduPilotPwm(Pwm, 16);
            }
        }

        FString JsonStr;
        FFileHelper::BufferToString(JsonStr, Bytes.GetData(), Bytes.Num());
        if (JsonStr.Contains(TEXT("pwm")) || JsonStr.Contains(TEXT("servo")))
        {
            TSharedPtr<FJsonObject> Root;
            const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
            if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
            {
                const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
                if (Root->TryGetArrayField(TEXT("pwm"), Arr) || Root->TryGetArrayField(TEXT("servo"), Arr))
                {
                    uint16 Pwm[16] = {};
                    int32 Count = 0;
                    if (Arr)
                    {
                        Count = FMath::Min(16, Arr->Num());
                        for (int32 i = 0; i < Count; ++i)
                        {
                            Pwm[i] = static_cast<uint16>((*Arr)[i]->AsNumber());
                        }
                    }
                    HandleArduPilotPwm(Pwm, Count);
                }
            }
        }

        APiSimModelImporter* OwnerPawn = Cast<APiSimModelImporter>(GetOwner());
        if (OwnerPawn && OwnerPawn->VirtualSensors)
        {
            const double SimTimeSec = (GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.0;
            const FString Json = OwnerPawn->VirtualSensors->BuildArduPilotJsonPayload(SimTimeSec);
            FTCHARToUTF8 Utf8(*Json);
            TArray<uint8> OutBytes;
            OutBytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
            SendBytes(ArduSocket, OutBytes, LastArduSenderIP, LastArduSenderPort);
            ++SensorTxInWindow;
        }
    });
}

void UPiSimAutopilotManager::TickPx4Mavlink(float DeltaTime, bool bUseSerial)
{
    if (bUseSerial)
    {
        DrainSerial();
    }
    else if (Px4TcpSocket)
    {
        DrainTcp();
    }
    else
    {
        DrainUdp(Px4UdpSocket, [this](const TArray<uint8>& Bytes, const FString&, int32)
        {
            MavParseBuffer.Append(Bytes);
        });
    }

    TArray<FPiSimMavlinkPacket> Packets;
    FPiSimMavlink::ParseStream(MavParseBuffer, Packets);
    for (const FPiSimMavlinkPacket& Pkt : Packets)
    {
        if (Pkt.MsgId == FPiSimMavlink::MSG_HIL_ACTUATOR_CONTROLS)
        {
            FPiSimHilActuatorControls Act;
            if (FPiSimMavlink::DecodeHilActuatorControls(Pkt, Act))
            {
                HandleHilActuators(Act);
            }
        }
        else if (Pkt.MsgId == FPiSimMavlink::MSG_HEARTBEAT)
        {
            Telemetry.bLinkUp = true;
        }
    }

    HilSensorTimer += DeltaTime;
    HilGpsTimer += DeltaTime;
    HeartbeatTimer += DeltaTime;

    const bool bForceGps = HilGpsTimer >= 0.1f;
    if (HilSensorTimer >= 0.004f)
    {
        SendVirtualSensors(FPlatformTime::Seconds(), bForceGps);
        HilSensorTimer = 0.0f;
        if (bForceGps)
        {
            HilGpsTimer = 0.0f;
        }
    }

    if (HeartbeatTimer >= 1.0f)
    {
        HeartbeatTimer = 0.0f;
        TArray<uint8> Hb = FPiSimMavlink::PackHeartbeat(SimSysId, SimCompId, MavSeq, 1, 8, 0, 0, 4);
        if (bUseSerial && SerialPort)
        {
            SerialPort->WriteBytes(Hb);
        }
        else if (Px4TcpSocket)
        {
            int32 Sent = 0;
            Px4TcpSocket->Send(Hb.GetData(), Hb.Num(), Sent);
        }
        else if (Px4UdpSocket)
        {
            SendBytes(Px4UdpSocket, Hb, Px4SitlHost, Px4SitlUdpPort);
        }
    }
}

void UPiSimAutopilotManager::SendVirtualSensors(double /*UnixSec*/, bool bForceGps)
{
    APiSimModelImporter* OwnerPawn = Cast<APiSimModelImporter>(GetOwner());
    if (!OwnerPawn || !OwnerPawn->VirtualSensors)
    {
        return;
    }

    const FPiSimImuSensorData& Imu = OwnerPawn->VirtualSensors->ImuData;
    const FPiSimBaroSensorData& Baro = OwnerPawn->VirtualSensors->BaroData;
    const FPiSimGpsSensorData& Gps = OwnerPawn->VirtualSensors->GpsData;
    const uint64 Tusec = UnixMicros();

    FPiSimHilSensor Sensor;
    Sensor.TimeUsec = Tusec;
    Sensor.Xacc = Imu.AccelNED.X;
    Sensor.Yacc = Imu.AccelNED.Y;
    Sensor.Zacc = Imu.AccelNED.Z;
    Sensor.Xgyro = Imu.GyroNED.X;
    Sensor.Ygyro = Imu.GyroNED.Y;
    Sensor.Zgyro = Imu.GyroNED.Z;
    Sensor.Xmag = Imu.MagNED.X;
    Sensor.Ymag = Imu.MagNED.Y;
    Sensor.Zmag = Imu.MagNED.Z;
    Sensor.AbsPressureHPa = Baro.AbsPressureHPa;
    Sensor.DiffPressureHPa = Baro.DiffPressureHPa;
    Sensor.PressureAltM = Baro.PressureAltitudeM;
    Sensor.TemperatureC = Baro.TemperatureC;

    TArray<uint8> SensorPkt = FPiSimMavlink::PackHilSensor(SimSysId, SimCompId, MavSeq, Sensor);
    TArray<uint8> GpsPkt;
    if (bForceGps)
    {
        FPiSimHilGps HilGps;
        HilGps.TimeUsec = Tusec;
        HilGps.LatE7 = static_cast<int32>(Gps.LatitudeDeg * 1.0e7);
        HilGps.LonE7 = static_cast<int32>(Gps.LongitudeDeg * 1.0e7);
        HilGps.AltMm = static_cast<int32>(Gps.AltitudeAMSL * 1000.0f);
        HilGps.Eph = static_cast<uint16>(Gps.EPH * 100.0f);
        HilGps.Epv = static_cast<uint16>(Gps.EPV * 100.0f);
        HilGps.VelCms = static_cast<uint16>(Gps.GroundSpeedMs * 100.0f);
        HilGps.VnCms = static_cast<int16>(Gps.VelNED.X * 100.0f);
        HilGps.VeCms = static_cast<int16>(Gps.VelNED.Y * 100.0f);
        HilGps.VdCms = static_cast<int16>(Gps.VelNED.Z * 100.0f);
        HilGps.CogCdeg = static_cast<uint16>(Gps.HeadingDeg * 100.0f);
        HilGps.FixType = static_cast<uint8>(Gps.FixType);
        HilGps.SatellitesVisible = static_cast<uint8>(Gps.SatellitesVisible);
        HilGps.YawCdeg = HilGps.CogCdeg;
        GpsPkt = FPiSimMavlink::PackHilGps(SimSysId, SimCompId, MavSeq, HilGps);
    }

    auto SendPkt = [this](const TArray<uint8>& Pkt)
    {
        if (Pkt.Num() == 0)
        {
            return;
        }
        if (SerialPort && SerialPort->IsOpen())
        {
            SerialPort->WriteBytes(Pkt);
        }
        else if (Px4TcpSocket)
        {
            int32 Sent = 0;
            Px4TcpSocket->Send(Pkt.GetData(), Pkt.Num(), Sent);
        }
        else if (Px4UdpSocket)
        {
            SendBytes(Px4UdpSocket, Pkt, Px4SitlHost, Px4SitlUdpPort);
        }
    };

    SendPkt(SensorPkt);
    SendPkt(GpsPkt);
    ++SensorTxInWindow;
}

void UPiSimAutopilotManager::HandleHilActuators(const FPiSimHilActuatorControls& Act)
{
    MixRoll = FMath::Clamp(Act.Controls[0], -1.0f, 1.0f);
    MixPitch = FMath::Clamp(Act.Controls[1], -1.0f, 1.0f);
    MixYaw = FMath::Clamp(Act.Controls[2], -1.0f, 1.0f);
    MixThrottle = FMath::Clamp(Act.Controls[3], 0.0f, 1.0f);
    for (int32 i = 0; i < 16; ++i)
    {
        LastRawControls[i] = Act.Controls[i];
        if (i < 8)
        {
            if (Telemetry.MotorPwmUs.Num() < 8)
            {
                Telemetry.MotorPwmUs.SetNumZeroed(8);
            }
            Telemetry.MotorPwmUs[i] = static_cast<int32>(1000.0f + FMath::Clamp(Act.Controls[i], 0.0f, 1.0f) * 1000.0f);
        }
    }
    Telemetry.bArmed = (Act.Mode & 128) != 0 || MixThrottle > 0.05f;
    ApplyMixerToOwner();
}

void UPiSimAutopilotManager::HandleArduPilotPwm(const uint16* Pwm, int32 Count)
{
    for (int32 i = 0; i < 16; ++i)
    {
        LastPwm[i] = (i < Count) ? Pwm[i] : 0;
        if (i < 8)
        {
            if (Telemetry.MotorPwmUs.Num() < 8)
            {
                Telemetry.MotorPwmUs.SetNumZeroed(8);
            }
            Telemetry.MotorPwmUs[i] = LastPwm[i];
        }
    }

    // ArduPilot plane: 1 aileron, 2 elevator, 3 throttle, 4 rudder
    MixRoll = PwmToNorm(LastPwm[0] ? LastPwm[0] : 1500, false);
    MixPitch = PwmToNorm(LastPwm[1] ? LastPwm[1] : 1500, false);
    MixThrottle = PwmToNorm(LastPwm[2] ? LastPwm[2] : 1000, true);
    MixYaw = PwmToNorm(LastPwm[3] ? LastPwm[3] : 1500, false);
    Telemetry.bArmed = MixThrottle > 0.05f;
    ApplyMixerToOwner();
}

void UPiSimAutopilotManager::ApplyMixerToOwner()
{
    LastActuatorTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    ++ActuatorRxInWindow;
    Telemetry.RollCmd = MixRoll;
    Telemetry.PitchCmd = MixPitch;
    Telemetry.YawCmd = MixYaw;
    Telemetry.ThrottleCmd = MixThrottle;

    if (APiSimModelImporter* OwnerPawn = Cast<APiSimModelImporter>(GetOwner()))
    {
        OwnerPawn->ApplyAutopilotMixer(MixRoll, MixPitch, MixYaw, MixThrottle, LastPwm, LastRawControls);
    }
}

bool UPiSimAutopilotManager::OpenDatagramSocket(FSocket*& OutSocket, int32 BindPort)
{
    DestroySocket(OutSocket);
    ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SS)
    {
        return false;
    }

    OutSocket = SS->CreateSocket(NAME_DGram, TEXT("PiSimAutopilotUdp"), true);
    if (!OutSocket)
    {
        return false;
    }

    OutSocket->SetReuseAddr(true);
    OutSocket->SetNonBlocking(true);
    TSharedRef<FInternetAddr> LocalAddr = SS->CreateInternetAddr();
    LocalAddr->SetAnyAddress();
    LocalAddr->SetPort(BindPort);
    if (BindPort > 0 && !OutSocket->Bind(*LocalAddr))
    {
        DestroySocket(OutSocket);
        return false;
    }
    return true;
}

void UPiSimAutopilotManager::DestroySocket(FSocket*& Socket)
{
    if (!Socket)
    {
        return;
    }
    if (ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM))
    {
        Socket->Close();
        SS->DestroySocket(Socket);
    }
    Socket = nullptr;
}

bool UPiSimAutopilotManager::SendBytes(FSocket* Socket, const TArray<uint8>& Data, const FString& TargetIP, int32 TargetPort)
{
    if (!Socket || Data.Num() == 0)
    {
        return false;
    }
    ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    TSharedRef<FInternetAddr> Addr = SS->CreateInternetAddr();
    bool bOk = false;
    Addr->SetIp(*TargetIP, bOk);
    Addr->SetPort(TargetPort);
    if (!bOk)
    {
        return false;
    }
    int32 Sent = 0;
    return Socket->SendTo(Data.GetData(), Data.Num(), Sent, *Addr);
}

void UPiSimAutopilotManager::DrainUdp(FSocket* Socket, TFunction<void(const TArray<uint8>&, const FString&, int32)> Handler)
{
    if (!Socket)
    {
        return;
    }
    ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    TArray<uint8> RecvBuf;
    RecvBuf.SetNumUninitialized(4096);
    for (int32 i = 0; i < 32; ++i)
    {
        uint32 Pending = 0;
        if (!Socket->HasPendingData(Pending) || Pending == 0)
        {
            break;
        }
        TSharedRef<FInternetAddr> Sender = SS->CreateInternetAddr();
        int32 Read = 0;
        if (Socket->RecvFrom(RecvBuf.GetData(), RecvBuf.Num(), Read, *Sender) && Read > 0)
        {
            TArray<uint8> Packet;
            Packet.Append(RecvBuf.GetData(), Read);
            Handler(Packet, Sender->ToString(false), Sender->GetPort());
        }
    }
}

bool UPiSimAutopilotManager::ConnectPx4Tcp()
{
    DestroySocket(Px4TcpSocket);
    ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    Px4TcpSocket = SS->CreateSocket(NAME_Stream, TEXT("PiSimPx4SitlTcp"), false);
    if (!Px4TcpSocket)
    {
        return false;
    }
    Px4TcpSocket->SetNonBlocking(true);
    TSharedRef<FInternetAddr> Addr = SS->CreateInternetAddr();
    bool bOk = false;
    Addr->SetIp(*Px4SitlHost, bOk);
    Addr->SetPort(Px4SitlTcpPort);
    if (!bOk)
    {
        DestroySocket(Px4TcpSocket);
        return false;
    }
    Px4TcpSocket->Connect(*Addr);
    return true;
}

void UPiSimAutopilotManager::DrainTcp()
{
    if (!Px4TcpSocket)
    {
        return;
    }
    TArray<uint8> RecvBuf;
    RecvBuf.SetNumUninitialized(4096);
    for (int32 i = 0; i < 16; ++i)
    {
        uint32 Pending = 0;
        if (!Px4TcpSocket->HasPendingData(Pending) || Pending == 0)
        {
            break;
        }
        int32 Read = 0;
        if (Px4TcpSocket->Recv(RecvBuf.GetData(), RecvBuf.Num(), Read) && Read > 0)
        {
            MavParseBuffer.Append(RecvBuf.GetData(), Read);
            Telemetry.bLinkUp = true;
        }
    }
}

void UPiSimAutopilotManager::DrainSerial()
{
    if (!SerialPort || !SerialPort->IsOpen())
    {
        return;
    }
    TArray<uint8> Chunk;
    if (SerialPort->ReadBytes(Chunk, 4096) > 0)
    {
        MavParseBuffer.Append(Chunk);
        Telemetry.bLinkUp = true;
    }
}
