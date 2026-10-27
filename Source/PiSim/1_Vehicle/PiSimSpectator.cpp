// PiSimSpectator.cpp
// Serbest Uçuş ve Filo Gözlemci Pawn'ı Uygulaması

#include "PiSimSpectator.h"
#include "Vehicle.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpectatorPawnMovement.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "EngineUtils.h"

// @state: WIP - Gözlemci Pawn yapıcısı
APiSimSpectatorPawn::APiSimSpectatorPawn()
{
    PrimaryActorTick.bCanEverTick = true;
    bAddDefaultMovementBindings = true;

    // Yay kolu oluştur (Kamera çarpışma testi kapalı, noclip yumuşak takip)
    SpectatorSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpectatorSpringArm"));
    SpectatorSpringArm->SetupAttachment(RootComponent);
    SpectatorSpringArm->TargetArmLength = OrbitDistance;
    SpectatorSpringArm->bDoCollisionTest = false;
    SpectatorSpringArm->bInheritPitch = false;
    SpectatorSpringArm->bInheritYaw = false;
    SpectatorSpringArm->bInheritRoll = false;
    SpectatorSpringArm->bEnableCameraLag = true;
    SpectatorSpringArm->CameraLagSpeed = 15.0f;

    // Kamera bileşeni
    SpectatorCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("SpectatorCamera"));
    SpectatorCamera->SetupAttachment(SpectatorSpringArm, USpringArmComponent::SocketName);

    CurrentOrbitRotation = FRotator(-25.0f, 0.0f, 0.0f);

    // Hareket bileşeni varsayılan hız ayarı
    if (USpectatorPawnMovement* Mov = Cast<USpectatorPawnMovement>(GetMovementComponent()))
    {
        Mov->MaxSpeed = FlySpeedMultiplier;
    }
}

// @state: WIP - Başlangıçta yay kolunu konumlandırır ve dünyadaki ilk aracı bulur
void APiSimSpectatorPawn::BeginPlay()
{
    Super::BeginPlay();

    if (SpectatorSpringArm)
    {
        SpectatorSpringArm->SetRelativeRotation(CurrentOrbitRotation);
    }

    // Dünyadaki ilk aracı otomatik olarak varsayılan odak hedefi yap (Serbest uçuş modunu bozmadan)
    if (!FocusedVehicle && GetWorld())
    {
        AActor* Found = UGameplayStatics::GetActorOfClass(GetWorld(), APiSimVehicle::StaticClass());
        if (Found)
        {
            FocusedVehicle = Cast<APiSimVehicle>(Found);
        }
    }
}

// @state: WIP - Her frame serbest uçuş veya seçili araç orbit hareketini yürütür
void APiSimSpectatorPawn::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    APlayerController* PC = Cast<APlayerController>(GetController());

    if (bIsOrbitingVehicle && IsValid(FocusedVehicle))
    {
        // 1. Orbit Modu: Konumu yumuşakça aracın üzerine taşır
        FVector TargetPos = FocusedVehicle->GetActorLocation() + OrbitTargetOffset;
        SetActorLocation(FMath::VInterpTo(GetActorLocation(), TargetPos, DeltaTime, 12.0f));

        // Fare ile 360° Orbit (Sol veya Sağ tık basılıyken)
        if (PC && (PC->IsInputKeyDown(EKeys::LeftMouseButton) || PC->IsInputKeyDown(EKeys::RightMouseButton)))
        {
            float MouseX = 0.0f, MouseY = 0.0f;
            PC->GetInputMouseDelta(MouseX, MouseY);
            if (FMath::Abs(MouseX) > 0.01f || FMath::Abs(MouseY) > 0.01f)
            {
                AddOrbitDelta(MouseX, MouseY);
            }
        }

        if (SpectatorSpringArm)
        {
            SpectatorSpringArm->TargetArmLength = FMath::FInterpTo(SpectatorSpringArm->TargetArmLength, OrbitDistance, DeltaTime, 10.0f);
            SpectatorSpringArm->SetRelativeRotation(CurrentOrbitRotation);
        }
    }
    else
    {
        // 2. Serbest Uçuş Modu: Sağ tık basılıyken fare ile bakış yönünü değiştir
        if (PC && PC->IsInputKeyDown(EKeys::RightMouseButton))
        {
            float MouseX = 0.0f, MouseY = 0.0f;
            PC->GetInputMouseDelta(MouseX, MouseY);
            if (FMath::Abs(MouseX) > 0.01f || FMath::Abs(MouseY) > 0.01f)
            {
                AddControllerYawInput(MouseX);
                AddControllerPitchInput(-MouseY);
            }
        }
    }
}

