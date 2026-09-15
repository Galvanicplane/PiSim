// PiSimModelImporterBase.cpp

#include "PiSimModelImporterBase.h"
#include "Engine/World.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "PiSimUDPManager.h"
#include "Kismet/GameplayStatics.h"

APiSimModelImporterBase::APiSimModelImporterBase()
{
    PrimaryActorTick.bCanEverTick = true;

    SceneRootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRootComponent"));
    RootComponent = SceneRootComponent;

    OrbitSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("OrbitSpringArm"));
    OrbitSpringArm->SetupAttachment(RootComponent);
    OrbitSpringArm->TargetArmLength = 400.0f;
    OrbitSpringArm->SetRelativeRotation(FRotator(-25.0f, 45.0f, 0.0f));
    OrbitSpringArm->bDoCollisionTest = false;
    OrbitSpringArm->bInheritPitch = false;
    OrbitSpringArm->bInheritRoll = false;
    OrbitSpringArm->bInheritYaw = true;

    OrbitCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("OrbitCamera"));
    OrbitCamera->SetupAttachment(OrbitSpringArm, USpringArmComponent::SocketName);

    AutoPossessPlayer = EAutoReceiveInput::Player0;
}

void APiSimModelImporterBase::BeginPlay()
{
    Super::BeginPlay();

    APlayerController* PC = Cast<APlayerController>(GetController());
    if (PC)
    {
        PC->bShowMouseCursor = true;
        PC->bEnableClickEvents = true;
        PC->bEnableMouseOverEvents = true;
    }
}

void APiSimModelImporterBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearSpawnedComponents();
    Super::EndPlay(EndPlayReason);
}

void APiSimModelImporterBase::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Camera Orbit logic
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (PC)
    {
        FVector2D CurrentMousePosition;
        if (PC->GetMousePosition(CurrentMousePosition.X, CurrentMousePosition.Y))
        {
            if (bIsOrbiting)
            {
                FVector2D Delta = CurrentMousePosition - PreviousMousePosition;
                FRotator ArmRot = OrbitSpringArm->GetRelativeRotation();
                ArmRot.Yaw += Delta.X * 0.4f;
                ArmRot.Pitch = FMath::Clamp(ArmRot.Pitch - Delta.Y * 0.4f, -85.0f, 85.0f);
                OrbitSpringArm->SetRelativeRotation(ArmRot);
            }
            else if (bIsPanning)
            {
                FVector2D Delta = CurrentMousePosition - PreviousMousePosition;
                FVector Right = OrbitCamera->GetRightVector();
                FVector Up = OrbitCamera->GetUpVector();
                FVector PanOffset = (-Right * Delta.X + Up * Delta.Y) * (OrbitSpringArm->TargetArmLength * 0.002f);
                OrbitSpringArm->AddRelativeLocation(PanOffset);
            }
            PreviousMousePosition = CurrentMousePosition;
        }
    }

    // Call domain-specific physics tick (Land, Sea, Air)
    if (bIsPhysicsSimulating)
    {
        ApplyDomainPhysics(DeltaTime);
    }
}

void APiSimModelImporterBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (PlayerInputComponent)
    {
        PlayerInputComponent->BindAction(TEXT("LeftMouseClick"), IE_Pressed, this, &APiSimModelImporterBase::OnLeftMouseDown);
        PlayerInputComponent->BindAction(TEXT("LeftMouseClick"), IE_Released, this, &APiSimModelImporterBase::OnLeftMouseUp);
        PlayerInputComponent->BindAction(TEXT("RightMouseClick"), IE_Pressed, this, &APiSimModelImporterBase::OnRightMouseDown);
        PlayerInputComponent->BindAction(TEXT("RightMouseClick"), IE_Released, this, &APiSimModelImporterBase::OnRightMouseUp);

        PlayerInputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &APiSimModelImporterBase::ZoomIn);
        PlayerInputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &APiSimModelImporterBase::ZoomOut);
    }
}

