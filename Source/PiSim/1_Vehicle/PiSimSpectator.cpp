// PiSimSpectator.cpp
// Serbest Editör Uçuşu, Seçili Araç Orbit ve Araç Kamerası (FPV) Gözlemci Pawn'ı Uygulaması

#include "PiSimSpectator.h"
#include "Vehicle.h"
#include "CameraSensor.h"
#include "GarageWidget.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpectatorPawnMovement.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
    static TMap<const APiSimSpectatorPawn*, bool> GSpectatorFpvMap;
}

bool APiSimSpectatorPawn::IsFpvCameraMode() const
{
    const bool* Found = GSpectatorFpvMap.Find(this);
    return Found ? *Found : false;
}

void APiSimSpectatorPawn::SetFpvCameraMode(bool bEnable)
{
    GSpectatorFpvMap.Add(this, bEnable);
}

// @state: WIP - Gözlemci Pawn yapıcısı
APiSimSpectatorPawn::APiSimSpectatorPawn()
{
    PrimaryActorTick.bCanEverTick = true;
    bAddDefaultMovementBindings = false; // Kendi pürüzsüz Unreal Editor uçuş kontrolümüzü kullanıyoruz

    // Yay kolu oluştur (Serbest uçuşta TargetArmLength=0, Orbit modunda=OrbitDistance)
    SpectatorSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpectatorSpringArm"));
    SpectatorSpringArm->SetupAttachment(RootComponent);
    SpectatorSpringArm->TargetArmLength = 0.0f;
    SpectatorSpringArm->bDoCollisionTest = false;
    SpectatorSpringArm->bInheritPitch = false;
    SpectatorSpringArm->bInheritYaw = false;
    SpectatorSpringArm->bInheritRoll = false;
    SpectatorSpringArm->bEnableCameraLag = false;

    // Kamera bileşeni
    SpectatorCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("SpectatorCamera"));
    SpectatorCamera->SetupAttachment(SpectatorSpringArm, USpringArmComponent::SocketName);

    CurrentOrbitRotation = FRotator(-20.0f, 0.0f, 0.0f);

    if (USpectatorPawnMovement* Mov = Cast<USpectatorPawnMovement>(GetMovementComponent()))
    {
        Mov->MaxSpeed = FlySpeedMultiplier;
        Mov->bIgnoreTimeDilation = true;
    }
}

// @state: WIP - Başlangıçta serbest editör kamerası olarak başlar (Otomatik araç odaklaması YAPMAZ!)
void APiSimSpectatorPawn::BeginPlay()
{
    Super::BeginPlay();

    bIsOrbitingVehicle = false;
    SetFpvCameraMode(false);

    if (SpectatorSpringArm)
    {
        SpectatorSpringArm->TargetArmLength = 0.0f;
        SpectatorSpringArm->SetWorldRotation(CurrentOrbitRotation);
    }

    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC && GetWorld())
    {
        PC = GetWorld()->GetFirstPlayerController();
    }

    if (PC)
    {
        PC->bShowMouseCursor = true;
        PC->bEnableClickEvents = true;
        PC->bEnableMouseOverEvents = true;
        FInputModeGameAndUI InputMode;
        InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        InputMode.SetHideCursorDuringCapture(false);
        PC->SetInputMode(InputMode);

        if (!DashboardWidget)
        {
            DashboardWidget = CreateWidget<UPiSimModularGarageWidget>(PC, UPiSimModularGarageWidget::StaticClass());
            if (DashboardWidget)
            {
                DashboardWidget->AddToViewport(10);
                UE_LOG(LogTemp, Log, TEXT("🚀 [PiSimSpectator] Dashboard Unreal arayüzü başlatıldı (Serbest Editör Kamerası Aktif)."));
            }
        }
    }
}

