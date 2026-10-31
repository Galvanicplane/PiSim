// CameraSensor.cpp

#include "CameraSensor.h"
#include "TelemetryGateway.h"
#include "Vehicle.h"
#include "Pi5Bridge.h"
#include "ProceduralMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"
#include "Modules/ModuleManager.h"
#include "GameFramework/Actor.h"

// @state: WIP - Kamera sensörü yapıcısı
UPiSimCameraSensor::UPiSimCameraSensor()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

// @state: WIP - Kamera render target ve sahne yakalama bileşenini kurar
void UPiSimCameraSensor::BeginPlay()
{
    Super::BeginPlay();

    if (GetOwner())
    {
        CachedGateway = GetOwner()->FindComponentByClass<UPiSimTelemetryGateway>();

        SceneCapture = NewObject<USceneCaptureComponent2D>(GetOwner());
        if (SceneCapture)
        {
            SceneCapture->RegisterComponent();
            SceneCapture->AttachToComponent(GetOwner()->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform, TargetBoneName);

            RenderTarget = NewObject<UTextureRenderTarget2D>(this);
            if (RenderTarget)
            {
                RenderTarget->InitAutoFormat(ResolutionWidth, ResolutionHeight);
                RenderTarget->UpdateResource();
                SceneCapture->TextureTarget = RenderTarget;
                SceneCapture->FOVAngle = FOVDegrees;
                SceneCapture->bCaptureEveryFrame = false; // FPS kontrolü ile manuel tetiklenir
            }
        }
    }
}

// @state: WIP - İstenen FPS aralığında (sadece canlı video abonesi varken) kamera çerçevesi yakalar
void UPiSimCameraSensor::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // On-Demand Kuralı: Abone yoksa sahne yakalama ve ReadPixels yapma (GPU/CPU tasarrufu)
    APiSimVehicle* OwnerVehicle = Cast<APiSimVehicle>(GetOwner());
    if (OwnerVehicle)
    {
        bool bSubscribed = OwnerVehicle->HasVideoSubscribers() || (OwnerVehicle->Pi5Bridge && OwnerVehicle->Pi5Bridge->IsVideoSubscribed());
        if (!bSubscribed)
        {
            return;
        }
    }

    TimeSinceLastCapture += DeltaTime;
    float Interval = 1.0f / FMath::Max(1.0f, CaptureFps);

    if (TimeSinceLastCapture >= Interval)
    {
        TimeSinceLastCapture = 0.0f;
        CaptureAndCompressFrame();
    }
}

