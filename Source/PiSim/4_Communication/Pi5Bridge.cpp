// Pi5Bridge.cpp

#include "Pi5Bridge.h"
#include "TelemetryGateway.h"
#include "Common/TcpSocketBuilder.h"
#include "Common/UdpSocketBuilder.h"
#include "HAL/PlatformProcess.h"
#include "GameFramework/Actor.h"

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

// @state: WIP - Pi5 köprüsü bileşen yapıcısı
UPiSimPi5Bridge::UPiSimPi5Bridge()
{
    PrimaryComponentTick.bCanEverTick = false;
}

// @state: WIP - Başlangıçta Gateway referansını bağlar
void UPiSimPi5Bridge::BeginPlay()
{
    Super::BeginPlay();

    if (GetOwner())
    {
        CachedGateway = GetOwner()->FindComponentByClass<UPiSimTelemetryGateway>();
    }
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
        .WithSendBufferSize(65535);

    VideoSocket = FUdpSocketBuilder(TEXT("PiSimPi5VideoSocket"))
        .AsNonBlocking()
        .AsReusable()
        .WithSendBufferSize(262144);

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
    }
}

// @state: WIP - Telemetri iş parçacığı döngüsü (Gateway'den okuyup UDP basar)
void UPiSimPi5Bridge::ExecuteTelemetryLoop(FThreadSafeBool& bRunning)
{
    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    TSharedRef<FInternetAddr> TargetAddr = SocketSubsystem->CreateInternetAddr();
    bool bIsValid = false;
    TargetAddr->SetIp(*TargetIP, bIsValid);
    TargetAddr->SetPort(TelemetryPort);

    while (bRunning)
    {
        if (CachedGateway && TelemetrySocket)
        {
            FString JsonPayload = CachedGateway->BuildSitlJsonState();
            FTCHARToUTF8 Utf8(*JsonPayload);
            int32 BytesSent = 0;
            TelemetrySocket->SendTo((const uint8*)Utf8.Get(), Utf8.Length(), BytesSent, *TargetAddr);
        }

        float SleepSeconds = 1.0f / FMath::Max(1.0f, TelemetryRateHz);
        FPlatformProcess::Sleep(SleepSeconds);
    }
}

// @state: WIP - Canlı video akış iş parçacığı döngüsü (JPEG karelerini UDP basar)
void UPiSimPi5Bridge::ExecuteVideoLoop(FThreadSafeBool& bRunning)
{
    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    TSharedRef<FInternetAddr> TargetAddr = SocketSubsystem->CreateInternetAddr();
    bool bIsValid = false;
    TargetAddr->SetIp(*TargetIP, bIsValid);
    TargetAddr->SetPort(VideoPort);

    TArray<uint8> JpegBytes;

    while (bRunning)
    {
        if (CachedGateway && VideoSocket)
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
