// PiSimAutopilotBridge.cpp
// Multi-Backend Autopilot Simulation Bridge Implementation.

#include "PiSimAutopilotBridge.h"
#include "IPAddress.h"
#include "Common/TcpSocketBuilder.h"
#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

UPiSimAutopilotBridge::UPiSimAutopilotBridge()
{
    PrimaryComponentTick.bCanEverTick = true;
    bWantsInitializeComponent = true;

    ChannelPwmValues.SetNumZeroed(16);
    for (int32 i = 0; i < 16; ++i)
    {
        ChannelPwmValues[i] = (i == 2) ? 1000.0f : 1500.0f; // Default center 1500µs, throttle 1000µs
    }
}

UPiSimAutopilotBridge::~UPiSimAutopilotBridge()
{
    Shutdown();
}

void UPiSimAutopilotBridge::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Shutdown();
    Super::EndPlay(EndPlayReason);
}

void UPiSimAutopilotBridge::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Calculate packet reception rate
    RateCalcTimer += DeltaTime;
    if (RateCalcTimer >= 1.0f)
    {
        PacketRateHz = (float)RxCountInWindow / RateCalcTimer;
        RxCountInWindow = 0;
        RateCalcTimer = 0.0f;
    }

    // Connection timeout check (2.5 seconds without packets)
    float CurrentTime = (GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.0f;
    if (bIsConnected && (CurrentTime - LastPacketTime > 2.5f))
    {
        bIsConnected = false;
        LastStatusMessage = TEXT("🟡 SITL Bağlantı Zaman Aşımı (Sinyal Bekleniyor)");
    }
}

bool UPiSimAutopilotBridge::StartArduPilotSitl(int32 InListenPort)
{
    Shutdown();

    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem) return false;

    TSharedRef<FInternetAddr> LocalAddr = SocketSubsystem->CreateInternetAddr();
    LocalAddr->SetAnyAddress();
    LocalAddr->SetPort(InListenPort);

    SitlSocket = SocketSubsystem->CreateSocket(NAME_DGram, TEXT("PiSimArduPilotSitlSocket"), true);
    if (!SitlSocket) return false;

    SitlSocket->SetReuseAddr(true);
    SitlSocket->SetNonBlocking(true);
    int32 BufferSize = 2 * 1024 * 1024;
    SitlSocket->SetReceiveBufferSize(BufferSize, BufferSize);
    SitlSocket->SetSendBufferSize(BufferSize, BufferSize);

    if (!SitlSocket->Bind(*LocalAddr))
    {
        UE_LOG(LogTemp, Error, TEXT("[PiSimAutopilotBridge] ArduPilot SITL soketi port %d üzerine bağlanamadı!"), InListenPort);
        SocketSubsystem->DestroySocket(SitlSocket);
        SitlSocket = nullptr;
        return false;
    }

    bIsRunning = true;
    ConnectedPort = InListenPort;
    LastStatusMessage = FString::Printf(TEXT("🟢 ArduPilot SITL Dinleniyor (UDP Port %d)"), InListenPort);

    ListenerFuture = Async(EAsyncExecution::Thread, [this]()
    {
        ListenerLoop();
    });

    UE_LOG(LogTemp, Log, TEXT("[PiSimAutopilotBridge] ArduPilot SITL soketi UDP Port %d üzerinde başlatıldı."), InListenPort);
    return true;
}

