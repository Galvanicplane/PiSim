// Fill out your copyright notice in the Description page of Project Settings.

#include "UDPVideoStreamer.h"
#include "UDPFunctionLibrary.h"
#include "Engine/TextureRenderTarget2D.h"
#include "TextureResource.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "TimerManager.h"
#include "Engine/World.h"

UUDPVideoStreamer::UUDPVideoStreamer()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UUDPVideoStreamer::BeginPlay()
{
    Super::BeginPlay();
    if (bAutoStart)
    {
        StartStreaming();
    }
}

void UUDPVideoStreamer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopStreaming();
    Super::EndPlay(EndPlayReason);
}

void UUDPVideoStreamer::StartStreaming()
{
    if (bIsStreaming)
    {
        return;
    }

    bIsStreaming = true;
    float Interval = (FrameRate > 0.0f) ? (1.0f / FrameRate) : 0.033f;

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(StreamTimerHandle, this, &UUDPVideoStreamer::OnTimerTick, Interval, true);
    }
}

void UUDPVideoStreamer::StopStreaming()
{
    bIsStreaming = false;
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(StreamTimerHandle);
    }
}

void UUDPVideoStreamer::OnTimerTick()
{
    CaptureAndSendFrame();
}

bool UUDPVideoStreamer::CaptureAndSendFrame()
{
    if (!RenderTarget)
    {
        return false;
    }

    FTextureRenderTargetResource* RenderTargetResource = RenderTarget->GameThread_GetRenderTargetResource();
    if (!RenderTargetResource)
    {
        return false;
    }

    TArray<FColor> RawPixels;
    if (!RenderTargetResource->ReadPixels(RawPixels))
    {
        return false;
    }

    int32 Width = RenderTarget->SizeX;
    int32 Height = RenderTarget->SizeY;
    if (Width <= 0 || Height <= 0 || RawPixels.Num() == 0)
    {
        return false;
    }

    // Compress raw BGRA pixels to JPEG using IImageWrapper API
    IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
    TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);
    if (!ImageWrapper.IsValid())
    {
        return false;
    }

    if (!ImageWrapper->SetRaw(RawPixels.GetData(), RawPixels.Num() * sizeof(FColor), Width, Height, ERGBFormat::BGRA, 8))
    {
        return false;
    }

    TArray64<uint8> CompressedJpeg64 = ImageWrapper->GetCompressed(JpegQuality);
    TArray<uint8> CompressedJpeg;
    CompressedJpeg.Append(CompressedJpeg64.GetData(), CompressedJpeg64.Num());

    if (CompressedJpeg.Num() == 0 || CompressedJpeg.Num() > 65507)
    {
        // Packet too large for single UDP datagram or compression failed
        return false;
    }

    return UUDPFunctionLibrary::SendUDPBytes(TargetIP, TargetPort, CompressedJpeg);
}
