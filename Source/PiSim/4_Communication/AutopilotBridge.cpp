// AutopilotBridge.cpp

#include "AutopilotBridge.h"
#include "TelemetryGateway.h"
#include "Vehicle.h"
#include "Servo.h"
#include "Propeller.h"
#include "Wheel.h"
#include "Common/UdpSocketBuilder.h"
#include "HAL/PlatformProcess.h"

// --- FAutopilotRxWorker ---

FAutopilotRxWorker::FAutopilotRxWorker(UPiSimModularAutopilotBridge* InOwner)
    : Owner(InOwner)
    , bRunning(true)
{
}

FAutopilotRxWorker::~FAutopilotRxWorker()
{
}

uint32 FAutopilotRxWorker::Run()
{
    if (Owner)
    {
        Owner->ExecuteRxLoop(bRunning);
    }
    return 0;
}

// --- UPiSimModularAutopilotBridge ---

// @state: WIP - Otopilot köprüsü yapıcısı
UPiSimModularAutopilotBridge::UPiSimModularAutopilotBridge()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;

    for (int32 i = 0; i < 16; ++i)
    {
        LatestPWM[i] = (i == 2) ? 1000 : 1500; // CH3 Throttle 1000, others 1500
    }
}

// @state: WIP - Başlangıçta araç ve telemetri gateway referanslarını bulur
void UPiSimModularAutopilotBridge::BeginPlay()
{
    Super::BeginPlay();

    CachedVehicle = Cast<APiSimVehicle>(GetOwner());
    if (CachedVehicle)
    {
        CachedGateway = CachedVehicle->FindComponentByClass<UPiSimTelemetryGateway>();
    }
}

// @state: WIP - Kapanışta SITL soket ve iş parçacığını temizler
void UPiSimModularAutopilotBridge::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopSitl();
    Super::EndPlay(EndPlayReason);
}