bool UPiSimAutopilotBridge::StartPx4Sitl(int32 InListenPort)
{
    Shutdown();

    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem) return false;

    TSharedRef<FInternetAddr> LocalAddr = SocketSubsystem->CreateInternetAddr();
    LocalAddr->SetAnyAddress();
    LocalAddr->SetPort(InListenPort);

    SitlSocket = SocketSubsystem->CreateSocket(NAME_DGram, TEXT("PiSimPx4SitlSocket"), true);
    if (!SitlSocket) return false;

    SitlSocket->SetReuseAddr(true);
    SitlSocket->SetNonBlocking(true);
    int32 BufferSize = 2 * 1024 * 1024;
    SitlSocket->SetReceiveBufferSize(BufferSize, BufferSize);
    SitlSocket->SetSendBufferSize(BufferSize, BufferSize);

    if (!SitlSocket->Bind(*LocalAddr))
    {
        UE_LOG(LogTemp, Error, TEXT("[PiSimAutopilotBridge] PX4 SITL soketi port %d üzerine bağlanamadı!"), InListenPort);
        SocketSubsystem->DestroySocket(SitlSocket);
        SitlSocket = nullptr;
        return false;
    }

    bIsRunning = true;
    ConnectedPort = InListenPort;
    LastStatusMessage = FString::Printf(TEXT("🟢 PX4 SITL Dinleniyor (UDP Port %d)"), InListenPort);

    ListenerFuture = Async(EAsyncExecution::Thread, [this]()
    {
        ListenerLoop();
    });

    UE_LOG(LogTemp, Log, TEXT("[PiSimAutopilotBridge] PX4 SITL soketi UDP Port %d üzerinde başlatıldı."), InListenPort);
    return true;
}

void UPiSimAutopilotBridge::Shutdown()
{
    bIsRunning = false;

    if (SitlSocket)
    {
        SitlSocket->Close();
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(SitlSocket);
        SitlSocket = nullptr;
    }

    if (ListenerFuture.IsValid())
    {
        ListenerFuture.Wait();
    }

    bIsConnected = false;
}

void UPiSimAutopilotBridge::ListenerLoop()
{
    TArray<uint8> RecvBuffer;
    RecvBuffer.SetNumUninitialized(65535);

    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    TSharedRef<FInternetAddr> SenderAddr = SocketSubsystem->CreateInternetAddr();

    while (bIsRunning && SitlSocket)
    {
        uint32 PendingDataSize = 0;
        if (SitlSocket->HasPendingData(PendingDataSize) && PendingDataSize > 0)
        {
            int32 BytesRead = 0;
            if (SitlSocket->RecvFrom(RecvBuffer.GetData(), RecvBuffer.Num(), BytesRead, *SenderAddr) && BytesRead > 0)
            {
                TArray<uint8> PacketData;
                PacketData.Append(RecvBuffer.GetData(), BytesRead);
                FString SenderIP = SenderAddr->ToString(false);
                int32 SenderPort = SenderAddr->GetPort();

                // Dispatch to Game Thread
                AsyncTask(ENamedThreads::GameThread, [this, PacketData, SenderIP, SenderPort]()
                {
                    ProcessIncomingPacket(PacketData, SenderIP, SenderPort);
                });
            }
        }
        else
        {
            FPlatformProcess::Sleep(0.001f);
        }
    }
}

