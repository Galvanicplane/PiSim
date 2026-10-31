// Pi5Bridge.cpp

#include "Pi5Bridge.h"
#include "TelemetryGateway.h"
#include "Common/TcpSocketBuilder.h"
#include "Common/UdpSocketBuilder.h"
#include "HAL/PlatformProcess.h"
#include "GameFramework/Actor.h"
#include "Vehicle.h"
#include "TelemetryLogger.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

// --- FPi5WorkerThread İş Parçacığı Mantığı ---

FPi5WorkerThread::FPi5WorkerThread(UPiSimPi5Bridge* InOwner, bool bInIsVideoThread)
    : OwnerBridge(InOwner)
    , bIsVideoThread(bInIsVideoThread)
    , bRunning(true)
{
}

FPi5WorkerThread::~FPi5WorkerThread()
{
}

bool FPi5WorkerThread::Init()
{
    return true;
}

uint32 FPi5WorkerThread::Run()
{
    if (!OwnerBridge)
    {
        return 0;
    }

    if (bIsVideoThread)
    {
        OwnerBridge->ExecuteVideoLoop(bRunning);
    }
    else
    {
        OwnerBridge->ExecuteTelemetryLoop(bRunning);
    }

    return 0;
}

void FPi5WorkerThread::Stop()
{
    bRunning = false;
}

void FPi5WorkerThread::Exit()
{
}

// --- UPiSimPi5Bridge Bileşeni Mantığı ---

FOnPi5BeaconDiscoveredNative UPiSimPi5Bridge::OnPi5BeaconDiscovered;
FString UPiSimPi5Bridge::SharedPi5TargetIP = TEXT("");

// @state: WIP - Pi5 köprüsü bileşen yapıcısı
UPiSimPi5Bridge::UPiSimPi5Bridge()
{
    PrimaryComponentTick.bCanEverTick = false;
}

// @state: WIP - Başlangıçta Gateway ve Araç referansını bağlar, VehicleId'ye göre portları ayarlar
void UPiSimPi5Bridge::BeginPlay()
{
    Super::BeginPlay();

    if (GetOwner())
    {
        CachedGateway = GetOwner()->FindComponentByClass<UPiSimTelemetryGateway>();
        APiSimVehicle* VehicleOwner = Cast<APiSimVehicle>(GetOwner());
        if (VehicleOwner)
        {
            VehicleId = VehicleOwner->VehicleId;
        }
    }

    if (!SharedPi5TargetIP.IsEmpty() && SharedPi5TargetIP != TEXT("0.0.0.0") && SharedPi5TargetIP != TEXT("127.0.0.1"))
    {
        TargetIP = SharedPi5TargetIP;
    }

    TelemetryPort = 7400; // Sabit standart telemetri portu (Artık 10 ofsetli port karmaşası yok)
    VideoPort = 7401;     // Sabit standart canlı video portu

    // Başlangıçta simülasyon başlar başlamaz telemetri akışını otomatik başlat (Port 7400)
    StartStreaming(TargetIP, TelemetryPort, VideoPort);
}

// @state: WIP - İsteğe bağlı video aboneliğini açıp kapatır (On-Demand Video Streaming)
void UPiSimPi5Bridge::SetVideoSubscribed(bool bSubscribe)
{
    FScopeLock Lock(&VideoSubscriptionLock);
    bIsVideoSubscribed = bSubscribe;

    APiSimVehicle* VehicleOwner = Cast<APiSimVehicle>(GetOwner());
    if (VehicleOwner)
    {
        if (bSubscribe)
        {
            VehicleOwner->AddVideoSubscriber(TargetIP, VideoPort);
        }
        else
        {
            VehicleOwner->RemoveVideoSubscriber(TargetIP, VideoPort);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("📹 [UPiSimPi5Bridge] Araç ID %d Video Aboneliği: %s (Port: %d)"),
        VehicleId, bSubscribe ? TEXT("AÇIK (Canlı Yayında)") : TEXT("KAPALI (Bant Genişliği Tasarrufu)"), VideoPort);
}

// @state: WIP - Bileşen yok edilirken arka plan iş parçacıklarını güvenle kapatır
void UPiSimPi5Bridge::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopStreaming();
    Super::EndPlay(EndPlayReason);
}

