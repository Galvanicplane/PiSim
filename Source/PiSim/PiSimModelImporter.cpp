// PiSimModelImporter.cpp
// Full-Featured Pawn for GameMode with 360 Orbit Camera, Mouse Controls, Interactive Screen UI, and Strict Visual vs UCX Collision Separation.

#include "PiSimModelImporter.h"
#include "PiSimModelImporterWidget.h"
#include "PiSimGarageRobot.h"
#include "PiSimUDPManager.h"
#include "ROS2MessageTypes.h"
#include "ROS2UE5Converter.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Materials/MaterialInterface.h"
#include "GameFramework/PlayerController.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "DrawDebugHelpers.h"

APiSimModelImporter::APiSimModelImporter()
{
    PrimaryActorTick.bCanEverTick = true;
    AutoPossessPlayer = EAutoReceiveInput::Player0;

    SceneRootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRootComponent"));
    SetRootComponent(SceneRootComponent);

    // SpaceX 360 Orbit Camera framing
    OrbitSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("OrbitSpringArm"));
    OrbitSpringArm->SetupAttachment(SceneRootComponent);
    OrbitSpringArm->TargetArmLength = 350.0f;
    OrbitSpringArm->SetRelativeRotation(FRotator(-20.0f, 45.0f, 0.0f));
    OrbitSpringArm->bUsePawnControlRotation = false;
    OrbitSpringArm->bDoCollisionTest = false;
    OrbitSpringArm->bEnableCameraLag = true;
    OrbitSpringArm->CameraLagSpeed = 12.0f;

    OrbitCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("OrbitCamera"));
    OrbitCamera->SetupAttachment(OrbitSpringArm, USpringArmComponent::SocketName);

    FpvCameraCapture = nullptr;
    bEnableVideoStream = false;
    ImportScaleMultiplier = 1.0f; // Pure 1:1 scale by default
    bIsPhysicsSimulating = false;
}



void APiSimModelImporter::BeginPlay()
{
    Super::BeginPlay();

    // Enable Mouse Cursor, HUD & Interactive Events for Player Controller
    if (GetWorld())
    {
        APlayerController* PC = GetWorld()->GetFirstPlayerController();
        if (PC)
        {
            PC->bShowMouseCursor = true;
            PC->bEnableClickEvents = true;
            PC->bEnableMouseOverEvents = true;
            FInputModeGameAndUI InputMode;
            InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            InputMode.SetHideCursorDuringCapture(false);
            PC->SetInputMode(InputMode);
        }

        // Create and Add Dedicated HUD Widget to Viewport (Z-Order: 100)
        if (!ImporterWidget)
        {
            ImporterWidget = CreateWidget<UPiSimModelImporterWidget>(GetWorld(), UPiSimModelImporterWidget::StaticClass());
            if (ImporterWidget)
            {
                ImporterWidget->TargetImporter = this;
                ImporterWidget->AddToViewport(100);
            }
        }
    }

    // Enforce Raspberry Pi 5 IP if default or empty (Matching proven PiSimPrimitiveCube behavior)
    if (BridgeTargetIP.IsEmpty() || BridgeTargetIP == TEXT("127.0.0.1"))
    {
        BridgeTargetIP = TEXT("192.168.1.20");
    }

    // Initialize UDP Network Manager for Raspberry Pi 5 Bridge
    UDPManager = MakeUnique<FPiSimUDPManager>();
    UDPManager->OnControlPacketReceived.AddUObject(this, &APiSimModelImporter::OnControlPacketReceived);
    bIsSocketBound = UDPManager->StartControlListener(7400);
    UDPManager->ReserveVideoSocket(5000);

    // Initialize Video Render Target for FPV Camera Streaming (320x240, PF_B8G8R8A8)
    if (!VideoRenderTarget)
    {
        VideoRenderTarget = NewObject<UTextureRenderTarget2D>(this);
        VideoRenderTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
        VideoRenderTarget->ClearColor = FLinearColor::Black;
        VideoRenderTarget->TargetGamma = 2.2f;
        VideoRenderTarget->bAutoGenerateMips = false;
        VideoRenderTarget->InitCustomFormat(320, 240, PF_B8G8R8A8, false);
        VideoRenderTarget->UpdateResourceImmediate(true);
    }

    if (FpvCameraCapture)
    {
        FpvCameraCapture->TextureTarget = VideoRenderTarget;
    }
    AddConnectionDebugLog(TEXT("📷 [FPV Kamera] 320x240 RenderTarget ve UDP Port 5000 soketi hazır"));

    if (bIsSocketBound)

    {
        ConnectionStage = 3;
        ConnectionStageText = TEXT("Aşama 3: Pi 5 Handshake Bekleniyor (Port 7400)");
        AddConnectionDebugLog(TEXT("✅ [Aşama 1] UDP Port 7400 soketi başarıyla açıldı (0.0.0.0:7400)"));
        AddConnectionDebugLog(FString::Printf(TEXT("📡 [Aşama 2] Telemetri hedefleri hazır: %s:7401 & 127.0.0.1:7401"), *BridgeTargetIP));
        AddConnectionDebugLog(TEXT("🟡 [Aşama 3] Pi 5 sinyali ve kontrol paketleri bekleniyor..."));

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(1001, 12.0f, FColor::Cyan,
                FString::Printf(TEXT("📡 [PiSim UDP] Dinleyici: 0.0.0.0:7400 | Hedef: %s:7401 | Pi 5 bekleniyor..."), *BridgeTargetIP));
        }
    }
    else
    {
        ConnectionStage = 1;
        ConnectionStageText = TEXT("❌ HATA: Port 7400 Soketi Açılamadı");
        AddConnectionDebugLog(TEXT("❌ [Aşama 1 HATA] UDP Port 7400 soketi açılamadı! (Port başka bir uygulama tarafından kullanılıyor olabilir)"));
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(1001, 12.0f, FColor::Red,
                TEXT("❌ [PiSim UDP HATA] Port 7400 soketi açılamadı! (Port kullanımda olabilir)"));
        }
    }

    // Auto-spawn model from Saved/Robots/Cache/robot_import_test.fbx at startup
    BuildAndSpawnRobotHierarchy(ImportScaleMultiplier);
}

void APiSimModelImporter::AddConnectionDebugLog(const FString& LogMsg)
{
    FString Timestamp = FDateTime::Now().ToString(TEXT("%H:%M:%S"));
    FString Entry = FString::Printf(TEXT("[%s] %s"), *Timestamp, *LogMsg);
    ConnectionDebugLogs.Add(Entry);
    if (ConnectionDebugLogs.Num() > 7)
    {
        ConnectionDebugLogs.RemoveAt(0);
    }
    UE_LOG(LogTemp, Warning, TEXT("[PiSim Debug] %s"), *Entry);
}

void APiSimModelImporter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UDPManager)
    {
        UDPManager->OnControlPacketReceived.RemoveAll(this);
        UDPManager->Shutdown();
        UDPManager.Reset();
    }
    Super::EndPlay(EndPlayReason);
}

void APiSimModelImporter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (PlayerInputComponent)
    {
        PlayerInputComponent->BindAction(TEXT("LeftMouseClick"), IE_Pressed, this, &APiSimModelImporter::OnLeftMouseDown);
        PlayerInputComponent->BindAction(TEXT("LeftMouseClick"), IE_Released, this, &APiSimModelImporter::OnLeftMouseUp);
        PlayerInputComponent->BindAction(TEXT("RightMouseClick"), IE_Pressed, this, &APiSimModelImporter::OnRightMouseDown);
        PlayerInputComponent->BindAction(TEXT("RightMouseClick"), IE_Released, this, &APiSimModelImporter::OnRightMouseUp);

        // Direct Key Binds for Mouse Scroll Wheel Zoom
        PlayerInputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &APiSimModelImporter::ZoomIn);
        PlayerInputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &APiSimModelImporter::ZoomOut);

        // G and F Keys to Control Wheel Rotation Speed (RPM)
        PlayerInputComponent->BindKey(EKeys::G, IE_Pressed, this, &APiSimModelImporter::IncreaseWheelRpm);
        PlayerInputComponent->BindKey(EKeys::F, IE_Pressed, this, &APiSimModelImporter::DecreaseWheelRpm);
    }
}

void APiSimModelImporter::IncreaseWheelRpm()
{
    AppliedWheelRpm += 1.0f;
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(701, 3.0f, FColor::Cyan,
            FString::Printf(TEXT("🏎️ >>> [TEKERLEK DÖNÜŞÜ (RPM)]: %+.1f RPM (G Tuşu: +1 RPM) <<<"), AppliedWheelRpm));
    }
    UE_LOG(LogTemp, Warning, TEXT("[PiSimModelImporter LOG] Tekerlek Dönüş Hızı: %+.1f RPM"), AppliedWheelRpm);
}

void APiSimModelImporter::DecreaseWheelRpm()
{
    AppliedWheelRpm -= 1.0f;
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(701, 3.0f, FColor::Orange,
            FString::Printf(TEXT("🏎️ >>> [TEKERLEK DÖNÜŞÜ (RPM)]: %+.1f RPM (F Tuşu: -1 RPM) <<<"), AppliedWheelRpm));
    }
    UE_LOG(LogTemp, Warning, TEXT("[PiSimModelImporter LOG] Tekerlek Dönüş Hızı: %+.1f RPM"), AppliedWheelRpm);
}

void APiSimModelImporter::OnLeftMouseDown()
{
    bIsLeftMouseDown = true;
}

void APiSimModelImporter::OnLeftMouseUp()
{
    bIsLeftMouseDown = false;
}

void APiSimModelImporter::OnRightMouseDown()
{
    bIsRightMouseDown = true;
}

void APiSimModelImporter::OnRightMouseUp()
{
    bIsRightMouseDown = false;
}

void APiSimModelImporter::ZoomIn()
{
    if (OrbitSpringArm)
    {
        OrbitSpringArm->TargetArmLength = FMath::Clamp(OrbitSpringArm->TargetArmLength - 35.0f, 30.0f, 2500.0f);
    }
}

void APiSimModelImporter::ZoomOut()
{
    if (OrbitSpringArm)
    {
        OrbitSpringArm->TargetArmLength = FMath::Clamp(OrbitSpringArm->TargetArmLength + 35.0f, 30.0f, 2500.0f);
    }
}

void APiSimModelImporter::OnControlPacketReceived(const TArray<uint8>& PacketData, const FString& SenderIP)
{
    bool bWasConnected = bIsPiConnected;
    if (!SenderIP.IsEmpty())
    {
        ConnectedPiIP = SenderIP;
        bIsPiConnected = true;
        LastPacketReceivedTime = (GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.0f;
        LastRxTimestampSec = LastPacketReceivedTime;
        LastRxPacketBytes = PacketData.Num();
        ConnectionStage = 4;
        ConnectionStageText = FString::Printf(TEXT("Aşama 4: Canlı Veri Akışı Aktif (%s)"), *SenderIP);
    }

    if (!bWasConnected)
    {
        AddConnectionDebugLog(FString::Printf(TEXT("🟢 [Aşama 4] Pi 5 BAĞLANTISI KURULDU! IP: %s (Paket: %d bayt)"), *SenderIP, PacketData.Num()));
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(1002, 6.0f, FColor::Green,
                FString::Printf(TEXT("✅ [PiSim UDP] BAĞLANTI KURULDU! IP: %s (Port 7400)"), *SenderIP));
        }
    }

    FROSTwistMessage TwistMsg;
    if (FROSTwistMessage::FromBinary(PacketData, TwistMsg))
    {
        TargetLinearX = TwistMsg.Linear.X;
        TargetAngularZ = TwistMsg.Angular.Z;
        LastRxLinearVel = FVector(TwistMsg.Linear.X, TwistMsg.Linear.Y, TwistMsg.Linear.Z);
        LastRxAngularVel = FVector(TwistMsg.Angular.X, TwistMsg.Angular.Y, TwistMsg.Angular.Z);

        // Differential drive kinematics model:
        // Track width L (~0.35m), Wheel radius R (~0.08m)
        float TrackWidth = 0.35f;
        float WheelRadius = 0.08f;

        float V_Left = TargetLinearX - (TargetAngularZ * TrackWidth * 0.5f);
        float V_Right = TargetLinearX + (TargetAngularZ * TrackWidth * 0.5f);

        // Convert linear speed (m/s) to RPM: RPM = (V / (2 * PI * R)) * 60
        LeftWheelsRpm = (V_Left / (2.0f * PI * WheelRadius)) * 60.0f;
        RightWheelsRpm = (V_Right / (2.0f * PI * WheelRadius)) * 60.0f;

        TotalPacketsReceived++;
        RxCountInWindow++;

        if (TotalPacketsReceived % 20 == 1 || FMath::Abs(TargetLinearX) > 0.01f || FMath::Abs(TargetAngularZ) > 0.01f)
        {
            AddConnectionDebugLog(FString::Printf(TEXT("🎮 RX #%d: LinX=%+.2f m/s, AngZ=%+.2f r/s -> Sol:%+.0f Sağ:%+.0f RPM"),
                TotalPacketsReceived, TargetLinearX, TargetAngularZ, LeftWheelsRpm, RightWheelsRpm));
            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(1003, 1.2f, FColor::Emerald,
                    FString::Printf(TEXT("🎮 [PiSim RX #%d] LinX: %+.2f m/s | AngZ: %+.2f r/s | RPM: Sol %+.0f, Sağ %+.0f"),
                        TotalPacketsReceived, TargetLinearX, TargetAngularZ, LeftWheelsRpm, RightWheelsRpm));
            }
        }
    }
}