// @state: WIP - Her frame yeni PWM paketi geldiyse motorlara ve servolara dağıtır
void UPiSimModularAutopilotBridge::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Paket oranı ve zaman aşımı hesaplama
    RateTimer += DeltaTime;
    if (RateTimer >= 1.0f)
    {
        PacketRateHz = (float)RxCountInWindow / RateTimer;
        RxCountInWindow = 0;
        RateTimer = 0.0f;
    }

    float CurrentTime = (GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.0f;
    if (bIsConnected && (CurrentTime - LastPacketTime > 2.0f))
    {
        bIsConnected = false;
        LastStatusMessage = TEXT("🟡 SITL Zaman Aşımı");
    }

    if (bNewPwmAvailable)
    {
        DispatchPwmToVehicleMotors();
        bNewPwmAvailable = false;
    }

    // Telemetri durumunu ArduPilot'a bas (Port 9002)
    if (CachedGateway && SendSocket)
    {
        ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
        if (SocketSubsystem)
        {
            TSharedRef<FInternetAddr> TargetAddr = SocketSubsystem->CreateInternetAddr();
            bool bIsValid = false;
            TargetAddr->SetIp(*TargetIP, bIsValid);
            TargetAddr->SetPort(SendPort);

            double WorldTimeSec = (GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.0;
            FString SitlJson = CachedGateway->BuildSitlJsonState(WorldTimeSec);
            FTCHARToUTF8 Utf8(*SitlJson);
            int32 BytesSent = 0;
            SendSocket->SendTo((const uint8*)Utf8.Get(), Utf8.Length(), BytesSent, *TargetAddr);
        }
    }
}

// @state: WIP - ArduPilot SITL dinleme ve gönderme soketlerini başlatır
bool UPiSimModularAutopilotBridge::StartSitl(int32 InListenPort, int32 InSendPort)
{
    ListenPort = InListenPort;
    SendPort = InSendPort;

    StopSitl();

    ListenSocket = FUdpSocketBuilder(TEXT("PiSimAutopilotListenSocket"))
        .AsNonBlocking()
        .AsReusable()
        .BoundToPort(ListenPort)
        .WithReceiveBufferSize(2 * 1024 * 1024);

    SendSocket = FUdpSocketBuilder(TEXT("PiSimAutopilotSendSocket"))
        .AsNonBlocking()
        .AsReusable()
        .WithSendBufferSize(2 * 1024 * 1024);

    if (!ListenSocket)
    {
        LastStatusMessage = FString::Printf(TEXT("❌ Port %d dinlenemedi"), ListenPort);
        return false;
    }

    RxWorker = new FAutopilotRxWorker(this);
    RxThread = FRunnableThread::Create(RxWorker, TEXT("PiSim_Autopilot_RxThread"));

    LastStatusMessage = FString::Printf(TEXT("🟢 SITL Aktif (Rx:%d, Tx:%d)"), ListenPort, SendPort);
    return true;
}

// @state: WIP - SITL iş parçacığını ve soketleri kapatır
void UPiSimModularAutopilotBridge::StopSitl()
{
    if (RxWorker)
    {
        RxWorker->Stop();
    }
    if (RxThread)
    {
        RxThread->WaitForCompletion();
        delete RxThread;
        RxThread = nullptr;
    }
    if (RxWorker)
    {
        delete RxWorker;
        RxWorker = nullptr;
    }

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

    bIsConnected = false;
    LastStatusMessage = TEXT("Kapalı");
}

// @state: WIP - Arka plan soket okuma döngüsü
void UPiSimModularAutopilotBridge::ExecuteRxLoop(FThreadSafeBool& bRunning)
{
    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem) return;

    TSharedRef<FInternetAddr> Sender = SocketSubsystem->CreateInternetAddr();
    uint8 RecvBuffer[2048];

    while (bRunning)
    {
        if (ListenSocket)
        {
            uint32 PendingDataSize = 0;
            while (ListenSocket->HasPendingData(PendingDataSize) && PendingDataSize > 0)
            {
                int32 BytesRead = 0;
                if (ListenSocket->RecvFrom(RecvBuffer, sizeof(RecvBuffer), BytesRead, *Sender))
                {
                    if (BytesRead >= sizeof(FArduPilotSitlServoPacket))
                    {
                        const FArduPilotSitlServoPacket* Packet = reinterpret_cast<const FArduPilotSitlServoPacket*>(RecvBuffer);
                        if (Packet->Magic == 18458)
                        {
                            FScopeLock Lock(&PwmLock);
                            for (int32 i = 0; i < 16; ++i)
                            {
                                LatestPWM[i] = Packet->PWM[i];
                            }
                            bNewPwmAvailable = true;
                            bIsConnected = true;
                            RxCountInWindow++;
                            LastPacketTime = (GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.0f;
                        }
                    }
                }
            }
        }
        FPlatformProcess::Sleep(0.002f);
    }
}

// @state: WIP - Alınan PWM sinyallerini eşleşen Servo, Pervane ve Tekerlek modüllerine iletir
void UPiSimModularAutopilotBridge::DispatchPwmToVehicleMotors()
{
    if (!GetOwner())
    {
        return;
    }

    uint16 LocalPWM[16];
    {
        FScopeLock Lock(&PwmLock);
        FMemory::Memcpy(LocalPWM, LatestPWM, sizeof(LocalPWM));
    }

    // Servolar (Kanatçık, Aileron, Elevator, Rudder, Dümen)
    TArray<UPiSimServoComponent*> Servos;
    GetOwner()->GetComponents<UPiSimServoComponent>(Servos);
    for (UPiSimServoComponent* Servo : Servos)
    {
        int32 ChIdx = Servo->ArduPilotChannel - 1;
        if (ChIdx >= 0 && ChIdx < 16)
        {
            Servo->SetFromPWM(LocalPWM[ChIdx]);
        }
    }

    // Pervaneler & İtki Motorları
    TArray<UPiSimPropellerComponent*> Props;
    GetOwner()->GetComponents<UPiSimPropellerComponent>(Props);
    for (UPiSimPropellerComponent* Prop : Props)
    {
        int32 ChIdx = Prop->ArduPilotChannel - 1;
        if (ChIdx >= 0 && ChIdx < 16)
        {
            Prop->SetFromPWM(LocalPWM[ChIdx]);
        }
    }

    // Tekerlekler (Tahrik & Direksiyon)
    TArray<UPiSimWheelComponent*> Wheels;
    GetOwner()->GetComponents<UPiSimWheelComponent>(Wheels);
    for (UPiSimWheelComponent* Wheel : Wheels)
    {
        int32 ChIdx = Wheel->ArduPilotChannel - 1;
        if (ChIdx >= 0 && ChIdx < 16)
        {
            Wheel->SetFromPWM(LocalPWM[ChIdx], Wheel->WheelMode == EPiSimWheelMode::SteeredWheel);
        }
    }
}