// @state: WIP - Her frame: 1) FPV Kamera Kilitlenmesi, 2) Seçili Araç Orbit, veya 3) Gerçek Unreal Editör Serbest Uçuşu
void APiSimSpectatorPawn::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC && GetWorld())
    {
        PC = GetWorld()->GetFirstPlayerController();
    }

    // ESC tuşuna basıldığında her zaman ana Serbest Editör Kamerasına geri dön!
    if (PC && PC->WasInputKeyJustPressed(EKeys::Escape))
    {
        if (IsFpvCameraMode() || bIsOrbitingVehicle)
        {
            ReturnToFreeFlight();
            return;
        }
    }

    // =========================================================================
    // MOD 1: ARAÇ KAMERASI (FPV) MODU (Her Tick Aracın Kamerası ile Aynı Transform)
    // =========================================================================
    if (IsFpvCameraMode() && IsValid(FocusedVehicle))
    {
        FVector CamWorldLoc = FocusedVehicle->GetActorLocation() + FVector(0.0f, 0.0f, 120.0f);
        FRotator CamWorldRot = FocusedVehicle->GetActorRotation();

        if (FocusedVehicle->CameraSensor)
        {
            FocusedVehicle->CameraSensor->GetCameraWorldTransform(CamWorldLoc, CamWorldRot);
            if (SpectatorCamera)
            {
                SpectatorCamera->SetFieldOfView(FocusedVehicle->CameraSensor->FOVDegrees);
            }
        }

        SetActorLocation(CamWorldLoc);
        CurrentOrbitRotation = CamWorldRot;
        if (SpectatorSpringArm)
        {
            SpectatorSpringArm->TargetArmLength = 0.0f;
            SpectatorSpringArm->SetWorldRotation(CamWorldRot);
        }
        return;
    }

    // =========================================================================
    // MOD 2: SEÇİLİ ARAÇ 360° ORBİT MODU (Sol panelden araç seçildiğinde aktif)
    // =========================================================================
    if (bIsOrbitingVehicle && IsValid(FocusedVehicle))
    {
        // Eğer kullanıcı Sağ Tık basılı tutarken WASD/QE tuşlarına basarsa,
        // tıpkı gerçek Unreal Editör'deki gibi Orbit'ten çıkıp serbest uçuşa geçsin!
        if (PC && PC->IsInputKeyDown(EKeys::RightMouseButton))
        {
            const bool bWantsToFly =
                PC->IsInputKeyDown(EKeys::W) || PC->IsInputKeyDown(EKeys::S) ||
                PC->IsInputKeyDown(EKeys::A) || PC->IsInputKeyDown(EKeys::D) ||
                PC->IsInputKeyDown(EKeys::E) || PC->IsInputKeyDown(EKeys::Q);
            if (bWantsToFly)
            {
                ReturnToFreeFlight();
                return;
            }
        }

        FVector TargetPos = FocusedVehicle->GetActorLocation() + OrbitTargetOffset;
        SetActorLocation(FMath::VInterpTo(GetActorLocation(), TargetPos, DeltaTime, 14.0f));

        if (PC && (PC->IsInputKeyDown(EKeys::LeftMouseButton) || PC->IsInputKeyDown(EKeys::RightMouseButton)))
        {
            float MouseX = 0.0f, MouseY = 0.0f;
            PC->GetInputMouseDelta(MouseX, MouseY);
            if (FMath::Abs(MouseX) > 0.005f || FMath::Abs(MouseY) > 0.005f)
            {
                AddOrbitDelta(MouseX, MouseY);
            }
        }

        if (SpectatorSpringArm)
        {
            SpectatorSpringArm->TargetArmLength = FMath::FInterpTo(SpectatorSpringArm->TargetArmLength, OrbitDistance, DeltaTime, 12.0f);
            SpectatorSpringArm->SetWorldRotation(CurrentOrbitRotation);
        }
        return;
    }

    // =========================================================================
    // MOD 3: GERÇEK UNREAL EDİTÖR SERBEST UÇUŞ MODU (NOCLIP FLY + MOUSE LOOK)
    // =========================================================================
    if (SpectatorSpringArm)
    {
        SpectatorSpringArm->TargetArmLength = 0.0f;
        SpectatorSpringArm->SetWorldRotation(CurrentOrbitRotation);
    }

    if (!PC) return;

    // Sağ tık basılıyken fare ile 360° serbest bakış (Unreal Editor Viewport gibi)
    if (PC->IsInputKeyDown(EKeys::RightMouseButton))
    {
        float MouseX = 0.0f, MouseY = 0.0f;
        PC->GetInputMouseDelta(MouseX, MouseY);
        if (FMath::Abs(MouseX) > 0.005f || FMath::Abs(MouseY) > 0.005f)
        {
            CurrentOrbitRotation.Yaw += MouseX * 1.35f;
            CurrentOrbitRotation.Pitch = FMath::Clamp(CurrentOrbitRotation.Pitch + (MouseY * 1.35f), -88.0f, 88.0f);
            CurrentOrbitRotation.Roll = 0.0f;
            if (SpectatorSpringArm)
            {
                SpectatorSpringArm->SetWorldRotation(CurrentOrbitRotation);
            }
            PC->SetControlRotation(CurrentOrbitRotation);
        }
    }

    // WASD + QE + Yön Tuşları ile 3B Uzayda Serbest Uçuş
    FVector MoveInput = FVector::ZeroVector;
    const FRotationMatrix CamRotMat(CurrentOrbitRotation);
    const FVector CamForward = CamRotMat.GetUnitAxis(EAxis::X);
    const FVector CamRight   = CamRotMat.GetUnitAxis(EAxis::Y);
    const FVector WorldUp    = FVector::UpVector;

    if (PC->IsInputKeyDown(EKeys::W) || PC->IsInputKeyDown(EKeys::Up))
    {
        MoveInput += CamForward;
    }
    if (PC->IsInputKeyDown(EKeys::S) || PC->IsInputKeyDown(EKeys::Down))
    {
        MoveInput -= CamForward;
    }
    if (PC->IsInputKeyDown(EKeys::D) || PC->IsInputKeyDown(EKeys::Right))
    {
        MoveInput += CamRight;
    }
    if (PC->IsInputKeyDown(EKeys::A) || PC->IsInputKeyDown(EKeys::Left))
    {
        MoveInput -= CamRight;
    }
    if (PC->IsInputKeyDown(EKeys::E) || PC->IsInputKeyDown(EKeys::SpaceBar))
    {
        MoveInput += WorldUp;
    }
    if (PC->IsInputKeyDown(EKeys::Q) || PC->IsInputKeyDown(EKeys::LeftControl))
    {
        MoveInput -= WorldUp;
    }

    if (!MoveInput.IsNearlyZero())
    {
        float SpeedBoost = (PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift)) ? 3.0f : 1.0f;
        FVector DeltaMove = MoveInput.GetSafeNormal() * (FlySpeedMultiplier * SpeedBoost * DeltaTime);
        AddActorWorldOffset(DeltaMove, false);
    }
}