void UPiSimAutopilotBridge::ProcessIncomingPacket(const TArray<uint8>& PacketBytes, const FString& SenderIP, int32 SenderPort)
{
    TotalPacketsReceived++;
    RxCountInWindow++;
    bIsConnected = true;
    ConnectedIP = SenderIP;
    ConnectedPort = SenderPort;
    LastPacketTime = (GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.0f;

    // =========================================================================
    // 1) ARDUPILOT SITL STANDART BİNARY SERVO PAKETİ (Magic: 18458)
    // =========================================================================
    if (PacketBytes.Num() >= (int32)sizeof(FArduPilotServoPacket16))
    {
        const FArduPilotServoPacket16* ServoPacket = reinterpret_cast<const FArduPilotServoPacket16*>(PacketBytes.GetData());
        if (ServoPacket->Magic == 18458)
        {
            // Kanal 0: Aileron / Sol Elevon (1000..2000 µs -> -1.0 .. +1.0)
            NormalizedRoll = FMath::Clamp((float)(ServoPacket->PWM[0] - 1500) / 500.0f, -1.0f, 1.0f);

            // Kanal 1: Elevator / Yunuslama (1000..2000 µs -> -1.0 .. +1.0)
            NormalizedPitch = FMath::Clamp((float)(ServoPacket->PWM[1] - 1500) / 500.0f, -1.0f, 1.0f);

            // Kanal 2: Throttle / İtki Pervanesi (1000..2000 µs -> 0.0 .. 1.0)
            NormalizedThrottle = FMath::Clamp((float)(ServoPacket->PWM[2] - 1000) / 1000.0f, 0.0f, 1.0f);

            // Kanal 3: Rudder / Sapma (1000..2000 µs -> -1.0 .. +1.0)
            NormalizedYaw = FMath::Clamp((float)(ServoPacket->PWM[3] - 1500) / 500.0f, -1.0f, 1.0f);

            ChannelPwmValues.SetNum(16);
            for (int32 c = 0; c < 16; ++c)
            {
                ChannelPwmValues[c] = (float)ServoPacket->PWM[c];
            }

            LastStatusMessage = FString::Printf(TEXT("🟢 ArduPilot SITL Aktif [%.0f Hz] (Thr: %%%0.0f, Roll: %+.2f, Pitch: %+.2f)"),
                PacketRateHz, NormalizedThrottle * 100.0f, NormalizedRoll, NormalizedPitch);

            OnAutopilotCommandReceived.Broadcast(NormalizedThrottle, NormalizedRoll, NormalizedPitch, NormalizedYaw, ChannelPwmValues);
            return;
        }
    }

    // =========================================================================
    // 2) ARDUPILOT SITL JSON PAKETİ ({ "magic": 18458, "pwm": [...] })
    // =========================================================================
    if (PacketBytes.Num() > 10 && PacketBytes[0] == '{')
    {
        FString JsonStr = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(PacketBytes.GetData())));
        TSharedPtr<FJsonObject> JsonObj;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);

        if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
        {
            const TArray<TSharedPtr<FJsonValue>>* PwmValues = nullptr;
            if (JsonObj->TryGetArrayField(TEXT("pwm"), PwmValues) && PwmValues)
            {
                ChannelPwmValues.SetNum(PwmValues->Num());
                for (int32 i = 0; i < PwmValues->Num(); ++i)
                {
                    ChannelPwmValues[i] = (float)(*PwmValues)[i]->AsNumber();
                }

                if (ChannelPwmValues.Num() >= 4)
                {
                    NormalizedRoll = FMath::Clamp((ChannelPwmValues[0] - 1500.0f) / 500.0f, -1.0f, 1.0f);
                    NormalizedPitch = FMath::Clamp((ChannelPwmValues[1] - 1500.0f) / 500.0f, -1.0f, 1.0f);
                    NormalizedThrottle = FMath::Clamp((ChannelPwmValues[2] - 1000.0f) / 1000.0f, 0.0f, 1.0f);
                    NormalizedYaw = FMath::Clamp((ChannelPwmValues[3] - 1500.0f) / 500.0f, -1.0f, 1.0f);

                    LastStatusMessage = FString::Printf(TEXT("🟢 ArduPilot JSON Aktif (Gaz: %%%0.0f, Aileron: %+.2f)"),
                        NormalizedThrottle * 100.0f, NormalizedRoll);

                    OnAutopilotCommandReceived.Broadcast(NormalizedThrottle, NormalizedRoll, NormalizedPitch, NormalizedYaw, ChannelPwmValues);
                    return;
                }
            }
        }
    }

    // =========================================================================
    // 3) PX4 MAVLINK HIL_ACTUATOR_CONTROLS PAKETİ (MsgID 93)
    // =========================================================================
    if (PacketBytes.Num() >= 12 && (PacketBytes[0] == 0xFD || PacketBytes[0] == 0xFE)) // MAVLink v2 or v1
    {
        bool bIsV2 = (PacketBytes[0] == 0xFD);
        int32 PayloadOffset = bIsV2 ? 10 : 6;
        uint32 MsgId = 0;

        if (bIsV2 && PacketBytes.Num() >= 10)
        {
            MsgId = PacketBytes[7] | (PacketBytes[8] << 8) | (PacketBytes[9] << 16);
        }
        else if (!bIsV2 && PacketBytes.Num() >= 6)
        {
            MsgId = PacketBytes[5];
        }

        // MsgID 93 = HIL_ACTUATOR_CONTROLS
        if (MsgId == 93 && PacketBytes.Num() >= PayloadOffset + 68) // 8 byte time_usec + 16*4 float + 1 byte mode + 1 byte flags
        {
            const float* Controls = reinterpret_cast<const float*>(PacketBytes.GetData() + PayloadOffset + 8);

            NormalizedRoll = FMath::Clamp(Controls[0], -1.0f, 1.0f);     // Aileron
            NormalizedPitch = FMath::Clamp(Controls[1], -1.0f, 1.0f);    // Elevator
            NormalizedYaw = FMath::Clamp(Controls[2], -1.0f, 1.0f);      // Rudder
            NormalizedThrottle = FMath::Clamp(Controls[3], 0.0f, 1.0f); // Throttle

            ChannelPwmValues.SetNum(16);
            for (int32 i = 0; i < 16; ++i)
            {
                ChannelPwmValues[i] = 1500.0f + (Controls[i] * 500.0f);
            }

            LastStatusMessage = FString::Printf(TEXT("🟢 PX4 MAVLink HIL Aktif (Gaz: %%%0.0f, Elevon: %+.2f)"),
                NormalizedThrottle * 100.0f, NormalizedRoll);

            OnAutopilotCommandReceived.Broadcast(NormalizedThrottle, NormalizedRoll, NormalizedPitch, NormalizedYaw, ChannelPwmValues);
            return;
        }
    }
}

