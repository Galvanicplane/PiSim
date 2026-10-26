// Pi5Bridge.cpp

#include "Pi5Bridge.h"
#include "TelemetryGateway.h"
#include "Common/TcpSocketBuilder.h"
#include "Common/UdpSocketBuilder.h"
#include "HAL/PlatformProcess.h"
#include "GameFramework/Actor.h"
#include "Vehicle.h"

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
            TelemetryPort = 7400 + (VehicleId * 10);
            VideoPort = 7401 + (VehicleId * 10);
        }
    }

    // Başlangıçta simülasyon başlar başlamaz telemetri akışını otomatik başlat (Port 7400)
    StartStreaming(TargetIP, TelemetryPort, VideoPort);
}

// @state: WIP - İsteğe bağlı video aboneliğini açıp kapatır (On-Demand Video Streaming)
void UPiSimPi5Bridge::SetVideoSubscribed(bool bSubscribe)
{
    FScopeLock Lock(&VideoSubscriptionLock);
    bIsVideoSubscribed = bSubscribe;
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
    if (!SocketSubsystem) return;

    FString ProbePayload = TEXT("{\"magic\":\"PISIM_PROBE\",\"source\":\"UE5_UNREAL\",\"msg\":\"HI FROM UNREAL\"}\n");
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
        if (CurrentSec - LastProbeTimeSec >= 1.5)
        {
            LastProbeTimeSec = CurrentSec;
            TriggerBeaconProbe();
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
                    if (RecvStr.Contains(TEXT("PISIM_BEACON")) || RecvStr.Contains(TEXT("Raspberry Pi")) || RecvStr.Contains(TEXT("HI FROM PI5")))
                    {
                        FString SenderIpStr = SenderAddr->ToString(false);
                        if (SenderIpStr != TEXT("127.0.0.1") && SenderIpStr != TEXT("0.0.0.0"))
                        {
                            if (TargetIP != SenderIpStr)
                            {
                                TargetIP = SenderIpStr;
                                UE_LOG(LogTemp, Warning, TEXT("⚡ [Pi5Bridge] 🎉 RASPBERRY PI 5 EŞLEŞTİRİLDİ! IP: %s (Port: %d)"), *TargetIP, TelemetryPort);
                            }
                            OnPi5BeaconDiscovered.Broadcast(RecvStr);
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

        if (CachedGateway && TelemetrySocket)
        {
            FString JsonPayload = CachedGateway->BuildSitlJsonState();
            FTCHARToUTF8 Utf8(*JsonPayload);
            int32 BytesSent = 0;

            // 1. Hedef IP'ye gönder
            TelemetrySocket->SendTo((const uint8*)Utf8.Get(), Utf8.Length(), BytesSent, *TargetAddr);

            // 2. Pi 5 henüz keşfedilmediyse (veya TargetIP hala 127.0.0.1 ise),
            // telemetriyi yerel ağ broadcast (255.255.255.255:7400) ve alt ağ broadcast (192.168.1.255 vb.) adreslerine de bas!
            // Böylece ağdaki Raspberry Pi 5 İLK SANİYEDEN İTİBAREN hiç elle ayar yapmadan canlı veriyi alır!
            if (CurrentTargetIP == TEXT("127.0.0.1"))
            {
                TelemetrySocket->SendTo((const uint8*)Utf8.Get(), Utf8.Length(), BytesSent, *BcastAddr);

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
                                    TSharedRef<FInternetAddr> SbAddr = SocketSubsystem->CreateInternetAddr();
                                    bool bValid = false;
                                    SbAddr->SetIp(*FString::Printf(TEXT("%s.%s.%s.255"), *Parts[0], *Parts[1], *Parts[2]), bValid);
                                    SbAddr->SetPort(CurrentPort);
                                    if (bValid)
                                    {
                                        TelemetrySocket->SendTo((const uint8*)Utf8.Get(), Utf8.Length(), BytesSent, *SbAddr);
                                    }
                                }
                            }
                        }
                    }
                }
            }

            PacketCount++;
            if (PacketCount == 1 || PacketCount % 500 == 0)
            {
                UE_LOG(LogTemp, Log, TEXT("📡 [Pi5Bridge] Telemetri Paketi Gönderildi (#%d) -> %s:%d (Broadcast: %s)"),
                    PacketCount, *CurrentTargetIP, CurrentPort, CurrentTargetIP == TEXT("127.0.0.1") ? TEXT("AÇIK") : TEXT("KAPALI"));
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
        {
            FScopeLock SubLock(&VideoSubscriptionLock);
            bSubscribed = bIsVideoSubscribed;
        }

        // On-demand: Sadece kullanıcı veya uç cihaz video izleme talebinde bulunduysa UDP paketi bas
        if (bSubscribed && CachedGateway && VideoSocket)
        {
            if (CachedGateway->GetLatestCameraFrame(JpegBytes) && JpegBytes.Num() > 0)
            {
                int32 BytesSent = 0;
                VideoSocket->SendTo(JpegBytes.GetData(), JpegBytes.Num(), BytesSent, *TargetAddr);
            }
        }

        FPlatformProcess::Sleep(0.033f); // ~30 FPS
    }
}