// @state: WIP - Giriş tuşlarını bağlar (F, V, ESC, Tab, MouseWheel)
void APiSimSpectatorPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (!PlayerInputComponent)
    {
        return;
    }

    PlayerInputComponent->BindKey(EKeys::F, IE_Pressed, this, &APiSimSpectatorPawn::HandleKeyFocus);
    PlayerInputComponent->BindKey(EKeys::V, IE_Pressed, this, &APiSimSpectatorPawn::HandleKeyFreeFlight);
    PlayerInputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &APiSimSpectatorPawn::HandleKeyFreeFlight);
    PlayerInputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &APiSimSpectatorPawn::HandleKeyTab);

    PlayerInputComponent->BindAxisKey(EKeys::MouseWheelAxis, this, &APiSimSpectatorPawn::AddZoomDelta);
}

// @state: WIP - Dashboard Unreal arayüzünü açar/kapatır (Tab tuşu)
void APiSimSpectatorPawn::ToggleDashboardVisibility()
{
    if (!DashboardWidget) return;

    if (DashboardWidget->IsVisible())
    {
        DashboardWidget->SetVisibility(ESlateVisibility::Hidden);
    }
    else
    {
        DashboardWidget->SetVisibility(ESlateVisibility::Visible);
    }
}

// @state: WIP - Sol panelden seçilen araca 360° Orbit modunda odaklanır
void APiSimSpectatorPawn::SetFocusedVehicle(APiSimVehicle* InVehicle)
{
    FocusedVehicle = InVehicle;
    if (FocusedVehicle)
    {
        SetFpvCameraMode(false);
        bIsOrbitingVehicle = true;
        if (SpectatorCamera)
        {
            SpectatorCamera->SetFieldOfView(90.0f);
        }
        if (DashboardWidget)
        {
            DashboardWidget->SetTargetVehicle(FocusedVehicle);
        }
        UE_LOG(LogTemp, Log, TEXT("🎯 [PiSimSpectator] Araç %d Orbit Moduna Alındı."), FocusedVehicle->VehicleId);
    }
}

// @state: WIP - Seçili aracın kendi üst kamerasına (FPV) kilitlenir
void APiSimSpectatorPawn::EnterVehicleCameraMode(APiSimVehicle* InVehicle)
{
    if (InVehicle)
    {
        FocusedVehicle = InVehicle;
    }
    if (!FocusedVehicle)
    {
        return;
    }

    bIsOrbitingVehicle = false;
    SetFpvCameraMode(true);

    if (SpectatorSpringArm)
    {
        SpectatorSpringArm->TargetArmLength = 0.0f;
    }

    UE_LOG(LogTemp, Log, TEXT("🎥 [PiSimSpectator] Araç %d FPV Kamera Moduna Geçildi (Çıkış: ESC)."), FocusedVehicle->VehicleId);
}

// @state: WIP - Sahnedeki bir sonraki araca odaklanır
void APiSimSpectatorPawn::FocusNextVehicle()
{
    if (!GetWorld()) return;

    TArray<AActor*> AllVehicles;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), APiSimVehicle::StaticClass(), AllVehicles);

    if (AllVehicles.Num() == 0) return;

    int32 CurrentIdx = -1;
    for (int32 i = 0; i < AllVehicles.Num(); ++i)
    {
        if (AllVehicles[i] == FocusedVehicle)
        {
            CurrentIdx = i;
            break;
        }
    }

    int32 NextIdx = (CurrentIdx + 1) % AllVehicles.Num();
    SetFocusedVehicle(Cast<APiSimVehicle>(AllVehicles[NextIdx]));
}

