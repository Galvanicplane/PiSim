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
    TargetIP = InIP;
    TelemetryPort = InPort;

    APiSimVehicle* VehicleOwner = Cast<APiSimVehicle>(GetOwner());
    if (VehicleOwner)
    {
        VehicleOwner->AddTelemetrySubscriber(InIP, InPort);
    }

    UE_LOG(LogTemp, Warning, TEXT("🎯 [Pi5Bridge] Hedef IP/Port Güncellendi: %s:%d"), *TargetIP, TelemetryPort);
    return true;
}

// @state: WIP - Telemetri iş parçacığı döngüsü (Gateway'den okuyup UDP basar)
void UPiSimPi5Bridge::ExecuteTelemetryLoop(FThreadSafeBool& bRunning)
{
    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    TSharedRef<FInternetAddr> TargetAddr = SocketSubsystem->CreateInternetAddr();
    TSharedRef<FInternetAddr> BcastAddr = SocketSubsystem->CreateInternetAddr();
    bool bIsValid = false;

    FString CurrentTargetIP = TargetIP;
    int32 CurrentPort = TelemetryPort;
    TargetAddr->SetIp(*CurrentTargetIP, bIsValid);
    TargetAddr->SetPort(CurrentPort);

    BcastAddr->SetIp(TEXT("255.255.255.255"), bIsValid);
    BcastAddr->SetPort(CurrentPort);

    UE_LOG(LogTemp, Warning, TEXT("🚀 [Pi5Bridge] Telemetri Döngüsü Başladı -> Hedef: %s:%d (Araç ID: %d)"),
        *CurrentTargetIP, CurrentPort, VehicleId);

    int32 PacketCount = 0;
    double LastProbeTimeSec = 0.0;

    while (bRunning)
    {
        // 1. Periyodik Probe Fırlatma (Port 7499'a 1.5 saniyede bir probe fırlat)
        double CurrentSec = FPlatformTime::Seconds();
        if (CurrentSec - LastProbeTimeSec >= 4.0)
        {
            LastProbeTimeSec = CurrentSec;
            // Sadece 0. araç periyodik probe fırlatsın, çoklu araçlarda soket kirliliği oluşmasın
            if (VehicleId == 0)
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
                        // Pi 5 Dinamik Araç Abonelik Komutu: {"cmd":"SUB_VEHICLE", "vehicle_code":"rover_01_...", "target_ip":"...", "telemetry_port":7400}
                        TSharedPtr<FJsonObject> JsonObj;
                        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RecvStr);
                        if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
                        {
                            FString TargetCode = JsonObj->HasField(TEXT("vehicle_code")) ? JsonObj->GetStringField(TEXT("vehicle_code")) : TEXT("");
                            int32 TargetId = JsonObj->HasField(TEXT("vehicle_id")) ? JsonObj->GetIntegerField(TEXT("vehicle_id")) : -1;
                            FString ClientIp = JsonObj->HasField(TEXT("client_ip")) ? JsonObj->GetStringField(TEXT("client_ip")) : SenderIpStr;
                            int32 ClientPort = JsonObj->HasField(TEXT("telemetry_port")) ? JsonObj->GetIntegerField(TEXT("telemetry_port")) : 7400;

                            // Çoklu araç desteği: Gelen abonelik isteğini seviyedeki tüm aktif araçlara dağıt
                            FScopeLock Lock(&APiSimVehicle::FleetLock);
                            for (APiSimVehicle* Vehicle : APiSimVehicle::ActiveFleet)
                            {
                                if (Vehicle && IsValid(Vehicle))
                                {
                                    bool bMatch = (!TargetCode.IsEmpty() && (Vehicle->VehicleCode == TargetCode || Vehicle->VehicleCode.StartsWith(TargetCode) || TargetCode.StartsWith(Vehicle->VehicleCode))) ||
                                                  (TargetId >= 0 && Vehicle->VehicleId == TargetId);
                                    if (bMatch)
                                    {
                                        Vehicle->AddTelemetrySubscriber(ClientIp, ClientPort);
                                        TargetIP = ClientIp;
                                        TelemetryPort = ClientPort;
                                        UE_LOG(LogTemp, Warning, TEXT("🎯 [Pi5Bridge] Araç [%s] (%d) CANLI TELEMETRİ ABONESİ EKLENDİ -> %s:%d"),
                                            *Vehicle->VehicleCode, Vehicle->VehicleId, *ClientIp, ClientPort);

                                        // Pi 5'e anında onay (ACK) paketi dön
                                        FString AckPayload = FString::Printf(TEXT("{\"magic\":\"PISIM_ACK_SUB\",\"vehicle_code\":\"%s\",\"status\":\"SUBSCRIBED\"}\n"), *Vehicle->VehicleCode);
                                        FTCHARToUTF8 Utf8Ack(*AckPayload);
                                        int32 AckSent = 0;
                                        DiscoverySocket->SendTo((const uint8*)Utf8Ack.Get(), Utf8Ack.Length(), AckSent, *SenderAddr);
                                    }
                                    else
                                    {
                                        Vehicle->RemoveTelemetrySubscriber(ClientIp, ClientPort);
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

                            // Çoklu araç desteği: Canlı video isteğini seviyedeki tüm aktif araçlara dağıt
                            FScopeLock Lock(&APiSimVehicle::FleetLock);
                            for (APiSimVehicle* Vehicle : APiSimVehicle::ActiveFleet)
                            {
                                if (Vehicle && IsValid(Vehicle))
                                {
                                    bool bMatch = (!TargetCode.IsEmpty() && (Vehicle->VehicleCode == TargetCode || Vehicle->VehicleCode.StartsWith(TargetCode) || TargetCode.StartsWith(Vehicle->VehicleCode))) ||
                                                  (TargetId >= 0 && Vehicle->VehicleId == TargetId);
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
                                        bIsVideoSubscribed = bSub;
                                        UE_LOG(LogTemp, Warning, TEXT("📹 [Pi5Bridge] Araç [%s] (%d) Video Aboneliği: %s -> %s:%d"),
                                            *Vehicle->VehicleCode, Vehicle->VehicleId, bSub ? TEXT("AÇIK") : TEXT("KAPALI"), *ClientIp, ClientPort);

                                        // Pi 5'e anında onay (ACK) paketi dön
                                        FString AckPayload = FString::Printf(TEXT("{\"magic\":\"PISIM_ACK_VIDEO\",\"vehicle_code\":\"%s\",\"subscribed\":%s}\n"), *Vehicle->VehicleCode, bSub ? TEXT("true") : TEXT("false"));
                                        FTCHARToUTF8 Utf8Ack(*AckPayload);
                                        int32 AckSent = 0;
                                        DiscoverySocket->SendTo((const uint8*)Utf8Ack.Get(), Utf8Ack.Length(), AckSent, *SenderAddr);
                                    }
                                }
                            }
                        }
                    }
                    else if (RecvStr.Contains(TEXT("PISIM_BEACON")) || RecvStr.Contains(TEXT("Raspberry Pi")) || RecvStr.Contains(TEXT("HI FROM PI5")))
                    {
                        if (SenderIpStr != TEXT("0.0.0.0"))
                        {
                            if (TargetIP != SenderIpStr)
                            {
                                TargetIP = SenderIpStr;
                                UE_LOG(LogTemp, Warning, TEXT("⚡ [Pi5Bridge] 🎉 RASPBERRY PI 5 EŞLEŞTİRİLDİ! IP: %s (Port: %d)"), *TargetIP, TelemetryPort);
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
            BcastAddr->SetPort(CurrentPort);
            UE_LOG(LogTemp, Warning, TEXT("🔄 [Pi5Bridge] Hedef IP/Port Güncellendi: %s:%d"), *CurrentTargetIP, CurrentPort);
        }

        if (!CachedGateway && GetOwner())
        {
            CachedGateway = GetOwner()->FindComponentByClass<UPiSimTelemetryGateway>();
        }

        APiSimVehicle* VehicleOwner = Cast<APiSimVehicle>(GetOwner());
        // Her araç bağımsız 2Hz telemetrisini doğrudan hedef cihaza (Pi 5 veya yerel IP) ve varsa abonelere aktarır
        if (VehicleOwner && CachedGateway && TelemetrySocket && !TargetIP.IsEmpty())
        {
            FString JsonPayload = CachedGateway->BuildSitlJsonState();
            FTCHARToUTF8 Utf8(*JsonPayload);
            int32 BytesSent = 0;

            // 1. Ana hedef cihaza (Pi 5 veya yerel istasyona) gönder
            TelemetrySocket->SendTo((const uint8*)Utf8.Get(), Utf8.Length(), BytesSent, *TargetAddr);

            // 1b. Yerel ağdaki tüm Pi 5 cihazlarına (LAN Broadcast) ulaştır (255.255.255.255:7400)
            TelemetrySocket->SendTo((const uint8*)Utf8.Get(), Utf8.Length(), BytesSent, *BcastAddr);

            // 2. Varsa kayıtlı ek abonelere de ilet
            for (const FPiSimClientEndpoint& Sub : VehicleOwner->Subscriptions.TelemetrySubscribers)
            {
                if (Sub.IPAddress != TargetIP)
                {
                    TSharedRef<FInternetAddr> SubAddr = SocketSubsystem->CreateInternetAddr();
                    bool bValid = false;
                    SubAddr->SetIp(*Sub.IPAddress, bValid);
                    SubAddr->SetPort(Sub.Port > 0 ? Sub.Port : 7400);
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

        float SleepSeconds = 1.0f / FMath::Max(1.0f, TelemetryRateHz);
        FPlatformProcess::Sleep(SleepSeconds);
    }

    UE_LOG(LogTemp, Warning, TEXT("🛑 [Pi5Bridge] Telemetri Döngüsü Durduruldu."));
}

// @state: WIP - Canlı video akış iş parçacığı döngüsü (Sadece abone olunduğunda JPEG karelerini UDP basar)
void UPiSimPi5Bridge::ExecuteVideoLoop(FThreadSafeBool& bRunning)
{
    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    TSharedRef<FInternetAddr> TargetAddr = SocketSubsystem->CreateInternetAddr();
    bool bIsValid = false;

    FString CurrentTargetIP = TargetIP;
    int32 CurrentPort = VideoPort;
    TargetAddr->SetIp(*CurrentTargetIP, bIsValid);
    TargetAddr->SetPort(CurrentPort);

    TArray<uint8> JpegBytes;

    while (bRunning)
    {
        if (TargetIP != CurrentTargetIP || VideoPort != CurrentPort)
        {
            CurrentTargetIP = TargetIP;
            CurrentPort = VideoPort;
            TargetAddr->SetIp(*CurrentTargetIP, bIsValid);
            TargetAddr->SetPort(CurrentPort);
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
            if (CachedGateway->GetLatestCameraFrame(JpegBytes) && JpegBytes.Num() > 0)
            {
                if (VehicleOwner && VehicleOwner->Subscriptions.VideoSubscribers.Num() > 0)
                {
                    for (const FPiSimClientEndpoint& Sub : VehicleOwner->Subscriptions.VideoSubscribers)
                    {
                        TSharedRef<FInternetAddr> SubAddr = SocketSubsystem->CreateInternetAddr();
                        bool bValid = false;
                        SubAddr->SetIp(*Sub.IPAddress, bValid);
                        SubAddr->SetPort(Sub.Port > 0 ? Sub.Port : 7401);
                        if (bValid)
                        {
                            int32 BytesSent = 0;
                            VideoSocket->SendTo(JpegBytes.GetData(), JpegBytes.Num(), BytesSent, *SubAddr);
                        }
                    }
                }
                else
                {
                    int32 BytesSent = 0;
                    VideoSocket->SendTo(JpegBytes.GetData(), JpegBytes.Num(), BytesSent, *TargetAddr);
                }
            }
        }

        FPlatformProcess::Sleep(0.033f); // ~30 FPS
    }
}