// @state: WIP - Render target verisini okuyup JPEG olarak sıkıştırır ve Gateway'e fırlatır
void UPiSimCameraSensor::CaptureAndCompressFrame()
{
    if (!SceneCapture || !RenderTarget || !CachedGateway)
    {
        return;
    }

    // Kamerayı aracın aktif fizik gövdesine (VisualMeshComponents[0] veya Actor) hizala (FPV Tavan/Ön açısı)
    APiSimVehicle* OwnerVehicle = Cast<APiSimVehicle>(GetOwner());
    if (OwnerVehicle)
    {
        FVector BaseLoc = OwnerVehicle->GetActorLocation();
        FRotator BaseRot = OwnerVehicle->GetActorRotation();
        if (OwnerVehicle->VisualMeshComponents.Num() > 0 && OwnerVehicle->VisualMeshComponents[0] && IsValid(OwnerVehicle->VisualMeshComponents[0]))
        {
            BaseLoc = OwnerVehicle->VisualMeshComponents[0]->GetComponentLocation();
            BaseRot = OwnerVehicle->VisualMeshComponents[0]->GetComponentRotation();
        }

        FVector FinalLocalOffset = CameraLocalOffset;
        if (const FTransform* CustomCamTransform = OwnerVehicle->SensorSocketTransforms.Find(TEXT("Camera")))
        {
            FinalLocalOffset = CustomCamTransform->GetLocation();
        }
        else if (bAutoClearVehicleBounds && OwnerVehicle->VisualMeshComponents.Num() > 0 && OwnerVehicle->VisualMeshComponents[0])
        {
            // Aracın gerçek 3B boyutunu (Bounding Box) ölç ve kameranın gövde içinde kalmasını engelle
            FBoxSphereBounds MeshBounds = OwnerVehicle->VisualMeshComponents[0]->CalcLocalBounds();
            float FrontEdgeX = MeshBounds.Origin.X + MeshBounds.BoxExtent.X + 15.0f; // Ön tamponun 15 cm ilerisi
            float TopEdgeZ   = MeshBounds.Origin.Z + MeshBounds.BoxExtent.Z + 25.0f; // Tavanın 25 cm üstü
            FinalLocalOffset.X = FMath::Max(CameraLocalOffset.X, FrontEdgeX);
            FinalLocalOffset.Z = FMath::Max(CameraLocalOffset.Z, TopEdgeZ);
        }

        FVector CamWorldLoc = BaseLoc + BaseRot.RotateVector(FinalLocalOffset);
        FRotator CamWorldRot = (BaseRot.Quaternion() * FRotator(CameraPitchDeg, 0.0f, 0.0f).Quaternion()).Rotator();
        SceneCapture->SetWorldLocationAndRotation(CamWorldLoc, CamWorldRot);
    }

    SceneCapture->CaptureScene();

    FTextureRenderTargetResource* Resource = RenderTarget->GameThread_GetRenderTargetResource();
    if (!Resource)
    {
        return;
    }

    TArray<FColor> RawPixels;
    if (Resource->ReadPixels(RawPixels))
    {
        IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
        TSharedPtr<IImageWrapper> JpegWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);

        if (JpegWrapper.IsValid() && JpegWrapper->SetRaw(RawPixels.GetData(), RawPixels.Num() * sizeof(FColor), ResolutionWidth, ResolutionHeight, ERGBFormat::BGRA, 8))
        {
            TArray64<uint8> CompressedBytes64 = JpegWrapper->GetCompressed(JpegQuality);
            TArray<uint8> CompressedBytes;
            CompressedBytes.Append(CompressedBytes64.GetData(), CompressedBytes64.Num());

            CachedGateway->FeedCameraFrame(CompressedBytes);
        }
    }
}

