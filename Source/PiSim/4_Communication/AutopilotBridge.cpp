// AutopilotBridge.cpp

#include "AutopilotBridge.h"
#include "TelemetryGateway.h"
#include "Vehicle.h"
#include "Servo.h"
#include "Propeller.h"
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
        LatestPWM[i] = 1000;
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

    if (bNewPwmAvailable)
    {
        DispatchPwmToVehicleMotors();
        bNewPwmAvailable = false;
    }

    // Telemetri durumunu ArduPilot'a bas
    if (CachedGateway && SendSocket)
    {
        ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
        TSharedRef<FInternetAddr> TargetAddr = SocketSubsystem->CreateInternetAddr();
        bool bIsValid = false;
        TargetAddr->SetIp(TEXT("127.0.0.1"), bIsValid);
        TargetAddr->SetPort(SendPort);

        FString SitlJson = CachedGateway->BuildSitlJsonState();
        FTCHARToUTF8 Utf8(*SitlJson);
        int32 BytesSent = 0;
        SendSocket->SendTo((const uint8*)Utf8.Get(), Utf8.Length(), BytesSent, *TargetAddr);
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
        .WithReceiveBufferSize(65535);

    SendSocket = FUdpSocketBuilder(TEXT("PiSimAutopilotSendSocket"))
        .AsNonBlocking()
        .AsReusable()
        .WithSendBufferSize(65535);

    if (!ListenSocket)
    {
        return false;
    }

    RxWorker = new FAutopilotRxWorker(this);
    RxThread = FRunnableThread::Create(RxWorker, TEXT("PiSim_Autopilot_RxThread"));
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
}

// @state: WIP - ArduPilot 16 kanal ikili PWM servo paketlerini okur
void UPiSimModularAutopilotBridge::ExecuteRxLoop(FThreadSafeBool& bRunning)
{
    uint8 RecvBuffer[512];

    while (bRunning)
    {
        if (ListenSocket)
        {
            uint32 PendingDataSize = 0;
            if (ListenSocket->HasPendingData(PendingDataSize) && PendingDataSize >= sizeof(FArduPilotSitlServoPacket))
            {
                int32 BytesRead = 0;
                TSharedRef<FInternetAddr> Sender = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
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
                        }
                    }
                }
            }
        }
        FPlatformProcess::Sleep(0.002f);
    }
}

// @state: WIP - Alınan PWM sinyallerini eşleşen Servo ve Pervane modüllerine iletir
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
}