void APiSimModelImporter::PublishImuTelemetry(float DeltaTime)
{
    if (!UDPManager || DeltaTime <= 0.0f || VisualMeshComponents.Num() == 0 || !VisualMeshComponents[0])
    {
        return;
    }

    UProceduralMeshComponent* Chassis = VisualMeshComponents[0];
    FVector CurrentLinearVelUE5 = Chassis->GetPhysicsLinearVelocity();
    CurrentForwardSpeedKmh = CurrentLinearVelUE5.Size() * 0.036f;

    CurrentLinearAccel = (CurrentLinearVelUE5 - PreviousLinearVelocityUE5) / DeltaTime;
    PreviousLinearVelocityUE5 = CurrentLinearVelUE5;

    FVector AngularVelUE5 = Chassis->GetPhysicsAngularVelocityInDegrees();
    FQuat OrientationUE5 = Chassis->GetComponentQuat();

    LastTxQuat = OrientationUE5;
    LastTxGyro = AngularVelUE5;
    LastTxAccel = CurrentLinearAccel;

    FROSImuMessage ImuMsg;
    ImuMsg.LinearAcceleration = FROS2UE5Converter::UE5ToROS2LinearAcceleration(CurrentLinearAccel);
    ImuMsg.AngularVelocity = FROS2UE5Converter::UE5ToROS2AngularVelocity(AngularVelUE5);
    FROS2UE5Converter::UE5ToROS2Quaternion(OrientationUE5, ImuMsg.Orientation.X, ImuMsg.Orientation.Y, ImuMsg.Orientation.Z, ImuMsg.Orientation.W);

    TArray<uint8> ImuBytes;
    if (ImuMsg.ToBinary(ImuBytes))
    {
        LastTxPacketBytes = ImuBytes.Num();

        // 1) Primary Target: BridgeTargetIP (Proven Raspberry Pi 5 Ethernet IP 192.168.1.20)
        FString PrimaryIP = (!BridgeTargetIP.IsEmpty()) ? BridgeTargetIP : TEXT("192.168.1.20");
        UDPManager->SendControlData(ImuBytes, PrimaryIP, FPiSimUDPManager::DEFAULT_TELEMETRY_PORT);

        // 2) Secondary Target: ConnectedPiIP (If dynamically discovered or local 127.0.0.1)
        if (!ConnectedPiIP.IsEmpty() && ConnectedPiIP != PrimaryIP && ConnectedPiIP != TEXT("None"))
        {
            UDPManager->SendControlData(ImuBytes, ConnectedPiIP, FPiSimUDPManager::DEFAULT_TELEMETRY_PORT);
        }

        // 3) Local loopback for developer testing
        if (PrimaryIP != TEXT("127.0.0.1") && ConnectedPiIP != TEXT("127.0.0.1"))
        {
            UDPManager->SendControlData(ImuBytes, TEXT("127.0.0.1"), FPiSimUDPManager::DEFAULT_TELEMETRY_PORT);
        }

        TotalPacketsSent++;
        TxCountInWindow++;

        if (TotalPacketsSent % 50 == 1)
        {
            AddConnectionDebugLog(FString::Printf(TEXT("📡 TX Telemetri #%d -> %s:7401 (Hız: %.1f km/h, 80 Bayt)"),
                TotalPacketsSent, *PrimaryIP, CurrentForwardSpeedKmh));
        }
    }
}

void APiSimModelImporter::CaptureAndSendVideoFrame()
{
    if (!VideoRenderTarget || !UDPManager)
    {
        return;
    }

    FTextureRenderTargetResource* Resource = VideoRenderTarget->GameThread_GetRenderTargetResource();
    if (!Resource)
    {
        return;
    }

    int32 Width = VideoRenderTarget->SizeX;
    int32 Height = VideoRenderTarget->SizeY;
    if (Width <= 0 || Height <= 0)
    {
        return;
    }

    TArray<FColor> RawPixels;
    // SetLinearToGamma(false) prevents double-gamma over-exposure
    FReadSurfaceDataFlags ReadPixelFlags(RCM_UNorm);
    ReadPixelFlags.SetLinearToGamma(false);

    if (!Resource->ReadPixels(RawPixels, ReadPixelFlags) || RawPixels.Num() == 0)
    {
        return;
    }

    for (FColor& Pixel : RawPixels)
    {
        Pixel.A = 255;
    }

    IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
    TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);
    if (!ImageWrapper.IsValid())
    {
        return;
    }

    if (!ImageWrapper->SetRaw(RawPixels.GetData(), RawPixels.Num() * sizeof(FColor), Width, Height, ERGBFormat::BGRA, 8))
    {
        return;
    }

    TArray64<uint8> CompressedJpeg64 = ImageWrapper->GetCompressed(VideoJpegQuality);
    if (CompressedJpeg64.Num() == 0)
    {
        return;
    }

    TArray<uint8> CompressedJpeg;
    CompressedJpeg.Append(CompressedJpeg64.GetData(), CompressedJpeg64.Num());

    // Save last sent frame to disk for verification
    FString SavedFramePath = FPaths::ProjectSavedDir() / TEXT("Robots/Cache/last_sent_frame.jpg");
    FFileHelper::SaveArrayToFile(CompressedJpeg, *SavedFramePath);

    static uint16 FrameSequence = 0;
    FrameSequence++;

    // Split JPEG into MTU-safe chunks of 1000 bytes with 4-byte header:
    // [FrameSeq (uint16 MSB, LSB), ChunkIdx (uint8), TotalChunks (uint8)]
    const int32 MAX_CHUNK_SIZE = 1000;
    int32 TotalBytes = CompressedJpeg.Num();
    uint8 TotalChunks = static_cast<uint8>((TotalBytes + MAX_CHUNK_SIZE - 1) / MAX_CHUNK_SIZE);

    FString PrimaryIP = (!BridgeTargetIP.IsEmpty()) ? BridgeTargetIP : TEXT("192.168.1.20");

    for (uint8 ChunkIdx = 0; ChunkIdx < TotalChunks; ++ChunkIdx)
    {
        int32 StartOffset = ChunkIdx * MAX_CHUNK_SIZE;
        int32 ChunkLength = FMath::Min(MAX_CHUNK_SIZE, TotalBytes - StartOffset);

        TArray<uint8> Packet;
        Packet.Reserve(4 + ChunkLength);

        Packet.Add(static_cast<uint8>((FrameSequence >> 8) & 0xFF));
        Packet.Add(static_cast<uint8>(FrameSequence & 0xFF));
        Packet.Add(ChunkIdx);
        Packet.Add(TotalChunks);
        Packet.Append(CompressedJpeg.GetData() + StartOffset, ChunkLength);

        // 1) Primary Target: Raspberry Pi 5 IP (192.168.1.20:5000)
        UDPManager->SendVideoData(Packet, PrimaryIP, VideoPort);

        // 2) Secondary Target: ConnectedPiIP (if dynamically discovered)
        if (!ConnectedPiIP.IsEmpty() && ConnectedPiIP != PrimaryIP && ConnectedPiIP != TEXT("None"))
        {
            UDPManager->SendVideoData(Packet, ConnectedPiIP, VideoPort);
        }

        // 3) Local loopback for developer testing
        if (PrimaryIP != TEXT("127.0.0.1") && ConnectedPiIP != TEXT("127.0.0.1"))
        {
            UDPManager->SendVideoData(Packet, TEXT("127.0.0.1"), VideoPort);
        }
    }

    TotalVideoFramesSent++;
}

