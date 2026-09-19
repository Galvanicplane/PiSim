// CameraSensor.h
// Sanal Kamera Sensörü: RenderTarget üzerinden sahneyi yakalar, JPEG sıkıştırması yapar ve TelemetryGateway'e besler.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "IPiSimInspectableModule.h"
#include "CameraSensor.generated.h"

class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UPiSimTelemetryGateway;

UCLASS(ClassGroup=(PiSim), meta=(BlueprintSpawnableComponent))
class PISIM_API UPiSimCameraSensor : public UActorComponent, public IPiSimInspectableModule
{
    GENERATED_BODY()

public:
    UPiSimCameraSensor();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // IPiSimInspectableModule
    virtual void GetInspectableProperties(TArray<FPiSimPropertyDescriptor>& OutProperties) override;
    virtual void SetInspectablePropertyFloat(FName PropertyId, float NewValue) override;
    virtual void SetInspectablePropertyString(FName PropertyId, const FString& NewValue) override;
    virtual void TriggerInspectableAction(FName ActionId) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Camera")
    FName TargetBoneName = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Camera")
    int32 ResolutionWidth = 640;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Camera")
    int32 ResolutionHeight = 480;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Camera")
    float FOVDegrees = 90.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Camera")
    int32 JpegQuality = 75;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Camera")
    float CaptureFps = 30.0f;

protected:
    UPROPERTY()
    USceneCaptureComponent2D* SceneCapture = nullptr;

    UPROPERTY()
    UTextureRenderTarget2D* RenderTarget = nullptr;

    UPROPERTY()
    UPiSimTelemetryGateway* CachedGateway = nullptr;

    float TimeSinceLastCapture = 0.0f;

    void CaptureAndCompressFrame();
};