bool UPiSimAutopilotBridge::SendArduPilotStateJson(const FString& JsonPayload, const FString& TargetIP, int32 TargetPort)
{
    if (!SitlSocket) return false;

    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    TSharedRef<FInternetAddr> TargetAddr = SocketSubsystem->CreateInternetAddr();
    bool bIsValidIP = false;
    TargetAddr->SetIp(*TargetIP, bIsValidIP);
    TargetAddr->SetPort(TargetPort);

    if (!bIsValidIP) return false;

    FTCHARToUTF8 Converter(*JsonPayload);
    int32 BytesSent = 0;
    bool bSuccess = SitlSocket->SendTo((const uint8*)Converter.Get(), Converter.Length(), BytesSent, *TargetAddr);
    if (bSuccess)
    {
        TotalPacketsSent++;
    }
    return bSuccess;
}

void UPiSimAutopilotBridge::SendPx4Sensors(const FPiSimImuSensorData& Imu, const FPiSimBaroSensorData& Baro, const FPiSimGpsSensorData& Gps, const FString& TargetIP, int32 TargetPort)
{
    // Minimal MAVLink v2 HIL_SENSOR (MsgID 107) and HIL_GPS (MsgID 113) serialization
    if (!SitlSocket) return;

    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    TSharedRef<FInternetAddr> TargetAddr = SocketSubsystem->CreateInternetAddr();
    bool bIsValidIP = false;
    TargetAddr->SetIp(*TargetIP, bIsValidIP);
    TargetAddr->SetPort(TargetPort);
    if (!bIsValidIP) return;

    uint64 TimeUsec = (GetWorld()) ? (uint64)(GetWorld()->GetTimeSeconds() * 1000000.0) : 0;

    // Build MAVLink v2 HIL_SENSOR packet (64-byte payload)
    #pragma pack(push, 1)
    struct FMavlinkHilSensorPayload
    {
        uint64 time_usec;
        float xacc;
        float yacc;
        float zacc;
        float xgyro;
        float ygyro;
        float zgyro;
        float xmag;
        float ymag;
        float zmag;
        float abs_pressure;
        float diff_pressure;
        float pressure_alt;
        float temperature;
        uint32 fields_updated;
    };
    #pragma pack(pop)

    FMavlinkHilSensorPayload SensorPayload;
    SensorPayload.time_usec = TimeUsec;
    SensorPayload.xacc = Imu.AccelNED.X;
    SensorPayload.yacc = Imu.AccelNED.Y;
    SensorPayload.zacc = Imu.AccelNED.Z;
    SensorPayload.xgyro = Imu.GyroNED.X;
    SensorPayload.ygyro = Imu.GyroNED.Y;
    SensorPayload.zgyro = Imu.GyroNED.Z;
    SensorPayload.xmag = Imu.MagNED.X;
    SensorPayload.ymag = Imu.MagNED.Y;
    SensorPayload.zmag = Imu.MagNED.Z;
    SensorPayload.abs_pressure = Baro.AbsPressureHPa;
    SensorPayload.diff_pressure = Baro.DiffPressureHPa;
    SensorPayload.pressure_alt = Baro.PressureAltitudeM;
    SensorPayload.temperature = Baro.TemperatureC;
    SensorPayload.fields_updated = 0x1FFF; // All 13 fields updated

    TArray<uint8> Packet;
    Packet.SetNum(10 + sizeof(FMavlinkHilSensorPayload) + 2);

    Packet[0] = 0xFD; // MAVLink v2 magic
    Packet[1] = sizeof(FMavlinkHilSensorPayload); // Payload length
    Packet[2] = 0;    // Incompat flags
    Packet[3] = 0;    // Compat flags
    Packet[4] = MavlinkSeq++; // Sequence
    Packet[5] = 1;    // System ID (Autopilot / Sim)
    Packet[6] = 1;    // Component ID
    Packet[7] = 107;  // MsgID low byte (107 = HIL_SENSOR)
    Packet[8] = 0;    // MsgID mid byte
    Packet[9] = 0;    // MsgID high byte

    FMemory::Memcpy(Packet.GetData() + 10, &SensorPayload, sizeof(FMavlinkHilSensorPayload));

    // CRC calculation (X.25 standard)
    uint16 Crc = 0xFFFF;
    for (int32 i = 1; i < Packet.Num() - 2; ++i)
    {
        uint8 Data = Packet[i];
        uint8 Tmp = Data ^ (uint8)(Crc & 0xFF);
        Tmp ^= (Tmp << 4);
        Crc = (Crc >> 8) ^ ((uint16)Tmp << 8) ^ ((uint16)Tmp << 3) ^ ((uint16)Tmp >> 4);
    }
    // CRC Extra for HIL_SENSOR is 108
    uint8 Extra = 108;
    uint8 Tmp = Extra ^ (uint8)(Crc & 0xFF);
    Tmp ^= (Tmp << 4);
    Crc = (Crc >> 8) ^ ((uint16)Tmp << 8) ^ ((uint16)Tmp << 3) ^ ((uint16)Tmp >> 4);

    Packet[Packet.Num() - 2] = (uint8)(Crc & 0xFF);
    Packet[Packet.Num() - 1] = (uint8)(Crc >> 8);

    int32 Sent = 0;
    SitlSocket->SendTo(Packet.GetData(), Packet.Num(), Sent, *TargetAddr);
    TotalPacketsSent++;
}

