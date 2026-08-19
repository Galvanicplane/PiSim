// PiSimPrimitiveCube.h
// Primitive Physics Cube Actor driven by ROS 2 Twist velocity commands, transmitting IMU telemetry and FPV video stream.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "PiSimUDPManager.h"
#include "ROS2MessageTypes.h"
#include "ROS2UE5Converter.h"
#include "PiSimPrimitiveCube.generated.h"

UCLASS(NotBlueprintable, NotBlueprintType, Placeable)
class PISIM_API APiSimPrimitiveCube : public AActor
{
    GENERATED_BODY()
    
public:    
    APiSimPrimitiveCube();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:    
    virtual void Tick(float DeltaTime) override;

    /** Static Mesh Component representing the primitive physics cube */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Components")
    UStaticMeshComponent* CubeMeshComponent;

    /** Scene Capture 2D Component for FPV Camera Stream */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Components")
    USceneCaptureComponent2D* CameraCaptureComponent;

    /** Render Target 2D for live FPV video capture */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Video")
    UTextureRenderTarget2D* VideoRenderTarget = nullptr;

    /** Toggle between Native ROS 2 CDR Protocol (True) and Legacy UDP Bridge Mode (False) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Protocol")
    bool bUseNativeROS2Mode = true;

    /** Enable live camera stream transmission */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Video")
    bool bEnableVideoStream = true;

    /** FPV Video frame rate (FPS), default 20.0 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Video")
    float VideoFrameRate = 20.0f;

    /** JPEG Compression Quality (1 - 100) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Video", meta = (ClampMin = "1", ClampMax = "100"))
    int32 VideoJpegQuality = 50;

    /** UDP Control Port (Default: 7400) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Network")
    int32 ControlPort = 7400;

    /** UDP FPV Video Port (Default: 5000) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Network")
    int32 VideoPort = 5000;

    /** Target Python Bridge IP address (Default: 192.168.1.20) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Network")
    FString BridgeTargetIP = TEXT("192.168.1.20");

    /** Print debug log messages */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Debug")
    bool bEnableDebugScreenLogs = false;

    /** Current target linear velocity in UE5 space (cm/s) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|State")
    FVector CurrentTargetLinearVelocityUE5 = FVector::ZeroVector;

    /** Current target angular velocity in UE5 space (deg/s) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|State")
    FVector CurrentTargetAngularVelocityUE5 = FVector::ZeroVector;

private:
    TUniquePtr<FPiSimUDPManager> UDPManager;

    FVector PreviousLinearVelocityUE5 = FVector::ZeroVector;
    float VideoTimer = 0.0f;

    void OnControlPacketReceived(const TArray<uint8>& PacketData, const FString& SenderIP);
    void ProcessTwistCommand(const FROSTwistMessage& TwistMsg);
    void PublishImuTelemetry(float DeltaTime);
    void CaptureAndSendVideoFrame();
};