// @state: WIP - Telemetri ve video UDP soketlerini açıp iki ayrı iş parçacığı başlatır
bool UPiSimPi5Bridge::StartStreaming(const FString& InTargetIP, int32 InTelemetryPort, int32 InVideoPort)
{
    TargetIP = InTargetIP;
    TelemetryPort = InTelemetryPort;
    VideoPort = InVideoPort;

    StopStreaming();

    if (!CachedGateway && GetOwner())
    {
        CachedGateway = GetOwner()->FindComponentByClass<UPiSimTelemetryGateway>();
    }

    // UDP Soketleri Oluştur
    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem)
    {
        return false;
    }

    TelemetrySocket = FUdpSocketBuilder(TEXT("PiSimPi5TelemetrySocket"))
        .AsNonBlocking()
        .AsReusable()
        .WithBroadcast()
        .WithSendBufferSize(65535);

    VideoSocket = FUdpSocketBuilder(TEXT("PiSimPi5VideoSocket"))
        .AsNonBlocking()
        .AsReusable()
        .WithBroadcast()
        .WithSendBufferSize(262144);

    DiscoverySocket = FUdpSocketBuilder(TEXT("PiSimPi5DiscoverySocket"))
        .AsNonBlocking()
        .AsReusable()
        .BoundToPort(7499)
        .WithBroadcast()
        .WithReceiveBufferSize(65535);

    // Thread 1: Telemetri Döngüsü
    TelemetryRunnable = new FPi5WorkerThread(this, false);
    TelemetryThread = FRunnableThread::Create(TelemetryRunnable, TEXT("PiSim_Pi5_TelemetryThread"));

    // Thread 2: Video Döngüsü
    VideoRunnable = new FPi5WorkerThread(this, true);
    VideoThread = FRunnableThread::Create(VideoRunnable, TEXT("PiSim_Pi5_VideoThread"));

    return true;
}

// @state: WIP - İş parçacıklarını ve ağ soketlerini temizler
void UPiSimPi5Bridge::StopStreaming()
{
    if (TelemetryRunnable)
    {
        TelemetryRunnable->Stop();
    }
    if (TelemetryThread)
    {
        TelemetryThread->WaitForCompletion();
        delete TelemetryThread;
        TelemetryThread = nullptr;
    }
    if (TelemetryRunnable)
    {
        delete TelemetryRunnable;
        TelemetryRunnable = nullptr;
    }

    if (VideoRunnable)
    {
        VideoRunnable->Stop();
    }
    if (VideoThread)
    {
        VideoThread->WaitForCompletion();
        delete VideoThread;
        VideoThread = nullptr;
    }
    if (VideoRunnable)
    {
        delete VideoRunnable;
        VideoRunnable = nullptr;
    }

    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (SocketSubsystem)
    {
        if (TelemetrySocket)
        {
            SocketSubsystem->DestroySocket(TelemetrySocket);
            TelemetrySocket = nullptr;
        }
        if (VideoSocket)
        {
            SocketSubsystem->DestroySocket(VideoSocket);
            VideoSocket = nullptr;
        }
        if (DiscoverySocket)
        {
            SocketSubsystem->DestroySocket(DiscoverySocket);
            DiscoverySocket = nullptr;
        }
    }
}

