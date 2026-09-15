// PiSimModelImporter.cpp
// Full-Featured Pawn for GameMode with 360 Orbit Camera, Mouse Controls, Interactive Screen UI, and Strict Visual vs UCX Collision Separation.

#include "PiSimModelImporter.h"
#include "PiSimModelImporterWidget.h"
#include "PiSimModelImporterBase.h"
#include "PiSimActuatorComponent.h"
#include "PiSimAeroWingComponent.h"
#include "PiSimAirImporter.h"
#include "PiSimLandImporter.h"
#include "PiSimSeaImporter.h"
#include "PiSimGarageRobot.h"
#include "PiSimHUD.h"
#include "PiSimUDPManager.h"
#include "PiSimVirtualSensorSuite.h"
#include "PiSimAutopilotManager.h"
#include "PiSimAutopilotBridge.h"
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

// FBX'ten okunan saf kemik dunya koordinatlari (B_Wing_L, B_Wing_R, chassis vb.)
static TMap<FString, FVector> G_FbxBoneWorldLocations;

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
    OrbitSpringArm->bEnableCameraLag = false;
    OrbitSpringArm->bEnableCameraRotationLag = false;
    OrbitSpringArm->CameraLagSpeed = 0.0f;
    OrbitSpringArm->CameraRotationLagSpeed = 0.0f;

    OrbitCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("OrbitCamera"));
    OrbitCamera->SetupAttachment(OrbitSpringArm, USpringArmComponent::SocketName);

    FpvCameraCapture = nullptr;
    bEnableVideoStream = false;
    ImportScaleMultiplier = 1.0f; // Pure 1:1 scale by default
    bIsPhysicsSimulating = false;

    // Gerçekçi 1.15m Uçan Kanat İHA Başlangıç Değerleri
    AeroConfig.WingArea = 0.26f;
    AeroConfig.Wingspan = 1.15f;
    AeroConfig.CL0 = 0.18f;
    AeroConfig.CLAlpha = 5.5f;
    AeroConfig.CD0 = 0.020f;
    AeroConfig.StallAngleDeg = 15.0f;
    AeroConfig.ElevonEffectiveness = 0.50f;
    AeroConfig.CoGForwardCm = 20.0f;
    AeroConfig.InertiaTensorScale = 2.5f;
    ChassisMassKg = 2.0f;

    VirtualSensors = CreateDefaultSubobject<UPiSimVirtualSensorSuite>(TEXT("VirtualSensors"));
    AutopilotManager = CreateDefaultSubobject<UPiSimAutopilotManager>(TEXT("AutopilotManager"));
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

            // Ensure APiSimHUD is spawned automatically for UI overlays
            if (!Cast<APiSimHUD>(PC->MyHUD))
            {
                APiSimHUD* NewHUD = GetWorld()->SpawnActor<APiSimHUD>(APiSimHUD::StaticClass());
                PC->MyHUD = NewHUD;
            }
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

    // Initialize Virtual Sensor Suite
    if (!VirtualSensors)
    {
        VirtualSensors = NewObject<UPiSimVirtualSensorSuite>(this, TEXT("VirtualSensors"));
        VirtualSensors->RegisterComponent();
    }

    // Initialize Autopilot Bridge (ArduPilot SITL / PX4 SITL / MAVLink)
    if (!AutopilotBridge)
    {
        AutopilotBridge = NewObject<UPiSimAutopilotBridge>(this, TEXT("AutopilotBridge"));
        AutopilotBridge->RegisterComponent();
        AutopilotBridge->OnAutopilotCommandReceived.AddDynamic(this, &APiSimModelImporter::OnAutopilotCommandReceived);
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
    if (AutopilotBridge)
    {
        AutopilotBridge->Shutdown();
    }

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

        // L and K Keys (and [ and ]) to Dynamically Adjust Lift Multiplier
        PlayerInputComponent->BindKey(EKeys::L, IE_Pressed, this, &APiSimModelImporter::IncreaseLiftScale);
        PlayerInputComponent->BindKey(EKeys::K, IE_Pressed, this, &APiSimModelImporter::DecreaseLiftScale);
        PlayerInputComponent->BindKey(EKeys::RightBracket, IE_Pressed, this, &APiSimModelImporter::IncreaseLiftScale);
        PlayerInputComponent->BindKey(EKeys::LeftBracket, IE_Pressed, this, &APiSimModelImporter::DecreaseLiftScale);

        // O and P Keys to Dynamically Nudge CoG Forward / Backward
        PlayerInputComponent->BindKey(EKeys::O, IE_Pressed, this, &APiSimModelImporter::NudgeCoGForward);
        PlayerInputComponent->BindKey(EKeys::P, IE_Pressed, this, &APiSimModelImporter::NudgeCoGBackward);
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

void APiSimModelImporter::IncreaseLiftScale()
{
    AeroConfig.AeroForceScale = FMath::Clamp(AeroConfig.AeroForceScale + 0.2f, 0.0f, 50.0f);
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(702, 3.0f, FColor::Cyan,
            FString::Printf(TEXT("🚀 >>> [LİFT KUVVET ÇARPANI]: %.2fx (L Tuşu: +0.2x | K Tuşu: -0.2x) <<<"), AeroConfig.AeroForceScale));
    }
    UE_LOG(LogTemp, Warning, TEXT("[PiSimModelImporter] Lift Kuvvet Çarpanı: %.2fx"), AeroConfig.AeroForceScale);
}

void APiSimModelImporter::DecreaseLiftScale()
{
    AeroConfig.AeroForceScale = FMath::Clamp(AeroConfig.AeroForceScale - 0.2f, 0.0f, 50.0f);
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(702, 3.0f, FColor::Orange,
            FString::Printf(TEXT("🚀 >>> [LİFT KUVVET ÇARPANI]: %.2fx (L Tuşu: +0.2x | K Tuşu: -0.2x) <<<"), AeroConfig.AeroForceScale));
    }
    UE_LOG(LogTemp, Warning, TEXT("[PiSimModelImporter] Lift Kuvvet Çarpanı: %.2fx"), AeroConfig.AeroForceScale);
}

void APiSimModelImporter::NudgeCoGForward()
{
    AeroConfig.CoGForwardCm += 2.0f;
    ApplyCenterOfMass();
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(703, 3.0f, FColor::Yellow,
            FString::Printf(TEXT("🟡 >>> [AĞIRLIK MERKEZİ (CoG)]: %+.1f cm (O Tuşu: İleri | P Tuşu: Geri) <<<"), AeroConfig.CoGForwardCm));
    }
    UE_LOG(LogTemp, Warning, TEXT("[PiSimModelImporter] CoGForwardCm: %+.1f cm"), AeroConfig.CoGForwardCm);
}

void APiSimModelImporter::NudgeCoGBackward()
{
    AeroConfig.CoGForwardCm -= 2.0f;
    ApplyCenterOfMass();
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(703, 3.0f, FColor::Yellow,
            FString::Printf(TEXT("🟡 >>> [AĞIRLIK MERKEZİ (CoG)]: %+.1f cm (O Tuşu: İleri | P Tuşu: Geri) <<<"), AeroConfig.CoGForwardCm));
    }
    UE_LOG(LogTemp, Warning, TEXT("[PiSimModelImporter] CoGForwardCm: %+.1f cm"), AeroConfig.CoGForwardCm);
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

void APiSimModelImporter::ApplyMotorTestValue(int32 MotorIndex, float NormalizedValue)
{
    if (ConfiguredMotors.IsValidIndex(MotorIndex))
    {
        ConfiguredMotors[MotorIndex].CurrentTestValue = FMath::Clamp(NormalizedValue, -1.0f, 1.0f);
    }
}

void APiSimModelImporter::SetAutopilotControlMode(EAutopilotControlMode NewMode)
{
    ControlMode = NewMode;
    if (ControlMode == EAutopilotControlMode::ArduPilotSITL)
    {
        if (AutopilotBridge)
        {
            AutopilotBridge->StartArduPilotSitl(9002);
            AddConnectionDebugLog(TEXT("✈️ [Otopilot] ArduPilot SITL Modu Aktif (UDP Port 9002 Dinleniyor)"));
        }
    }
    else if (ControlMode == EAutopilotControlMode::PX4SITL)
    {
        if (AutopilotBridge)
        {
            AutopilotBridge->StartPx4Sitl(14560);
            AddConnectionDebugLog(TEXT("🛸 [Otopilot] PX4 SITL / MAVLink Modu Aktif (UDP Port 14560 Dinleniyor)"));
        }
    }
    else
    {
        if (AutopilotBridge)
        {
            AutopilotBridge->Shutdown();
        }
        AddConnectionDebugLog(TEXT("🎮 [Otopilot] Direkt ROS 2 Modu Aktif (UDP Port 7400)"));
    }
}

void APiSimModelImporter::CycleAutopilotControlMode()
{
    uint8 Next = ((uint8)ControlMode + 1) % 3;
    SetAutopilotControlMode((EAutopilotControlMode)Next);
}

bool APiSimModelImporter::IsAutopilotDriving() const
{
    if (AutopilotManager && AutopilotManager->ShouldDriveActuators())
    {
        return true;
    }
    if (ControlMode != EAutopilotControlMode::DirectROS2 && AutopilotBridge && AutopilotBridge->bIsConnected)
    {
        return true;
    }
    return false;
}

void APiSimModelImporter::ApplyAutopilotMixer(float Roll, float Pitch, float Yaw, float Throttle, const uint16* Pwm, const float* RawControls)
{
    TargetLinearX = Throttle;
    TargetAngularZ = Yaw;

    // Route commands to AttachedActuators (Modular Components)
    for (UPiSimActuatorComponent* Act : AttachedActuators)
    {
        if (!Act) continue;
        if (Act->Role == EPiSimMotorRole::Thruster)
        {
            Act->SetNormalizedCommand(FMath::Clamp(Throttle, 0.0f, 1.0f));
        }
        else if (Act->Role == EPiSimMotorRole::ServoJoint)
        {
            FString ActName = Act->ActuatorName.ToLower();
            if (ActName.Contains(TEXT("elevon_l")) || ActName.Contains(TEXT("aileron_l")))
            {
                // Sol Elevon = Pitch + Roll
                float AngleL = (Pitch + Roll) * 20.0f;
                Act->SetTargetAngle(AngleL);
            }
            else if (ActName.Contains(TEXT("elevon_r")) || ActName.Contains(TEXT("aileron_r")))
            {
                // Sağ Elevon = Pitch - Roll
                float AngleR = (Pitch - Roll) * 20.0f;
                Act->SetTargetAngle(AngleR);
            }
            else if (ActName.Contains(TEXT("rudder")) || ActName.Contains(TEXT("steer")))
            {
                float RudderAngle = Yaw * 25.0f;
                Act->SetTargetAngle(RudderAngle);
            }
            else if (ActName.Contains(TEXT("elevator")))
            {
                float ElevatorAngle = Pitch * 20.0f;
                Act->SetTargetAngle(ElevatorAngle);
            }
        }
        else if (Act->Role == EPiSimMotorRole::DriveWheel)
        {
            FVector RelLoc = Act->GetRelativeLocation();
            float WheelRpm = (RelLoc.Y < 0.0f) ? LeftWheelsRpm : RightWheelsRpm;
            Act->SetTargetVelocity(WheelRpm);
        }
    }

    // Also update ConfiguredMotors for visual meshes and HUD sliders
    for (int32 m = 0; m < ConfiguredMotors.Num(); ++m)
    {
        FPiSimMotorItem& Motor = ConfiguredMotors[m];
        if (Motor.Role == EPiSimMotorRole::Thruster)
        {
            Motor.CurrentTestValue = Throttle;
        }
        else if (Motor.Role == EPiSimMotorRole::ServoJoint)
        {
            FString LowerName = Motor.BoneName.ToLower();
            if (LowerName.Contains(TEXT("elevon_l")) || LowerName.Contains(TEXT("aileron_l")))
            {
                Motor.CurrentTestValue = FMath::Clamp(Pitch + Roll, -1.0f, 1.0f);
            }
            else if (LowerName.Contains(TEXT("elevon_r")) || LowerName.Contains(TEXT("aileron_r")))
            {
                Motor.CurrentTestValue = FMath::Clamp(Pitch - Roll, -1.0f, 1.0f);
            }
            else if (LowerName.Contains(TEXT("rudder")) || LowerName.Contains(TEXT("steer")))
            {
                Motor.CurrentTestValue = FMath::Clamp(Yaw, -1.0f, 1.0f);
            }
            else if (LowerName.Contains(TEXT("elevator")))
            {
                Motor.CurrentTestValue = FMath::Clamp(Pitch, -1.0f, 1.0f);
            }
        }
    }
}