// @state: WIP - Orbit modunu açıp kapatır
void APiSimSpectatorPawn::ToggleOrbitMode()
{
    if (bIsOrbitingVehicle || IsFpvCameraMode())
    {
        ReturnToFreeFlight();
    }
    else if (FocusedVehicle)
    {
        bIsOrbitingVehicle = true;
    }
}

// @state: WIP - Serbest noclip editör uçuş moduna geri döner (ESC)
void APiSimSpectatorPawn::ReturnToFreeFlight()
{
    // Kamera sıçramasını önlemek için mevcut gerçek kamera dünya konumunu yakala
    if (SpectatorCamera)
    {
        FVector CurrentCamLoc = SpectatorCamera->GetComponentLocation();
        FRotator CurrentCamRot = SpectatorCamera->GetComponentRotation();

        if (IsFpvCameraMode() && IsValid(FocusedVehicle))
        {
            // FPV'den çıkarken kamerayı aracın biraz arkasına ve üstüne al ki mesh içinde kalmasın
            CurrentCamLoc = FocusedVehicle->GetActorLocation() - (FocusedVehicle->GetActorForwardVector() * 350.0f) + FVector(0.0f, 0.0f, 180.0f);
            CurrentCamRot = (FocusedVehicle->GetActorLocation() - CurrentCamLoc).Rotation();
        }

        SetActorLocation(CurrentCamLoc);
        CurrentOrbitRotation = FRotator(CurrentCamRot.Pitch, CurrentCamRot.Yaw, 0.0f);
        SpectatorCamera->SetFieldOfView(90.0f);
    }

    SetFpvCameraMode(false);
    bIsOrbitingVehicle = false;

    if (SpectatorSpringArm)
    {
        SpectatorSpringArm->TargetArmLength = 0.0f;
        SpectatorSpringArm->SetWorldRotation(CurrentOrbitRotation);
    }

    UE_LOG(LogTemp, Log, TEXT("🦅 [PiSimSpectator] Ana Serbest Editör Kamerasına Dönüldü."));
}

// @state: WIP - Fare tekerleği ile Orbit mesafesini veya Serbest Uçuşta ileri/hız ayarını günceller
void APiSimSpectatorPawn::AddZoomDelta(float Direction)
{
    if (FMath::Abs(Direction) < 0.01f) return;

    if (bIsOrbitingVehicle)
    {
        OrbitDistance = FMath::Clamp(OrbitDistance - (Direction * 80.0f), MinOrbitDistance, MaxOrbitDistance);
        if (SpectatorSpringArm)
        {
            SpectatorSpringArm->TargetArmLength = OrbitDistance;
        }
    }
    else if (!IsFpvCameraMode())
    {
        // Serbest editör modunda tekerlek kamerayı bakış yönünde ileri/geri kaydırır
        const FVector CamForward = FRotationMatrix(CurrentOrbitRotation).GetUnitAxis(EAxis::X);
        AddActorWorldOffset(CamForward * (Direction * 140.0f), false);
    }
}

// @state: WIP - Fare sürüklemesi ile orbit açısını döndürür
void APiSimSpectatorPawn::AddOrbitDelta(float DeltaX, float DeltaY)
{
    CurrentOrbitRotation.Yaw += DeltaX * 0.75f;
    CurrentOrbitRotation.Pitch = FMath::Clamp(CurrentOrbitRotation.Pitch + (DeltaY * 0.75f), -85.0f, 85.0f);
    CurrentOrbitRotation.Roll = 0.0f;
    if (SpectatorSpringArm)
    {
        SpectatorSpringArm->SetWorldRotation(CurrentOrbitRotation);
    }
}

void APiSimSpectatorPawn::HandleKeyFocus()
{
    ToggleOrbitMode();
}

void APiSimSpectatorPawn::HandleKeyFreeFlight()
{
    ReturnToFreeFlight();
}

void APiSimSpectatorPawn::HandleMoveForward(float Val) {}
void APiSimSpectatorPawn::HandleMoveRight(float Val) {}
void APiSimSpectatorPawn::HandleMoveUp(float Val) {}
void APiSimSpectatorPawn::HandleTurn(float Val) {}
void APiSimSpectatorPawn::HandleLookUp(float Val) {}

void APiSimSpectatorPawn::HandleKeyTab()
{
    ToggleDashboardVisibility();
}