// @state: WIP - Giriş tuşlarını bağlar (WASD, QE, F, V, MouseWheel)
void APiSimSpectatorPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (!PlayerInputComponent)
    {
        return;
    }

    // Tuş Eylemleri
    PlayerInputComponent->BindKey(EKeys::F, IE_Pressed, this, &APiSimSpectatorPawn::HandleKeyFocus);
    PlayerInputComponent->BindKey(EKeys::V, IE_Pressed, this, &APiSimSpectatorPawn::HandleKeyFreeFlight);
    PlayerInputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &APiSimSpectatorPawn::HandleKeyFreeFlight);

    // Zoom
    PlayerInputComponent->BindAxisKey(EKeys::MouseWheelAxis, this, &APiSimSpectatorPawn::AddZoomDelta);
}

// @state: WIP - Odaklanılan aracı günceller
void APiSimSpectatorPawn::SetFocusedVehicle(APiSimVehicle* InVehicle)
{
    FocusedVehicle = InVehicle;
    if (FocusedVehicle)
    {
        bIsOrbitingVehicle = true;
        UE_LOG(LogTemp, Log, TEXT("🎯 [PiSimSpectator] Araç %d Odaklandı ve Orbit Moduna Geçildi."), FocusedVehicle->VehicleId);
    }
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
    if (bIsOrbitingVehicle)
    {
        ReturnToFreeFlight();
    }
    else if (FocusedVehicle)
    {
        bIsOrbitingVehicle = true;
    }
    else
    {
        FocusNextVehicle();
    }
}

// @state: WIP - Serbest noclip uçuş moduna geri döner
void APiSimSpectatorPawn::ReturnToFreeFlight()
{
    bIsOrbitingVehicle = false;
    UE_LOG(LogTemp, Log, TEXT("🦅 [PiSimSpectator] Serbest Uçuş Moduna Geçildi."));
}

// @state: WIP - Fare tekeri ile kamera zoom yapar
void APiSimSpectatorPawn::AddZoomDelta(float Direction)
{
    if (FMath::Abs(Direction) < 0.01f) return;

    OrbitDistance = FMath::Clamp(OrbitDistance - (Direction * 80.0f), MinOrbitDistance, MaxOrbitDistance);
    if (SpectatorSpringArm)
    {
        SpectatorSpringArm->TargetArmLength = OrbitDistance;
    }
}

// @state: WIP - Fare sürüklemesi ile orbit açısını döndürür
void APiSimSpectatorPawn::AddOrbitDelta(float DeltaX, float DeltaY)
{
    CurrentOrbitRotation.Yaw += DeltaX * 0.7f;
    CurrentOrbitRotation.Pitch = FMath::Clamp(CurrentOrbitRotation.Pitch + (DeltaY * 0.7f), -85.0f, 85.0f);
}

void APiSimSpectatorPawn::HandleKeyFocus()
{
    ToggleOrbitMode();
}

void APiSimSpectatorPawn::HandleKeyFreeFlight()
{
    ReturnToFreeFlight();
}

void APiSimSpectatorPawn::HandleMoveForward(float Val)
{
    if (!bIsOrbitingVehicle && FMath::Abs(Val) > 0.01f)
    {
        MoveForward(Val);
    }
}

void APiSimSpectatorPawn::HandleMoveRight(float Val)
{
    if (!bIsOrbitingVehicle && FMath::Abs(Val) > 0.01f)
    {
        MoveRight(Val);
    }
}

void APiSimSpectatorPawn::HandleMoveUp(float Val)
{
    if (!bIsOrbitingVehicle && FMath::Abs(Val) > 0.01f)
    {
        MoveUp_World(Val);
    }
}

void APiSimSpectatorPawn::HandleTurn(float Val)
{
    if (!bIsOrbitingVehicle && FMath::Abs(Val) > 0.01f)
    {
        AddControllerYawInput(Val);
    }
}

void APiSimSpectatorPawn::HandleLookUp(float Val)
{
    if (!bIsOrbitingVehicle && FMath::Abs(Val) > 0.01f)
    {
        AddControllerPitchInput(Val);
    }
}