FString UPiSimAutopilotBridge::GenerateArduPilotParamFile(
    const FString& VehicleType,
    float MassKg,
    float WingAreaM2,
    float WingspanM,
    int32 NumMotors,
    FString& OutBatchPath,
    const FString& CustomSavePath)
{
    float SafeMass = FMath::Max(0.2f, MassKg);
    float SafeArea = FMath::Max(0.02f, WingAreaM2);
    float SafeSpan = FMath::Max(0.2f, WingspanM);

    // 1) Aerodynamic Stall & Cruise Speed Estimation (First-Principles physics)
    // V_stall = sqrt( (2 * m * g) / (rho * S * CL_max) )
    const float AirDensityRho = 1.225f; // kg/m³
    const float CLMax = 1.35f;          // Maximum clean lift coefficient
    const float Gravity = 9.80665f;
    float VStall = FMath::Sqrt((2.0f * SafeMass * Gravity) / (AirDensityRho * SafeArea * CLMax));

    float AirspeedMin = FMath::Clamp(VStall * 1.15f, 6.0f, 35.0f);
    float AirspeedCruise = FMath::Clamp(VStall * 1.65f, 10.0f, 50.0f);
    float AirspeedMax = FMath::Clamp(VStall * 2.60f, 15.0f, 80.0f);
    float TrimArspdCm = AirspeedCruise * 100.0f;

    // 2) Build ArduPilot Parameter Content
    FString P;
    P += TEXT("# =============================================================================\n");
    P += FString::Printf(TEXT("# ArduPilot Parameter Defaults generated automatically by PiSim Studio\n"));
    P += FString::Printf(TEXT("# Vehicle Type    : %s\n"), *VehicleType);
    P += FString::Printf(TEXT("# Total Mass      : %.2f kg\n"), SafeMass);
    P += FString::Printf(TEXT("# Wing Area (S)   : %.3f m2\n"), SafeArea);
    P += FString::Printf(TEXT("# Wingspan  (b)   : %.2f m\n"), SafeSpan);
    P += FString::Printf(TEXT("# Est. Stall Speed: %.1f m/s (%.1f km/h)\n"), VStall, VStall * 3.6f);
    P += FString::Printf(TEXT("# Cruise Airspeed : %.1f m/s (%.1f km/h)\n"), AirspeedCruise, AirspeedCruise * 3.6f);
    P += TEXT("# =============================================================================\n\n");

    P += TEXT("# 1) Airspeed & Flight Envelope\n");
    P += FString::Printf(TEXT("AIRSPEED_MIN    %.2f\n"), AirspeedMin);
    P += FString::Printf(TEXT("AIRSPEED_CRUISE %.2f\n"), AirspeedCruise);
    P += FString::Printf(TEXT("AIRSPEED_MAX    %.2f\n"), AirspeedMax);
    P += FString::Printf(TEXT("TRIM_ARSPD_CM   %.0f\n"), TrimArspdCm);
    P += TEXT("TRIM_THROTTLE   45\n");
    P += TEXT("THR_MAX         100\n");
    P += TEXT("PTCH_LIM_MIN_DEG -20.00\n");
    P += TEXT("PTCH_LIM_MAX_DEG  25.00\n");
    P += TEXT("ROLL_LIMIT_DEG    60.00\n");
    P += TEXT("ARSPD_USE       1\n\n");

    P += TEXT("# 2) Servo Output Channels & Mixer\n");
    if (VehicleType.Equals(TEXT("plane-elevon"), ESearchCase::IgnoreCase))
    {
        P += TEXT("SERVO1_FUNCTION 77\n"); // Elevon Left
        P += TEXT("SERVO1_MIN      1000\n");
        P += TEXT("SERVO1_MAX      2000\n");
        P += TEXT("SERVO1_TRIM     1500\n");
        P += TEXT("SERVO2_FUNCTION 78\n"); // Elevon Right
        P += TEXT("SERVO2_MIN      1000\n");
        P += TEXT("SERVO2_MAX      2000\n");
        P += TEXT("SERVO2_TRIM     1500\n");
        P += TEXT("SERVO3_FUNCTION 70\n"); // Throttle
        P += TEXT("SERVO3_MIN      1000\n");
        P += TEXT("SERVO3_MAX      2000\n");
        P += TEXT("SERVO3_TRIM     1000\n");
        P += TEXT("SERVO4_FUNCTION 0\n");
        P += TEXT("MIXING_TYPE     1\n\n");
    }
    else if (VehicleType.Equals(TEXT("plane-vtail"), ESearchCase::IgnoreCase))
    {
        P += TEXT("SERVO1_FUNCTION 4\n");  // Aileron
        P += TEXT("SERVO1_MIN      1000\n");
        P += TEXT("SERVO1_MAX      2000\n");
        P += TEXT("SERVO1_TRIM     1500\n");
        P += TEXT("SERVO2_FUNCTION 79\n"); // V-Tail Left
        P += TEXT("SERVO2_MIN      1000\n");
        P += TEXT("SERVO2_MAX      2000\n");
        P += TEXT("SERVO2_TRIM     1500\n");
        P += TEXT("SERVO3_FUNCTION 70\n"); // Throttle
        P += TEXT("SERVO3_MIN      1000\n");
        P += TEXT("SERVO3_MAX      2000\n");
        P += TEXT("SERVO3_TRIM     1000\n");
        P += TEXT("SERVO4_FUNCTION 80\n"); // V-Tail Right
        P += TEXT("SERVO4_MIN      1000\n");
        P += TEXT("SERVO4_MAX      2000\n");
        P += TEXT("SERVO4_TRIM     1500\n");
        P += TEXT("MIXING_TYPE     2\n\n");
    }
    else
    {
        // Standard 4-Channel Plane
        P += TEXT("SERVO1_FUNCTION 4\n");  // Aileron
        P += TEXT("SERVO1_MIN      1000\n");
        P += TEXT("SERVO1_MAX      2000\n");
        P += TEXT("SERVO1_TRIM     1500\n");
        P += TEXT("SERVO2_FUNCTION 19\n"); // Elevator
        P += TEXT("SERVO2_MIN      1000\n");
        P += TEXT("SERVO2_MAX      2000\n");
        P += TEXT("SERVO2_TRIM     1500\n");
        P += TEXT("SERVO3_FUNCTION 70\n"); // Throttle
        P += TEXT("SERVO3_MIN      1000\n");
        P += TEXT("SERVO3_MAX      2000\n");
        P += TEXT("SERVO3_TRIM     1000\n");
        P += TEXT("SERVO4_FUNCTION 21\n"); // Rudder
        P += TEXT("SERVO4_MIN      1000\n");
        P += TEXT("SERVO4_MAX      2000\n");
        P += TEXT("SERVO4_TRIM     1500\n");
        P += TEXT("MIXING_TYPE     0\n\n");
    }

    P += TEXT("# 3) Safety, Telemetry & SITL Rates\n");
    P += TEXT("ARMING_CHECK    0\n");
    P += TEXT("FS_GCS_ENABL    0\n");
    P += TEXT("FS_SHORT_ACTN   0\n");
    P += TEXT("FS_LONG_ACTN    0\n");
    P += TEXT("BATT_MONITOR    4\n");
    P += TEXT("SIM_RATE_HZ     50\n");
    P += TEXT("RLL2SRV_TCONST  0.25\n");
    P += TEXT("PTCH2SRV_TCONST 0.25\n");
    P += TEXT("AHRS_EKF_TYPE   10\n");
    P += TEXT("EK3_ENABLE      0\n\n");

    // 3) Determine Save Paths
    FString FinalParamPath = CustomSavePath;
    if (FinalParamPath.IsEmpty())
    {
        FinalParamPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Robots/Config/pisim_plane.param"));
    }
    FPaths::NormalizeFilename(FinalParamPath);

    FFileHelper::SaveStringToFile(P, *FinalParamPath);

    // 4) Generate Launch Batch Script (.bat) in the same directory
    FString ConfigDir = FPaths::GetPath(FinalParamPath);
    OutBatchPath = ConfigDir / TEXT("launch_arduplane_pisim.bat");
    FPaths::NormalizeFilename(OutBatchPath);

    FString Bat;
    Bat += TEXT("@echo off\n");
    Bat += TEXT("echo ========================================================\n");
    Bat += TEXT("echo   Starting ArduPilot SITL for PiSim Aircraft Model\n");
    Bat += FString::Printf(TEXT("echo   Vehicle: %s - Mass: %.2f kg - S: %.3f m2\n"), *VehicleType, SafeMass, SafeArea);
    Bat += TEXT("echo ========================================================\n\n");
    Bat += TEXT("set ARDU_DIR=C:\\Users\\erena\\OneDrive\\Belgeler\\Mission Planner\\sitl\n");
    Bat += TEXT("if not exist \"%ARDU_DIR%\\ArduPlane.exe\" (\n");
    Bat += TEXT("    echo [HATA] ArduPlane.exe bulunamadi: %ARDU_DIR%\n");
    Bat += TEXT("    pause\n");
    Bat += TEXT("    exit /b 1\n");
    Bat += TEXT(")\n\n");
    Bat += TEXT("taskkill /F /IM ArduPlane.exe >nul 2>&1\n\n");
    Bat += TEXT("cd /d \"%ARDU_DIR%\"\n");
    Bat += FString::Printf(TEXT("ArduPlane.exe --model json:127.0.0.1 --defaults \"%s\" --home 41.1035,28.5539,120,0 --sim-port-in 9003 --sim-port-out 9002 --serial0 tcp:5760\n\n"), *FinalParamPath);
    Bat += TEXT("pause\n");

    FFileHelper::SaveStringToFile(Bat, *OutBatchPath);

    return FinalParamPath;
}