// @state: WIP - KURAL: Unreal Engine aktif olarak port 7499 üzerinden ağa probe fırlatır
void UPiSimPi5Bridge::TriggerBeaconProbe()
{
    if (!DiscoverySocket) return;

    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    APiSimVehicle* VehicleOwner = Cast<APiSimVehicle>(GetOwner());
    FString VCode = VehicleOwner && !VehicleOwner->VehicleCode.IsEmpty() ? VehicleOwner->VehicleCode : TEXT("rover_01");
    FString VName = VehicleOwner && !VehicleOwner->VehicleName.IsEmpty() ? VehicleOwner->VehicleName : TEXT("Rover 01");
    FString ProbePayload = FString::Printf(TEXT("{\"magic\":\"PISIM_PROBE\",\"source\":\"UE5_UNREAL\",\"vehicle_code\":\"%s\",\"vehicle_name\":\"%s\",\"msg\":\"HI FROM %s\"}\n"),
        *VCode, *VName, *VName);
    FTCHARToUTF8 Utf8(*ProbePayload);
    int32 BytesSent = 0;

    TArray<FString> ProbeTargets;
    ProbeTargets.Add(TEXT("255.255.255.255"));

    // Yerel adaptör adreslerini tara ve alt ağ broadcast (örn: 192.168.1.255) adreslerini ekle
    TArray<TSharedPtr<FInternetAddr>> LocalAddrs;
    if (SocketSubsystem->GetLocalAdapterAddresses(LocalAddrs))
    {
        for (const TSharedPtr<FInternetAddr>& Addr : LocalAddrs)
        {
            if (Addr.IsValid())
            {
                FString IpStr = Addr->ToString(false);
                if (!IpStr.StartsWith(TEXT("127.")) && !IpStr.StartsWith(TEXT("169.254.")))
                {
                    TArray<FString> Parts;
                    IpStr.ParseIntoArray(Parts, TEXT("."));
                    if (Parts.Num() == 4)
                    {
                        ProbeTargets.AddUnique(FString::Printf(TEXT("%s.%s.%s.255"), *Parts[0], *Parts[1], *Parts[2]));
                    }
                }
            }
        }
    }
    else
    {
        bool bCanBindAll = false;
        TSharedPtr<FInternetAddr> LocalAddr = SocketSubsystem->GetLocalHostAddr(*GLog, bCanBindAll);
        if (LocalAddr.IsValid())
        {
            FString IpStr = LocalAddr->ToString(false);
            TArray<FString> Parts;
            IpStr.ParseIntoArray(Parts, TEXT("."));
            if (Parts.Num() == 4)
            {
                ProbeTargets.AddUnique(FString::Printf(TEXT("%s.%s.%s.255"), *Parts[0], *Parts[1], *Parts[2]));
            }
        }
    }

    if (!TargetIP.IsEmpty() && TargetIP != TEXT("127.0.0.1") && TargetIP != TEXT("0.0.0.0"))
    {
        ProbeTargets.AddUnique(TargetIP);
    }

    for (const FString& TargetStr : ProbeTargets)
    {
        TSharedRef<FInternetAddr> Addr = SocketSubsystem->CreateInternetAddr();
        bool bValid = false;
        Addr->SetIp(*TargetStr, bValid);
        Addr->SetPort(7499);
        if (bValid)
        {
            DiscoverySocket->SendTo((const uint8*)Utf8.Get(), Utf8.Length(), BytesSent, *Addr);
        }
    }
}