// @state: WIP - Otopark UI parametre listesini döner
void UPiSimCameraSensor::GetInspectableProperties(TArray<FPiSimPropertyDescriptor>& OutProperties)
{
    FPiSimPropertyDescriptor FovDesc;
    FovDesc.PropertyId = TEXT("FOVDegrees");
    FovDesc.DisplayName = TEXT("Kamera Görüş Açısı (FOV)");
    FovDesc.PropertyType = EPiSimPropertyType::Slider;
    FovDesc.MinValue = 30.0f;
    FovDesc.MaxValue = 140.0f;
    FovDesc.CurrentFloatValue = FOVDegrees;
    FovDesc.bContinuousTickUpdate = false;
    OutProperties.Add(FovDesc);

    FPiSimPropertyDescriptor FpsDesc;
    FpsDesc.PropertyId = TEXT("CaptureFps");
    FpsDesc.DisplayName = TEXT("Yayın Hızı (FPS)");
    FpsDesc.PropertyType = EPiSimPropertyType::Slider;
    FpsDesc.MinValue = 5.0f;
    FpsDesc.MaxValue = 60.0f;
    FpsDesc.CurrentFloatValue = CaptureFps;
    FpsDesc.bContinuousTickUpdate = false;
    OutProperties.Add(FpsDesc);

    FPiSimPropertyDescriptor QualityDesc;
    QualityDesc.PropertyId = TEXT("JpegQuality");
    QualityDesc.DisplayName = TEXT("JPEG Kalitesi (%)");
    QualityDesc.PropertyType = EPiSimPropertyType::Slider;
    QualityDesc.MinValue = 10.0f;
    QualityDesc.MaxValue = 100.0f;
    QualityDesc.CurrentFloatValue = (float)JpegQuality;
    QualityDesc.bContinuousTickUpdate = false;
    OutProperties.Add(QualityDesc);

    FPiSimPropertyDescriptor OffsetXDesc;
    OffsetXDesc.PropertyId = TEXT("CameraOffsetX");
    OffsetXDesc.DisplayName = TEXT("Kamera İleri Ofset X (cm)");
    OffsetXDesc.PropertyType = EPiSimPropertyType::Slider;
    OffsetXDesc.MinValue = -200.0f;
    OffsetXDesc.MaxValue = 300.0f;
    OffsetXDesc.CurrentFloatValue = CameraLocalOffset.X;
    OffsetXDesc.bContinuousTickUpdate = false;
    OutProperties.Add(OffsetXDesc);

    FPiSimPropertyDescriptor OffsetZDesc;
    OffsetZDesc.PropertyId = TEXT("CameraOffsetZ");
    OffsetZDesc.DisplayName = TEXT("Kamera Yükseklik Ofset Z (cm)");
    OffsetZDesc.PropertyType = EPiSimPropertyType::Slider;
    OffsetZDesc.MinValue = 0.0f;
    OffsetZDesc.MaxValue = 300.0f;
    OffsetZDesc.CurrentFloatValue = CameraLocalOffset.Z;
    OffsetZDesc.bContinuousTickUpdate = false;
    OutProperties.Add(OffsetZDesc);

    FPiSimPropertyDescriptor PitchDesc;
    PitchDesc.PropertyId = TEXT("CameraPitchDeg");
    PitchDesc.DisplayName = TEXT("Kamera Eğim Açısı Pitch (°)");
    PitchDesc.PropertyType = EPiSimPropertyType::Slider;
    PitchDesc.MinValue = -45.0f;
    PitchDesc.MaxValue = 45.0f;
    PitchDesc.CurrentFloatValue = CameraPitchDeg;
    PitchDesc.bContinuousTickUpdate = false;
    OutProperties.Add(PitchDesc);
}

// @state: WIP - UI sliderından gelen float değerini kameraya yansıtır
void UPiSimCameraSensor::SetInspectablePropertyFloat(FName PropertyId, float NewValue)
{
    if (PropertyId == TEXT("FOVDegrees"))
    {
        FOVDegrees = FMath::Clamp(NewValue, 20.0f, 160.0f);
        if (SceneCapture) SceneCapture->FOVAngle = FOVDegrees;
    }
    else if (PropertyId == TEXT("CaptureFps"))
    {
        CaptureFps = FMath::Clamp(NewValue, 1.0f, 60.0f);
    }
    else if (PropertyId == TEXT("JpegQuality"))
    {
        JpegQuality = FMath::Clamp((int32)NewValue, 5, 100);
    }
    else if (PropertyId == TEXT("CameraOffsetX"))
    {
        CameraLocalOffset.X = NewValue;
        bAutoClearVehicleBounds = false;
    }
    else if (PropertyId == TEXT("CameraOffsetZ"))
    {
        CameraLocalOffset.Z = NewValue;
        bAutoClearVehicleBounds = false;
    }
    else if (PropertyId == TEXT("CameraPitchDeg"))
    {
        CameraPitchDeg = FMath::Clamp(NewValue, -60.0f, 60.0f);
    }
}

// @state: WIP - UI metin kutusundan gelen string değerini işler
void UPiSimCameraSensor::SetInspectablePropertyString(FName PropertyId, const FString& NewValue)
{
}

// @state: WIP - UI aksiyon butonunu işler
void UPiSimCameraSensor::TriggerInspectableAction(FName ActionId)
{
}
