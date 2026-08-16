// PiSimPrimitiveCube.cpp
#include "PiSimPrimitiveCube.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/Engine.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "TextureResource.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

APiSimPrimitiveCube::APiSimPrimitiveCube()
{
    PrimaryActorTick.bCanEverTick = true;

    // Create Cube mesh component
    CubeMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CubeMeshComponent"));
    RootComponent = CubeMeshComponent;

    // Set default basic shape cube mesh
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMeshAsset.Succeeded())
    {
        CubeMeshComponent->SetStaticMesh(CubeMeshAsset.Object);
    }

    // Enable Physics & Mobility
    CubeMeshComponent->SetMobility(EComponentMobility::Movable);
    CubeMeshComponent->SetSimulatePhysics(true);
    CubeMeshComponent->SetEnableGravity(true);
    CubeMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    CubeMeshComponent->SetCollisionProfileName(TEXT("PhysicsActor"));

    // Attach FPV Camera Capture Component
    CameraCaptureComponent = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("CameraCaptureComponent"));
    CameraCaptureComponent->SetupAttachment(RootComponent);
    CameraCaptureComponent->SetRelativeLocation(FVector(60.0f, 0.0f, 10.0f));
    CameraCaptureComponent->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));
    CameraCaptureComponent->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
    CameraCaptureComponent->bCaptureEveryFrame = true;
    CameraCaptureComponent->bCaptureOnMovement = false;
}

void APiSimPrimitiveCube::BeginPlay()
{
    Super::BeginPlay();

    // Enforce Raspberry Pi 5 IP if default or empty
    if (BridgeTargetIP.IsEmpty() || BridgeTargetIP == TEXT("127.0.0.1"))
    {
        BridgeTargetIP = TEXT("192.168.1.20");
    }

    // Fully autonomous C++ Render Target creation with sRGB TargetGamma
    if (!VideoRenderTarget)
    {
        VideoRenderTarget = NewObject<UTextureRenderTarget2D>(this);
        VideoRenderTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
        VideoRenderTarget->ClearColor = FLinearColor::Black;
        VideoRenderTarget->TargetGamma = 0.3f; // 1.2f, 2.2f;
        VideoRenderTarget->bAutoGenerateMips = false;
        VideoRenderTarget->InitCustomFormat(320, 240, PF_B8G8R8A8, false);
        VideoRenderTarget->UpdateResourceImmediate(true);
    }

    if (CameraCaptureComponent)
    {
        CameraCaptureComponent->TextureTarget = VideoRenderTarget;
        CameraCaptureComponent->bCaptureEveryFrame = true;
    }

    // Initialize UDP Network Manager
    UDPManager = MakeUnique<FPiSimUDPManager>();

    // Bind packet delegate
    UDPManager->OnControlPacketReceived.AddUObject(this, &APiSimPrimitiveCube::OnControlPacketReceived);

    // Start Control listener on UDP Port 7400
    UDPManager->StartControlListener(ControlPort);

    // Reserve FPV Video stream socket on UDP Port 5000
    UDPManager->ReserveVideoSocket(VideoPort);
}

void APiSimPrimitiveCube::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UDPManager)
    {
        UDPManager->OnControlPacketReceived.RemoveAll(this);
        UDPManager->Shutdown();
        UDPManager.Reset();
    }

    Super::EndPlay(EndPlayReason);
}

void APiSimPrimitiveCube::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (CubeMeshComponent && CubeMeshComponent->IsSimulatingPhysics())
    {
        // Convert target velocities from Local Robot Space to World Space
        FVector PhysicsVel = CubeMeshComponent->GetPhysicsLinearVelocity();
        FVector WorldLinearVelUE5 = GetActorTransform().TransformVector(CurrentTargetLinearVelocityUE5);
        WorldLinearVelUE5.Z = PhysicsVel.Z;

        CubeMeshComponent->SetPhysicsLinearVelocity(WorldLinearVelUE5);

        FVector WorldAngularVelUE5 = GetActorTransform().TransformVector(CurrentTargetAngularVelocityUE5);
        CubeMeshComponent->SetPhysicsAngularVelocityInDegrees(WorldAngularVelUE5);
    }

    // Publish IMU telemetry back to Python bridge
    PublishImuTelemetry(DeltaTime);

    // Handle FPV video frame streaming
    if (bEnableVideoStream && VideoFrameRate > 0.0f)
    {
        VideoTimer += DeltaTime;
        float Interval = 1.0f / VideoFrameRate;
        if (VideoTimer >= Interval)
        {
            VideoTimer = 0.0f;
            CaptureAndSendVideoFrame();
        }
    }
}

