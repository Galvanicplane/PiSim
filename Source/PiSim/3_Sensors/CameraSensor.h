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
    int32 ResolutionWidth = 480;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Camera")
    int32 ResolutionHeight = 270;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Camera")
    float FOVDegrees = 90.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Camera")
    int32 JpegQuality = 55;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Camera")
    float CaptureFps = 15.0f;

    /** Kameranın araç merkezine göre yerel konumu (cm): X=İleri, Y=Sağ, Z=Yukarı */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Camera")
    FVector CameraLocalOffset = FVector(110.0f, 0.0f, 95.0f);

    /** Kameranın aşağı/yukarı bakış açısı (Derece, eksi değer hafif aşağı bakar) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Camera")
    float CameraPitchDeg = -8.0f;

    /** Aracın 3B sınır kutusuna (Bounding Box) göre kamerayı otomatik olarak ön-üst dışına çıkar */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Camera")
    bool bAutoClearVehicleBounds = true;

    /** Aracın üzerindeki kameranın anlık Dünya (World) Konum ve Rotasyonunu hesaplar (FPV ve RenderTarget için) */
    bool GetCameraWorldTransform(FVector& OutLocation, FRotator& OutRotation) const;

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