EVehicleDomain APiSimModelImporterBase::DetectVehicleDomain(const FString& RootBoneName)
{
    FString Lower = RootBoneName.ToLower();

    // 1) AIR (Hava Aracı): airframe, fuselage, wing, plane, drone, body
    if (Lower.Contains(TEXT("airframe")) || Lower.Contains(TEXT("fuselage")) || 
        Lower.Contains(TEXT("wing")) || Lower.Contains(TEXT("plane")) || 
        Lower.Contains(TEXT("drone")) || Lower.Contains(TEXT("aero")))
    {
        return EVehicleDomain::Air;
    }

    // 2) SEA (Deniz Aracı): hull, boat, usv, keel, vessel, ship
    if (Lower.Contains(TEXT("hull")) || Lower.Contains(TEXT("boat")) || 
        Lower.Contains(TEXT("usv")) || Lower.Contains(TEXT("keel")) || 
        Lower.Contains(TEXT("vessel")) || Lower.Contains(TEXT("ship")) || Lower.Contains(TEXT("sub")))
    {
        return EVehicleDomain::Sea;
    }

    // 3) LAND (Kara Aracı): chassis, base_link, car, rover, track
    if (Lower.Contains(TEXT("chassis")) || Lower.Contains(TEXT("car")) || 
        Lower.Contains(TEXT("rover")) || Lower.Contains(TEXT("track")) || Lower.Contains(TEXT("base_link")))
    {
        return EVehicleDomain::Land;
    }

    // Varsayılan / Fallback
    return EVehicleDomain::Land;
}

void APiSimModelImporterBase::SetupDomainComponents()
{
    // Override in derived classes (Air, Land, Sea)
}

void APiSimModelImporterBase::ApplyDomainPhysics(float DeltaTime)
{
    // Override in derived classes (Air, Land, Sea)
}

void APiSimModelImporterBase::ClearSpawnedComponents()
{
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

    for (UProceduralMeshComponent* Comp : VisualMeshComponents)
    {
        if (Comp) Comp->DestroyComponent();
    }
    VisualMeshComponents.Empty();

    for (UProceduralMeshComponent* Comp : CollisionMeshComponents)
    {
        if (Comp) Comp->DestroyComponent();
    }
    CollisionMeshComponents.Empty();

    for (UProceduralMeshComponent* Comp : SensorMarkerComponents)
    {
        if (Comp) Comp->DestroyComponent();
    }
    SensorMarkerComponents.Empty();

    for (USceneCaptureComponent2D* Cam : SpawnedCameraComponents)
    {
        if (Cam) Cam->DestroyComponent();
    }
    SpawnedCameraComponents.Empty();
    FpvCameraCapture = nullptr;
}

void APiSimModelImporterBase::OnLeftMouseDown()
{
    bIsOrbiting = true;
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (PC) PC->GetMousePosition(PreviousMousePosition.X, PreviousMousePosition.Y);
}

void APiSimModelImporterBase::OnLeftMouseUp()
{
    bIsOrbiting = false;
}

void APiSimModelImporterBase::OnRightMouseDown()
{
    bIsPanning = true;
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (PC) PC->GetMousePosition(PreviousMousePosition.X, PreviousMousePosition.Y);
}

void APiSimModelImporterBase::OnRightMouseUp()
{
    bIsPanning = false;
}

void APiSimModelImporterBase::ZoomIn()
{
    if (OrbitSpringArm)
    {
        OrbitSpringArm->TargetArmLength = FMath::Clamp(OrbitSpringArm->TargetArmLength - 30.0f, 50.0f, 2000.0f);
    }
}

void APiSimModelImporterBase::ZoomOut()
{
    if (OrbitSpringArm)
    {
        OrbitSpringArm->TargetArmLength = FMath::Clamp(OrbitSpringArm->TargetArmLength + 30.0f, 50.0f, 2000.0f);
    }
}