void APiSimModelImporter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 1) Telemetry rate window calculation (every 0.5 sec)
    RateCalcTimer += DeltaTime;
    if (RateCalcTimer >= 0.5f)
    {
        RxPacketRateHz = (float)RxCountInWindow / RateCalcTimer;
        TxPacketRateHz = (float)TxCountInWindow / RateCalcTimer;
        RxCountInWindow = 0;
        TxCountInWindow = 0;
        RateCalcTimer = 0.0f;

        // Auto timeout if no packets received for 3 seconds
        if (GetWorld() && (GetWorld()->GetTimeSeconds() - LastPacketReceivedTime > 3.0f))
        {
            if (bIsPiConnected)
            {
                bIsPiConnected = false;
                ConnectionStage = 5;
                ConnectionStageText = TEXT("Aşama 5: Zaman Aşımı (Pi 5 Bağlantısı Koptu)");
                AddConnectionDebugLog(TEXT("🔴 [Aşama 5] ZAMAN AŞIMI: 3 saniyedir paket gelmedi, bağlantı koptu!"));
                if (GEngine)
                {
                    GEngine->AddOnScreenDebugMessage(1004, 4.0f, FColor::Red,
                        TEXT("⚠️ [PiSim UDP] BAĞLANTI KOPTU! (3s Paket Alınamadı)"));
                }
            }
        }
    }

    // 2) Publish IMU Telemetry to Pi 5 at ~50 Hz
    TelemetryTimer += DeltaTime;
    if (TelemetryTimer >= 0.02f)
    {
        PublishImuTelemetry(TelemetryTimer);
        TelemetryTimer = 0.0f;
    }

    // 3) Canlı Tekerlek Fiziksel Diferansiyel Dönüşü (Gövdeye göre Y < 0: Sol, Y > 0: Sağ)
    if (bIsPhysicsSimulating)
    {
        for (int32 i = 1; i < VisualMeshComponents.Num(); ++i)
        {
            if (VisualMeshComponents[i] && ConfiguredMotors.IsValidIndex(i))
            {
                const FPiSimMotorItem& Motor = ConfiguredMotors[i];
                if (Motor.Role == EPiSimMotorRole::DriveWheel || Motor.Role == EPiSimMotorRole::TrackPad || Motor.Role == EPiSimMotorRole::Thruster)
                {
                    FVector RelLoc = VisualMeshComponents[i]->GetRelativeLocation();
                    float BaseRpm = (RelLoc.Y < 0.0f) ? LeftWheelsRpm : RightWheelsRpm;
                    float IndividualTestRpm = Motor.CurrentTestValue * Motor.MaxVelocityRPM;
                    float TotalRpm = BaseRpm + IndividualTestRpm;

                    if (FMath::Abs(TotalRpm) > 0.001f)
                    {
                        float AngularSpeedDegPerSec = TotalRpm * 6.0f; // 1 RPM = 6 deg/sec
                        FVector LocalAxle = FVector(1.0f, 0.0f, 0.0f); // Roll X axle
                        FVector WorldAxle = VisualMeshComponents[i]->GetComponentTransform().TransformVectorNoScale(LocalAxle);
                        VisualMeshComponents[i]->SetPhysicsAngularVelocityInDegrees(WorldAxle * AngularSpeedDegPerSec, false);
                    }
                }
            }
        }
    }

    // 4) Kamerayı gövdenin dünya konumuna kilitle (Araç hareket ettikçe kamera tam arkasında kalsın)
    if (VisualMeshComponents.IsValidIndex(0) && VisualMeshComponents[0] && OrbitSpringArm)
    {
        FVector ChassisLoc = VisualMeshComponents[0]->GetComponentLocation();
        OrbitSpringArm->SetWorldLocation(ChassisLoc + FVector(0.0f, 0.0f, 60.0f));
    }

    // 5) FPV Canlı Kamera Yayını (UDP Port 5000)
    if (bEnableVideoStream && VideoRenderTarget && FpvCameraCapture && UDPManager)
    {
        VideoStreamTimer += DeltaTime;
        float Interval = 1.0f / FMath::Max(1.0f, VideoFrameRate);
        if (VideoStreamTimer >= Interval)
        {
            VideoStreamTimer = 0.0f;
            CaptureAndSendVideoFrame();
            VideoFramesInWindow++;
        }

        VideoFpsTimer += DeltaTime;
        if (VideoFpsTimer >= 1.0f)
        {
            VideoFpsActual = (float)VideoFramesInWindow / VideoFpsTimer;
            VideoFramesInWindow = 0;
            VideoFpsTimer = 0.0f;
        }

        // 3D Viewport Debug Visual: Kamera Konumu, Görüş Konisi ve Yön Oku
        if (GetWorld())
        {
            FVector CamLoc = FpvCameraCapture->GetComponentLocation();
            FRotator CamRot = FpvCameraCapture->GetComponentRotation();
            DrawDebugCamera(GetWorld(), CamLoc, CamRot, 90.0f, 25.0f, FColor::Cyan, false, -1.0f, 0);
            DrawDebugDirectionalArrow(GetWorld(), CamLoc, CamLoc + CamRot.Vector() * 45.0f, 8.0f, FColor::Yellow, false, -1.0f, 0, 2.2f);
            DrawDebugBox(GetWorld(), CamLoc, FVector(4.0f), FColor::Emerald, false, -1.0f, 0, 1.5f);
            DrawDebugString(GetWorld(), CamLoc + FVector(0.0f, 0.0f, 14.0f), TEXT("📷 S_Cam (FPV Kamera)"), nullptr, FColor::Cyan, 0.0f, true, 1.0f);
        }
    }

    // Mouse Orbit & Pan Camera Dragging

    if (GetWorld())
    {
        APlayerController* PC = GetWorld()->GetFirstPlayerController();
        if (PC && OrbitSpringArm)
        {
            float MouseX = 0.0f, MouseY = 0.0f;
            PC->GetInputMouseDelta(MouseX, MouseY);

            // Left Mouse Drag -> 360 Orbit Camera
            if (PC->IsInputKeyDown(EKeys::LeftMouseButton) && (FMath::Abs(MouseX) > 0.001f || FMath::Abs(MouseY) > 0.001f))
            {
                FRotator ArmRot = OrbitSpringArm->GetRelativeRotation();
                ArmRot.Yaw += MouseX * 2.5f;
                ArmRot.Pitch = FMath::Clamp(ArmRot.Pitch + MouseY * 2.0f, -80.0f, 80.0f);
                OrbitSpringArm->SetRelativeRotation(ArmRot);
            }

            // Right Mouse Drag -> Pan Camera Target
            if (PC->IsInputKeyDown(EKeys::RightMouseButton) && (FMath::Abs(MouseX) > 0.001f || FMath::Abs(MouseY) > 0.001f))
            {
                FVector ArmLoc = OrbitSpringArm->GetRelativeLocation();
                ArmLoc.Y += MouseX * 1.5f;
                ArmLoc.Z += MouseY * 1.5f;
                OrbitSpringArm->SetRelativeLocation(ArmLoc);
            }
        }
    }
}

void APiSimModelImporter::ClearSpawnedComponents()
{
    for (UProceduralMeshComponent* VisComp : VisualMeshComponents)
    {
        if (VisComp) VisComp->DestroyComponent();
    }
    VisualMeshComponents.Empty();

    for (UProceduralMeshComponent* CollComp : CollisionMeshComponents)
    {
        if (CollComp) CollComp->DestroyComponent();
    }
    CollisionMeshComponents.Empty();

    for (UPhysicsConstraintComponent* Constraint : JointConstraints)
    {
        if (Constraint) Constraint->DestroyComponent();
    }
    JointConstraints.Empty();

    for (USceneCaptureComponent2D* CamComp : SpawnedCameraComponents)
    {
        if (CamComp) CamComp->DestroyComponent();
    }
    SpawnedCameraComponents.Empty();
    FpvCameraCapture = nullptr;

    for (UProceduralMeshComponent* MarkerComp : SensorMarkerComponents)
    {
        if (MarkerComp) MarkerComp->DestroyComponent();
    }
    SensorMarkerComponents.Empty();

    VisualSections.Empty();
    UCXSections.Empty();
    SensorSections.Empty();
    PureBoneNames.Empty();
    bIsPhysicsSimulating = false;
    bEnableVideoStream = false;
}

void APiSimModelImporter::ImportAndSpawnRobot()
{
    BuildAndSpawnRobotHierarchy(ImportScaleMultiplier);
}

void APiSimModelImporter::SetScale_0_1X()
{
    ImportScaleMultiplier = 0.1f;
    BuildAndSpawnRobotHierarchy(ImportScaleMultiplier);
}

void APiSimModelImporter::SetScale_1_0X()
{
    ImportScaleMultiplier = 1.0f;
    BuildAndSpawnRobotHierarchy(ImportScaleMultiplier);
}

void APiSimModelImporter::SetScale_10_0X()
{
    ImportScaleMultiplier = 10.0f;
    BuildAndSpawnRobotHierarchy(ImportScaleMultiplier);
}

void APiSimModelImporter::TogglePhysicsSimulation()
{
    bIsPhysicsSimulating = !bIsPhysicsSimulating;
    SetPhysicsSimulationActive(bIsPhysicsSimulating);
}