// @state: WIP - Hedef IP ve portu günceller
bool UPiSimPi5Bridge::SetTargetIP(const FString& InIP, int32 InPort)
{
    // Pi 5 telemetri köprüsü portunun yanlışlıkla ArduPilot SITL portuna (9002 vb.) çekilmesini engelle!
    int32 SafePort = (InPort >= 9000 || InPort <= 0) ? 7400 : InPort;
    TelemetryPort = SafePort;

    if (!InIP.IsEmpty() && InIP != TEXT("0.0.0.0"))
    {
        if (InIP != TEXT("127.0.0.1"))
        {
            SharedPi5TargetIP = InIP;
            TargetIP = InIP;
        }
        else if (SharedPi5TargetIP.IsEmpty())
        {
            TargetIP = InIP;
        }
    }

    // Filodaki tüm aktif araçların hedef IP'sini senkronize et
    {
        FScopeLock Lock(&APiSimVehicle::FleetLock);
        for (APiSimVehicle* Vehicle : APiSimVehicle::ActiveFleet)
        {
            if (Vehicle && IsValid(Vehicle))
            {
                Vehicle->AddTelemetrySubscriber(TargetIP, SafePort);
                if (Vehicle->Pi5Bridge)
                {
                    Vehicle->Pi5Bridge->TargetIP = TargetIP;
                    Vehicle->Pi5Bridge->TelemetryPort = SafePort;
                }
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("🎯 [Pi5Bridge] Hedef IP/Port Tüm Filo İçin Güncellendi: %s:%d"), *TargetIP, TelemetryPort);
    return true;
}

// @state: WIP - Telemetri iş parçacığı döngüsü (Gateway'den okuyup UDP basar)
void UPiSimPi5Bridge::ExecuteTelemetryLoop(FThreadSafeBool& bRunning)
{
    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    TSharedRef<FInternetAddr> TargetAddr = SocketSubsystem->CreateInternetAddr();
    TSharedRef<FInternetAddr> LoopbackAddr = SocketSubsystem->CreateInternetAddr();
    TSharedRef<FInternetAddr> BcastAddr = SocketSubsystem->CreateInternetAddr();
    bool bIsValid = false;

    if (!SharedPi5TargetIP.IsEmpty() && SharedPi5TargetIP != TEXT("127.0.0.1") && SharedPi5TargetIP != TEXT("0.0.0.0"))
    {
        TargetIP = SharedPi5TargetIP;
    }
    if (TelemetryPort >= 9000 || TelemetryPort <= 0)
    {
        TelemetryPort = 7400;
    }

    FString CurrentTargetIP = TargetIP;
    int32 CurrentPort = TelemetryPort;
    TargetAddr->SetIp(*CurrentTargetIP, bIsValid);
    TargetAddr->SetPort(CurrentPort);

    LoopbackAddr->SetIp(TEXT("127.0.0.1"), bIsValid);
    LoopbackAddr->SetPort(CurrentPort);

    BcastAddr->SetIp(TEXT("255.255.255.255"), bIsValid);
    BcastAddr->SetPort(CurrentPort);

    // Çoklu ağ adaptörü (Wi-Fi + WSL/VirtualBox) olan bilgisayarlarda her alt ağın broadcast (örn. 192.168.1.255) adresini hazırla
    TArray<TSharedRef<FInternetAddr>> SubnetBcastAddrs;
    TArray<TSharedPtr<FInternetAddr>> LocalAddrs;
    if (SocketSubsystem->GetLocalAdapterAddresses(LocalAddrs))
    {
        TArray<FString> AddedBcasts;
        for (const TSharedPtr<FInternetAddr>& Addr : LocalAddrs)
        {
            if (Addr.IsValid())
            {
                FString IpStr = Addr->ToString(false);
                if (!IpStr.StartsWith(TEXT("127.")) && !IpStr.StartsWith(TEXT("169.254.")))
                {
                    TArray<FString> Parts;
                    IpStr.ParseIntoArray(Parts, TEXT("."));
                    if (Parts.Num() == 4)
                    {
                        FString SubBcast = FString::Printf(TEXT("%s.%s.%s.255"), *Parts[0], *Parts[1], *Parts[2]);
                        if (!AddedBcasts.Contains(SubBcast))
                        {
                            AddedBcasts.Add(SubBcast);
                            TSharedRef<FInternetAddr> SAddr = SocketSubsystem->CreateInternetAddr();
                            bool bSubValid = false;
                            SAddr->SetIp(*SubBcast, bSubValid);
                            SAddr->SetPort(CurrentPort);
                            if (bSubValid)
                            {
                                SubnetBcastAddrs.Add(SAddr);
                            }
                        }
                    }
                }
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("🚀 [Pi5Bridge] Telemetri Döngüsü Başladı -> Hedef: %s:%d (Araç ID: %d)"),
        *CurrentTargetIP, CurrentPort, VehicleId);

    int32 PacketCount = 0;
    double LastProbeTimeSec = 0.0;
    double LastTelemetrySendSec = 0.0;

    while (bRunning)
    {
        // Ortak Pi 5 IP adresi başka bir araç tarafından keşfedildiyse bu aracı da anında eşitle
        if (!SharedPi5TargetIP.IsEmpty() && SharedPi5TargetIP != TEXT("127.0.0.1") && TargetIP != SharedPi5TargetIP)
        {
            TargetIP = SharedPi5TargetIP;
        }
        if (TelemetryPort >= 9000 || TelemetryPort <= 0)
        {
            TelemetryPort = 7400;
        }

        // 1. Periyodik Probe Fırlatma (Port 7499'a 4 saniyede bir probe fırlat)
        double CurrentSec = FPlatformTime::Seconds();
        if (CurrentSec - LastProbeTimeSec >= 4.0)
        {
            LastProbeTimeSec = CurrentSec;
            if (VehicleId <= 1)
            {
                TriggerBeaconProbe();
            }
        }

        // 2. Pi 5 Keşif Radarı: Port 7499 üzerinden gelen Beacon ve Heartbeat paketlerini dinle
        if (DiscoverySocket)
        {
            uint32 PendingSize = 0;
            while (DiscoverySocket->HasPendingData(PendingSize))
            {
                TArray<uint8> RecvBuffer;
                RecvBuffer.SetNumUninitialized(FMath::Min(PendingSize, 4096u));
                TSharedRef<FInternetAddr> SenderAddr = SocketSubsystem->CreateInternetAddr();
                int32 BytesRead = 0;
                if (DiscoverySocket->RecvFrom(RecvBuffer.GetData(), RecvBuffer.Num(), BytesRead, *SenderAddr) && BytesRead > 0)
                {
                    RecvBuffer.Add(0);
                    FString RecvStr = UTF8_TO_TCHAR((const char*)RecvBuffer.GetData());
                    FString SenderIpStr = SenderAddr->ToString(false);
                    APiSimVehicle* OwnerVehicle = Cast<APiSimVehicle>(GetOwner());

                    if (RecvStr.Contains(TEXT("SUB_VEHICLE")))
                    {
                        // Pi 5 Dinamik Araç Seçim Komutu: Tüm araçlar 2Hz telemetri göndermeye devam eder, hiçbir araç pasife düşürülmez!
                        TSharedPtr<FJsonObject> JsonObj;
                        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RecvStr);
                        if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
                        {
                            FString TargetCode = JsonObj->HasField(TEXT("vehicle_code")) ? JsonObj->GetStringField(TEXT("vehicle_code")) : TEXT("");
                            FString ClientIp = JsonObj->HasField(TEXT("client_ip")) ? JsonObj->GetStringField(TEXT("client_ip")) : SenderIpStr;
                            int32 ClientPort = JsonObj->HasField(TEXT("telemetry_port")) ? JsonObj->GetIntegerField(TEXT("telemetry_port")) : 7400;
                            if (ClientPort >= 9000 || ClientPort <= 0) ClientPort = 7400;

                            if (!ClientIp.IsEmpty() && ClientIp != TEXT("0.0.0.0") && ClientIp != TEXT("127.0.0.1"))
                            {
                                SharedPi5TargetIP = ClientIp;
                            }

                            FScopeLock Lock(&APiSimVehicle::FleetLock);
                            for (APiSimVehicle* Vehicle : APiSimVehicle::ActiveFleet)
                            {
                                if (Vehicle && IsValid(Vehicle))
                                {
                                    Vehicle->AddTelemetrySubscriber(ClientIp, ClientPort);
                                    if (Vehicle->Pi5Bridge)
                                    {
                                        Vehicle->Pi5Bridge->TargetIP = ClientIp;
                                        Vehicle->Pi5Bridge->TelemetryPort = ClientPort;
                                    }
                                    if (!TargetCode.IsEmpty() && Vehicle->VehicleCode == TargetCode)
                                    {
                                        FString AckPayload = FString::Printf(TEXT("{\"magic\":\"PISIM_ACK_SUB\",\"vehicle_code\":\"%s\",\"status\":\"SUBSCRIBED\"}\n"), *Vehicle->VehicleCode);
                                        FTCHARToUTF8 Utf8Ack(*AckPayload);
                                        int32 AckSent = 0;
                                        DiscoverySocket->SendTo((const uint8*)Utf8Ack.Get(), Utf8Ack.Length(), AckSent, *SenderAddr);
                                    }
                                }
                            }
                        }
                    }
                    else if (RecvStr.Contains(TEXT("SUB_VIDEO")))
                    {
                        // Pi 5 Canlı Video Açma/Kapatma Komutu: {"cmd":"SUB_VIDEO", "vehicle_code":"...", "subscribe":true/false, "video_port":7401}
                        TSharedPtr<FJsonObject> JsonObj;
                        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RecvStr);
                        if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
                        {
                            FString TargetCode = JsonObj->HasField(TEXT("vehicle_code")) ? JsonObj->GetStringField(TEXT("vehicle_code")) : TEXT("");
                            int32 TargetId = JsonObj->HasField(TEXT("vehicle_id")) ? JsonObj->GetIntegerField(TEXT("vehicle_id")) : -1;
                            bool bSub = JsonObj->HasField(TEXT("subscribe")) ? JsonObj->GetBoolField(TEXT("subscribe")) : true;
                            FString ClientIp = JsonObj->HasField(TEXT("client_ip")) ? JsonObj->GetStringField(TEXT("client_ip")) : SenderIpStr;
                            int32 ClientPort = JsonObj->HasField(TEXT("video_port")) ? JsonObj->GetIntegerField(TEXT("video_port")) : 7401;

                            if (!ClientIp.IsEmpty() && ClientIp != TEXT("0.0.0.0") && ClientIp != TEXT("127.0.0.1"))
                            {
                                SharedPi5TargetIP = ClientIp;
                            }

                            // Çoklu araç desteği: Sadece seçili aracın videosunu aç, diğer araçların video yayınını kapat
                            FScopeLock Lock(&APiSimVehicle::FleetLock);
                            for (APiSimVehicle* Vehicle : APiSimVehicle::ActiveFleet)
                            {
                                if (Vehicle && IsValid(Vehicle))
                                {
                                    bool bMatch = false;
                                    if (!TargetCode.IsEmpty())
                                    {
                                        bMatch = (Vehicle->VehicleCode == TargetCode || Vehicle->VehicleCode.StartsWith(TargetCode) || TargetCode.StartsWith(Vehicle->VehicleCode));
                                    }
                                    else if (TargetId >= 0)
                                    {
                                        bMatch = (Vehicle->VehicleId == TargetId);
                                    }
                                    else
                                    {
                                        bMatch = (Vehicle->bIsFocusedVehicle || Vehicle == APiSimVehicle::ActiveFleet[0]);
                                    }

                                    if (bMatch)
                                    {
                                        if (bSub)
                                        {
                                            Vehicle->AddVideoSubscriber(ClientIp, ClientPort);
                                        }
                                        else
                                        {
                                            Vehicle->RemoveVideoSubscriber(ClientIp, ClientPort);
                                        }
                                        if (Vehicle->Pi5Bridge)
                                        {
                                            Vehicle->Pi5Bridge->bIsVideoSubscribed = bSub;
                                        }
                                        UE_LOG(LogTemp, Warning, TEXT("📹 [Pi5Bridge] Araç [%s] (%d) Video Aboneliği: %s -> %s:%d"),
                                            *Vehicle->VehicleCode, Vehicle->VehicleId, bSub ? TEXT("AÇIK") : TEXT("KAPALI"), *ClientIp, ClientPort);

                                        // Pi 5'e anında onay (ACK) paketi dön
                                        FString AckPayload = FString::Printf(TEXT("{\"magic\":\"PISIM_ACK_VIDEO\",\"vehicle_code\":\"%s\",\"subscribed\":%s}\n"), *Vehicle->VehicleCode, bSub ? TEXT("true") : TEXT("false"));
                                        FTCHARToUTF8 Utf8Ack(*AckPayload);
                                        int32 AckSent = 0;
                                        DiscoverySocket->SendTo((const uint8*)Utf8Ack.Get(), Utf8Ack.Length(), AckSent, *SenderAddr);
                                    }
                                    else if (bSub)
                                    {
                                        // Başka bir aracın videosuna abone olunduysa diğer araçların video yayınını durdur
                                        Vehicle->RemoveVideoSubscriber(ClientIp, ClientPort);
                                        if (Vehicle->Pi5Bridge)
                                        {
                                            Vehicle->Pi5Bridge->bIsVideoSubscribed = false;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    else if (RecvStr.Contains(TEXT("PISIM_BEACON")) || RecvStr.Contains(TEXT("Raspberry Pi")) || RecvStr.Contains(TEXT("HI FROM PI5")))
                    {
                        if (SenderIpStr != TEXT("0.0.0.0"))
                        {
                            SharedPi5TargetIP = SenderIpStr;
                            if (TargetIP != SenderIpStr)
                            {
                                TargetIP = SenderIpStr;
                                TelemetryPort = 7400;
                                UE_LOG(LogTemp, Warning, TEXT("⚡ [Pi5Bridge] 🎉 RASPBERRY PI 5 EŞLEŞTİRİLDİ! IP: %s (Port: %d)"), *TargetIP, TelemetryPort);
                            }
                            // Filodaki tüm araçların hedef IP'sini anında Pi 5 IP'sine eşitle
                            {
                                FScopeLock Lock(&APiSimVehicle::FleetLock);
                                for (APiSimVehicle* Vehicle : APiSimVehicle::ActiveFleet)
                                {
                                    if (Vehicle && IsValid(Vehicle) && Vehicle->Pi5Bridge)
                                    {
                                        Vehicle->Pi5Bridge->TargetIP = SenderIpStr;
                                        Vehicle->Pi5Bridge->TelemetryPort = 7400;
                                    }
                                }
                            }
                            OnPi5BeaconDiscovered.Broadcast(RecvStr);
                        }
                    }
                    else if (RecvStr.Contains(TEXT("PISIM_REQ_SQL_TABLE")))
                    {
                        if (OwnerVehicle && OwnerVehicle->TelemetryLogger)
                        {
                            FString TableJson = OwnerVehicle->TelemetryLogger->GetRecentTelemetryTableJson(50);
                            FString RespPayload = FString::Printf(TEXT("{\"magic\":\"PISIM_RESP_SQL_TABLE\",\"data\":%s}\n"), *TableJson);
                            FTCHARToUTF8 Utf8Resp(*RespPayload);
                            int32 RespBytes = 0;
                            DiscoverySocket->SendTo((const uint8*)Utf8Resp.Get(), Utf8Resp.Length(), RespBytes, *SenderAddr);
                            UE_LOG(LogTemp, Warning, TEXT("📤 [Pi5Bridge] Pi 5'e SQL Telemetri Tablosu Gönderildi -> %s"), *SenderAddr->ToString(true));
                        }
                    }
                }
            }
        }

        // IP veya Port dışarıdan (Beacon veya UI ile) güncellendiğinde soket hedefini anında yenile
        if (TargetIP != CurrentTargetIP || TelemetryPort != CurrentPort)
        {
            CurrentTargetIP = TargetIP;
            CurrentPort = TelemetryPort;
            TargetAddr->SetIp(*CurrentTargetIP, bIsValid);
            TargetAddr->SetPort(CurrentPort);
            LoopbackAddr->SetPort(CurrentPort);
            BcastAddr->SetPort(CurrentPort);
            for (TSharedRef<FInternetAddr>& SAddr : SubnetBcastAddrs)
            {
                SAddr->SetPort(CurrentPort);
            }
            UE_LOG(LogTemp, Warning, TEXT("🔄 [Pi5Bridge] Araç #%d Hedef IP/Port Güncellendi: %s:%d"), VehicleId, *CurrentTargetIP, CurrentPort);
        }

        if (!CachedGateway && GetOwner())
        {
            CachedGateway = GetOwner()->FindComponentByClass<UPiSimTelemetryGateway>();
        }

        APiSimVehicle* VehicleOwner = Cast<APiSimVehicle>(GetOwner());
        double TelemetryIntervalSec = 1.0 / FMath::Max(1.0f, TelemetryRateHz);
        // Her araç bağımsız 2Hz telemetrisini doğrudan hedef cihaza (Pi 5 veya yerel IP) ve tüm alt ağlara aktarır
        if (VehicleOwner && CachedGateway && TelemetrySocket && !TargetIP.IsEmpty() && (CurrentSec - LastTelemetrySendSec >= TelemetryIntervalSec))
        {
            LastTelemetrySendSec = CurrentSec;
            FString JsonPayload = CachedGateway->BuildSitlJsonState();
            FTCHARToUTF8 Utf8(*JsonPayload);
            int32 BytesSent = 0;

            // 1. Ana hedef cihaza (Pi 5 veya yerel istasyona) gönder
            TelemetrySocket->SendTo((const uint8*)Utf8.Get(), Utf8.Length(), BytesSent, *TargetAddr);

            // 1b. Aynı bilgisayarda çalışan Pi 5 Dashboard / Monitör için 127.0.0.1:7400 adresine de gönder
            if (CurrentTargetIP != TEXT("127.0.0.1"))
            {
                TelemetrySocket->SendTo((const uint8*)Utf8.Get(), Utf8.Length(), BytesSent, *LoopbackAddr);
            }

            // 1c. Yerel ağdaki tüm Pi 5 cihazlarına (Genel + Alt Ağ Broadcast) ulaştır (x.x.x.255:7400)
            TelemetrySocket->SendTo((const uint8*)Utf8.Get(), Utf8.Length(), BytesSent, *BcastAddr);
            for (const TSharedRef<FInternetAddr>& SAddr : SubnetBcastAddrs)
            {
                TelemetrySocket->SendTo((const uint8*)Utf8.Get(), Utf8.Length(), BytesSent, *SAddr);
            }

            // 2. Varsa kayıtlı ek abonelere de ilet
            for (const FPiSimClientEndpoint& Sub : VehicleOwner->Subscriptions.TelemetrySubscribers)
            {
                if (Sub.IPAddress != CurrentTargetIP && Sub.IPAddress != TEXT("127.0.0.1"))
                {
                    TSharedRef<FInternetAddr> SubAddr = SocketSubsystem->CreateInternetAddr();
                    bool bValid = false;
                    SubAddr->SetIp(*Sub.IPAddress, bValid);
                    int32 SubPort = (Sub.Port > 0 && Sub.Port < 9000) ? Sub.Port : 7400;
                    SubAddr->SetPort(SubPort);
                    if (bValid)
                    {
                        TelemetrySocket->SendTo((const uint8*)Utf8.Get(), Utf8.Length(), BytesSent, *SubAddr);
                    }
                }
            }

            PacketCount++;
            if (PacketCount == 1 || PacketCount % 500 == 0)
            {
                UE_LOG(LogTemp, Log, TEXT("📡 [Pi5Bridge] Canlı Telemetri İletildi (#%d) -> Araç %s (ID: %d) -> Hedef: %s:%d"),
                    PacketCount, *VehicleOwner->VehicleCode, VehicleOwner->VehicleId, *CurrentTargetIP, CurrentPort);
            }
        }

        // DiscoverySocket (Port 7499 - SUB_VIDEO, SUB_VEHICLE, BEACON) mesajlarını 25ms gecikmeyle anında yakala
        FPlatformProcess::Sleep(0.025f);
    }

    UE_LOG(LogTemp, Warning, TEXT("🛑 [Pi5Bridge] Telemetri Döngüsü Durduruldu."));
}

// @state: WIP - Canlı video akış iş parçacığı döngüsü (Sadece abone olunduğunda JPEG karelerini UDP basar)
void UPiSimPi5Bridge::ExecuteVideoLoop(FThreadSafeBool& bRunning)
{
    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    TSharedRef<FInternetAddr> TargetAddr = SocketSubsystem->CreateInternetAddr();
    TSharedRef<FInternetAddr> LoopbackAddr = SocketSubsystem->CreateInternetAddr();
    bool bIsValid = false;

    FString CurrentTargetIP = TargetIP;
    int32 CurrentPort = VideoPort;
    TargetAddr->SetIp(*CurrentTargetIP, bIsValid);
    TargetAddr->SetPort(CurrentPort);
    LoopbackAddr->SetIp(TEXT("127.0.0.1"), bIsValid);
    LoopbackAddr->SetPort(CurrentPort);

    TArray<uint8> JpegBytes;

    while (bRunning)
    {
        if (!SharedPi5TargetIP.IsEmpty() && SharedPi5TargetIP != TEXT("127.0.0.1") && TargetIP != SharedPi5TargetIP)
        {
            TargetIP = SharedPi5TargetIP;
        }
        if (TargetIP != CurrentTargetIP || VideoPort != CurrentPort)
        {
            CurrentTargetIP = TargetIP;
            CurrentPort = VideoPort;
            TargetAddr->SetIp(*CurrentTargetIP, bIsValid);
            TargetAddr->SetPort(CurrentPort);
            LoopbackAddr->SetPort(CurrentPort);
        }

        if (!CachedGateway && GetOwner())
        {
            CachedGateway = GetOwner()->FindComponentByClass<UPiSimTelemetryGateway>();
        }

        bool bSubscribed = false;
        APiSimVehicle* VehicleOwner = Cast<APiSimVehicle>(GetOwner());
        if (VehicleOwner)
        {
            bSubscribed = VehicleOwner->HasVideoSubscribers() || bIsVideoSubscribed;
        }
        else
        {
            FScopeLock SubLock(&VideoSubscriptionLock);
            bSubscribed = bIsVideoSubscribed;
        }

        // On-demand: Sadece kullanıcı veya uç cihaz video izleme talebinde bulunduysa UDP paketi bas (ASLA DEPOLANMAZ)
        if (bSubscribed && CachedGateway && VideoSocket)
        {
            if (CachedGateway->GetLatestCameraFrame(JpegBytes) && JpegBytes.Num() > 0 && JpegBytes.Num() < 65000)
            {
                int32 BytesSent = 0;
                // 1. Ana hedefe (Pi 5 IP:7401) gönder
                VideoSocket->SendTo(JpegBytes.GetData(), JpegBytes.Num(), BytesSent, *TargetAddr);

                // 2. Aynı makinedeki testler için 127.0.0.1:7401 adresine de gönder
                if (CurrentTargetIP != TEXT("127.0.0.1"))
                {
                    VideoSocket->SendTo(JpegBytes.GetData(), JpegBytes.Num(), BytesSent, *LoopbackAddr);
                }

                // 3. Kayıtlı diğer video abonelerine gönder
                if (VehicleOwner && VehicleOwner->Subscriptions.VideoSubscribers.Num() > 0)
                {
                    for (const FPiSimClientEndpoint& Sub : VehicleOwner->Subscriptions.VideoSubscribers)
                    {
                        if (Sub.IPAddress != CurrentTargetIP && Sub.IPAddress != TEXT("127.0.0.1"))
                        {
                            TSharedRef<FInternetAddr> SubAddr = SocketSubsystem->CreateInternetAddr();
                            bool bValid = false;
                            SubAddr->SetIp(*Sub.IPAddress, bValid);
                            SubAddr->SetPort(Sub.Port > 0 ? Sub.Port : 7401);
                            if (bValid)
                            {
                                VideoSocket->SendTo(JpegBytes.GetData(), JpegBytes.Num(), BytesSent, *SubAddr);
                            }
                        }
                    }
                }
            }
        }

        FPlatformProcess::Sleep(0.050f); // ~20 FPS
    }
}