void APiSimModelImporter::OnAutopilotCommandReceived(float Throttle, float Roll, float Pitch, float Yaw, const TArray<float>& RawChannels)
{
    if (ControlMode == EAutopilotControlMode::DirectROS2)
    {
        return; // Direkt ROS 2 modundaysa SITL komutlarını işletme
    }

    ApplyAutopilotMixer(Roll, Pitch, Yaw, Throttle, nullptr, RawChannels.Num() > 0 ? RawChannels.GetData() : nullptr);
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

        const bool bAutopilotOwnsActuators = IsAutopilotDriving();

        // Route ROS Twist commands to AttachedActuators (Modular Components)
        if (!bAutopilotOwnsActuators)
        {
        for (UPiSimActuatorComponent* Act : AttachedActuators)
        {
            if (!Act) continue;
            if (Act->Role == EPiSimMotorRole::Thruster)
            {
                // Gaz Kolu / İtki: Linear.X normalize değeri
                Act->SetNormalizedCommand(FMath::Clamp(TargetLinearX, -1.0f, 1.0f));
            }
            else if (Act->Role == EPiSimMotorRole::ServoJoint)
            {
                FString ActName = Act->ActuatorName.ToLower();
                if (ActName.Contains(TEXT("elevon_l")) || ActName.Contains(TEXT("aileron_l")))
                {
                    float ElevonL = (TwistMsg.Angular.Y + TwistMsg.Angular.Z) * 15.0f;
                    Act->SetTargetAngle(ElevonL);
                }
                else if (ActName.Contains(TEXT("elevon_r")) || ActName.Contains(TEXT("aileron_r")))
                {
                    float ElevonR = (TwistMsg.Angular.Y - TwistMsg.Angular.Z) * 15.0f;
                    Act->SetTargetAngle(ElevonR);
                }
                else if (ActName.Contains(TEXT("rudder")) || ActName.Contains(TEXT("steer")))
                {
                    float RudderAngle = TwistMsg.Angular.Z * 20.0f;
                    Act->SetTargetAngle(RudderAngle);
                }
            }
            else if (Act->Role == EPiSimMotorRole::DriveWheel)
            {
                FVector RelLoc = Act->GetRelativeLocation();
                float WheelRpm = (RelLoc.Y < 0.0f) ? LeftWheelsRpm : RightWheelsRpm;
                Act->SetTargetVelocity(WheelRpm);
            }
        }

        // Sync ConfiguredMotors with ROS Twist for both HUD animation and vehicle actuation
        for (int32 m = 0; m < ConfiguredMotors.Num(); ++m)
        {
            FPiSimMotorItem& Motor = ConfiguredMotors[m];
            if (Motor.Role == EPiSimMotorRole::Thruster)
            {
                Motor.CurrentTestValue = FMath::Clamp(TargetLinearX, -1.0f, 1.0f);
            }
            else if (Motor.Role == EPiSimMotorRole::ServoJoint)
            {
                FString LowerName = Motor.BoneName.ToLower();
                if (LowerName.Contains(TEXT("elevon_l")) || LowerName.Contains(TEXT("aileron_l")))
                {
                    Motor.CurrentTestValue = FMath::Clamp(TwistMsg.Angular.Y + TwistMsg.Angular.Z, -1.0f, 1.0f);
                }
                else if (LowerName.Contains(TEXT("elevon_r")) || LowerName.Contains(TEXT("aileron_r")))
                {
                    Motor.CurrentTestValue = FMath::Clamp(TwistMsg.Angular.Y - TwistMsg.Angular.Z, -1.0f, 1.0f);
                }
                else if (LowerName.Contains(TEXT("rudder")) || LowerName.Contains(TEXT("steer")))
                {
                    Motor.CurrentTestValue = FMath::Clamp(TwistMsg.Angular.Z, -1.0f, 1.0f);
                }
            }
        }
        }

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

    // 4) Update Virtual Sensors & Feed Autopilot Bridge (ArduPilot SITL / PX4 SITL)
    if (VirtualSensors && Chassis)
    {
        VirtualSensors->UpdateSensors(DeltaTime, Chassis->GetComponentLocation(), CurrentLinearVelUE5, CurrentLinearAccel, AngularVelUE5, OrientationUE5, CurrentForwardSpeedKmh);

        if (AutopilotBridge && AutopilotBridge->IsSocketOpen())
        {
            if (ControlMode == EAutopilotControlMode::ArduPilotSITL)
            {
                // ArduPilot JSON SITL protocol: simulator MUST send state first (before ArduPilot sends servo commands).
                // We send proactively to ArduPilot's state-listen port (--sim-port-in), default 9003.
                // Once ArduPilot receives state, it will start sending servo commands back (bIsConnected → true).
                double WorldTimeSec = (GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.0;
                FString SitlJson = VirtualSensors->BuildArduPilotJsonPayload(WorldTimeSec);
                FString ArduIp = AutopilotBridge->bIsConnected ? AutopilotBridge->ConnectedIP : TEXT("127.0.0.1");
                // ArduPilot listens for state on --sim-port-in (9003). Do NOT use ConnectedPort (ephemeral sender port).
                AutopilotBridge->SendArduPilotStateJson(SitlJson, ArduIp, AutopilotBridge->ArduPilotStatePort);
            }
            else if (ControlMode == EAutopilotControlMode::PX4SITL)
            {
                if (AutopilotBridge->bIsConnected)
                {
                    AutopilotBridge->SendPx4Sensors(VirtualSensors->ImuData, VirtualSensors->BaroData, VirtualSensors->GpsData, AutopilotBridge->ConnectedIP, 14560);
                }
            }
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

    // =========================================================================================
    // 🔒 [DO NOT MODIFY - WHEEL PHYSICS & STEER LOCK]
    // 🛑 KESİNLİKLE DEĞİŞTİRİLEMEZ: DİREKSİYONLU TEKERLEK KONTROL VE HİZALAMA BLOĞU!
    // Bu blok hem AI hem de insan geliştiriciler için KİLİTLİDİR. Hava aracı, deniz aracı veya
    // başka hiçbir özellik için bu mantık ve rotasyon formülleri ASLA değiştirilemez!
    // =========================================================================================
    // 3) Canlı Direksiyon Açısını Her Tick Koru (Hedef Açıya Kilitle)
    for (int32 m = 0; m < ConfiguredMotors.Num(); ++m)
    {
        const FPiSimMotorItem& Motor = ConfiguredMotors[m];
        if (Motor.Role == EPiSimMotorRole::ServoJoint || Motor.Role == EPiSimMotorRole::SteeredWheel)
        {
            float TargetAngle = FMath::Lerp(Motor.MinLimitDeg, Motor.MaxLimitDeg, (Motor.CurrentTestValue + 1.0f) * 0.5f);

            // Sadece gerçek direksiyon mafsalı mı kontrol et (Elevon / kanat yüzeyleri tekerlek mafsalı değildir!):
            bool bIsSteerJoint = SteerBoneComponents.Contains(Motor.BoneName) || (Motor.Role == EPiSimMotorRole::SteeredWheel);

            // 1) Steer kemiğinin dönüşünü güncelle (Fizik simüle etmez, sadece biz döndürürüz)
            if (USceneComponent** FoundSteer = SteerBoneComponents.Find(Motor.BoneName))
            {
                (*FoundSteer)->SetRelativeRotation(FRotator(0.0f, TargetAngle, 0.0f));
            }

            // 2) SADECE gerçek direksiyon mafsalı ise ilgili tekerleğin fizik kısıtlamasını da aynı 1 eksende (Yaw) yönlendir
            if (bIsSteerJoint)
            {
                int32 TargetSection = FindVisualSectionForBone(m);
                if (TargetSection > 0 && VisualSections.IsValidIndex(TargetSection) && VisualSections.IsValidIndex(0))
                {
                    if (bIsPhysicsSimulating)
                    {
                        UPhysicsConstraintComponent* Constraint = FindConstraintForSection(TargetSection);
                        if (Constraint)
                        {
                            FVector RelLoc = VisualSections[TargetSection].PivotPoint - VisualSections[0].PivotPoint;
                            FTransform SteerPose(FRotator(0.0f, TargetAngle, 0.0f), RelLoc);
                            Constraint->SetConstraintReferenceFrame(EConstraintFrame::Frame1, SteerPose);
                        }
                    }
                }
            }
        }
    }
    // 🔒 [END OF WHEEL STEER LOCK]
    // =========================================================================================

    // 4) Canlı Tekerlek ve Pervane İtki Fiziği
    FVector AppliedThrustVecWorld = FVector::ZeroVector;
    FVector AppliedThrustTorqueWorld = FVector::ZeroVector;

    if (bIsPhysicsSimulating)
    {
        for (int32 i = 1; i < VisualMeshComponents.Num(); ++i)
        {
            if (VisualMeshComponents[i] && ConfiguredMotors.IsValidIndex(i))
            {
                const FPiSimMotorItem& Motor = ConfiguredMotors[i];
                // =========================================================================================
                // 🔒 [DO NOT MODIFY - WHEEL DRIVE ANGULAR VELOCITY LOCK]
                // 🛑 KESİNLİKLE DEĞİŞTİRİLEMEZ: SÜRÜŞ TEKERLEĞİ VE PALET DÖNÜŞ VE HIZ HESAPLAMA BLOĞU!
                // Yerel aks LocalAxle = FVector(1,0,0) (Roll) olup, Yaw eksenini bozmadan sadece Roll
                // açısal hızı güncellenir. Bu hesaplama kesinlikle sabittir ve değiştirilemez!
                // =========================================================================================
                if (Motor.Role == EPiSimMotorRole::DriveWheel || Motor.Role == EPiSimMotorRole::TrackPad)
                {
                    FVector RelLoc = VisualMeshComponents[i]->GetRelativeLocation();
                    float BaseRpm = (RelLoc.Y < 0.0f) ? LeftWheelsRpm : RightWheelsRpm;
                    float IndividualTestRpm = Motor.CurrentTestValue * Motor.MaxVelocityRPM;
                    float TotalRpm = BaseRpm + IndividualTestRpm;

                    if (FMath::Abs(TotalRpm) > 0.001f)
                    {
                        float AngularSpeedDegPerSec = TotalRpm * 6.0f; // 1 RPM = 6 deg/sec
                        FVector LocalAxle = FVector(1.0f, 0.0f, 0.0f); // Roll X axle
                        FVector WorldAxle = VisualMeshComponents[i]->GetComponentTransform().TransformVectorNoScale(LocalAxle).GetSafeNormal();

                        // Direksiyon Yaw açısal hızını bozmadan sadece tekerlek yuvarlanma (Roll) eksenini güncelle
                        FVector CurrentAngVel = VisualMeshComponents[i]->GetPhysicsAngularVelocityInDegrees();
                        FVector NonRollVel = CurrentAngVel - (WorldAxle * (CurrentAngVel | WorldAxle));
                        FVector DesiredRollVel = WorldAxle * AngularSpeedDegPerSec;
                        VisualMeshComponents[i]->SetPhysicsAngularVelocityInDegrees(NonRollVel + DesiredRollVel, false);
                    }
                }
                // 🔒 [END OF WHEEL DRIVE LOCK]
                // =========================================================================================
                else if (Motor.Role == EPiSimMotorRole::Thruster)
                {
                    // Kemiğin uzandığı doğrultu (FBX TransformLink matrisinden çıkarılan birim vektör)
                    FVector LocalThrustAxis = VisualSections.IsValidIndex(i) ? VisualSections[i].BoneDirection : Motor.BoneDirection;
                    if (LocalThrustAxis.IsNearlyZero()) LocalThrustAxis = FVector(0.0f, 1.0f, 0.0f);

                    // 1) Pervanenin Görsel Dönüşü (Spin RPM) – Kemiğin uzanma ekseni etrafında döner
                    float PropRpm = Motor.CurrentTestValue * Motor.MaxVelocityRPM;
                    if (FMath::Abs(PropRpm) > 0.001f)
                    {
                        float AngularSpeedDegPerSec = PropRpm * 6.0f;
                        FQuat SpinQuat(LocalThrustAxis, FMath::DegreesToRadians(AngularSpeedDegPerSec * DeltaTime));
                        VisualMeshComponents[i]->AddLocalRotation(SpinQuat);
                    }

                    // 2) Fiziksel İtki Kuvveti – Kemiğin uzandığı yönde şasiye itki uygula (Newton)
                    if (VisualMeshComponents.IsValidIndex(0) && VisualMeshComponents[0] && FMath::Abs(Motor.CurrentTestValue) > 0.001f)
                    {
                        float MaxThrustN = (Motor.MaxTorqueNm > 0.1f) ? Motor.MaxTorqueNm : 18.0f;
                        float ThrustNewtons = Motor.CurrentTestValue * MaxThrustN;

                        // Şasinin dünya yönüne dönüştür: kemiğin uzanım yönünde itki uygula
                        FVector WorldThrustAxis = VisualMeshComponents[0]->GetComponentTransform()
                            .TransformVectorNoScale(LocalThrustAxis).GetSafeNormal();

                        // bReverseThrust: itki yönünü ters çevir (Pusher prop / ters yön için)
                        float DirectionSign = Motor.bReverseThrust ? -1.0f : 1.0f;

                        FVector ForceVec = WorldThrustAxis * (DirectionSign * ThrustNewtons);
                        // İtki doğrudan şasiye (merkeze) uygulanır (1 Newton = 100 Unreal Engine Force Units):
                        VisualMeshComponents[0]->AddForce(ForceVec * 100.0f);

                        FVector PropWorldLocDbg = VisualMeshComponents[i]->GetComponentLocation();
                        FVector CurrentCoM = VisualMeshComponents[0]->GetCenterOfMass();
                        FVector ThrustArmM = (PropWorldLocDbg - CurrentCoM) * 0.01f;

                        AppliedThrustVecWorld += ForceVec;
                        AppliedThrustTorqueWorld += FVector::CrossProduct(ThrustArmM, ForceVec);

                        // Debug: itki yönü görsel ok (🟣 Mor ok) - görsel amaçlı pervanenin konumundan çizilir
                        if (AeroConfig.bShowAeroGizmos)
                        {
                            float ArrowLen = FMath::Clamp(FMath::Abs(ThrustNewtons) * 4.0f, 20.0f, 150.0f);
                            DrawDebugDirectionalArrow(GetWorld(), PropWorldLocDbg,
                                PropWorldLocDbg + ForceVec.GetSafeNormal() * ArrowLen,
                                15.0f, FColor(220, 30, 255), false, 0.0f, 0, 2.5f);
                        }
                    }
                }

                else if (Motor.Role == EPiSimMotorRole::ServoJoint && !WheelToSteerBoneMap.Contains(i))
                {
                    // Uçan Kanat Kontrol Yüzeyleri (Elevon / Aileron) Canlı Görsel Sapması
                    float TargetAngle = FMath::Lerp(Motor.MinLimitDeg, Motor.MaxLimitDeg, (Motor.CurrentTestValue + 1.0f) * 0.5f);
                    FQuat HingeQuat(FVector(1.0f, 0.0f, 0.0f), FMath::DegreesToRadians(TargetAngle));
                    VisualMeshComponents[i]->SetRelativeRotation(FRotator(HingeQuat));
                }
            }
        }
    }

    // -----------------------------------------------------------------------------------------
    // 4.5) SABİT KANAT & UÇAN KANAT (FLYING WING) AERODİNAMİK LIFT & DRAG FİZİĞİ
    // -----------------------------------------------------------------------------------------
    if (VisualSections.IsValidIndex(0))
    {
        AeroConfig.bEnableAerodynamics = VisualSections[0].bEnableAerodynamics;
        AeroConfig.WingArea = VisualSections[0].WingArea;
        AeroConfig.Wingspan = VisualSections[0].Wingspan;
        AeroConfig.CL0 = VisualSections[0].CL0;
        AeroConfig.CLAlpha = VisualSections[0].CLAlpha;
        AeroConfig.CD0 = VisualSections[0].CD0;
        AeroConfig.StallAngleDeg = VisualSections[0].StallAngleDeg;
        AeroConfig.ElevonEffectiveness = VisualSections[0].ElevonEffectiveness;
        AeroConfig.CoGForwardCm = VisualSections[0].CoGForwardCm;
        AeroConfig.CoLForwardCm = VisualSections[0].CoLForwardCm;
        AeroConfig.ChassisBoneLengthCm = VisualSections[0].BoneLength;
    }

    // =========================================================================
    // KHAN & NAHON (2015) DAĞITILMIŞ GERÇEK AERODİNAMİK YÜZEY HESAPLAMA MOTORU
    // =========================================================================
    if (bIsPhysicsSimulating && AeroConfig.bEnableAerodynamics && VisualMeshComponents.IsValidIndex(0) && VisualMeshComponents[0])
    {
        // 1) Temel Gövde Kinematiği
        FVector WorldLinearVel = VisualMeshComponents[0]->GetPhysicsLinearVelocity() * 0.01f; // cm/s -> m/s
        FVector WorldAngVelDeg = VisualMeshComponents[0]->GetPhysicsAngularVelocityInDegrees();
        FVector WorldAngVelRad = FMath::DegreesToRadians(WorldAngVelDeg); // rad/s
        float Airspeed = WorldLinearVel.Size();
        AeroConfig.CurrentAirspeedKmh = Airspeed * 3.6f;

        const FTransform& BodyTransform = VisualMeshComponents[0]->GetComponentTransform();
        FVector BodyLocation = BodyTransform.GetLocation();
        FVector WorldCoM = VisualMeshComponents[0]->GetCenterOfMass();

        // Model Eksenleri:
        // Chassis kemik yönü FBX'ten (0, -1, 0) burun yönünü verir
        FVector LocalForward = (VisualSections.IsValidIndex(0) && !VisualSections[0].BoneDirection.IsNearlyZero())
            ? VisualSections[0].BoneDirection.GetSafeNormal()
            : FVector(0.0f, -1.0f, 0.0f);
        FVector LocalRight = FVector(1.0f, 0.0f, 0.0f); // Sağ Kanat (+X)
        FVector LocalUp = FVector(0.0f, 0.0f, 1.0f);    // Üst Gövde (+Z)

        FVector WorldForward = BodyTransform.TransformVectorNoScale(LocalForward).GetSafeNormal();
        FVector WorldRight = BodyTransform.TransformVectorNoScale(LocalRight).GetSafeNormal();
        FVector WorldUp = BodyTransform.TransformVectorNoScale(LocalUp).GetSafeNormal();

        // 2) Kontrol Yüzeyi (Elevon) Sapma Açıları
        float ElevonAngleDegL = 0.0f;
        float ElevonAngleDegR = 0.0f;
        for (const FPiSimMotorItem& Motor : ConfiguredMotors)
        {
            FString LowerName = Motor.BoneName.ToLower();
            if (LowerName.Contains(TEXT("elevon_l")) || LowerName.Contains(TEXT("aileron_l")))
            {
                ElevonAngleDegL = FMath::Lerp(Motor.MinLimitDeg, Motor.MaxLimitDeg, (Motor.CurrentTestValue + 1.0f) * 0.5f);
            }
            else if (LowerName.Contains(TEXT("elevon_r")) || LowerName.Contains(TEXT("aileron_r")))
            {
                ElevonAngleDegR = FMath::Lerp(Motor.MinLimitDeg, Motor.MaxLimitDeg, (Motor.CurrentTestValue + 1.0f) * 0.5f);
            }
        }

        // 3) Kanat Yüzeylerinin 3D Konumlarını Belirle (Doğrudan Blender Kemik Koordinatları veya UCX / Açıklık Ofseti)
        FVector LocalPosL = FVector(-AeroConfig.Wingspan * 25.0f, -AeroConfig.CoLForwardCm, 0.0f); // -X (Sol kanat)
        FVector LocalPosR = FVector(+AeroConfig.Wingspan * 25.0f, -AeroConfig.CoLForwardCm, 0.0f); // +X (Sağ kanat)

        bool bFoundBoneL = false;
        bool bFoundBoneR = false;

        // Öncelik 1: Doğrudan FBX Kemik Haritasından oku (Önce B_Wing_L / B_Wing_R, yoksa Elevon_L / Elevon_R)
        for (const auto& Pair : G_FbxBoneWorldLocations)
        {
            FString LowerB = Pair.Key.ToLower();
            if (!bFoundBoneL && (LowerB.Contains(TEXT("wing_l")) || LowerB.Contains(TEXT("kanat_l"))))
            {
                LocalPosL = Pair.Value - VisualSections[0].PivotPoint;
                bFoundBoneL = true;
            }
            if (!bFoundBoneR && (LowerB.Contains(TEXT("wing_r")) || LowerB.Contains(TEXT("kanat_r"))))
            {
                LocalPosR = Pair.Value - VisualSections[0].PivotPoint;
                bFoundBoneR = true;
            }
        }
        if (!bFoundBoneL || !bFoundBoneR)
        {
            for (const auto& Pair : G_FbxBoneWorldLocations)
            {
                FString LowerB = Pair.Key.ToLower();
                if (!bFoundBoneL && (LowerB.Contains(TEXT("elevon_l")) || LowerB.Contains(TEXT("aileron_l"))))
                {
                    LocalPosL = Pair.Value - VisualSections[0].PivotPoint;
                    bFoundBoneL = true;
                }
                if (!bFoundBoneR && (LowerB.Contains(TEXT("elevon_r")) || LowerB.Contains(TEXT("aileron_r"))))
                {
                    LocalPosR = Pair.Value - VisualSections[0].PivotPoint;
                    bFoundBoneR = true;
                }
            }
        }

        // Öncelik 2: Eğer kemik haritasında yoksa UCX parçalarından al
        if (!bFoundBoneL || !bFoundBoneR)
        {
            for (const FImporterMeshSection& UcxSec : UCXSections)
            {
                FString LowerU = UcxSec.MeshName.ToLower();
                if (!bFoundBoneL && (LowerU.Contains(TEXT("wing_l")) || LowerU.Contains(TEXT("kanat_l")) || LowerU.Contains(TEXT("left"))))
                {
                    LocalPosL = UcxSec.PivotPoint - VisualSections[0].PivotPoint;
                }
                else if (!bFoundBoneR && (LowerU.Contains(TEXT("wing_r")) || LowerU.Contains(TEXT("kanat_r")) || LowerU.Contains(TEXT("right"))))
                {
                    LocalPosR = UcxSec.PivotPoint - VisualSections[0].PivotPoint;
                }
            }
        }

        FVector WorldSurfaceLocL = BodyTransform.TransformPosition(LocalPosL);
        FVector WorldSurfaceLocR = BodyTransform.TransformPosition(LocalPosR);

        // 4) Khan & Nahon Aerodinamik Fonksiyonu (Lambda)
        auto CalcKhanNahonSurface = [&](
            const FVector& SurfaceWorldPos,
            float FlapDeflectionDeg,
            FVector& OutForceN,
            FVector& OutTorqueNm,
            float& OutAoADeg,
            FVector& OutLiftVecN,
            FVector& OutDragVecN)
        {
            // Kanat parça parametreleri
            float Chord = FMath::Max(0.05f, AeroConfig.WingArea / FMath::Max(0.1f, AeroConfig.Wingspan));
            float HalfArea = AeroConfig.WingArea * 0.5f;
            float AspectRatio = FMath::Max(1.0f, (AeroConfig.Wingspan * AeroConfig.Wingspan) / FMath::Max(0.01f, AeroConfig.WingArea));
            float LiftSlope = AeroConfig.CLAlpha;
            float SkinFriction = AeroConfig.CD0;
            float FlapFraction = FMath::Clamp(AeroConfig.ElevonEffectiveness * 0.5f, 0.05f, 0.40f);
            float FlapAngleRad = FMath::DegreesToRadians(FlapDeflectionDeg);

            // Aspect Ratio düzeltmesi
            float CorrectedLiftSlope = LiftSlope * AspectRatio /
                (AspectRatio + 2.0f * (AspectRatio + 4.0f) / (AspectRatio + 2.0f));

            // Flap etkinliği ve sıfır taşıma açısı kayması
            float Theta = FMath::Acos(FMath::Clamp(2.0f * FlapFraction - 1.0f, -1.0f, 1.0f));
            float FlapEffectiveness = 1.0f - (Theta - FMath::Sin(Theta)) / PI;
            float FlapEffCorrection = FMath::Lerp(0.8f, 0.4f, FMath::Clamp((FMath::Abs(FlapDeflectionDeg) - 10.0f) / 50.0f, 0.0f, 1.0f));
            float DeltaLift = CorrectedLiftSlope * FlapEffectiveness * FlapEffCorrection * FlapAngleRad;

            float ZeroLiftAoABase = FMath::DegreesToRadians(AeroConfig.CL0 * -5.0f);
            float ZeroLiftAoA = ZeroLiftAoABase - DeltaLift / CorrectedLiftSlope;

            float StallAngleHighBase = FMath::DegreesToRadians(AeroConfig.StallAngleDeg);
            float StallAngleLowBase = -StallAngleHighBase;

            float CLMaxFraction = FMath::Clamp(1.0f - 0.5f * (FlapFraction - 0.1f) / 0.3f, 0.0f, 1.0f);
            float CLMaxHigh = CorrectedLiftSlope * (StallAngleHighBase - ZeroLiftAoABase) + DeltaLift * CLMaxFraction;
            float CLMaxLow = CorrectedLiftSlope * (StallAngleLowBase - ZeroLiftAoABase) + DeltaLift * CLMaxFraction;

            float StallAngleHigh = ZeroLiftAoA + CLMaxHigh / CorrectedLiftSlope;
            float StallAngleLow = ZeroLiftAoA + CLMaxLow / CorrectedLiftSlope;

            // Bağıl Hava Hızı: -velocity - Cross(angularVelocity, relativePosition)
            FVector RelPosM = (SurfaceWorldPos - WorldCoM) * 0.01f; // cm -> m
            FVector V_rot = FVector::CrossProduct(WorldAngVelRad, RelPosM);
            FVector WorldAirVelocity = -WorldLinearVel - V_rot; // Uçağa çarpan bağıl rüzgar akış vektörü

            // Kord ekseni (WorldForward), Normal ekseni (WorldUp)
            float V_chord = WorldAirVelocity | WorldForward;
            float V_normal = WorldAirVelocity | WorldUp;
            float SpeedInPlane = FMath::Sqrt(V_chord * V_chord + V_normal * V_normal);

            // Hücum Açısı (AoA): Burun yukarı kalktığında bağıl rüzgar alttan çarpar (V_normal > 0 -> AoA > 0)
            float AoA = FMath::Atan2(V_normal, FMath::Max(0.01f, -V_chord));
            OutAoADeg = FMath::RadiansToDegrees(AoA);

            // Drag ve Lift Yön Vektörleri
            // Drag: Bağıl rüzgarın akış yönünde (harekete ters yönde geriye doğru)
            FVector DragDir = (SpeedInPlane > 0.05f) ?
                ((WorldForward * V_chord + WorldUp * V_normal) / SpeedInPlane) : -WorldForward;

            // Lift: Drag ve sağ kanat eksenine dik, yukarı (+WorldUp) doğrultusunda kaldırma
            FVector LiftDir = FVector::CrossProduct(WorldRight, DragDir).GetSafeNormal();
            if ((LiftDir | WorldUp) < 0.0f)
            {
                LiftDir = -LiftDir; // Lift daima kanat üst yüzeyine (+WorldUp) doğru kaldırır
            }

            // Katsayı Hesaplama (Khan & Nahon 2015 Düşük AoA vs Stall Dikişli Model)
            auto TorqCoeffProportion = [](float EffAng) {
                return 0.25f - 0.175f * (1.0f - 2.0f * FMath::Abs(EffAng) / PI);
            };

            auto FrictionAt90 = [](float FlapRad) {
                return 1.98f - 4.26e-2f * FlapRad * FlapRad + 2.1e-1f * FlapRad;
            };

            auto CoeffsLowAoA = [&](float A) -> FVector {
                float CL_val = CorrectedLiftSlope * (A - ZeroLiftAoA);
                float InducedAng = CL_val / (PI * AspectRatio);
                float EffAng = A - ZeroLiftAoA - InducedAng;
                float TangentialCoeff = SkinFriction * FMath::Cos(EffAng);
                float NormalCoeff = (CL_val + FMath::Sin(EffAng) * TangentialCoeff) / FMath::Max(0.001f, FMath::Cos(EffAng));
                float CD_val = NormalCoeff * FMath::Sin(EffAng) + TangentialCoeff * FMath::Cos(EffAng);
                float CM_val = -NormalCoeff * TorqCoeffProportion(EffAng);
                return FVector(CL_val, CD_val, CM_val);
            };

            auto CoeffsStall = [&](float A) -> FVector {
                float CL_low = (A > StallAngleHigh) ?
                    CorrectedLiftSlope * (StallAngleHigh - ZeroLiftAoA) :
                    CorrectedLiftSlope * (StallAngleLow - ZeroLiftAoA);
                float InducedAng = CL_low / (PI * AspectRatio);

                float LerpP = (A > StallAngleHigh) ?
                    ((PI * 0.5f - FMath::Clamp(A, -PI * 0.5f, PI * 0.5f)) / FMath::Max(0.001f, PI * 0.5f - StallAngleHigh)) :
                    ((-PI * 0.5f - FMath::Clamp(A, -PI * 0.5f, PI * 0.5f)) / FMath::Max(0.001f, -PI * 0.5f - StallAngleLow));
                LerpP = FMath::Clamp(LerpP, 0.0f, 1.0f);
                InducedAng = FMath::Lerp(0.0f, InducedAng, LerpP);
                float EffAng = A - ZeroLiftAoA - InducedAng;

                float NormalCoeff = FrictionAt90(FlapAngleRad) * FMath::Sin(EffAng) *
                    (1.0f / (0.56f + 0.44f * FMath::Abs(FMath::Sin(EffAng))) -
                     0.41f * (1.0f - FMath::Exp(-17.0f / AspectRatio)));
                float TangentialCoeff = 0.5f * SkinFriction * FMath::Cos(EffAng);

                float CL_val = NormalCoeff * FMath::Cos(EffAng) - TangentialCoeff * FMath::Sin(EffAng);
                float CD_val = NormalCoeff * FMath::Sin(EffAng) + TangentialCoeff * FMath::Cos(EffAng);
                float CM_val = -NormalCoeff * TorqCoeffProportion(EffAng);
                return FVector(CL_val, CD_val, CM_val);
            };

            // Düşük AoA ile Stall aralığını yumuşakça birleştir (Linear Stitching)
            float PaddingHigh = FMath::DegreesToRadians(FMath::Lerp(15.0f, 5.0f, FMath::Clamp((FlapDeflectionDeg + 50.0f) / 100.0f, 0.0f, 1.0f)));
            float PaddingLow = FMath::DegreesToRadians(FMath::Lerp(15.0f, 5.0f, FMath::Clamp((-FlapDeflectionDeg + 50.0f) / 100.0f, 0.0f, 1.0f)));
            float PaddedStallHigh = StallAngleHigh + PaddingHigh;
            float PaddedStallLow = StallAngleLow - PaddingLow;

            FVector AeroCoeffs;
            if (AoA < StallAngleHigh && AoA > StallAngleLow)
            {
                AeroCoeffs = CoeffsLowAoA(AoA);
            }
            else if (AoA > PaddedStallHigh || AoA < PaddedStallLow)
            {
                AeroCoeffs = CoeffsStall(AoA);
            }
            else
            {
                if (AoA > StallAngleHigh)
                {
                    FVector Low = CoeffsLowAoA(StallAngleHigh);
                    FVector Stl = CoeffsStall(PaddedStallHigh);
                    float T = (AoA - StallAngleHigh) / FMath::Max(0.001f, PaddedStallHigh - StallAngleHigh);
                    AeroCoeffs = FMath::Lerp(Low, Stl, FMath::Clamp(T, 0.0f, 1.0f));
                }
                else
                {
                    FVector Low = CoeffsLowAoA(StallAngleLow);
                    FVector Stl = CoeffsStall(PaddedStallLow);
                    float T = (AoA - StallAngleLow) / FMath::Max(0.001f, PaddedStallLow - StallAngleLow);
                    AeroCoeffs = FMath::Lerp(Low, Stl, FMath::Clamp(T, 0.0f, 1.0f));
                }
            }

            // Dinamik Basınç: q = 0.5 * rho * V^2
            float DynamicPressure = 0.5f * AeroConfig.AirDensity * SpeedInPlane * SpeedInPlane;

            // Lift, Drag Kuvvetleri ve Profil Pitching Moment Torku
            OutLiftVecN = LiftDir * (AeroCoeffs.X * DynamicPressure * HalfArea * AeroConfig.AeroForceScale);
            OutDragVecN = DragDir * (AeroCoeffs.Y * DynamicPressure * HalfArea * AeroConfig.AeroForceScale);
            OutForceN = OutLiftVecN + OutDragVecN;

            // Kanat kordu etrafındaki aerodinamik moment (N·m)
            OutTorqueNm = WorldRight * (AeroCoeffs.Z * DynamicPressure * HalfArea * Chord * AeroConfig.AeroForceScale);
        };

        // Sol Kanat ve Sağ Kanat Kuvvetlerini Hesapla
        FVector ForceL, TorqueL, LiftVecL, DragVecL;
        FVector ForceR, TorqueR, LiftVecR, DragVecR;
        float AlphaL = 0.0f, AlphaR = 0.0f;

        CalcKhanNahonSurface(WorldSurfaceLocL, ElevonAngleDegL, ForceL, TorqueL, AlphaL, LiftVecL, DragVecL);
        CalcKhanNahonSurface(WorldSurfaceLocR, ElevonAngleDegR, ForceR, TorqueR, AlphaR, LiftVecR, DragVecR);

        // KUVVETLERİ GERÇEK KANAT KONUMLARINDAN TATBİK ET (1 Newton = 100 Unreal Engine Force Units):
        VisualMeshComponents[0]->AddForceAtLocation(ForceL * 100.0f, WorldSurfaceLocL);
        VisualMeshComponents[0]->AddForceAtLocation(ForceR * 100.0f, WorldSurfaceLocR);

        if (!TorqueL.IsNearlyZero()) VisualMeshComponents[0]->AddTorqueInRadians(TorqueL * 10000.0f);
        if (!TorqueR.IsNearlyZero()) VisualMeshComponents[0]->AddTorqueInRadians(TorqueR * 10000.0f);

        // Telemetriyi Güncelle
        AeroConfig.CurrentLiftNewtons = LiftVecL.Size() + LiftVecR.Size();
        AeroConfig.CurrentDragNewtons = DragVecL.Size() + DragVecR.Size();
        AeroConfig.CurrentAlphaDeg = (AlphaL + AlphaR) * 0.5f;

        // 3D Gizmo Çizimi (Her kuvvet benzersiz ve belirgin renkli)
        if (AeroConfig.bShowAeroGizmos)
        {
            // 🟢 Lift (Kaldırma): Neon Açık Yeşil
            DrawDebugDirectionalArrow(GetWorld(), WorldSurfaceLocL, WorldSurfaceLocL + LiftVecL * 5.0f, 15.0f, FColor(0, 255, 120), false, 0.0f, 0, 2.5f);
            DrawDebugDirectionalArrow(GetWorld(), WorldSurfaceLocR, WorldSurfaceLocR + LiftVecR * 5.0f, 15.0f, FColor(0, 255, 120), false, 0.0f, 0, 2.5f);
            // 🟠 Drag (Direnç): Parlak Turuncu
            DrawDebugDirectionalArrow(GetWorld(), WorldSurfaceLocL, WorldSurfaceLocL + DragVecL * 5.0f, 15.0f, FColor(255, 130, 0), false, 0.0f, 0, 2.0f);
            DrawDebugDirectionalArrow(GetWorld(), WorldSurfaceLocR, WorldSurfaceLocR + DragVecR * 5.0f, 15.0f, FColor(255, 130, 0), false, 0.0f, 0, 2.0f);
        }

        // -------------------------------------------------------------------------
        // TELEMETRİ: GÖVDE EKSENLERİNDE KUVVETLER VE CoG ETRAFINDAKİ MOMENTLER
        // -------------------------------------------------------------------------
        FVector TotalLiftWorld = LiftVecL + LiftVecR;
        FVector TotalDragWorld = DragVecL + DragVecR;
        FVector TotalGravWorld = FVector(0.0f, 0.0f, -ChassisMassKg * 9.81f);

        FVector ArmM_L = (WorldSurfaceLocL - WorldCoM) * 0.01f; // m
        FVector ArmM_R = (WorldSurfaceLocR - WorldCoM) * 0.01f; // m

        FVector LiftMomentWorld = FVector::CrossProduct(ArmM_L, LiftVecL) + FVector::CrossProduct(ArmM_R, LiftVecR) + TorqueL + TorqueR;
        FVector DragMomentWorld = FVector::CrossProduct(ArmM_L, DragVecL) + FVector::CrossProduct(ArmM_R, DragVecR);
        // Yerçekimi tam olarak CoG'den geçtiği için dönme momenti = 0'dır.

        auto WorldToBody = [&](const FVector& Vec) -> FVector
        {
            return FVector(
                Vec | WorldForward, // Fx / Mx (İleri / Roll)
                Vec | WorldRight,   // Fy / My (Yan / Pitch)
                Vec | WorldUp       // Fz / Mz (Dikey / Yaw)
            );
        };

        FlightTelemetry.LiftForceBodyN = WorldToBody(TotalLiftWorld);
        FlightTelemetry.DragForceBodyN = WorldToBody(TotalDragWorld);
        FlightTelemetry.ThrustForceBodyN = WorldToBody(AppliedThrustVecWorld);
        FlightTelemetry.GravityForceBodyN = WorldToBody(TotalGravWorld);
        FlightTelemetry.NetForceBodyN = FlightTelemetry.LiftForceBodyN + FlightTelemetry.DragForceBodyN + FlightTelemetry.ThrustForceBodyN + FlightTelemetry.GravityForceBodyN;

        FlightTelemetry.LiftMomentBodyNm = WorldToBody(LiftMomentWorld);
        FlightTelemetry.DragMomentBodyNm = WorldToBody(DragMomentWorld);
        FlightTelemetry.ThrustMomentBodyNm = WorldToBody(AppliedThrustTorqueWorld);
        FlightTelemetry.NetMomentBodyNm = FlightTelemetry.LiftMomentBodyNm + FlightTelemetry.DragMomentBodyNm + FlightTelemetry.ThrustMomentBodyNm;

        FlightTelemetry.AirspeedKmh = Airspeed * 3.6f;
        FlightTelemetry.AlphaDeg = (AlphaL + AlphaR) * 0.5f;
        FlightTelemetry.LiftDragRatio = (AeroConfig.CurrentDragNewtons > 0.01f) ? (AeroConfig.CurrentLiftNewtons / AeroConfig.CurrentDragNewtons) : 0.0f;
        FlightTelemetry.CoGForwardCm = AeroConfig.CoGForwardCm;
        FlightTelemetry.LiftScale = AeroConfig.AeroForceScale;
    }
    else
    {
        AeroConfig.CurrentLiftNewtons = 0.0f;
        AeroConfig.CurrentDragNewtons = 0.0f;
        AeroConfig.CurrentAlphaDeg = 0.0f;

        FVector TotalGravWorld = FVector(0.0f, 0.0f, -ChassisMassKg * 9.81f);
        if (VisualMeshComponents.IsValidIndex(0) && VisualMeshComponents[0])
        {
            const FTransform& BodyTransform = VisualMeshComponents[0]->GetComponentTransform();
            FVector LocalForward = (VisualSections.IsValidIndex(0) && !VisualSections[0].BoneDirection.IsNearlyZero())
                ? VisualSections[0].BoneDirection.GetSafeNormal()
                : FVector(0.0f, -1.0f, 0.0f);
            FVector LocalRight = FVector(1.0f, 0.0f, 0.0f);
            FVector LocalUp = FVector(0.0f, 0.0f, 1.0f);

            FVector WorldFwd = BodyTransform.TransformVectorNoScale(LocalForward).GetSafeNormal();
            FVector WorldRgt = BodyTransform.TransformVectorNoScale(LocalRight).GetSafeNormal();
            FVector WorldU = BodyTransform.TransformVectorNoScale(LocalUp).GetSafeNormal();

            auto WorldToBody = [&](const FVector& Vec) -> FVector
            {
                return FVector(Vec | WorldFwd, Vec | WorldRgt, Vec | WorldU);
            };

            FlightTelemetry.LiftForceBodyN = FVector::ZeroVector;
            FlightTelemetry.DragForceBodyN = FVector::ZeroVector;
            FlightTelemetry.ThrustForceBodyN = WorldToBody(AppliedThrustVecWorld);
            FlightTelemetry.GravityForceBodyN = WorldToBody(TotalGravWorld);
            FlightTelemetry.NetForceBodyN = FlightTelemetry.ThrustForceBodyN + FlightTelemetry.GravityForceBodyN;

            FlightTelemetry.LiftMomentBodyNm = FVector::ZeroVector;
            FlightTelemetry.DragMomentBodyNm = FVector::ZeroVector;
            FlightTelemetry.ThrustMomentBodyNm = WorldToBody(AppliedThrustTorqueWorld);
            FlightTelemetry.NetMomentBodyNm = FlightTelemetry.ThrustMomentBodyNm;
        }

        FlightTelemetry.AirspeedKmh = 0.0f;
        FlightTelemetry.AlphaDeg = 0.0f;
        FlightTelemetry.LiftDragRatio = 0.0f;
        FlightTelemetry.CoGForwardCm = AeroConfig.CoGForwardCm;
        FlightTelemetry.LiftScale = AeroConfig.AeroForceScale;
    }

    // =========================================================================================
    // 3D CANLI DEBUG GİZMO GÖRSELLEŞTİRİCİLERİ (AĞIRLIK MERKEZİ VE YERÇEKİMİ)
    // =========================================================================================
    if (GetWorld() && VisualMeshComponents.IsValidIndex(0) && VisualMeshComponents[0] && AeroConfig.bShowAeroGizmos)
    {
        FVector CoGWorld = VisualMeshComponents[0]->GetCenterOfMass();

        // 🟡 SARI KÜRE: CoG (Ağırlık Merkezi - Chaos Yerçekiminin Doğal Çektiği Nokta)
        DrawDebugSphere(GetWorld(), CoGWorld, 8.0f, 12, FColor(255, 235, 0), false, 0.0f, 0, 2.5f);
        DrawDebugString(GetWorld(), CoGWorld + FVector(0.0f, 0.0f, 12.0f), TEXT("🟡 CoG (Ağırlık Merkezi)"), nullptr, FColor::Yellow, 0.0f, true);

        // 🔴 KIRMIZI OK: Ağırlık (Yerçekimi) -> CoG noktasından doğrudan aşağı (-Z)
        float WeightN = ChassisMassKg * 9.81f;
        float WeightArrowLen = FMath::Clamp(WeightN * 3.0f, 25.0f, 250.0f);
        DrawDebugDirectionalArrow(GetWorld(), CoGWorld, CoGWorld + FVector(0.0f, 0.0f, -WeightArrowLen), 14.0f, FColor(255, 40, 40), false, 0.0f, 2.0f);
    }

    // 4) Kamerayı gövdenin dünya konumuna kilitle (Araç hareket ettikçe kamera tam arkasında kalsın)
    if (VisualMeshComponents.IsValidIndex(0) && VisualMeshComponents[0] && OrbitSpringArm)
    {
        if (OrbitSpringArm->GetAttachParent() != VisualMeshComponents[0])
        {
            FVector ChassisLoc = VisualMeshComponents[0]->GetComponentLocation();
            OrbitSpringArm->SetWorldLocation(ChassisLoc + FVector(0.0f, 0.0f, 60.0f));
        }
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

            // L ve K Tuşları: Lift / Aero Kuvvet Çarpanını Canlı Dinamik Artır / Azalt
            bool bLDown = PC->IsInputKeyDown(EKeys::L) || PC->IsInputKeyDown(EKeys::RightBracket);
            if (bLDown && !bWasLDown) { IncreaseLiftScale(); }
            bWasLDown = bLDown;

            bool bKDown = PC->IsInputKeyDown(EKeys::K) || PC->IsInputKeyDown(EKeys::LeftBracket);
            if (bKDown && !bWasKDown) { DecreaseLiftScale(); }
            bWasKDown = bKDown;

            // O ve P Tuşları: Ağırlık Merkezini (CoG) Canlı Dinamik İleri / Geri Kaydır
            bool bODown = PC->IsInputKeyDown(EKeys::O);
            if (bODown && !bWasODown) { NudgeCoGForward(); }
            bWasODown = bODown;

            bool bPDown = PC->IsInputKeyDown(EKeys::P);
            if (bPDown && !bWasPDown) { NudgeCoGBackward(); }
            bWasPDown = bPDown;
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
    SectionConstraintMap.Empty();

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

    for (auto& Pair : SteerBoneComponents)
    {
        if (Pair.Value) Pair.Value->DestroyComponent();
    }
    SteerBoneComponents.Empty();
    WheelToSteerBoneMap.Empty();

    for (UPiSimActuatorComponent* Actuator : AttachedActuators)
    {
        if (Actuator) Actuator->DestroyComponent();
    }
    AttachedActuators.Empty();

    for (UPiSimAeroWingComponent* Wing : AttachedWingBodies)
    {
        if (Wing) Wing->DestroyComponent();
    }
    AttachedWingBodies.Empty();

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

void APiSimModelImporter::MultiplyScale_0_1X()
{
    ImportScaleMultiplier *= 0.1f;
    BuildAndSpawnRobotHierarchy(ImportScaleMultiplier);
}

void APiSimModelImporter::MultiplyScale_10_0X()
{
    ImportScaleMultiplier *= 10.0f;
    BuildAndSpawnRobotHierarchy(ImportScaleMultiplier);
}

void APiSimModelImporter::TogglePhysicsSimulation()
{
    bIsPhysicsSimulating = !bIsPhysicsSimulating;
    SetPhysicsSimulationActive(bIsPhysicsSimulating);
}

void APiSimModelImporter::ToggleModelFile()
{
    if (ActiveModelFileName.Equals(TEXT("robot_import_test.fbx"), ESearchCase::IgnoreCase))
    {
        ActiveModelFileName = TEXT("robot_import_test1.fbx");
    }
    else
    {
        ActiveModelFileName = TEXT("robot_import_test.fbx");
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(799, 5.0f, FLinearColor(0.2f, 0.8f, 1.0f).ToFColor(true),
            FString::Printf(TEXT("📦 [MODEL DEĞİŞTİRİLDİ] Aktif Model: %s"), *ActiveModelFileName));
    }

    UE_LOG(LogTemp, Warning, TEXT("📦 [MODEL GEÇİŞİ] Yeni aktif model dosyası: %s"), *ActiveModelFileName);

    ImportAndSpawnRobot();
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
    G_FbxBoneWorldLocations.Empty();


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
        FVector BonePosition = FVector::ZeroVector;
        bool bHasBonePosition = false;
        FVector BoneDirection = FVector(0.0f, 1.0f, 0.0f);
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
            FVector SubBoneDir = FVector(0.0f, 1.0f, 0.0f);
            FVector SubBonePos = FVector::ZeroVector;
            bool bSubHasBonePos = false;

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
                else if (DName.Equals(TEXT("TransformLink")) && DPropLen > 12)
                {
                    uint32 ArrayLen = *reinterpret_cast<const uint32*>(&FileBytes[DPropsStart + 1]);
                    uint32 Encoding = *reinterpret_cast<const uint32*>(&FileBytes[DPropsStart + 5]);
                    uint32 CompLen = *reinterpret_cast<const uint32*>(&FileBytes[DPropsStart + 9]);
                    int32 DataOffset = DPropsStart + 13;

                    int32 UncompSize = ArrayLen * 8; // double array (16 doubles = 128 bytes)
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

                    if (DataPtr && ArrayLen >= 16)
                    {
                        const double* Mat = reinterpret_cast<const double*>(DataPtr);
                        // Mat[4..6] is the local Y axis of the bone in Blender (Head -> Tail).
                        // In Blender -> Unreal coordinate conversion: (X, -Y, Z)
                        FVector Dir(Mat[4], -Mat[5], Mat[6]);
                        if (!Dir.IsNearlyZero())
                        {
                            SubBoneDir = Dir.GetSafeNormal();
                        }

                        // Mat[12..14] is the bone translation in FBX space
                        float MatScale = FVector(Mat[0], Mat[1], Mat[2]).Size();
                        float PosScale = (MatScale < 10.0f) ? Scale : 1.0f;
                        SubBonePos = FVector(Mat[12] * PosScale, Mat[13] * PosScale, Mat[14] * PosScale);
                        bSubHasBonePos = true;
                        G_FbxBoneWorldLocations.Add(LabelStr, SubBonePos);
                        UE_LOG(LogTemp, Warning, TEXT("🦴 [FBX KEMİK KONUMU] '%s' -> %s"), *LabelStr, *SubBonePos.ToString());
                    }
                }
                DP = (int32)DEnd;
            }

            if (SubIndices.Num() > 0)
            {
                FRawFbxSubDef SubDef;
                SubDef.BoneLabel = LabelStr;
                SubDef.Indices = SubIndices;
                SubDef.BonePosition = SubBonePos;
                SubDef.bHasBonePosition = bSubHasBonePos;
                SubDef.BoneDirection = SubBoneDir;
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
                                    Sec.BoneDirection = SubDef->BoneDirection;

                                    // Gövdenin boyuna uzanımını (Pivot -> Burun mesafesi) hesapla
                                    float MaxForwardProj = 0.0f;
                                    FVector ForwardAxis = FVector(0.0f, 1.0f, 0.0f); // Model ileri standardı (+Y)
                                    for (const FVector& V : SubVerts)
                                    {
                                        float Proj = (V - Pivot) | ForwardAxis;
                                        if (Proj > MaxForwardProj) MaxForwardProj = Proj;
                                    }
                                    Sec.BoneLength = (MaxForwardProj > 5.0f) ? MaxForwardProj : 100.0f;

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
        if (!bAlreadyInVisual && (MName.StartsWith(TEXT("M_"), ESearchCase::IgnoreCase) ||
                                  MName.StartsWith(TEXT("B_"), ESearchCase::IgnoreCase) ||
                                  Lower.Contains(TEXT("wing")) ||
                                  Lower.Contains(TEXT("steer")) ||
                                  Lower.Contains(TEXT("servo")) ||
                                  Lower.Contains(TEXT("joint"))))
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

    FString FbxPath = FPaths::ProjectSavedDir() / TEXT("Robots/Cache") / ActiveModelFileName;
    if (!FPaths::FileExists(FbxPath))
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 8.0f, FColor::Red,
                FString::Printf(TEXT(">>> [HATA] '%s' dosyası bulunamadı! Lütfen Saved/Robots/Cache/ klasörüne ekleyin. <<<"), *ActiveModelFileName));
        }
        UE_LOG(LogTemp, Error, TEXT("[PiSimModelImporter HATA] %s bulunamadi!"), *FbxPath);
        return;
    }

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
    // [KÖK KEMİK DİSPATCHER] CHASSIS -> LAND, AIRFRAME -> AIR, HULL -> SEA
    // -----------------------------------------------------------------------------------------
    FString RootBoneName = (VisualSections.Num() > 0) ? VisualSections[0].MeshName : TEXT("");
    VehicleDomain = APiSimModelImporterBase::DetectVehicleDomain(RootBoneName);

    if (VehicleDomain == EVehicleDomain::Air)
    {
        UE_LOG(LogTemp, Warning, TEXT("✈️ [PISIM DISPATCHER] Kök Kemik: '%s' -> HAVA ARACI (Air Importer)"), *RootBoneName);
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(777, 8.0f, FColor::Cyan,
                FString::Printf(TEXT("✈️ [PISIM DISPATCHER] Kök Kemik: '%s' -> HAVA ARACI (Air Importer & Soket Mimarisi)"), *RootBoneName));
        }
    }
    else if (VehicleDomain == EVehicleDomain::Sea)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚓ [PISIM DISPATCHER] Kök Kemik: '%s' -> DENİZ ARACI (Sea Importer / hull)"), *RootBoneName);
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(777, 8.0f, FColor::Emerald,
                FString::Printf(TEXT("⚓ [PISIM DISPATCHER] Kök Kemik: '%s' -> DENİZ ARACI (Sea Importer / hull)"), *RootBoneName));
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("🚗 [PISIM DISPATCHER] Kök Kemik: '%s' -> KARA ARACI (Land Importer / chassis)"), *RootBoneName);
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(777, 8.0f, FColor::Yellow,
                FString::Printf(TEXT("🚗 [PISIM DISPATCHER] Kök Kemik: '%s' -> KARA ARACI (Land Importer / chassis)"), *RootBoneName));
        }
    }

    // -----------------------------------------------------------------------------------------
    // 1) HER PARÇA İÇİN GÖRSEL RENDER VE UCX COLLISION'I TEK BİR DİNAMİK GÖVDEDE BİRLEŞTİR
    // -----------------------------------------------------------------------------------------
    for (int32 i = 0; i < VisualSections.Num(); ++i)
    {
        FName CompName = *FString::Printf(TEXT("RobotSubMesh_%d_%s"), i, *VisualSections[i].MeshName);
        UProceduralMeshComponent* VisComp = NewObject<UProceduralMeshComponent>(this, CompName);
        VisComp->SetMobility(EComponentMobility::Movable);

        // Hiyerarşik Kemik Bağlantısı: Gövde dışındaki parçalar gövdeye veya Steer mafsalına bağlanır
        if (i == 0)
        {
            VisComp->SetupAttachment(SceneRootComponent);
            VisComp->SetRelativeLocation(VisualSections[i].PivotPoint);
        }
        else if (FString* SteerBoneName = WheelToSteerBoneMap.Find(i))
        {
            // ÖN DİREKSİYONLU TEKERLEK: Doğrudan kendi Steer mafsal kemiğine bağlanır!
            USceneComponent* SteerComp = SteerBoneComponents[*SteerBoneName];
            VisComp->SetupAttachment(SteerComp);
            VisComp->SetRelativeLocation(FVector::ZeroVector); // Mafsal tam tekerlek pivotundadır
            UE_LOG(LogTemp, Warning, TEXT("🔗 [HİYERARŞİ] '%s' tekerleği '%s' steer mafsalına bağlandı."), *VisualSections[i].MeshName, **SteerBoneName);
        }
        else if (VisualMeshComponents.IsValidIndex(0) && VisualMeshComponents[0])
        {
            // ARKA SÜRÜŞ TEKERLEKLERİ: Doğrudan şasiye bağlanır
            VisComp->SetupAttachment(VisualMeshComponents[0]);
            VisComp->SetRelativeLocation(VisualSections[i].PivotPoint - VisualSections[0].PivotPoint);
            UE_LOG(LogTemp, Warning, TEXT("🔗 [HİYERARŞİ] '%s' tekerleği doğrudan şasiye bağlandı."), *VisualSections[i].MeshName);
        }
        else
        {
            VisComp->SetupAttachment(SceneRootComponent);
            VisComp->SetRelativeLocation(VisualSections[i].PivotPoint);
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
        // İLGİLİ UCX CONVEX HULL'UNU BUL VE PARÇAYA ZIRH OLARAK GİYDİR
        VisComp->ClearCollisionConvexMeshes();
        bool bHasCollision = false;

        // 1) Bu görsel parçayla doğrudan eşleşen UCX'leri ekle
        for (int32 j = 0; j < UCXSections.Num(); ++j)
        {
            if (UCXSections[j].MeshName.Contains(VisualSections[i].MeshName, ESearchCase::IgnoreCase))
            {
                VisComp->AddCollisionConvexMesh(UCXSections[j].Vertices);
                bHasCollision = true;
            }
        }

        // 2) Eğer bu ana gövde (i == 0) ise ve ayrı alt görsel parçalara atanmamış kanat/gövde UCX'leri varsa (örn. UCX_B_Wing_R, UCX_B_Wing_L):
        // Bunları ana şasiye göre ofsetleyerek ana fizik gövdesine ekle!
        if (i == 0 && UCXSections.Num() > 0)
        {
            for (int32 j = 0; j < UCXSections.Num(); ++j)
            {
                bool bBelongsToOtherVisual = false;
                for (int32 other = 1; other < VisualSections.Num(); ++other)
                {
                    if (UCXSections[j].MeshName.Contains(VisualSections[other].MeshName, ESearchCase::IgnoreCase))
                    {
                        bBelongsToOtherVisual = true;
                        break;
                    }
                }

                if (!bBelongsToOtherVisual)
                {
                    FVector Offset = UCXSections[j].PivotPoint - VisualSections[0].PivotPoint;
                    TArray<FVector> OffsetVerts = UCXSections[j].Vertices;
                    for (FVector& V : OffsetVerts)
                    {
                        V += Offset;
                    }
                    VisComp->AddCollisionConvexMesh(OffsetVerts);
                    bHasCollision = true;
                    UE_LOG(LogTemp, Warning, TEXT("🛡️ [UCX ZIRHI ŞASİYE MONTE EDİLDİ] '%s' şasiye eklendi (Ofset: %s, Verts: %d)"),
                        *UCXSections[j].MeshName, *Offset.ToString(), OffsetVerts.Num());
                }
            }
        }

        // Çarpışma durumu: SADECE UCX varsa collision aktif, UCX yoksa NO COLLISION!
        if (bHasCollision)
        {
            VisComp->bUseComplexAsSimpleCollision = false;
            VisComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            VisComp->SetCollisionObjectType(ECC_WorldDynamic);
            VisComp->SetCollisionResponseToAllChannels(ECR_Block);
            VisComp->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block); // Zemini bloklar
            VisComp->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
            VisComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
        }
        else
        {
            // UCX'i olmayan parçalar saf görsel kalır, asla otomatik collision almaz!
            VisComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }
        VisComp->RecreatePhysicsState();
        VisComp->UpdateBounds();

        VisualMeshComponents.Add(VisComp);

        if (i == 0)
        {
            // Şasi oluşturulduktan hemen sonra, saf Steer kemiklerini (M_Steer_FR, M_Steer_FL vb.) şasiye bağla
            SteerBoneComponents.Empty();
            WheelToSteerBoneMap.Empty();

            for (const FString& PureBoneName : PureBoneNames)
            {
                FString LowerPure = PureBoneName.ToLower();
                if (LowerPure.Contains(TEXT("steer")) || LowerPure.Contains(TEXT("servo")) || LowerPure.StartsWith(TEXT("m_steer")))
                {
                    FString Clean = PureBoneName;
                    Clean.RemoveFromStart(TEXT("M_Steer_"), ESearchCase::IgnoreCase);
                    Clean.RemoveFromStart(TEXT("Steer_"), ESearchCase::IgnoreCase);
                    Clean.RemoveFromStart(TEXT("M_"), ESearchCase::IgnoreCase);
                    FString CleanLower = Clean.ToLower();

                    int32 MatchedWheelIdx = INDEX_NONE;
                    for (int32 w = 1; w < VisualSections.Num(); ++w)
                    {
                        FString LowerW = VisualSections[w].MeshName.ToLower();

                        // Sadece tekerlek mesh'leri ile eşleştir — elevon, aero yüzeyleri ASLA
                        bool bIsWheelMesh = LowerW.Contains(TEXT("wheel")) ||
                                            LowerW.Contains(TEXT("teker")) ||
                                            LowerW.StartsWith(TEXT("w_"));
                        if (!bIsWheelMesh) continue;

                        // Clean token'ı mesh isminde tam sözcük sınırında ara:
                        // "_FR", "_1", "_L" gibi kısa suffix'lerin "M_Elevon_1" gibi isimlerle yanlış eşleşmesini engelle
                        bool bSuffixMatch = LowerW.EndsWith(TEXT("_") + CleanLower) ||
                                            LowerW.Contains(TEXT("_") + CleanLower + TEXT("_")) ||
                                            LowerW.Equals(CleanLower);

                        if (bSuffixMatch)
                        {
                            MatchedWheelIdx = w;
                            break;
                        }
                    }

                    if (MatchedWheelIdx != INDEX_NONE)
                    {
                        FName SteerCompName = *FString::Printf(TEXT("SteerJointComp_%s"), *PureBoneName);
                        USceneComponent* SteerComp = NewObject<USceneComponent>(this, SteerCompName);
                        SteerComp->SetMobility(EComponentMobility::Movable);
                        SteerComp->SetupAttachment(VisualMeshComponents[0]);
                        FVector SteerPivotOffset = VisualSections[MatchedWheelIdx].PivotPoint - VisualSections[0].PivotPoint;
                        SteerComp->SetRelativeLocation(SteerPivotOffset);
                        SteerComp->SetRelativeRotation(FRotator::ZeroRotator);
                        SteerComp->RegisterComponent();

                        SteerBoneComponents.Add(PureBoneName, SteerComp);
                        WheelToSteerBoneMap.Add(MatchedWheelIdx, PureBoneName);

                        UE_LOG(LogTemp, Warning, TEXT("🎯 [STEER MAFSALI KURULDU] '%s' şasiye bağlandı | Konum: %s | Eşleşen Tekerlek: [%d] %s"),
                            *PureBoneName, *SteerPivotOffset.ToString(), MatchedWheelIdx, *VisualSections[MatchedWheelIdx].MeshName);
                    }
                    else
                    {
                        UE_LOG(LogTemp, Warning, TEXT("⚠️ [STEER] '%s' için tekerlek mesh eşleşmesi bulunamadı (Clean='%s') – Steer bone atlandı."),
                            *PureBoneName, *Clean);
                    }
                }
            }
        }
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
        MotorItem.BoneDirection = VisualSections[i].BoneDirection;

        FString LowerName = VisualSections[i].MeshName.ToLower();
        if (LowerName.StartsWith(TEXT("m_wheel")) || LowerName.StartsWith(TEXT("w_")) || LowerName.Contains(TEXT("wheel")))
        {
            MotorItem.Role = EPiSimMotorRole::DriveWheel;
            MotorItem.MaxVelocityRPM = 500.0f;
            MotorItem.MaxTorqueNm = 15.0f;
        }
        else if (LowerName.StartsWith(TEXT("m_steer")) || LowerName.StartsWith(TEXT("steer")))
        {
            MotorItem.Role = EPiSimMotorRole::ServoJoint;
            MotorItem.MinLimitDeg = -45.0f;
            MotorItem.MaxLimitDeg = +45.0f;
            MotorItem.MaxTorqueNm = 20.0f;
        }
        else if (LowerName.Contains(TEXT("elevon")) || LowerName.Contains(TEXT("aileron")) || LowerName.Contains(TEXT("flap")) || LowerName.Contains(TEXT("rudder")) || LowerName.Contains(TEXT("elevator")))
        {
            // Sabit Kanat / Uçan Kanat Kontrol Yüzeyleri (Elevon, Aileron, Rudder)
            MotorItem.Role = EPiSimMotorRole::ServoJoint;
            MotorItem.MinLimitDeg = -20.0f;
            MotorItem.MaxLimitDeg = +20.0f;
            MotorItem.MaxTorqueNm = 5.0f;
        }
        else if (LowerName.Contains(TEXT("thrust")) || LowerName.Contains(TEXT("prop")) || LowerName.Contains(TEXT("rotor")) || LowerName.Contains(TEXT("motor")))
        {
            // Drone ve Uçak İtki Pervaneleri (M_Thrust, M_Prop, Propeller, Rotor, Motor vb.)
            MotorItem.Role = EPiSimMotorRole::Thruster;
            MotorItem.MaxVelocityRPM = 10000.0f;
            MotorItem.MaxTorqueNm = 18.0f; // 18N net itki
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
        OrbitSpringArm->bEnableCameraLag = false;
        OrbitSpringArm->bEnableCameraRotationLag = false;
        OrbitSpringArm->CameraLagSpeed = 0.0f;
        OrbitSpringArm->CameraRotationLagSpeed = 0.0f;
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
    // -----------------------------------------------------------------------------------------
    // 10) SOKET TABANLI MODÜLER BİLEŞENLER (WINGS, BLDC THRUSTER, ELEVON SERVOS)
    // Kullanıcı Talimatı: "Pawn sınıfına fiziksel ve görsel modeli ekleyecek, ondan sonra soket ekler
    // gibi motorları, kuvvet gövdelerini (kanat/tekne), kontrol yüzeylerini ve sensörlerini ekleyecek"
    // -----------------------------------------------------------------------------------------
    if (VehicleDomain == EVehicleDomain::Air && VisualMeshComponents.IsValidIndex(0) && VisualMeshComponents[0])
    {
        UPrimitiveComponent* AirframeBody = VisualMeshComponents[0];

        // 1) Kanat Kaldırma Gövdelerini (Aero Wing Components) Soket Noktalarına Tak
        FVector LeftWingLoc = G_FbxBoneWorldLocations.Contains(TEXT("B_Wing_L")) ? 
            (G_FbxBoneWorldLocations[TEXT("B_Wing_L")] - VisualSections[0].PivotPoint) : FVector(0.0f, -AeroConfig.Wingspan * 25.0f, 0.0f);
        FVector RightWingLoc = G_FbxBoneWorldLocations.Contains(TEXT("B_Wing_R")) ? 
            (G_FbxBoneWorldLocations[TEXT("B_Wing_R")] - VisualSections[0].PivotPoint) : FVector(0.0f, AeroConfig.Wingspan * 25.0f, 0.0f);

        UPiSimAeroWingComponent* LeftWingComp = NewObject<UPiSimAeroWingComponent>(this, TEXT("ModularSocket_LeftWing"));
        LeftWingComp->WingName = TEXT("LeftWing");
        LeftWingComp->bIsRightWing = false;
        LeftWingComp->WingArea = AeroConfig.WingArea * 0.5f;
        LeftWingComp->Wingspan = AeroConfig.Wingspan * 0.5f;
        LeftWingComp->CL0 = AeroConfig.CL0;
        LeftWingComp->CLAlpha = AeroConfig.CLAlpha;
        LeftWingComp->CD0 = AeroConfig.CD0;
        LeftWingComp->StallAngleDeg = AeroConfig.StallAngleDeg;
        LeftWingComp->ElevonEffectiveness = AeroConfig.ElevonEffectiveness;
        LeftWingComp->SetupAttachment(AirframeBody);
        LeftWingComp->SetRelativeLocation(LeftWingLoc);
        LeftWingComp->RegisterComponent();
        AttachedWingBodies.Add(LeftWingComp);

        UPiSimAeroWingComponent* RightWingComp = NewObject<UPiSimAeroWingComponent>(this, TEXT("ModularSocket_RightWing"));
        RightWingComp->WingName = TEXT("RightWing");
        RightWingComp->bIsRightWing = true;
        RightWingComp->WingArea = AeroConfig.WingArea * 0.5f;
        RightWingComp->Wingspan = AeroConfig.Wingspan * 0.5f;
        RightWingComp->CL0 = AeroConfig.CL0;
        RightWingComp->CLAlpha = AeroConfig.CLAlpha;
        RightWingComp->CD0 = AeroConfig.CD0;
        RightWingComp->StallAngleDeg = AeroConfig.StallAngleDeg;
        RightWingComp->ElevonEffectiveness = AeroConfig.ElevonEffectiveness;
        RightWingComp->SetupAttachment(AirframeBody);
        RightWingComp->SetRelativeLocation(RightWingLoc);
        RightWingComp->RegisterComponent();
        AttachedWingBodies.Add(RightWingComp);

        // 2) Aktüatörleri Tak (Thruster/BLDC ESC ve Elevon Servoları)
        for (int32 m = 0; m < ConfiguredMotors.Num(); ++m)
        {
            const FPiSimMotorItem& MItem = ConfiguredMotors[m];
            if (MItem.Role == EPiSimMotorRole::Thruster)
            {
                FName ThrusterName = *FString::Printf(TEXT("ModularSocket_Thruster_%d"), m);
                UPiSimActuatorComponent* ThrusterAct = NewObject<UPiSimActuatorComponent>(this, ThrusterName);
                ThrusterAct->BoneIndex = m;
                ThrusterAct->ActuatorName = MItem.BoneName;
                ThrusterAct->Role = EPiSimMotorRole::Thruster;
                ThrusterAct->MotorType = EPiSimMotorType::BLDC_ESC;
                ThrusterAct->Ros2Topic = TEXT("/actuator/thrust_cmd");
                ThrusterAct->BoneDirection = MItem.BoneDirection;
                ThrusterAct->MaxTorqueNm = (MItem.MaxTorqueNm > 0.1f) ? MItem.MaxTorqueNm : 18.0f;
                ThrusterAct->MaxVelocityRPM = MItem.MaxVelocityRPM;
                ThrusterAct->bReverseDirection = MItem.bReverseThrust;

                int32 VisIdx = FindVisualSectionForBone(m);
                if (VisIdx > 0 && VisualMeshComponents.IsValidIndex(VisIdx))
                {
                    ThrusterAct->LinkedVisualMesh = VisualMeshComponents[VisIdx];
                }

                FVector PropOffset = G_FbxBoneWorldLocations.Contains(MItem.BoneName) ?
                    (G_FbxBoneWorldLocations[MItem.BoneName] - VisualSections[0].PivotPoint) :
                    ((VisIdx > 0 && VisualSections.IsValidIndex(VisIdx)) ? (VisualSections[VisIdx].PivotPoint - VisualSections[0].PivotPoint) : FVector::ZeroVector);

                ThrusterAct->SetupAttachment(AirframeBody);
                ThrusterAct->SetRelativeLocation(PropOffset);
                ThrusterAct->RegisterComponent();
                AttachedActuators.Add(ThrusterAct);
            }
            else if (MItem.Role == EPiSimMotorRole::ServoJoint && !WheelToSteerBoneMap.Contains(m))
            {
                FName ElevonName = *FString::Printf(TEXT("ModularSocket_Servo_%d"), m);
                UPiSimActuatorComponent* ServoAct = NewObject<UPiSimActuatorComponent>(this, ElevonName);
                ServoAct->BoneIndex = m;
                ServoAct->ActuatorName = MItem.BoneName;
                ServoAct->Role = EPiSimMotorRole::ServoJoint;
                ServoAct->MotorType = EPiSimMotorType::Servo_Position;
                ServoAct->MinLimitDeg = MItem.MinLimitDeg;
                ServoAct->MaxLimitDeg = MItem.MaxLimitDeg;

                int32 VisIdx = FindVisualSectionForBone(m);
                if (VisIdx > 0 && VisualMeshComponents.IsValidIndex(VisIdx))
                {
                    ServoAct->LinkedVisualMesh = VisualMeshComponents[VisIdx];
                }

                FVector ElevonOffset = (VisIdx > 0 && VisualSections.IsValidIndex(VisIdx)) ?
                    (VisualSections[VisIdx].PivotPoint - VisualSections[0].PivotPoint) : FVector::ZeroVector;

                ServoAct->SetupAttachment(AirframeBody);
                ServoAct->SetRelativeLocation(ElevonOffset);
                ServoAct->RegisterComponent();
                AttachedActuators.Add(ServoAct);
            }
        }

        UE_LOG(LogTemp, Warning, TEXT("✈️ [PISIM MODÜLER SOKET SİSTEMİ] %d Kanat Gövdesi ve %d Aktüatör Hava Aracına Bağlandı!"),
            AttachedWingBodies.Num(), AttachedActuators.Num());
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
        SectionConstraintMap.Empty();

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

    // 2) Steer Kemiklerini (Kinematik Mafsalları) Kontrol Et & Doğrula
    // Kullanıcı Kesin Kuralı: Steer kemiği FİZİK SİMÜLE ETMEZ (Kinematiktir)!
    // Sadece 1 eksende (Yaw) döner ve sadece slider / kod ile döndürülür!
    for (auto& SteerPair : SteerBoneComponents)
    {
        if (SteerPair.Value)
        {
            SteerPair.Value->SetRelativeRotation(FRotator::ZeroRotator);
            UE_LOG(LogTemp, Warning, TEXT("🎯 [STEER KEMİĞİ HAZIR] '%s' Şasiye bağlı 1-Eksen (Yaw) Kinematik Mafsal! (Fizik Simülasyonu: KAPALI)"), *SteerPair.Key);
        }
    }

    // 3) Her Parçaya (Gövde ve Tüm Tekerleklere) Kütle, Zırh ve Dinamik Fizik Ver
    for (int32 i = 0; i < VisualMeshComponents.Num(); ++i)
    {
        UProceduralMeshComponent* VisComp = VisualMeshComponents[i];
        if (!VisComp || !VisualSections.IsValidIndex(i)) continue;

        EPiSimMotorRole PartRole = ConfiguredMotors.IsValidIndex(i) ? ConfiguredMotors[i].Role : EPiSimMotorRole::None;
        bool bIsWheel = (PartRole == EPiSimMotorRole::DriveWheel || PartRole == EPiSimMotorRole::SteeredWheel || PartRole == EPiSimMotorRole::FreeCaster || PartRole == EPiSimMotorRole::TrackPad);
        bool bIsThruster = (PartRole == EPiSimMotorRole::Thruster);
        bool bIsAeroSurface = (PartRole == EPiSimMotorRole::ServoJoint && !WheelToSteerBoneMap.Contains(i));

        // İtki Pervaneleri, Uçuş Kontrol Yüzeyleri (Elevon) ve tanımsız parçalar: Şasiye kinematik bağlan
        // Tekerlek OLMAYAN tüm i>0 parçalar (thruster, elevon, None-rol) kinematik → fizik simüle etmez
        bool bIsKinematic = (i > 0) && (bIsThruster || bIsAeroSurface || (!bIsWheel));
        if (bIsKinematic)
        {
            VisComp->SetSimulatePhysics(false);
            VisComp->SetEnableGravity(false);
            VisComp->SetMobility(EComponentMobility::Movable);
            if (VisualMeshComponents.IsValidIndex(0) && VisualMeshComponents[0])
            {
                VisComp->AttachToComponent(VisualMeshComponents[0], FAttachmentTransformRules::KeepWorldTransform);
            }
            UE_LOG(LogTemp, Warning, TEXT("🔒 [KİNEMATİK] [%d] '%s' rol=%d → şasiye kinematik bağlandı"),
                i, *VisualSections[i].MeshName, (int32)PartRole);
            continue;
        }

        float Mass = 2.0f;
        if (i == 0)
        {
            Mass = (VisualSections[0].MassKg > 0.01f && VisualSections[0].MassKg != 2.5f) ? VisualSections[0].MassKg : ChassisMassKg;
            if (Mass >= 30.0f)
            {
                // Modelde tekerlek var mı kontrol et
                bool bHasWheels = false;
                for (const FPiSimMotorItem& M : ConfiguredMotors)
                {
                    if (M.Role == EPiSimMotorRole::DriveWheel || M.Role == EPiSimMotorRole::SteeredWheel)
                    {
                        bHasWheels = true;
                        break;
                    }
                }
                if (!bHasWheels)
                {
                    Mass = 2.0f; // Drone ve uçan kanat için 2.0 kg
                    UE_LOG(LogTemp, Warning, TEXT("✈️ [HAVA ARACI TESPİT EDİLDİ] Tekerlek bulunamadı. Gövde kütlesi otomatik 2.0 kg ayarlandı."));
                }
            }
        }

        // İlgili UCX Convex Hull'unu aktar (UCX yoksa asla collision verilmez!)
        VisComp->ClearCollisionConvexMeshes();
        bool bHasCollision = false;

        for (const FImporterMeshSection& UcxSec : UCXSections)
        {
            if (UcxSec.MeshName.Contains(VisualSections[i].MeshName, ESearchCase::IgnoreCase))
            {
                VisComp->AddCollisionConvexMesh(UcxSec.Vertices);
                bHasCollision = true;
            }
        }

        // Kök gövde (i == 0) ise diğer parçalara atanmamış kanat/gövde UCX'lerini ofsetleyerek ekle
        if (i == 0 && UCXSections.Num() > 0)
        {
            for (const FImporterMeshSection& UcxSec : UCXSections)
            {
                bool bBelongsToOtherVisual = false;
                for (int32 other = 1; other < VisualSections.Num(); ++other)
                {
                    if (UcxSec.MeshName.Contains(VisualSections[other].MeshName, ESearchCase::IgnoreCase))
                    {
                        bBelongsToOtherVisual = true;
                        break;
                    }
                }

                if (!bBelongsToOtherVisual)
                {
                    FVector Offset = UcxSec.PivotPoint - VisualSections[0].PivotPoint;
                    TArray<FVector> OffsetVerts = UcxSec.Vertices;
                    for (FVector& V : OffsetVerts)
                    {
                        V += Offset;
                    }
                    VisComp->AddCollisionConvexMesh(OffsetVerts);
                    bHasCollision = true;
                }
            }
        }

        if (bHasCollision)
        {
            VisComp->bUseComplexAsSimpleCollision = false;
            VisComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            VisComp->SetCollisionObjectType(ECC_WorldDynamic);
            VisComp->SetCollisionResponseToAllChannels(ECR_Block);
            VisComp->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block); // Zeminle çarpış
            VisComp->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
        }
        else
        {
            // UCX yoksa NO COLLISION!
            VisComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }
        VisComp->RecreatePhysicsState();

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

        // =========================================================================================
        // 🔒 [DO NOT MODIFY - WHEEL PHYSICS CONSTRAINT CREATION LOCK]
        // 🛑 KESİNLİKLE DEĞİŞTİRİLEMEZ: TEKERLEK EKLEM KISITLAMASI (CONSTRAINT) BLOĞU!
        // Tekerlekler sadece şasiye (VisualMeshComponents[0]) 1-DOF Roll olarak bağlanır.
        // PivotPoint ofseti VisualSections[i].PivotPoint - VisualSections[0].PivotPoint olarak
        // tam vertex-centroid merkezine kilitlidir. Bu hiyerarşiyi ve ofseti ASLA DEĞİŞTİRMEYİN!
        // =========================================================================================
        // 4) SADECE Tekerlekler için şasi ile fiziksel eklem (Constraint) bağla
        if (i > 0 && bIsWheel && VisualMeshComponents.IsValidIndex(0) && VisualMeshComponents[0])
        {
            FName ConstraintName = *FString::Printf(TEXT("PhysicsJoint_%d_%s"), i, *VisualSections[i].MeshName);
            UPhysicsConstraintComponent* Constraint = NewObject<UPhysicsConstraintComponent>(this, ConstraintName);

            Constraint->SetupAttachment(VisualMeshComponents[0]);
            FVector WheelRelLoc = VisualSections[i].PivotPoint - VisualSections[0].PivotPoint;
            Constraint->SetRelativeLocation(WheelRelLoc);

            // Tüm tekerlekler için 1-DOF Roll kuralı:
            ConfigureConstraintDof(Constraint, i);
            Constraint->RegisterComponent();
            Constraint->SetConstrainedComponents(VisualMeshComponents[0], NAME_None, VisComp, NAME_None);

            // Eğer bu tekerlek bir Steer kemiğine bağlıysa, referans yönelimini Steer kemiğine eşitle
            if (FString* SteerBoneName = WheelToSteerBoneMap.Find(i))
            {
                FTransform InitialSteerPose(FRotator::ZeroRotator, WheelRelLoc);
                Constraint->SetConstraintReferenceFrame(EConstraintFrame::Frame1, InitialSteerPose);
                UE_LOG(LogTemp, Warning, TEXT("🔗 [DİREKSİYON BAĞLANTISI] '%s' tekerleği '%s' steer mafsalına kilitlendi! (1-DOF Roll)"),
                    *VisualSections[i].MeshName, **SteerBoneName);
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("🔗 [STANDART SÜRÜŞ TEKERLEĞİ] '%s' tekerleği doğrudan şasiye bağlandı. (1-DOF Roll)"),
                    *VisualSections[i].MeshName);
            }

            JointConstraints.Add(Constraint);
            SectionConstraintMap.Add(i, Constraint);
        }
        // 🔒 [END OF WHEEL CONSTRAINT CREATION LOCK]
        // =========================================================================================
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

int32 APiSimModelImporter::FindVisualSectionForBone(int32 BoneIndex)
{
    if (BoneIndex <= 0 || !ConfiguredMotors.IsValidIndex(BoneIndex)) return INDEX_NONE;
    if (VisualSections.IsValidIndex(BoneIndex) && VisualMeshComponents.IsValidIndex(BoneIndex))
    {
        return BoneIndex;
    }

    // Saf Kemik (Pure Bone - Örn: M_Steer_FR, M_Steer_FL)
    FString PureName = ConfiguredMotors[BoneIndex].BoneName;

    // 1) Doğrudan WheelToSteerBoneMap haritasından ara
    for (const auto& Pair : WheelToSteerBoneMap)
    {
        if (Pair.Value.Equals(PureName, ESearchCase::IgnoreCase))
        {
            return Pair.Key;
        }
    }

    FString Clean = PureName;
    Clean.RemoveFromStart(TEXT("M_Steer_"), ESearchCase::IgnoreCase);
    Clean.RemoveFromStart(TEXT("Steer_"), ESearchCase::IgnoreCase);
    Clean.RemoveFromStart(TEXT("M_"), ESearchCase::IgnoreCase);

    // İlgili tekerleği (FR -> M_Wheel_FR) eşleştir
    for (int32 i = 1; i < VisualSections.Num(); ++i)
    {
        FString VName = VisualSections[i].MeshName;
        if (VName.Contains(Clean, ESearchCase::IgnoreCase) && 
            (VName.Contains(TEXT("Wheel"), ESearchCase::IgnoreCase) || VName.StartsWith(TEXT("W_"))))
        {
            return i;
        }
    }

    for (int32 i = 1; i < VisualSections.Num(); ++i)
    {
        if (VisualSections[i].MeshName.Contains(Clean, ESearchCase::IgnoreCase))
        {
            return i;
        }
    }

    return INDEX_NONE;
}

UPhysicsConstraintComponent* APiSimModelImporter::FindConstraintForSection(int32 SectionIndex)
{
    if (SectionIndex <= 0 || !VisualSections.IsValidIndex(SectionIndex)) return nullptr;

    if (UPhysicsConstraintComponent** Found = SectionConstraintMap.Find(SectionIndex))
    {
        if (Found && *Found) return *Found;
    }

    FString TargetPrefix = FString::Printf(TEXT("PhysicsJoint_%d_"), SectionIndex);
    for (UPhysicsConstraintComponent* Constraint : JointConstraints)
    {
        if (Constraint && Constraint->GetName().StartsWith(TargetPrefix))
        {
            return Constraint;
        }
    }

    // Kesinlikle SectionIndex - 1 fallback'i YAPMA!
    // Tekerlek olmayan parçalar için (örn. Elevon) rastgele tekerlek kısıtlaması dönmek
    // tekerleklerin elevon pivotuna yapışmasına (snap) neden oluyordu!
    return nullptr;
}

// =========================================================================================
// 🔒 [DO NOT MODIFY - WHEEL CONSTRAINT 1-DOF DEGREE OF FREEDOM LOCK]
// 🛑 KESİNLİKLE DEĞİŞTİRİLEMEZ: TEKERLEK SERBESTLİK DERECESİ (DOF) AYARLARI!
// Tekerlekler 3-DOF Linear Locked + Angular Twist (Roll) Free + Swing1/2 Locked standardındadır.
// Bu ayarlar aracın zeminde düzgün yuvarlanması, patlamaması ve devrilmemesi için sabittir!
// =========================================================================================
void APiSimModelImporter::ConfigureConstraintDof(UPhysicsConstraintComponent* Constraint, int32 SectionIndex)
{
    if (!Constraint || !VisualSections.IsValidIndex(SectionIndex)) return;

    // 1) Doğrusal Hareket (Linear Motion) - Tekerlek şasiden kopamaz
    Constraint->SetLinearXLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);
    Constraint->SetLinearYLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);
    Constraint->SetLinearZLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);
    Constraint->SetDisableCollision(true); // Şasi ile tekerlek birbirini itmesin

    // 2) Açısal Hareket (Angular Motion) - GENEL GEÇER TEKERLEK STANDARDI:
    // TÜM TEKERLEKLER (Ön veya Arka ayrımı olmaksızın) TEK BİR EKSENDE (Roll / Twist) DÖNER!
    // Twist (Roll / Yuvarlanma): ACM_Free (Tekerlek zeminde ileri/geri serbestçe döner)
    // Swing1 (Yaw / Direksiyon): ACM_Locked (Direksiyon ekseni kilitli - Steer mafsalının referans açısıyla yönlendirilir)
    // Swing2 (Pitch / Kamber): ACM_Locked (Kamber açısı kilitli - devrilmez)
    Constraint->SetAngularTwistLimit(EAngularConstraintMotion::ACM_Free, 0.0f);
    Constraint->SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Locked, 0.0f);
    Constraint->SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Locked, 0.0f);
    Constraint->SetOrientationDriveTwistAndSwing(false, false);
}
// 🔒 [END OF WHEEL CONSTRAINT DOF LOCK]
// =========================================================================================