// =========================================================================================
// [AŞAMA 1 & 2] FBX AYRIŞTIRMA VE LİSTELERE AYIRMA (VisualSections, UCXSections, SensorSections)
// =========================================================================================
bool APiSimModelImporter::ParseBinaryFbxFile(const FString& FilePath, TArray<FImporterMeshSection>& OutVisual, TArray<FImporterMeshSection>& OutUCX, TArray<FImporterSensorSection>& OutSensors, TArray<FString>& OutPureBones, float Scale)
{
    OutVisual.Empty();
    OutUCX.Empty();
    OutSensors.Empty();
    OutPureBones.Empty();


    TArray<uint8> FileBytes;
    if (!FFileHelper::LoadFileToArray(FileBytes, *FilePath) || FileBytes.Num() < 64)
    {
        UE_LOG(LogTemp, Error, TEXT("[PiSimModelImporter] FBX dosyasi okunamadi: %s"), *FilePath);
        return false;
    }

    int32 FileSize = FileBytes.Num();
    int32 Pos = 27;

    struct FRawFbxGeom
    {
        FString Label;
        TArray<FVector> Vertices;
        TArray<int32> Polygons;
    };

    struct FRawFbxSubDef
    {
        FString BoneLabel;
        TArray<int32> Indices;
    };

    TMap<uint64, FString> ModelMap;
    TMap<uint64, FRawFbxGeom> GeomMap;
    TMap<uint64, FString> DeformerMap;
    TMap<uint64, FRawFbxSubDef> SubDeformerMap;

    TMap<uint64, TArray<uint64>> ChildToParents;
    TMap<uint64, TArray<uint64>> ParentToChildren;

    // 1) FBX Node Parser Helper
    auto ParseNodeHeader = [&](int32 P, uint32& OutEnd, uint32& OutNumProps, uint32& OutPropLen, FString& OutName, int32& OutPropsStart, int32& OutChildStart) -> bool
    {
        if (P + 13 > FileSize) return false;
        OutEnd = *reinterpret_cast<const uint32*>(&FileBytes[P]);
        OutNumProps = *reinterpret_cast<const uint32*>(&FileBytes[P + 4]);
        OutPropLen = *reinterpret_cast<const uint32*>(&FileBytes[P + 8]);
        uint8 NLen = FileBytes[P + 12];
        if (OutEnd == 0 || OutEnd > (uint32)FileSize || P + 13 + NLen > FileSize) return false;
        OutName = FString(NLen, (const ANSICHAR*)&FileBytes[P + 13]);
        OutPropsStart = P + 13 + NLen;
        OutChildStart = OutPropsStart + OutPropLen;
        return true;
    };

    // 2) Top-Level Iterate: Find Objects and Connections
    int32 ObjectsStart = 0, ObjectsEnd = 0;
    int32 ConnsStart = 0, ConnsEnd = 0;

    while (Pos + 13 < FileSize)
    {
        uint32 NEnd, NNumProps, NPropLen;
        FString NName;
        int32 NPropsStart, NChildStart;
        if (!ParseNodeHeader(Pos, NEnd, NNumProps, NPropLen, NName, NPropsStart, NChildStart)) break;

        if (NName.Equals(TEXT("Objects")))
        {
            ObjectsStart = NChildStart;
            ObjectsEnd = (int32)NEnd;
        }
        else if (NName.Equals(TEXT("Connections")))
        {
            ConnsStart = NChildStart;
            ConnsEnd = (int32)NEnd;
        }
        Pos = (int32)NEnd;
    }

    // 3) Parse Connections (OO, ChildID, ParentID)
    int32 CP = ConnsStart;
    while (CP + 13 < ConnsEnd)
    {
        uint32 CEnd, CNumProps, CPropLen;
        FString CName;
        int32 CPropsStart, CChildStart;
        if (!ParseNodeHeader(CP, CEnd, CNumProps, CPropLen, CName, CPropsStart, CChildStart)) break;

        if (CName.Equals(TEXT("C")) && CPropLen >= 19)
        {
            uint32 StrLen = *reinterpret_cast<const uint32*>(&FileBytes[CPropsStart + 1]);
            int32 Off = CPropsStart + 5 + StrLen;
            if (Off + 18 <= (int32)CEnd)
            {
                uint64 ChildID = *reinterpret_cast<const uint64*>(&FileBytes[Off + 1]);
                Off += 9;
                uint64 ParentID = *reinterpret_cast<const uint64*>(&FileBytes[Off + 1]);

                ChildToParents.FindOrAdd(ChildID).Add(ParentID);
                ParentToChildren.FindOrAdd(ParentID).Add(ChildID);
            }
        }
        CP = (int32)CEnd;
    }

    // 4) Parse Objects (Model, Geometry, Deformer)
    int32 OP = ObjectsStart;
    while (OP + 13 < ObjectsEnd)
    {
        uint32 OEnd, ONumProps, OPropLen;
        FString OName;
        int32 OPropsStart, OChildStart;
        if (!ParseNodeHeader(OP, OEnd, ONumProps, OPropLen, OName, OPropsStart, OChildStart)) break;

        uint64 ObjID = 0;
        if (OPropsStart + 9 <= OChildStart && FileBytes[OPropsStart] == 'L')
        {
            ObjID = *reinterpret_cast<const uint64*>(&FileBytes[OPropsStart + 1]);
        }

        // Extract Label String
        FString LabelStr = TEXT("");
        for (int32 off = OPropsStart; off + 5 < OChildStart && off < OPropsStart + 60; ++off)
        {
            if (FileBytes[off] == 'S')
            {
                uint32 sLen = *reinterpret_cast<const uint32*>(&FileBytes[off + 1]);
                if (off + 5 + (int32)sLen <= OChildStart)
                {
                    LabelStr = FString(sLen, (const ANSICHAR*)&FileBytes[off + 5]);
                    int32 NullIdx;
                    if (LabelStr.FindChar('\0', NullIdx)) LabelStr = LabelStr.Left(NullIdx);
                    break;
                }
            }
        }

        if (OName.Equals(TEXT("Model")))
        {
            ModelMap.Add(ObjID, LabelStr);
        }
        else if (OName.Equals(TEXT("Geometry")))
        {
            FRawFbxGeom Geom;
            Geom.Label = LabelStr;

            int32 GP = OChildStart;
            while (GP + 13 < (int32)OEnd)
            {
                uint32 GEnd, GNumProps, GPropLen;
                FString GName;
                int32 GPropsStart, GChildStart;
                if (!ParseNodeHeader(GP, GEnd, GNumProps, GPropLen, GName, GPropsStart, GChildStart)) break;

                if (GName.Equals(TEXT("Vertices")) && GPropLen > 12)
                {
                    uint8 TypeCode = FileBytes[GPropsStart];
                    uint32 ArrayLen = *reinterpret_cast<const uint32*>(&FileBytes[GPropsStart + 1]);
                    uint32 Encoding = *reinterpret_cast<const uint32*>(&FileBytes[GPropsStart + 5]);
                    uint32 CompLen = *reinterpret_cast<const uint32*>(&FileBytes[GPropsStart + 9]);
                    int32 DataOffset = GPropsStart + 13;

                    int32 ElemSize = (TypeCode == 'd' ? 8 : 4);
                    int32 UncompSize = ArrayLen * ElemSize;
                    TArray<uint8> UncompBuf;
                    const uint8* DataPtr = nullptr;

                    if (Encoding == 0 && DataOffset + UncompSize <= (int32)GEnd)
                    {
                        DataPtr = &FileBytes[DataOffset];
                    }
                    else if (Encoding == 1 && CompLen > 0 && DataOffset + (int32)CompLen <= (int32)GEnd)
                    {
                        UncompBuf.AddUninitialized(UncompSize);
                        if (FCompression::UncompressMemory(NAME_Zlib, (void*)UncompBuf.GetData(), (int64)UncompSize, (const void*)&FileBytes[DataOffset], (int64)CompLen))
                        {
                            DataPtr = UncompBuf.GetData();
                        }
                    }

                    if (DataPtr && ArrayLen > 0)
                    {
                        // Blender to Unreal coordinate conversion: (X, -Y, Z)
                        if (TypeCode == 'd')
                        {
                            const double* VData = reinterpret_cast<const double*>(DataPtr);
                            for (uint32 v = 0; v + 2 < ArrayLen; v += 3)
                            {
                                Geom.Vertices.Add(FVector(VData[v] * Scale, -VData[v + 1] * Scale, VData[v + 2] * Scale));
                            }
                        }
                        else if (TypeCode == 'f')
                        {
                            const float* VData = reinterpret_cast<const float*>(DataPtr);
                            for (uint32 v = 0; v + 2 < ArrayLen; v += 3)
                            {
                                Geom.Vertices.Add(FVector(VData[v] * Scale, -VData[v + 1] * Scale, VData[v + 2] * Scale));
                            }
                        }
                    }
                }
                else if (GName.Equals(TEXT("PolygonVertexIndex")) && GPropLen > 12)
                {
                    uint32 ArrayLen = *reinterpret_cast<const uint32*>(&FileBytes[GPropsStart + 1]);
                    uint32 Encoding = *reinterpret_cast<const uint32*>(&FileBytes[GPropsStart + 5]);
                    uint32 CompLen = *reinterpret_cast<const uint32*>(&FileBytes[GPropsStart + 9]);
                    int32 DataOffset = GPropsStart + 13;

                    int32 UncompSize = ArrayLen * 4;
                    TArray<uint8> UncompBuf;
                    const uint8* DataPtr = nullptr;

                    if (Encoding == 0 && DataOffset + UncompSize <= (int32)GEnd)
                    {
                        DataPtr = &FileBytes[DataOffset];
                    }
                    else if (Encoding == 1 && CompLen > 0 && DataOffset + (int32)CompLen <= (int32)GEnd)
                    {
                        UncompBuf.AddUninitialized(UncompSize);
                        if (FCompression::UncompressMemory(NAME_Zlib, (void*)UncompBuf.GetData(), (int64)UncompSize, (const void*)&FileBytes[DataOffset], (int64)CompLen))
                        {
                            DataPtr = UncompBuf.GetData();
                        }
                    }

                    if (DataPtr && ArrayLen > 0)
                    {
                        const int32* PIndices = reinterpret_cast<const int32*>(DataPtr);
                        TArray<int32> PolyLoop;
                        for (uint32 idx = 0; idx < ArrayLen; ++idx)
                        {
                            int32 Val = PIndices[idx];
                            bool bIsLast = (Val < 0);
                            int32 RealIdx = bIsLast ? (-Val - 1) : Val;
                            PolyLoop.Add(RealIdx);

                            if (bIsLast)
                            {
                                if (PolyLoop.Num() >= 3)
                                {
                                    // Reverse winding order (0, p+1, p) because Y is inverted (-Y) for Unreal Engine!
                                    for (int32 p = 1; p < PolyLoop.Num() - 1; ++p)
                                    {
                                        Geom.Polygons.Add(PolyLoop[0]);
                                        Geom.Polygons.Add(PolyLoop[p + 1]);
                                        Geom.Polygons.Add(PolyLoop[p]);
                                    }
                                }
                                PolyLoop.Empty();
                            }
                        }
                    }
                }
                GP = (int32)GEnd;
            }
            GeomMap.Add(ObjID, Geom);
        }
        else if (OName.Equals(TEXT("Deformer")))
        {
            int32 DP = OChildStart;
            TArray<int32> SubIndices;

            while (DP + 13 < (int32)OEnd)
            {
                uint32 DEnd, DNumProps, DPropLen;
                FString DName;
                int32 DPropsStart, DChildStart;
                if (!ParseNodeHeader(DP, DEnd, DNumProps, DPropLen, DName, DPropsStart, DChildStart)) break;

                if (DName.Equals(TEXT("Indexes")) && DPropLen > 12)
                {
                    uint32 ArrayLen = *reinterpret_cast<const uint32*>(&FileBytes[DPropsStart + 1]);
                    uint32 Encoding = *reinterpret_cast<const uint32*>(&FileBytes[DPropsStart + 5]);
                    uint32 CompLen = *reinterpret_cast<const uint32*>(&FileBytes[DPropsStart + 9]);
                    int32 DataOffset = DPropsStart + 13;

                    int32 UncompSize = ArrayLen * 4;
                    TArray<uint8> UncompBuf;
                    const uint8* DataPtr = nullptr;

                    if (Encoding == 0 && DataOffset + UncompSize <= (int32)DEnd)
                    {
                        DataPtr = &FileBytes[DataOffset];
                    }
                    else if (Encoding == 1 && CompLen > 0 && DataOffset + (int32)CompLen <= (int32)DEnd)
                    {
                        UncompBuf.AddUninitialized(UncompSize);
                        if (FCompression::UncompressMemory(NAME_Zlib, (void*)UncompBuf.GetData(), (int64)UncompSize, (const void*)&FileBytes[DataOffset], (int64)CompLen))
                        {
                            DataPtr = UncompBuf.GetData();
                        }
                    }

                    if (DataPtr && ArrayLen > 0)
                    {
                        const int32* PIndices = reinterpret_cast<const int32*>(DataPtr);
                        for (uint32 idx = 0; idx < ArrayLen; ++idx)
                        {
                            SubIndices.Add(PIndices[idx]);
                        }
                    }
                }
                DP = (int32)DEnd;
            }

            if (SubIndices.Num() > 0)
            {
                FRawFbxSubDef SubDef;
                SubDef.BoneLabel = LabelStr;
                SubDef.Indices = SubIndices;
                SubDeformerMap.Add(ObjID, SubDef);
            }
            else
            {
                DeformerMap.Add(ObjID, LabelStr);
            }
        }
        OP = (int32)OEnd;
    }

    // 5) Extract and Classify Each Geometry per Model with Local Centering and Smoothed Normals
    for (const auto& GeomPair : GeomMap)
    {
        uint64 GeomID = GeomPair.Key;
        const FRawFbxGeom& Geom = GeomPair.Value;

        // Find Parent Model
        FString ModelName = Geom.Label;
        if (const TArray<uint64>* Parents = ChildToParents.Find(GeomID))
        {
            for (uint64 PID : *Parents)
            {
                if (const FString* MName = ModelMap.Find(PID))
                {
                    ModelName = *MName;
                    break;
                }
            }
        }

        bool bIsSensorMesh = ModelName.StartsWith(TEXT("S_"), ESearchCase::IgnoreCase) ||
                             ModelName.StartsWith(TEXT("Sensor_"), ESearchCase::IgnoreCase);

        bool bIsUCXModel = !bIsSensorMesh && (ModelName.StartsWith(TEXT("UCX_"), ESearchCase::IgnoreCase) ||
                           ModelName.StartsWith(TEXT("UBX_"), ESearchCase::IgnoreCase) ||
                           ModelName.StartsWith(TEXT("USP_"), ESearchCase::IgnoreCase) ||
                           ModelName.StartsWith(TEXT("UCX"), ESearchCase::IgnoreCase) ||
                           ModelName.Contains(TEXT("UCX"), ESearchCase::IgnoreCase));

        // Find Skin Deformers connected to this Geometry
        TArray<uint64> GeomSkinDeformers;
        if (const TArray<uint64>* Children = ParentToChildren.Find(GeomID))
        {
            for (uint64 CID : *Children)
            {
                if (DeformerMap.Contains(CID)) GeomSkinDeformers.Add(CID);
            }
        }

        if (GeomSkinDeformers.Num() > 0)
        {
            for (uint64 SkinDefID : GeomSkinDeformers)
            {
                if (const TArray<uint64>* SubDefs = ParentToChildren.Find(SkinDefID))
                {
                    for (uint64 SubDefID : *SubDefs)
                    {
                        if (const FRawFbxSubDef* SubDef = SubDeformerMap.Find(SubDefID))
                        {
                            TSet<int32> BoneVertSet(SubDef->Indices);
                            TArray<FVector> SubVerts;
                            TArray<int32> SubTris;
                            TMap<int32, int32> VertMap;

                            for (int32 p = 0; p + 2 < Geom.Polygons.Num(); p += 3)
                            {
                                int32 V0 = Geom.Polygons[p];
                                int32 V1 = Geom.Polygons[p + 1];
                                int32 V2 = Geom.Polygons[p + 2];

                                if (BoneVertSet.Contains(V0) && BoneVertSet.Contains(V1) && BoneVertSet.Contains(V2))
                                {
                                    if (!VertMap.Contains(V0)) { VertMap.Add(V0, SubVerts.Num()); SubVerts.Add(Geom.Vertices[V0]); }
                                    if (!VertMap.Contains(V1)) { VertMap.Add(V1, SubVerts.Num()); SubVerts.Add(Geom.Vertices[V1]); }
                                    if (!VertMap.Contains(V2)) { VertMap.Add(V2, SubVerts.Num()); SubVerts.Add(Geom.Vertices[V2]); }

                                    SubTris.Add(VertMap[V0]);
                                    SubTris.Add(VertMap[V1]);
                                    SubTris.Add(VertMap[V2]);
                                }
                            }

                            if (SubVerts.Num() > 0)
                            {
                                FString MeshLabel = SubDef->BoneLabel.IsEmpty() ? ModelName : SubDef->BoneLabel;
                                bool bBoneIsSensor = MeshLabel.StartsWith(TEXT("S_"), ESearchCase::IgnoreCase) ||
                                                     MeshLabel.StartsWith(TEXT("Sensor_"), ESearchCase::IgnoreCase) ||
                                                     bIsSensorMesh;

                                // 1) Calculate Exact Pivot Point (Centroid in World Space)
                                FVector Center = FVector::ZeroVector;
                                for (const FVector& V : SubVerts) Center += V;
                                FVector Pivot = Center / (float)SubVerts.Num();

                                if (bBoneIsSensor)
                                {
                                    // SENSÖR YUVASI: Görsel ve fizik collision OLUŞTURMAZ! Sadece konumu listeye kaydedilir.
                                    FImporterSensorSection SensorSec;
                                    SensorSec.SensorName = MeshLabel;
                                    SensorSec.PivotPoint = Pivot;
                                    SensorSec.Rotation = FRotator::ZeroRotator;
                                    OutSensors.Add(SensorSec);
                                }
                                else if (SubTris.Num() > 0)
                                {
                                    FImporterMeshSection Sec;
                                    Sec.MeshName = bIsUCXModel ? FString::Printf(TEXT("%s_%s"), *ModelName, *MeshLabel) : MeshLabel;
                                    Sec.PivotPoint = Pivot;

                                    // 2) Center Vertices around local origin (0,0,0) for component
                                    for (FVector& V : SubVerts)
                                    {
                                        V = V - Sec.PivotPoint;
                                    }

                                    Sec.Vertices = SubVerts;
                                    Sec.Triangles = SubTris;

                                    // 3) Compute Outward-Facing Smoothed Vertex Normals
                                    Sec.Normals.Init(FVector::ZeroVector, SubVerts.Num());
                                    for (int32 t = 0; t + 2 < SubTris.Num(); t += 3)
                                    {
                                        int32 i0 = SubTris[t], i1 = SubTris[t + 1], i2 = SubTris[t + 2];
                                        FVector TriNormal = ((SubVerts[i2] - SubVerts[i0]) ^ (SubVerts[i1] - SubVerts[i0])).GetSafeNormal();
                                        Sec.Normals[i0] += TriNormal;
                                        Sec.Normals[i1] += TriNormal;
                                        Sec.Normals[i2] += TriNormal;
                                    }
                                    for (FVector& Norm : Sec.Normals)
                                    {
                                        Norm = Norm.GetSafeNormal();
                                        if (Norm.IsNearlyZero()) Norm = FVector::UpVector;
                                    }

                                    if (bIsUCXModel)
                                    {
                                        OutUCX.Add(Sec);
                                    }
                                    else
                                    {
                                        OutVisual.Add(Sec);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        else if (Geom.Vertices.Num() > 0)
        {
            // Static unskinned geometry
            FVector Center = FVector::ZeroVector;
            for (const FVector& V : Geom.Vertices) Center += V;
            FVector Pivot = Center / (float)Geom.Vertices.Num();

            if (bIsSensorMesh)
            {
                // SENSÖR YUVASI: Görsel ve fizik collision OLUŞTURMAZ! Sadece konumu listeye kaydedilir.
                FImporterSensorSection SensorSec;
                SensorSec.SensorName = ModelName;
                SensorSec.PivotPoint = Pivot;
                SensorSec.Rotation = FRotator::ZeroRotator;
                OutSensors.Add(SensorSec);
            }
            else if (Geom.Polygons.Num() > 0)
            {
                FImporterMeshSection Sec;
                Sec.MeshName = ModelName;
                Sec.PivotPoint = Pivot;

                TArray<FVector> CenteredVerts = Geom.Vertices;
                for (FVector& V : CenteredVerts) V = V - Sec.PivotPoint;

                Sec.Vertices = CenteredVerts;
                Sec.Triangles = Geom.Polygons;

                Sec.Normals.Init(FVector::ZeroVector, CenteredVerts.Num());
                for (int32 t = 0; t + 2 < Geom.Polygons.Num(); t += 3)
                {
                    int32 i0 = Geom.Polygons[t], i1 = Geom.Polygons[t + 1], i2 = Geom.Polygons[t + 2];
                    FVector TriNormal = ((CenteredVerts[i2] - CenteredVerts[i0]) ^ (CenteredVerts[i1] - CenteredVerts[i0])).GetSafeNormal();
                    Sec.Normals[i0] += TriNormal;
                    Sec.Normals[i1] += TriNormal;
                    Sec.Normals[i2] += TriNormal;
                }
                for (FVector& Norm : Sec.Normals)
                {
                    Norm = Norm.GetSafeNormal();
                    if (Norm.IsNearlyZero()) Norm = FVector::UpVector;
                }

                if (bIsUCXModel) OutUCX.Add(Sec);
                else OutVisual.Add(Sec);
            }
        }
    }

    // =========================================================================================
    // HEM LOGA HEM DE EKRANA HER ÜÇ LİSTEYİ DE DETAYLICA YAZDIR!
    // =========================================================================================
    UE_LOG(LogTemp, Warning, TEXT("===================================================================="));
    UE_LOG(LogTemp, Warning, TEXT(">>> [PiSimModelImporter] FBX AYRIŞTIRMA RAPORU (%s) <<<"), *FilePath);
    UE_LOG(LogTemp, Warning, TEXT("🎨 GÖRSEL PARÇA LİSTESİ (Toplam: %d Adet):"), OutVisual.Num());
    for (int32 v = 0; v < OutVisual.Num(); ++v)
    {
        UE_LOG(LogTemp, Warning, TEXT("   [%d] VisualMesh: '%s' | Vertices: %d | Triangles: %d | Pivot: %s"),
            v, *OutVisual[v].MeshName, OutVisual[v].Vertices.Num(), OutVisual[v].Triangles.Num() / 3, *OutVisual[v].PivotPoint.ToString());
    }

    UE_LOG(LogTemp, Warning, TEXT("🛡️ UCX COLLISION PARÇA LİSTESİ (Toplam: %d Adet):"), OutUCX.Num());
    for (int32 u = 0; u < OutUCX.Num(); ++u)
    {
        UE_LOG(LogTemp, Warning, TEXT("   [%d] UCXMesh: '%s' | Vertices: %d | Triangles: %d | Pivot: %s"),
            u, *OutUCX[u].MeshName, OutUCX[u].Vertices.Num(), OutUCX[u].Triangles.Num() / 3, *OutUCX[u].PivotPoint.ToString());
    }

    UE_LOG(LogTemp, Warning, TEXT("📡 SENSÖR YUVASI LİSTESİ (Toplam: %d Adet):"), OutSensors.Num());
    for (int32 s = 0; s < OutSensors.Num(); ++s)
    {
        UE_LOG(LogTemp, Warning, TEXT("   [%d] SensorSlot: '%s' | Pivot: %s"),
            s, *OutSensors[s].SensorName, *OutSensors[s].PivotPoint.ToString());
    }
    UE_LOG(LogTemp, Warning, TEXT("===================================================================="));

    // Canlı Ekrana Renkli Bildirimler Bas
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(501, 15.0f, FColor::Cyan,
            FString::Printf(TEXT("🎨 [GÖRSEL LİSTE: %d PARÇA]"), OutVisual.Num()));
        for (int32 v = 0; v < FMath::Min(OutVisual.Num(), 4); ++v)
        {
            GEngine->AddOnScreenDebugMessage(510 + v, 15.0f, FColor::White,
                FString::Printf(TEXT("   • Visual [%d]: %s (%d Verts, %d Tris)"), v, *OutVisual[v].MeshName, OutVisual[v].Vertices.Num(), OutVisual[v].Triangles.Num() / 3));
        }

        GEngine->AddOnScreenDebugMessage(530, 15.0f, FColor::Yellow,
            FString::Printf(TEXT("🛡️ [UCX COLLISION LİSTE: %d PARÇA]"), OutUCX.Num()));
        for (int32 u = 0; u < FMath::Min(OutUCX.Num(), 4); ++u)
        {
            GEngine->AddOnScreenDebugMessage(540 + u, 15.0f, FColor::Orange,
                FString::Printf(TEXT("   • UCX [%d]: %s (%d Verts)"), u, *OutUCX[u].MeshName, OutUCX[u].Vertices.Num()));
        }

        GEngine->AddOnScreenDebugMessage(550, 15.0f, FColor::Green,
            FString::Printf(TEXT("📡 [SENSÖR YUVALARI: %d ADET]"), OutSensors.Num()));
        for (int32 s = 0; s < FMath::Min(OutSensors.Num(), 4); ++s)
        {
            GEngine->AddOnScreenDebugMessage(560 + s, 15.0f, FColor::Emerald,
                FString::Printf(TEXT("   • Sensor [%d]: %s (Konum: %s)"), s, *OutSensors[s].SensorName, *OutSensors[s].PivotPoint.ToString()));
        }
    }

    // Saf Hiyerarşik Kemikleri Yakala (Mesh'i olmayan M_Steer_FR, M_Steer_FL vb.)
    OutPureBones.Empty();
    for (const auto& Pair : ModelMap)
    {
        FString MName = Pair.Value;
        FString Lower = MName.ToLower();
        if (Lower.Contains(TEXT("armature")) || Lower.Contains(TEXT("root")) || Lower.StartsWith(TEXT("ucx")) || Lower.StartsWith(TEXT("s_")))
        {
            continue;
        }
        bool bAlreadyInVisual = false;
        for (const FImporterMeshSection& Sec : OutVisual)
        {
            if (Sec.MeshName.Equals(MName, ESearchCase::IgnoreCase))
            {
                bAlreadyInVisual = true;
                break;
            }
        }
        if (!bAlreadyInVisual && (MName.StartsWith(TEXT("M_"), ESearchCase::IgnoreCase) || Lower.Contains(TEXT("steer")) || Lower.Contains(TEXT("servo")) || Lower.Contains(TEXT("joint"))))
        {
            OutPureBones.AddUnique(MName);
        }
    }

    return (OutVisual.Num() > 0 || OutUCX.Num() > 0 || OutSensors.Num() > 0);
}

// =========================================================================================
// [AŞAMA 3] HİYERARŞİK BAĞLAMA, GÖRSEL ÇİZİM (Visual) VE COLLISION AKTİFLEŞTİRME (UCX)
// =========================================================================================
void APiSimModelImporter::BuildAndSpawnRobotHierarchy(float Scale)
{
    ClearSpawnedComponents();

    FString FbxPath = FPaths::ProjectSavedDir() / TEXT("Robots/Cache/robot_import_test.fbx");
    if (!ParseBinaryFbxFile(FbxPath, VisualSections, UCXSections, SensorSections, PureBoneNames, Scale))

    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 8.0f, FColor::Red,
                FString::Printf(TEXT(">>> [PiSimModelImporter HATA] %s okunamadi! <<<"), *FbxPath));
        }
        return;
    }

    UMaterialInterface* DefaultMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

    // -----------------------------------------------------------------------------------------
    // 1) HER PARÇA İÇİN GÖRSEL RENDER VE UCX COLLISION'I TEK BİR DİNAMİK GÖVDEDE BİRLEŞTİR
    // -----------------------------------------------------------------------------------------
    for (int32 i = 0; i < VisualSections.Num(); ++i)
    {
        FName CompName = *FString::Printf(TEXT("RobotSubMesh_%d_%s"), i, *VisualSections[i].MeshName);
        UProceduralMeshComponent* VisComp = NewObject<UProceduralMeshComponent>(this, CompName);
        VisComp->SetMobility(EComponentMobility::Movable);

        // Hiyerarşik Kemik Bağlantısı: Gövde dışındaki parçalar gövdeye bağlanır
        if (i == 0)
        {
            VisComp->SetupAttachment(SceneRootComponent);
            VisComp->SetRelativeLocation(VisualSections[i].PivotPoint);
        }
        else
        {
            if (VisualMeshComponents.IsValidIndex(0) && VisualMeshComponents[0])
            {
                VisComp->SetupAttachment(VisualMeshComponents[0]);
                // Gövdeye göre bağıl konum: (TekerlekPivot - GövdePivot)
                VisComp->SetRelativeLocation(VisualSections[i].PivotPoint - VisualSections[0].PivotPoint);
            }
            else
            {
                VisComp->SetupAttachment(SceneRootComponent);
                VisComp->SetRelativeLocation(VisualSections[i].PivotPoint);
            }
        }

        VisComp->RegisterComponent();

        TArray<FVector2D> UV0;
        TArray<FLinearColor> VertexColors;
        TArray<FProcMeshTangent> Tangents;

        // Görsel Modeli Çiz (Render)
        VisComp->CreateMeshSection_LinearColor(0, VisualSections[i].Vertices, VisualSections[i].Triangles, VisualSections[i].Normals, UV0, VertexColors, Tangents, false);
        if (DefaultMat) VisComp->SetMaterial(0, DefaultMat);

        VisComp->SetVisibility(true);
        VisComp->SetHiddenInGame(false);

        // İLGİLİ UCX CONVEX HULL'UNU BUL VE PARÇAYA ZIRH OLARAK GİYDİR
        VisComp->ClearCollisionConvexMeshes();
        bool bFoundUCX = false;
        for (int32 j = 0; j < UCXSections.Num(); ++j)
        {
            if (UCXSections[j].MeshName.Contains(VisualSections[i].MeshName, ESearchCase::IgnoreCase))
            {
                VisComp->AddCollisionConvexMesh(UCXSections[j].Vertices);
                bFoundUCX = true;
                break;
            }
        }
        if (!bFoundUCX)
        {
            VisComp->AddCollisionConvexMesh(VisualSections[i].Vertices);
        }

        // STATİK HALDE DE SOLID ÇARPIŞMA %100 AKTİF!
        VisComp->bUseComplexAsSimpleCollision = false;
        VisComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        VisComp->SetCollisionObjectType(ECC_WorldDynamic);
        VisComp->SetCollisionResponseToAllChannels(ECR_Block);
        VisComp->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block); // Zemini bloklar
        VisComp->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
        VisComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
        VisComp->RecreatePhysicsState();
        VisComp->UpdateBounds();

        VisualMeshComponents.Add(VisComp);
    }

    // -----------------------------------------------------------------------------------------
    // 3) UCX COLLISION MESH COMPONENTLARI OLUŞTUR (Görsel Zırh İnceleme Toggle'ı İçin)
    // -----------------------------------------------------------------------------------------
    for (int32 j = 0; j < UCXSections.Num(); ++j)
    {
        FName CollCompName = *FString::Printf(TEXT("UcxCollisionMesh_%d_%s"), j, *UCXSections[j].MeshName);
        UProceduralMeshComponent* CollMesh = NewObject<UProceduralMeshComponent>(this, CollCompName);
        CollMesh->SetMobility(EComponentMobility::Movable);

        if (VisualMeshComponents.IsValidIndex(0) && VisualMeshComponents[0])
        {
            CollMesh->SetupAttachment(VisualMeshComponents[0]);
            CollMesh->SetRelativeLocation(UCXSections[j].PivotPoint - VisualSections[0].PivotPoint);
        }
        else
        {
            CollMesh->SetupAttachment(SceneRootComponent);
            CollMesh->SetRelativeLocation(UCXSections[j].PivotPoint);
        }

        CollMesh->RegisterComponent();

        TArray<FVector2D> UV0;
        TArray<FLinearColor> VertexColors;
        TArray<FProcMeshTangent> Tangents;
        CollMesh->CreateMeshSection_LinearColor(0, UCXSections[j].Vertices, UCXSections[j].Triangles, UCXSections[j].Normals, UV0, VertexColors, Tangents, false);
        if (DefaultMat) CollMesh->SetMaterial(0, DefaultMat);

        CollMesh->SetVisibility(bShowCollisionView);
        CollMesh->SetHiddenInGame(!bShowCollisionView);
        CollMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // Purely for visual inspector toggle

        CollisionMeshComponents.Add(CollMesh);
    }

    // -----------------------------------------------------------------------------------------
    // 4) MOTORLARI VE KEMİKLERİ OTOMATİK SINIFLANDIR & LİSTEYE EKLE
    // -----------------------------------------------------------------------------------------
    ConfiguredMotors.Empty();
    for (int32 i = 0; i < VisualSections.Num(); ++i)
    {
        FPiSimMotorItem MotorItem;
        MotorItem.BoneIndex = i;
        MotorItem.BoneName = VisualSections[i].MeshName;

        FString LowerName = VisualSections[i].MeshName.ToLower();
        if (LowerName.StartsWith(TEXT("m_wheel")) || LowerName.StartsWith(TEXT("w_")) || LowerName.Contains(TEXT("wheel")))
        {
            MotorItem.Role = EPiSimMotorRole::DriveWheel;
            MotorItem.MaxVelocityRPM = 500.0f;
            MotorItem.MaxTorqueNm = 15.0f;
        }
        else if (LowerName.StartsWith(TEXT("m_steer")) || LowerName.StartsWith(TEXT("steer")))
        {
            MotorItem.Role = EPiSimMotorRole::SteeredWheel;
            MotorItem.MinLimitDeg = -45.0f;
            MotorItem.MaxLimitDeg = +45.0f;
            MotorItem.MaxTorqueNm = 20.0f;
        }
        else if (LowerName.StartsWith(TEXT("m_caster")) || LowerName.StartsWith(TEXT("caster")))
        {
            MotorItem.Role = EPiSimMotorRole::FreeCaster;
        }
        else if (LowerName.StartsWith(TEXT("m_servo")) || LowerName.StartsWith(TEXT("joint")) || LowerName.StartsWith(TEXT("bone")) || LowerName.Contains(TEXT("arm")))
        {
            MotorItem.Role = EPiSimMotorRole::ServoJoint;
            MotorItem.MinLimitDeg = -90.0f;
            MotorItem.MaxLimitDeg = +90.0f;
            MotorItem.MaxTorqueNm = 25.0f;
        }
        else if (LowerName.StartsWith(TEXT("m_piston")) || LowerName.StartsWith(TEXT("piston")) || LowerName.StartsWith(TEXT("linear")))
        {
            MotorItem.Role = EPiSimMotorRole::LinearActuator;
            MotorItem.MinLimitDeg = 0.0f;
            MotorItem.MaxLimitDeg = 100.0f;
            MotorItem.MaxTorqueNm = 5000.0f;
        }
        else if (LowerName.StartsWith(TEXT("m_thrust")) || LowerName.StartsWith(TEXT("prop")) || LowerName.StartsWith(TEXT("thruster")))
        {
            MotorItem.Role = EPiSimMotorRole::Thruster;
            MotorItem.MaxVelocityRPM = 6000.0f;
            MotorItem.MaxTorqueNm = 30.0f;
        }
        else if (LowerName.StartsWith(TEXT("m_track")) || LowerName.StartsWith(TEXT("track")))
        {
            MotorItem.Role = EPiSimMotorRole::TrackPad;
            MotorItem.MaxVelocityRPM = 300.0f;
            MotorItem.MaxTorqueNm = 40.0f;
        }
        else if (i > 0)
        {
            MotorItem.Role = EPiSimMotorRole::DriveWheel;
            MotorItem.MaxVelocityRPM = 500.0f;
            MotorItem.MaxTorqueNm = 15.0f;
        }
        else
        {
            MotorItem.Role = EPiSimMotorRole::None; // Root chassis
        }

        ConfiguredMotors.Add(MotorItem);
    }

    // Saf Hiyerarşik Kemikleri (M_Steer vb.) de Sadece Motor Listesine Ekle
    // Kullanıcı Talimatı: "o M_Steer kemikleri sadece ama sadece servo olacak"
    for (const FString& PureBone : PureBoneNames)
    {
        FPiSimMotorItem PureMotorItem;
        PureMotorItem.BoneIndex = ConfiguredMotors.Num();
        PureMotorItem.BoneName = PureBone;
        PureMotorItem.Role = EPiSimMotorRole::ServoJoint;
        PureMotorItem.MinLimitDeg = -45.0f;
        PureMotorItem.MaxLimitDeg = +45.0f;
        PureMotorItem.MaxTorqueNm = 20.0f;
        ConfiguredMotors.Add(PureMotorItem);
    }

    // -----------------------------------------------------------------------------------------
    // 5) SENSÖRLERİ SINIFLANDIR & LİSTEYE EKLE
    // -----------------------------------------------------------------------------------------
    ConfiguredSensors.Empty();
    for (int32 s = 0; s < SensorSections.Num(); ++s)
    {
        FPiSimSensorItem SensorItem;
        SensorItem.SensorIndex = s;
        SensorItem.SensorName = SensorSections[s].SensorName;
        SensorItem.PivotPoint = SensorSections[s].PivotPoint;
        SensorItem.Rotation = SensorSections[s].Rotation;

        FString LowerSensor = SensorSections[s].SensorName.ToLower();
        if (LowerSensor.Contains(TEXT("cam")))
        {
            SensorItem.Type = EPiSimSensorType::Camera;
            SensorItem.FovAngle = 90.0f;
            SensorItem.Fps = 25;
            SensorItem.Port = 5000;
        }
        else if (LowerSensor.Contains(TEXT("imu")))
        {
            SensorItem.Type = EPiSimSensorType::IMU;
            SensorItem.Fps = 100;
            SensorItem.Port = 7401;
        }
        else if (LowerSensor.Contains(TEXT("gps")))
        {
            SensorItem.Type = EPiSimSensorType::GPS;
            SensorItem.Fps = 10;
        }
        else if (LowerSensor.Contains(TEXT("lidar")))
        {
            SensorItem.Type = EPiSimSensorType::LiDAR;
            SensorItem.Fps = 20;
        }
        else
        {
            SensorItem.Type = EPiSimSensorType::Unknown;
        }

        ConfiguredSensors.Add(SensorItem);
    }

    // -----------------------------------------------------------------------------------------
    // 6) EĞER HİÇ UCX YOKSA: GÖRSEL PARÇALARIN KENDİSİNE COLLISION VER (Güvenlik Sigortası)
    // -----------------------------------------------------------------------------------------
    if (UCXSections.Num() == 0)
    {
        for (int32 i = 0; i < VisualMeshComponents.Num(); ++i)
        {
            VisualMeshComponents[i]->ClearCollisionConvexMeshes();
            VisualMeshComponents[i]->AddCollisionConvexMesh(VisualSections[i].Vertices);
            VisualMeshComponents[i]->bUseComplexAsSimpleCollision = false;
            VisualMeshComponents[i]->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            VisualMeshComponents[i]->SetCollisionObjectType(ECC_WorldDynamic);
            VisualMeshComponents[i]->SetCollisionResponseToAllChannels(ECR_Block);
            VisualMeshComponents[i]->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
            VisualMeshComponents[i]->RecreatePhysicsState();
            VisualMeshComponents[i]->UpdateBounds();
        }
    }

    // Robot parçalarının kendi kendine çarpışmasını engelle (Self-Collision Filtering)
    for (int32 i = 0; i < VisualMeshComponents.Num(); ++i)
    {
        for (int32 j = i + 1; j < VisualMeshComponents.Num(); ++j)
        {
            if (VisualMeshComponents[i] && VisualMeshComponents[j])
            {
                VisualMeshComponents[i]->IgnoreComponentWhenMoving(VisualMeshComponents[j], true);
                VisualMeshComponents[j]->IgnoreComponentWhenMoving(VisualMeshComponents[i], true);
            }
        }
    }

    // 7) Kamerayı direkt olarak araç gövdesine (Chassis) kilitle!
    if (VisualMeshComponents.IsValidIndex(0) && VisualMeshComponents[0] && OrbitSpringArm)
    {
        OrbitSpringArm->AttachToComponent(VisualMeshComponents[0], FAttachmentTransformRules::SnapToTargetNotIncludingScale);
        OrbitSpringArm->SetRelativeLocation(FVector(0.0f, 0.0f, 60.0f));
        OrbitSpringArm->TargetArmLength = 350.0f;
        OrbitSpringArm->bInheritPitch = false;
        OrbitSpringArm->bInheritRoll = false;
        OrbitSpringArm->bInheritYaw = true;
    }

    // 8) DİNAMİK SENSÖR YERLEŞTİRME (S_Cam_..., S_Camera_..., vb.)
    int32 DiscoveredCameras = 0;
    for (const FImporterSensorSection& Sensor : SensorSections)
    {
        if (Sensor.SensorName.StartsWith(TEXT("S_Cam"), ESearchCase::IgnoreCase) ||
            Sensor.SensorName.StartsWith(TEXT("S_Camera"), ESearchCase::IgnoreCase) ||
            Sensor.SensorName.Contains(TEXT("Cam"), ESearchCase::IgnoreCase))
        {
            FName CamCompName = *FString::Printf(TEXT("FpvCamComp_%d_%s"), DiscoveredCameras, *Sensor.SensorName);
            USceneCaptureComponent2D* NewCam = NewObject<USceneCaptureComponent2D>(this, CamCompName);
            NewCam->SetMobility(EComponentMobility::Movable);
            NewCam->FOVAngle = 90.0f;
            NewCam->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
            NewCam->bCaptureEveryFrame = true;
            NewCam->bCaptureOnMovement = false;
            NewCam->bAlwaysPersistRenderingState = true;
            NewCam->TextureTarget = VideoRenderTarget;

            // Kalibrasyon: UE5 Lumen ve Fiziksel Aydınlatmada kusursuz FPV pozlaması (EV100 Min: -10, Max: 20)
            NewCam->PostProcessSettings.bOverride_AutoExposureMethod = true;
            NewCam->PostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Histogram;
            NewCam->PostProcessSettings.bOverride_AutoExposureMinBrightness = true;
            NewCam->PostProcessSettings.AutoExposureMinBrightness = -10.0f;
            NewCam->PostProcessSettings.bOverride_AutoExposureMaxBrightness = true;
            NewCam->PostProcessSettings.AutoExposureMaxBrightness = 20.0f;
            NewCam->PostProcessSettings.bOverride_AutoExposureBias = true;
            NewCam->PostProcessSettings.AutoExposureBias = 0.0f;
            NewCam->PostProcessSettings.bOverride_BloomIntensity = true;
            NewCam->PostProcessSettings.BloomIntensity = 0.0f;
            NewCam->PostProcessSettings.bOverride_MotionBlurAmount = true;
            NewCam->PostProcessSettings.MotionBlurAmount = 0.0f;

            // Sensör küpünün tam koordinatlarına monte et (90 derece sola dönüş ofseti ile)
            FRotator CamRotation = Sensor.Rotation + FRotator(0.0f, -90.0f, 0.0f);

            if (VisualMeshComponents.IsValidIndex(0) && VisualMeshComponents[0])
            {
                NewCam->AttachToComponent(VisualMeshComponents[0], FAttachmentTransformRules::SnapToTargetNotIncludingScale);
                // Gövdeye göre bağıl konum: (SensörPivot - GövdePivot)
                FVector RelativeLoc = Sensor.PivotPoint - VisualSections[0].PivotPoint;
                NewCam->SetRelativeLocation(RelativeLoc);
                NewCam->SetRelativeRotation(CamRotation);
                AddConnectionDebugLog(FString::Printf(TEXT("📷 [Sensör Eklendi] '%s' gövdeye bağlandı -> Bağıl Konum: (X=%.1f, Y=%.1f, Z=%.1f), Rot: (P=%.1f, Y=%.1f, R=%.1f)"),
                    *Sensor.SensorName, RelativeLoc.X, RelativeLoc.Y, RelativeLoc.Z, CamRotation.Pitch, CamRotation.Yaw, CamRotation.Roll));
            }
            else
            {
                NewCam->AttachToComponent(SceneRootComponent, FAttachmentTransformRules::KeepRelativeTransform);
                NewCam->SetRelativeLocation(Sensor.PivotPoint);
                NewCam->SetRelativeRotation(CamRotation);
                AddConnectionDebugLog(FString::Printf(TEXT("📷 [Sensör Eklendi] '%s' kök bileşene bağlandı -> Konum: %s"),
                    *Sensor.SensorName, *Sensor.PivotPoint.ToString()));
            }

            NewCam->RegisterComponent();
            SpawnedCameraComponents.Add(NewCam);

            if (!FpvCameraCapture)
            {
                FpvCameraCapture = NewCam;
                bEnableVideoStream = true;
            }

            DiscoveredCameras++;
        }
    }

    // 9) SENSÖR GÖRSEL İŞARETÇİLERİ (S_... Marker Meshleri)
    for (int32 s = 0; s < SensorSections.Num(); ++s)
    {
        const FImporterSensorSection& Sensor = SensorSections[s];
        FName MarkerCompName = *FString::Printf(TEXT("SensorMarkerComp_%d_%s"), s, *Sensor.SensorName);
        UProceduralMeshComponent* MarkerComp = NewObject<UProceduralMeshComponent>(this, MarkerCompName);
        if (MarkerComp)
        {
            MarkerComp->SetMobility(EComponentMobility::Movable);
            if (VisualMeshComponents.IsValidIndex(0) && VisualMeshComponents[0])
            {
                MarkerComp->AttachToComponent(VisualMeshComponents[0], FAttachmentTransformRules::SnapToTargetNotIncludingScale);
                FVector RelativeLoc = Sensor.PivotPoint - VisualSections[0].PivotPoint;
                MarkerComp->SetRelativeLocation(RelativeLoc);
                MarkerComp->SetRelativeRotation(Sensor.Rotation);
            }
            else
            {
                MarkerComp->AttachToComponent(SceneRootComponent, FAttachmentTransformRules::KeepRelativeTransform);
                MarkerComp->SetRelativeLocation(Sensor.PivotPoint);
                MarkerComp->SetRelativeRotation(Sensor.Rotation);
            }

            float H = 4.0f; // 8x8x8 cm küp
            TArray<FVector> BoxVerts = {
                FVector(-H,-H,-H), FVector(H,-H,-H), FVector(H,H,-H), FVector(-H,H,-H),
                FVector(-H,-H,H),  FVector(H,-H,H),  FVector(H,H,H),  FVector(-H,H,H)
            };
            TArray<int32> BoxTris = {
                0,2,1, 0,3,2, 4,5,6, 4,6,7,
                0,1,5, 0,5,4, 1,2,6, 1,6,5,
                2,3,7, 2,7,6, 3,0,4, 3,4,7
            };
            TArray<FVector> BoxNorms; BoxNorms.Init(FVector::UpVector, BoxVerts.Num());
            TArray<FVector2D> BoxUVs; BoxUVs.Init(FVector2D::ZeroVector, BoxVerts.Num());
            TArray<FProcMeshTangent> BoxTangs;
            TArray<FColor> BoxColors; BoxColors.Init(FColor(0, 220, 255, 200), BoxVerts.Num());

            MarkerComp->CreateMeshSection(0, BoxVerts, BoxTris, BoxNorms, BoxUVs, BoxColors, BoxTangs, false);
            MarkerComp->SetVisibility(bShowSensorMarkers);
            MarkerComp->SetHiddenInGame(!bShowSensorMarkers);
            MarkerComp->RegisterComponent();
            SensorMarkerComponents.Add(MarkerComp);
        }
    }

    if (DiscoveredCameras == 0)
    {
        bEnableVideoStream = false;
        FpvCameraCapture = nullptr;
        AddConnectionDebugLog(TEXT("ℹ️ Modelde 'S_Cam_...' sensör küpü bulunamadı. Kamera eklenmedi."));
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(555, 8.0f, FColor::Yellow,
                TEXT("ℹ️ [PiSim Sensör] Modelde 'S_Cam_...' sensörü bulunamadı (Kamera yok)"));
        }
    }
    else
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(555, 8.0f, FColor::Green,
                FString::Printf(TEXT("📷 [PiSim Sensör] %d adet S_Cam kamerası model koordinatlarına yerleştirildi!"), DiscoveredCameras));
        }
    }
}