void APiSimPrimitiveCube::OnControlPacketReceived(const TArray<uint8>& PacketData, const FString& SenderIP)
{
    FROSTwistMessage TwistMsg;
    if (FROSTwistMessage::FromBinary(PacketData, TwistMsg))
    {
        ProcessTwistCommand(TwistMsg);
    }
}

void APiSimPrimitiveCube::ProcessTwistCommand(const FROSTwistMessage& TwistMsg)
{
    CurrentTargetLinearVelocityUE5 = FROS2UE5Converter::ROS2ToUE5LinearVelocity(TwistMsg.Linear);
    CurrentTargetAngularVelocityUE5 = FROS2UE5Converter::ROS2ToUE5AngularVelocity(TwistMsg.Angular);
}

void APiSimPrimitiveCube::PublishImuTelemetry(float DeltaTime)
{
    if (!UDPManager || DeltaTime <= 0.0f)
    {
        return;
    }

    FVector CurrentLinearVelUE5 = CubeMeshComponent ? CubeMeshComponent->GetPhysicsLinearVelocity() : FVector::ZeroVector;
    FVector LinearAccelUE5 = (CurrentLinearVelUE5 - PreviousLinearVelocityUE5) / DeltaTime;
    PreviousLinearVelocityUE5 = CurrentLinearVelUE5;

    FVector AngularVelUE5 = CubeMeshComponent ? CubeMeshComponent->GetPhysicsAngularVelocityInDegrees() : FVector::ZeroVector;
    FQuat OrientationUE5 = GetActorTransform().GetRotation();

    FROSImuMessage ImuMsg;
    ImuMsg.LinearAcceleration = FROS2UE5Converter::UE5ToROS2LinearAcceleration(LinearAccelUE5);
    ImuMsg.AngularVelocity = FROS2UE5Converter::UE5ToROS2AngularVelocity(AngularVelUE5);

    FROS2UE5Converter::UE5ToROS2Quaternion(
        OrientationUE5,
        ImuMsg.Orientation.X,
        ImuMsg.Orientation.Y,
        ImuMsg.Orientation.Z,
        ImuMsg.Orientation.W
    );

    TArray<uint8> ImuBytes;
    if (ImuMsg.ToBinary(ImuBytes))
    {
        UDPManager->SendControlData(ImuBytes, BridgeTargetIP, FPiSimUDPManager::DEFAULT_TELEMETRY_PORT);
    }
}

void APiSimPrimitiveCube::CaptureAndSendVideoFrame()
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
    // SetLinearToGamma(false) to prevent double-gamma over-exposure / heavenly glow
    FReadSurfaceDataFlags ReadPixelFlags(RCM_UNorm);
    ReadPixelFlags.SetLinearToGamma(false);

    if (!Resource->ReadPixels(RawPixels, ReadPixelFlags) || RawPixels.Num() == 0)
    {
        return;
    }

    // Force opaque alpha channel on all pixels
    for (FColor& Pixel : RawPixels)
    {
        Pixel.A = 255;
    }

    // Compress raw BGRA pixels to JPEG using IImageWrapper module
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

    // Save last sent frame to disk for visual verification
    FString SavedFramePath = FPaths::ProjectSavedDir() / TEXT("Robots/Cache/last_sent_frame.jpg");
    FFileHelper::SaveArrayToFile(CompressedJpeg, *SavedFramePath);

    // Static frame sequence counter
    static uint16 FrameSequence = 0;
    FrameSequence++;

    // Split JPEG into MTU-safe chunks of 1000 bytes with 4-byte header [FrameSeq (uint16), ChunkIdx (uint8), TotalChunks (uint8)]
    const int32 MAX_CHUNK_SIZE = 1000;
    int32 TotalBytes = CompressedJpeg.Num();
    uint8 TotalChunks = static_cast<uint8>((TotalBytes + MAX_CHUNK_SIZE - 1) / MAX_CHUNK_SIZE);

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

        UDPManager->SendVideoData(Packet, BridgeTargetIP, VideoPort);
    }
}