void APiSimModelImporter::UpdateAllConstraintDofs()
{
    for (int32 i = 1; i < VisualSections.Num(); ++i)
    {
        UPhysicsConstraintComponent* Constraint = FindConstraintForSection(i);
        if (Constraint)
        {
            ConfigureConstraintDof(Constraint, i);
        }
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

    int32 TargetSection = FindVisualSectionForBone(BoneIndex);

    EPiSimMotorRole MotorRole = ConfiguredMotors[BoneIndex].Role;
    if (MotorRole == EPiSimMotorRole::DriveWheel || MotorRole == EPiSimMotorRole::Thruster || MotorRole == EPiSimMotorRole::TrackPad)
    {
        if (TargetSection > 0 && VisualMeshComponents.IsValidIndex(TargetSection) && VisualMeshComponents[TargetSection])
        {
            float IndividualRpm = Value * ConfiguredMotors[BoneIndex].MaxVelocityRPM;
            if (bIsPhysicsSimulating)
            {
                float AngularSpeedDegPerSec = IndividualRpm * 6.0f;
                FVector LocalAxle = FVector(1.0f, 0.0f, 0.0f);
                FVector WorldAxle = VisualMeshComponents[TargetSection]->GetComponentTransform().TransformVectorNoScale(LocalAxle).GetSafeNormal();
                FVector CurrentAngVel = VisualMeshComponents[TargetSection]->GetPhysicsAngularVelocityInDegrees();
                FVector NonRollVel = CurrentAngVel - (WorldAxle * (CurrentAngVel | WorldAxle));
                FVector DesiredRollVel = WorldAxle * AngularSpeedDegPerSec;
                VisualMeshComponents[TargetSection]->SetPhysicsAngularVelocityInDegrees(NonRollVel + DesiredRollVel, false);
            }
            else
            {
                VisualMeshComponents[TargetSection]->AddLocalRotation(FRotator(Value * 15.0f, 0.0f, 0.0f));
            }
        }
    }
    else if (MotorRole == EPiSimMotorRole::SteeredWheel || MotorRole == EPiSimMotorRole::ServoJoint)
    {
        float TargetAngle = FMath::Lerp(ConfiguredMotors[BoneIndex].MinLimitDeg, ConfiguredMotors[BoneIndex].MaxLimitDeg, (Value + 1.0f) * 0.5f);
        
        bool bIsSteerJoint = SteerBoneComponents.Contains(ConfiguredMotors[BoneIndex].BoneName) || (MotorRole == EPiSimMotorRole::SteeredWheel);

        // 1) Steer kemiğini 1 eksende (Yaw) döndür (Fizik simüle etmez, sadece biz döndürürüz)
        if (USceneComponent** FoundSteer = SteerBoneComponents.Find(ConfiguredMotors[BoneIndex].BoneName))
        {
            (*FoundSteer)->SetRelativeRotation(FRotator(0.0f, TargetAngle, 0.0f));
        }

        // 2) SADECE gerçek direksiyon mafsalı ise tekerlek kısıtlamasını yönlendir
        if (bIsSteerJoint && TargetSection > 0 && VisualSections.IsValidIndex(TargetSection) && VisualSections.IsValidIndex(0))
        {
            if (bIsPhysicsSimulating)
            {
                UPhysicsConstraintComponent* Constraint = FindConstraintForSection(TargetSection);
                if (Constraint)
                {
                    FVector RelLoc = VisualSections[TargetSection].PivotPoint - VisualSections[0].PivotPoint;
                    FTransform SteerPose(FRotator(0.0f, TargetAngle, 0.0f), RelLoc);
                    Constraint->SetConstraintReferenceFrame(EConstraintFrame::Frame1, SteerPose);
                }
            }
            else if (VisualMeshComponents.IsValidIndex(TargetSection) && VisualMeshComponents[TargetSection])
            {
                // Statik modda görsel güncelleme (Tekerlek zaten Steer kemiğine bağlıdır)
                VisualMeshComponents[TargetSection]->SetRelativeRotation(FRotator(0.0f, TargetAngle, 0.0f));
            }
        }
        else if (!bIsSteerJoint && TargetSection > 0 && VisualMeshComponents.IsValidIndex(TargetSection) && VisualMeshComponents[TargetSection])
        {
            // Aero kontrol yüzeyi (Elevon / Aileron) doğrudan menteşe ekseninde sapar
            FQuat HingeQuat(FVector(1.0f, 0.0f, 0.0f), FMath::DegreesToRadians(TargetAngle));
            VisualMeshComponents[TargetSection]->SetRelativeRotation(FRotator(HingeQuat));
        }
    }
    else if (MotorRole == EPiSimMotorRole::LinearActuator)
    {
        float Stroke = Value * ConfiguredMotors[BoneIndex].MaxLimitDeg; // cm
        if (TargetSection > 0 && VisualSections.IsValidIndex(TargetSection) && VisualSections.IsValidIndex(0) && VisualMeshComponents.IsValidIndex(TargetSection) && VisualMeshComponents[TargetSection])
        {
            FVector BaseLoc = VisualSections[TargetSection].PivotPoint - VisualSections[0].PivotPoint;
            VisualMeshComponents[TargetSection]->SetRelativeLocation(BaseLoc + FVector(Stroke, 0.0f, 0.0f));
        }
    }
}

void APiSimModelImporter::RemoveMotorFromBone(int32 BoneIndex)
{
    if (BoneIndex <= 0 || !ConfiguredMotors.IsValidIndex(BoneIndex)) return;
    ConfiguredMotors[BoneIndex].Role = EPiSimMotorRole::None;
    ConfiguredMotors[BoneIndex].CurrentTestValue = 0.0f;
    UpdateAllConstraintDofs();
    UpdateVisualMaterials();
}

void APiSimModelImporter::AssignMotorToBone(int32 BoneIndex, EPiSimMotorRole NewRole)
{
    if (BoneIndex <= 0 || !ConfiguredMotors.IsValidIndex(BoneIndex)) return; // Gövdeye motor atanamaz
    ConfiguredMotors[BoneIndex].Role = NewRole;
    UpdateAllConstraintDofs();
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

void APiSimModelImporter::ApplyCenterOfMass()
{
    if (!VisualMeshComponents.IsValidIndex(0) || !VisualMeshComponents[0]) return;

    UPrimitiveComponent* VisComp = VisualMeshComponents[0];
    FBodyInstance* BI = VisComp->GetBodyInstance();
    if (!BI) return;

    // Gövde eksenleri: Chassis kemik yönü FBX'ten (0, -1, 0) burun yönünü verir
    FVector LocalForward = (VisualSections.IsValidIndex(0) && !VisualSections[0].BoneDirection.IsNearlyZero())
        ? VisualSections[0].BoneDirection.GetSafeNormal()
        : FVector(0.0f, -1.0f, 0.0f);
    FVector ForwardDirWorld = VisComp->GetComponentTransform().TransformVectorNoScale(LocalForward).GetSafeNormal();
    if (ForwardDirWorld.IsNearlyZero()) ForwardDirWorld = VisComp->GetForwardVector();

    // Kullanıcının belirlediği fiziksel ağırlık merkezi (CoG): Şasi kökünden burun (+LocalForward) yönünde CoGForwardCm kadar ileri
    FVector DesiredCoGWorld = VisComp->GetComponentLocation() + (ForwardDirWorld * AeroConfig.CoGForwardCm);

    // Chaos motorunun hesapladığı saf geometrik merkezi bulmak için mevcut COMNudge'ı hesaptan düş
    FVector CurrentCoMWorld = VisComp->GetCenterOfMass();
    FVector CurrentNudgeWorld = VisComp->GetComponentTransform().TransformVector(BI->COMNudge);
    FVector RawGeometricCoMWorld = CurrentCoMWorld - CurrentNudgeWorld;

    // İstenen CoG noktasına ulaşmak için gereken ofseti hesapla ve yerel bileşen eksenine dönüştür:
    FVector RequiredWorldOffset = DesiredCoGWorld - RawGeometricCoMWorld;
    FVector LocalNudge = VisComp->GetComponentTransform().InverseTransformVector(RequiredWorldOffset);

    BI->COMNudge = LocalNudge;
    BI->UpdateMassProperties();

    UE_LOG(LogTemp, Warning, TEXT("⚖️ [FİZİK CoG UYGULANDI] Chaos CoM -> İleri: %.1f cm (CoGForwardCm) | COMNudge: (%.1f, %.1f, %.1f)"),
        AeroConfig.CoGForwardCm, LocalNudge.X, LocalNudge.Y, LocalNudge.Z);
}

void APiSimModelImporter::SetCoGToBoneEnd()
{
    float BoneLen = AeroConfig.ChassisBoneLengthCm;
    if (BoneLen <= 1.0f && VisualSections.IsValidIndex(0) && VisualSections[0].BoneLength > 1.0f)
    {
        BoneLen = VisualSections[0].BoneLength;
    }
    AeroConfig.CoGForwardCm = BoneLen;
    if (VisualSections.IsValidIndex(0))
    {
        VisualSections[0].CoGForwardCm = BoneLen;
    }
    ApplyCenterOfMass();
}

void APiSimModelImporter::SetCoGToCenterOfMass()
{
    if (VisualMeshComponents.IsValidIndex(0) && VisualMeshComponents[0])
    {
        UPrimitiveComponent* VisComp = VisualMeshComponents[0];
        FBodyInstance* BI = VisComp->GetBodyInstance();
        if (BI)
        {
            BI->COMNudge = FVector::ZeroVector;
            BI->UpdateMassProperties();
        }

        FVector ChassisRoot = VisComp->GetComponentLocation();
        FVector CoMLoc = VisComp->GetCenterOfMass();
        FVector Diff = CoMLoc - ChassisRoot;

        FVector LocalForward = (VisualSections.IsValidIndex(0) && !VisualSections[0].BoneDirection.IsNearlyZero())
            ? VisualSections[0].BoneDirection.GetSafeNormal()
            : FVector(0.0f, -1.0f, 0.0f);
        FVector ForwardDirWorld = VisComp->GetComponentTransform().TransformVectorNoScale(LocalForward).GetSafeNormal();
        if (ForwardDirWorld.IsNearlyZero()) ForwardDirWorld = VisComp->GetForwardVector();

        float ForwardProj = Diff | ForwardDirWorld;
        AeroConfig.CoGForwardCm = ForwardProj;
        if (VisualSections.IsValidIndex(0))
        {
            VisualSections[0].CoGForwardCm = ForwardProj;
        }
    }
}

void APiSimModelImporter::ToggleAeroGizmos()
{
    AeroConfig.bShowAeroGizmos = !AeroConfig.bShowAeroGizmos;
}