// =========================================================================================
// [AŞAMA 4] FİZİK VE YERÇEKİMİNİ AKTİFLEŞTİRME / KAPATMA (Simulate Physics)
// =========================================================================================
void APiSimModelImporter::SetPhysicsSimulationActive(bool bActive)
{
    if (VisualMeshComponents.Num() == 0)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT(">>> [HATA] Sahnede yüklü parça bulunamadı! <<<"));
        }
        return;
    }

    if (!bActive)
    {
        // -------------------------------------------------------------------------------------
        // FİZİĞİ DURDUR (Statik Garaj Moduna Dön)
        // -------------------------------------------------------------------------------------
        for (UPhysicsConstraintComponent* Constraint : JointConstraints)
        {
            if (Constraint) Constraint->DestroyComponent();
        }
        JointConstraints.Empty();

        for (int32 i = 0; i < VisualMeshComponents.Num(); ++i)
        {
            if (VisualMeshComponents[i])
            {
                VisualMeshComponents[i]->SetSimulatePhysics(false);
                VisualMeshComponents[i]->SetEnableGravity(false);
            }
        }

        // Hiyerarşiyi ve konumu yeniden sıfırla
        BuildAndSpawnRobotHierarchy(ImportScaleMultiplier);

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(801, 8.0f, FColor::Orange, TEXT("🛑 >>> [FİZİK SİMÜLASYONU DURDURULDU] Robot Statik Moda Alındı <<<"));
        }
        UE_LOG(LogTemp, Warning, TEXT("[PiSimModelImporter LOG] Fizik simülasyonu durduruldu."));
        return;
    }

    // -----------------------------------------------------------------------------------------
    // FİZİĞİ AKTİFLEŞTİR (Canlı Chaos Multi-Body Gazebo Simülasyonu)
    // -----------------------------------------------------------------------------------------
    UE_LOG(LogTemp, Warning, TEXT("===================================================================="));
    UE_LOG(LogTemp, Warning, TEXT("[PiSimModelImporter LOG] >>> CANLI CHAOS FİZİK SİMÜLASYONU BAŞLATILIYOR <<<"));
    UE_LOG(LogTemp, Warning, TEXT("===================================================================="));

    // 1) Ana Gövdeyi Kök Bileşen Yap
    if (VisualMeshComponents.IsValidIndex(0) && VisualMeshComponents[0])
    {
        VisualMeshComponents[0]->SetMobility(EComponentMobility::Movable);
        VisualMeshComponents[0]->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
        SetRootComponent(VisualMeshComponents[0]);
    }

    // 2) Her Parçaya Kütle, Zırh ve Dinamik Fizik Ver
    for (int32 i = 0; i < VisualMeshComponents.Num(); ++i)
    {
        UProceduralMeshComponent* VisComp = VisualMeshComponents[i];
        if (!VisComp || !VisualSections.IsValidIndex(i)) continue;

        float Mass = (i == 0) ? 30.0f : 2.5f;

        // İlgili UCX Convex Hull'unu aktar (veya kendi geometrisi)
        VisComp->ClearCollisionConvexMeshes();
        bool bFoundUCX = false;
        for (const FImporterMeshSection& UcxSec : UCXSections)
        {
            if (UcxSec.MeshName.Contains(VisualSections[i].MeshName, ESearchCase::IgnoreCase))
            {
                VisComp->AddCollisionConvexMesh(UcxSec.Vertices);
                bFoundUCX = true;
                break;
            }
        }
        if (!bFoundUCX)
        {
            VisComp->AddCollisionConvexMesh(VisualSections[i].Vertices);
        }

        VisComp->bUseComplexAsSimpleCollision = false;
        VisComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        VisComp->SetCollisionObjectType(ECC_WorldDynamic);
        VisComp->SetCollisionResponseToAllChannels(ECR_Block);
        VisComp->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block); // Zeminle çarpış
        VisComp->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
        VisComp->RecreatePhysicsState();
        VisComp->UpdateBounds();

        // Dinamik Fiziği Aç
        VisComp->SetMobility(EComponentMobility::Movable);
        VisComp->SetSimulatePhysics(true);
        VisComp->SetEnableGravity(true);
        VisComp->SetMassOverrideInKg(NAME_None, Mass, true);
        VisComp->SetLinearDamping(0.8f);
        VisComp->SetAngularDamping(1.5f);
        VisComp->WakeRigidBody();

        UE_LOG(LogTemp, Warning, TEXT("[FİZİK LOG] '%s' bileşenine CANLI DİNAMİK FİZİK verildi | Kütle: %.1f kg | Yerçekimi: AÇIK"),
            *VisualSections[i].MeshName, Mass);

        // 3) Gövde Dışındaki Tekerlekler İçin Fiziksel Eklem (Constraint) Bağla
        if (i > 0 && VisualMeshComponents[0])
        {
            FName ConstraintName = *FString::Printf(TEXT("PhysicsJoint_%d_%s"), i, *VisualSections[i].MeshName);
            UPhysicsConstraintComponent* Constraint = NewObject<UPhysicsConstraintComponent>(this, ConstraintName);
            Constraint->SetupAttachment(VisualMeshComponents[0]);
            Constraint->SetRelativeLocation(VisualSections[i].PivotPoint - VisualSections[0].PivotPoint);
            Constraint->RegisterComponent();

            Constraint->SetConstrainedComponents(VisualMeshComponents[0], NAME_None, VisComp, NAME_None);
            Constraint->SetAngularTwistLimit(EAngularConstraintMotion::ACM_Free, 0.0f); // Serbest tekerlek dönüşü
            Constraint->SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Locked, 0.0f);
            Constraint->SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Locked, 0.0f);
            Constraint->SetLinearXLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);
            Constraint->SetLinearYLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);
            Constraint->SetLinearZLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);

            JointConstraints.Add(Constraint);
        }
    }

    // Ekran Bildirimleri
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(801, 10.0f, FColor::Cyan,
            FString::Printf(TEXT("🚀 >>> [CHAOS FİZİK AKTİF!] %d Parça Canlı Simüle Ediliyor <<<"), VisualMeshComponents.Num()));
        
        GEngine->AddOnScreenDebugMessage(802, 10.0f, FColor::Emerald,
            FString::Printf(TEXT("🛡️ >>> [GÖVDE: 30.0 KG | YERÇEKİMİ: AÇIK] Zemin Çarpışması Aktif <<<")));

        GEngine->AddOnScreenDebugMessage(803, 10.0f, FColor::Yellow,
            FString::Printf(TEXT("⚙️ >>> [%d ADET EKLEM KISITLAMASI BAĞLANDI] Tekerlekler Serbest Dönüyor! <<<"), JointConstraints.Num()));
    }
}

