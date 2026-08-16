// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "UDPVideoStreamer.generated.h"

/**
 * Actor Component to stream live camera frames (from a TextureRenderTarget2D) over UDP to Raspberry Pi 5.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PISIM_API UUDPVideoStreamer : public UActorComponent
{
    GENERATED_BODY()

public:
    UUDPVideoStreamer();

    /** Target IP address (e.g. Raspberry Pi 5 IP address or 127.0.0.1 for local testing) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Video Stream")
    FString TargetIP = TEXT("127.0.0.1");

    /** Target UDP port dedicated for video streaming (Default: 5006) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Video Stream")
    int32 TargetPort = 5006;

    /** Streaming Frame Rate (FPS), default 30.0 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Video Stream")
    float FrameRate = 30.0f;

    /** JPEG compression quality (1 to 100), default 60 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Video Stream", meta = (ClampMin = "1", ClampMax = "100"))
    int32 JpegQuality = 60;

    /** Texture Render Target 2D attached to SceneCaptureComponent2D to capture frames from */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Video Stream")
    UTextureRenderTarget2D* RenderTarget = nullptr;

    /** Start streaming frames automatically on BeginPlay */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Video Stream")
    bool bAutoStart = true;

    /** Start sending live camera stream */
    UFUNCTION(BlueprintCallable, Category = "UDP Video Stream")
    void StartStreaming();

    /** Stop sending live camera stream */
    UFUNCTION(BlueprintCallable, Category = "UDP Video Stream")
    void StopStreaming();

    /** Capture a single frame from the RenderTarget and send it via UDP */
    UFUNCTION(BlueprintCallable, Category = "UDP Video Stream")
    bool CaptureAndSendFrame();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void OnTimerTick();
    FTimerHandle StreamTimerHandle;
    bool bIsStreaming = false;
};