// =========================================================================================
// [AŞAMA 5] 3-SEKMELİ PiSim ROBOT STUDIO METOTLARI
// =========================================================================================
void APiSimModelImporter::SetActiveTab(EPiSimActiveTab NewTab)
{
    CurrentActiveTab = NewTab;
}

void APiSimModelImporter::ToggleDisplayMode()
{
    SetDisplayMode(!bShowCollisionView);
}

void APiSimModelImporter::SetDisplayMode(bool bCollision)
{
    bShowCollisionView = bCollision;
    UpdateVisualMaterials();
}

void APiSimModelImporter::SelectBone(int32 Index)
{
    SelectedBoneIndex = Index;
    UpdateVisualMaterials();
}

void APiSimModelImporter::SelectSensor(int32 Index)
{
    SelectedSensorIndex = Index;
}

void APiSimModelImporter::SetMotorTestValue(int32 BoneIndex, float Value)
{
    if (BoneIndex <= 0 || !ConfiguredMotors.IsValidIndex(BoneIndex)) return; // Gövdeye motor atanamaz
    ConfiguredMotors[BoneIndex].CurrentTestValue = Value;

    if (VisualMeshComponents.IsValidIndex(BoneIndex) && VisualMeshComponents[BoneIndex])
    {
        EPiSimMotorRole MotorRole = ConfiguredMotors[BoneIndex].Role;
        if (MotorRole == EPiSimMotorRole::DriveWheel || MotorRole == EPiSimMotorRole::Thruster || MotorRole == EPiSimMotorRole::TrackPad)
        {
            float IndividualRpm = Value * ConfiguredMotors[BoneIndex].MaxVelocityRPM;
            if (bIsPhysicsSimulating)
            {
                float AngularSpeedDegPerSec = IndividualRpm * 6.0f;
                FVector LocalAxle = FVector(1.0f, 0.0f, 0.0f);
                FVector WorldAxle = VisualMeshComponents[BoneIndex]->GetComponentTransform().TransformVectorNoScale(LocalAxle);
                VisualMeshComponents[BoneIndex]->SetPhysicsAngularVelocityInDegrees(WorldAxle * AngularSpeedDegPerSec, false);
            }
            else
            {
                VisualMeshComponents[BoneIndex]->AddLocalRotation(FRotator(Value * 15.0f, 0.0f, 0.0f));
            }
        }
        else if (MotorRole == EPiSimMotorRole::SteeredWheel || MotorRole == EPiSimMotorRole::ServoJoint)
        {
            float TargetAngle = FMath::Lerp(ConfiguredMotors[BoneIndex].MinLimitDeg, ConfiguredMotors[BoneIndex].MaxLimitDeg, (Value + 1.0f) * 0.5f);
            VisualMeshComponents[BoneIndex]->SetRelativeRotation(FRotator(0.0f, TargetAngle, 0.0f));
        }
        else if (MotorRole == EPiSimMotorRole::LinearActuator)
        {
            float Stroke = Value * ConfiguredMotors[BoneIndex].MaxLimitDeg; // cm
            if (BoneIndex > 0 && VisualSections.IsValidIndex(BoneIndex) && VisualSections.IsValidIndex(0))
            {
                FVector BaseLoc = VisualSections[BoneIndex].PivotPoint - VisualSections[0].PivotPoint;
                VisualMeshComponents[BoneIndex]->SetRelativeLocation(BaseLoc + FVector(Stroke, 0.0f, 0.0f));
            }
        }
    }
    else
    {
        // Saf kemik (örn: M_Steer_FR, M_Steer_FL) servo olarak test ediliyorsa:
        // Kendisine bağlı olan ön tekerleği (M_Wheel_FR, M_Wheel_FL) Yaw ekseninde servo gibi döndürür
        EPiSimMotorRole MotorRole = ConfiguredMotors[BoneIndex].Role;
        if (MotorRole == EPiSimMotorRole::ServoJoint || MotorRole == EPiSimMotorRole::SteeredWheel)
        {
            float TargetAngle = FMath::Lerp(ConfiguredMotors[BoneIndex].MinLimitDeg, ConfiguredMotors[BoneIndex].MaxLimitDeg, (Value + 1.0f) * 0.5f);
            FString PureName = ConfiguredMotors[BoneIndex].BoneName;

            FString SteerSuffix = PureName;
            SteerSuffix.RemoveFromStart(TEXT("M_Steer_"), ESearchCase::IgnoreCase);
            SteerSuffix.RemoveFromStart(TEXT("Steer_"), ESearchCase::IgnoreCase);

            for (int32 c = 0; c < VisualMeshComponents.Num(); ++c)
            {
                if (VisualSections.IsValidIndex(c) && VisualMeshComponents[c])
                {
                    FString CompName = VisualSections[c].MeshName;
                    if (CompName.Contains(SteerSuffix, ESearchCase::IgnoreCase) &&
                        (CompName.Contains(TEXT("Wheel"), ESearchCase::IgnoreCase) || CompName.StartsWith(TEXT("W_"))))
                    {
                        VisualMeshComponents[c]->SetRelativeRotation(FRotator(0.0f, TargetAngle, 0.0f));
                        break;
                    }
                }
            }
        }
    }
}

void APiSimModelImporter::RemoveMotorFromBone(int32 BoneIndex)
{
    if (BoneIndex <= 0 || !ConfiguredMotors.IsValidIndex(BoneIndex)) return;
    ConfiguredMotors[BoneIndex].Role = EPiSimMotorRole::None;
    ConfiguredMotors[BoneIndex].CurrentTestValue = 0.0f;
    UpdateVisualMaterials();
}

void APiSimModelImporter::AssignMotorToBone(int32 BoneIndex, EPiSimMotorRole NewRole)
{
    if (BoneIndex <= 0 || !ConfiguredMotors.IsValidIndex(BoneIndex)) return; // Gövdeye motor atanamaz
    ConfiguredMotors[BoneIndex].Role = NewRole;
    UpdateVisualMaterials();
}

void APiSimModelImporter::RemoveSensor(int32 SensorIndex)
{
    if (!ConfiguredSensors.IsValidIndex(SensorIndex)) return;
    ConfiguredSensors[SensorIndex].bIsActive = false;

    if (ConfiguredSensors[SensorIndex].Type == EPiSimSensorType::Camera)
    {
        bEnableVideoStream = false;
        FpvCameraCapture = nullptr;
    }
}

void APiSimModelImporter::AddNewVirtualSensor(EPiSimSensorType InType, FString InSensorName)
{
    FPiSimSensorItem NewSensor;
    NewSensor.SensorIndex = ConfiguredSensors.Num();
    NewSensor.Type = InType;
    NewSensor.bIsActive = true;
    NewSensor.PivotPoint = FVector::ZeroVector; // Attached to root chassis bone (0,0,0)
    NewSensor.Rotation = FRotator::ZeroRotator;

    FString TypePrefix = TEXT("Virtual_Sensor");
    switch (InType)
    {
        case EPiSimSensorType::Camera:
            TypePrefix = TEXT("Virtual_Camera");
            NewSensor.FovAngle = 90.0f;
            NewSensor.Fps = 25;
            NewSensor.Port = 7402;
            break;
        case EPiSimSensorType::IMU:
            TypePrefix = TEXT("Virtual_IMU");
            NewSensor.Fps = 50;
            NewSensor.Port = 7401;
            break;
        case EPiSimSensorType::GPS:
            TypePrefix = TEXT("Virtual_GPS");
            NewSensor.Fps = 10;
            NewSensor.Port = 7403;
            break;
        case EPiSimSensorType::LiDAR:
            TypePrefix = TEXT("Virtual_LiDAR");
            NewSensor.Fps = 20;
            NewSensor.Port = 7404;
            break;
        case EPiSimSensorType::Ultrasonic:
            TypePrefix = TEXT("Virtual_Sonar");
            NewSensor.Fps = 30;
            NewSensor.Port = 7405;
            break;
        default:
            TypePrefix = TEXT("Virtual_Sensor");
            break;
    }

    if (InSensorName.IsEmpty())
    {
        NewSensor.SensorName = FString::Printf(TEXT("%s_%d"), *TypePrefix, NewSensor.SensorIndex + 1);
    }
    else
    {
        NewSensor.SensorName = InSensorName;
    }

    // If Camera, create a SceneCaptureComponent2D attached to the root component if none exists
    if (InType == EPiSimSensorType::Camera && !FpvCameraCapture && VisualMeshComponents.Num() > 0 && VisualMeshComponents[0])
    {
        FpvCameraCapture = NewObject<USceneCaptureComponent2D>(this, TEXT("Virtual_FpvCameraCapture"));
        if (FpvCameraCapture)
        {
            FpvCameraCapture->AttachToComponent(VisualMeshComponents[0], FAttachmentTransformRules::KeepRelativeTransform);
            FpvCameraCapture->SetRelativeLocation(FVector(20.0f, 0.0f, 15.0f));
            FpvCameraCapture->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));
            FpvCameraCapture->FOVAngle = NewSensor.FovAngle;
            FpvCameraCapture->CaptureSource = SCS_FinalColorLDR;
            FpvCameraCapture->bCaptureEveryFrame = true;
            FpvCameraCapture->bCaptureOnMovement = false;

            if (!VideoRenderTarget)
            {
                VideoRenderTarget = NewObject<UTextureRenderTarget2D>(this);
                VideoRenderTarget->InitCustomFormat(320, 240, PF_B8G8R8A8, false);
                VideoRenderTarget->UpdateResourceImmediate(true);
            }
            FpvCameraCapture->TextureTarget = VideoRenderTarget;
            FpvCameraCapture->RegisterComponent();
            bEnableVideoStream = true;
        }
    }

    int32 NewIdx = ConfiguredSensors.Add(NewSensor);
    SelectSensor(NewIdx);
}

void APiSimModelImporter::ToggleSensorMarkers(bool bShow)
{
    bShowSensorMarkers = bShow;
    for (UProceduralMeshComponent* MarkerComp : SensorMarkerComponents)
    {
        if (MarkerComp)
        {
            MarkerComp->SetVisibility(bShow);
            MarkerComp->SetHiddenInGame(!bShow);
        }
    }
}

void APiSimModelImporter::UpdateVisualMaterials()
{
    for (int32 i = 0; i < VisualMeshComponents.Num(); ++i)
    {
        if (VisualMeshComponents[i])
        {
            VisualMeshComponents[i]->SetVisibility(!bShowCollisionView);
            VisualMeshComponents[i]->SetHiddenInGame(bShowCollisionView);
        }
    }

    for (int32 j = 0; j < CollisionMeshComponents.Num(); ++j)
    {
        if (CollisionMeshComponents[j])
        {
            CollisionMeshComponents[j]->SetVisibility(bShowCollisionView);
            CollisionMeshComponents[j]->SetHiddenInGame(!bShowCollisionView);
        }
    }
}
