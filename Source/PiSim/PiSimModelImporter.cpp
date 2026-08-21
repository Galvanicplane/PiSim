// PiSimModelImporter.cpp
// Full-Featured Pawn for GameMode with 360 Orbit Camera, Mouse Controls, Interactive Screen UI, and Strict Visual vs UCX Collision Separation.

#include "PiSimModelImporter.h"
#include "PiSimGarageRobot.h"
#include "PiSimHUD.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Materials/MaterialInterface.h"
#include "GameFramework/PlayerController.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"

APiSimModelImporter::APiSimModelImporter()
{
    PrimaryActorTick.bCanEverTick = true;
    AutoPossessPlayer = EAutoReceiveInput::Player0;

    SceneRootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRootComponent"));
    SetRootComponent(SceneRootComponent);

    // SpaceX 360 Orbit Camera framing
    OrbitSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("OrbitSpringArm"));
    OrbitSpringArm->SetupAttachment(SceneRootComponent);
    OrbitSpringArm->TargetArmLength = 350.0f;
    OrbitSpringArm->SetRelativeRotation(FRotator(-20.0f, 45.0f, 0.0f));
    OrbitSpringArm->bUsePawnControlRotation = false;
    OrbitSpringArm->bDoCollisionTest = false;
    OrbitSpringArm->bEnableCameraLag = true;
    OrbitSpringArm->CameraLagSpeed = 12.0f;

    OrbitCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("OrbitCamera"));
    OrbitCamera->SetupAttachment(OrbitSpringArm, USpringArmComponent::SocketName);

    ImportScaleMultiplier = 1.0f; // Pure 1:1 scale by default
    bIsPhysicsSimulating = false;
}

void APiSimModelImporter::BeginPlay()
{
    Super::BeginPlay();

    // Enable Mouse Cursor, HUD & Interactive Events for Player Controller
    if (GetWorld())
    {
        APlayerController* PC = GetWorld()->GetFirstPlayerController();
        if (PC)
        {
            PC->bShowMouseCursor = true;
            PC->bEnableClickEvents = true;
            PC->bEnableMouseOverEvents = true;
            FInputModeGameAndUI InputMode;
            InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            InputMode.SetHideCursorDuringCapture(false);
            PC->SetInputMode(InputMode);

            if (!PC->GetHUD())
            {
                PC->ClientSetHUD(APiSimHUD::StaticClass());
            }
        }
    }

    // Auto-spawn model from Saved/Robots/Cache/robot_import_test.fbx at startup
    BuildAndSpawnRobotHierarchy(ImportScaleMultiplier);
}

void APiSimModelImporter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (PlayerInputComponent)
    {
        PlayerInputComponent->BindAction(TEXT("LeftMouseClick"), IE_Pressed, this, &APiSimModelImporter::OnLeftMouseDown);
        PlayerInputComponent->BindAction(TEXT("LeftMouseClick"), IE_Released, this, &APiSimModelImporter::OnLeftMouseUp);
        PlayerInputComponent->BindAction(TEXT("RightMouseClick"), IE_Pressed, this, &APiSimModelImporter::OnRightMouseDown);
        PlayerInputComponent->BindAction(TEXT("RightMouseClick"), IE_Released, this, &APiSimModelImporter::OnRightMouseUp);
        PlayerInputComponent->BindAxis(TEXT("MouseWheelAxis"), this, &APiSimModelImporter::OnMouseWheelAxis);
    }
}

void APiSimModelImporter::OnLeftMouseDown()
{
    bIsLeftMouseDown = true;
}

void APiSimModelImporter::OnLeftMouseUp()
{
    bIsLeftMouseDown = false;
}

void APiSimModelImporter::OnRightMouseDown()
{
    bIsRightMouseDown = true;
}

void APiSimModelImporter::OnRightMouseUp()
{
    bIsRightMouseDown = false;
}

void APiSimModelImporter::OnMouseWheelAxis(float Val)
{
    if (FMath::Abs(Val) > 0.01f && OrbitSpringArm)
    {
        OrbitSpringArm->TargetArmLength = FMath::Clamp(OrbitSpringArm->TargetArmLength - Val * 35.0f, 50.0f, 1500.0f);
    }
}

void APiSimModelImporter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Mouse Orbit Camera Dragging
    if (GetWorld())
    {
        APlayerController* PC = GetWorld()->GetFirstPlayerController();
        if (PC && OrbitSpringArm)
        {
            float MouseX = 0.0f, MouseY = 0.0f;
            PC->GetInputMouseDelta(MouseX, MouseY);

            // Left Mouse Drag -> 360 Orbit Camera
            if (PC->IsInputKeyDown(EKeys::LeftMouseButton) && (FMath::Abs(MouseX) > 0.001f || FMath::Abs(MouseY) > 0.001f))
            {
                FRotator ArmRot = OrbitSpringArm->GetRelativeRotation();
                ArmRot.Yaw += MouseX * 2.5f;
                ArmRot.Pitch = FMath::Clamp(ArmRot.Pitch + MouseY * 2.0f, -80.0f, 80.0f);
                OrbitSpringArm->SetRelativeRotation(ArmRot);
            }

            // Right Mouse Drag -> Pan Camera Target
            if (PC->IsInputKeyDown(EKeys::RightMouseButton) && (FMath::Abs(MouseX) > 0.001f || FMath::Abs(MouseY) > 0.001f))
            {
                FVector ArmLoc = OrbitSpringArm->GetRelativeLocation();
                ArmLoc.Y += MouseX * 1.5f;
                ArmLoc.Z += MouseY * 1.5f;
                OrbitSpringArm->SetRelativeLocation(ArmLoc);
            }

            // Mouse Wheel Zoom
            if (PC->IsInputKeyDown(EKeys::MouseScrollUp))
            {
                OrbitSpringArm->TargetArmLength = FMath::Clamp(OrbitSpringArm->TargetArmLength - 25.0f, 50.0f, 1500.0f);
            }
            else if (PC->IsInputKeyDown(EKeys::MouseScrollDown))
            {
                OrbitSpringArm->TargetArmLength = FMath::Clamp(OrbitSpringArm->TargetArmLength + 25.0f, 50.0f, 1500.0f);
            }
        }
    }
}

void APiSimModelImporter::ClearSpawnedComponents()
{
    for (UProceduralMeshComponent* VisComp : VisualMeshComponents)
    {
        if (VisComp) VisComp->DestroyComponent();
    }
    VisualMeshComponents.Empty();

    for (UProceduralMeshComponent* CollComp : CollisionMeshComponents)
    {
        if (CollComp) CollComp->DestroyComponent();
    }
    CollisionMeshComponents.Empty();

    for (UPhysicsConstraintComponent* Constraint : JointConstraints)
    {
        if (Constraint) Constraint->DestroyComponent();
    }
    JointConstraints.Empty();

    VisualSections.Empty();
    UCXSections.Empty();
    bIsPhysicsSimulating = false;
}

void APiSimModelImporter::ImportAndSpawnRobot()
{
    BuildAndSpawnRobotHierarchy(ImportScaleMultiplier);
}

void APiSimModelImporter::SetScale_0_1X()
{
    ImportScaleMultiplier = 0.1f;
    BuildAndSpawnRobotHierarchy(ImportScaleMultiplier);
}

void APiSimModelImporter::SetScale_1_0X()
{
    ImportScaleMultiplier = 1.0f;
    BuildAndSpawnRobotHierarchy(ImportScaleMultiplier);
}

void APiSimModelImporter::SetScale_10_0X()
{
    ImportScaleMultiplier = 10.0f;
    BuildAndSpawnRobotHierarchy(ImportScaleMultiplier);
}

void APiSimModelImporter::TogglePhysicsSimulation()
{
    bIsPhysicsSimulating = !bIsPhysicsSimulating;
    SetPhysicsSimulationActive(bIsPhysicsSimulating);
}

// =========================================================================================
// [AŞAMA 1 & 2] FBX AYRIŞTIRMA VE LİSTELERE AYIRMA (VisualSections vs UCXSections)
// =========================================================================================
bool APiSimModelImporter::ParseBinaryFbxFile(const FString& FilePath, TArray<FImporterMeshSection>& OutVisual, TArray<FImporterMeshSection>& OutUCX, float Scale)
{
    OutVisual.Empty();
    OutUCX.Empty();

    if (!FPaths::FileExists(FilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("[PiSimModelImporter] FBX dosyasi diskte bulunamadi: %s"), *FilePath);
        return false;
    }

    TArray<FGLBMeshSection> ExtractedSections;
    if (!APiSimGarageRobot::ParseFbxAllBinaryMeshes(FilePath, ExtractedSections, Scale) || ExtractedSections.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("[PiSimModelImporter] FBX dosya ayrıştırma başarısız: %s"), *FilePath);
        return false;
    }

    for (const FGLBMeshSection& Sec : ExtractedSections)
    {
        FImporterMeshSection ImpSec;
        ImpSec.MeshName = Sec.MeshName;
        ImpSec.ParentSectionIndex = Sec.ParentSectionIndex;
        ImpSec.DepthLevel = Sec.DepthLevel;
        ImpSec.PivotPoint = Sec.PivotPoint;
        ImpSec.Vertices = Sec.Vertices;
        ImpSec.Triangles = Sec.Triangles;
        ImpSec.Normals = Sec.Normals;
        ImpSec.MassKg = (Sec.MassKg > 0.0f) ? Sec.MassKg : 2.5f;
        ImpSec.Friction = (Sec.Friction > 0.0f) ? Sec.Friction : 0.85f;
        ImpSec.MinAngle = -90.0f;
        ImpSec.MaxAngle = 90.0f;
        ImpSec.RotationAxis = FVector(0.0f, 1.0f, 0.0f);

        // UCX ve Görsel Ayrımı
        if (Sec.MeshName.StartsWith(TEXT("UCX_"), ESearchCase::IgnoreCase) ||
            Sec.MeshName.StartsWith(TEXT("UBX_"), ESearchCase::IgnoreCase) ||
            Sec.MeshName.StartsWith(TEXT("USP_"), ESearchCase::IgnoreCase))
        {
            OutUCX.Add(ImpSec);
            UE_LOG(LogTemp, Warning, TEXT("[PiSimModelImporter] UCX Çarpışma Parçası Ayrıldı: %s (%d Verts)"), *Sec.MeshName, Sec.Vertices.Num());
        }
        else
        {
            OutVisual.Add(ImpSec);
            UE_LOG(LogTemp, Warning, TEXT("[PiSimModelImporter] Görsel Parça Ayrıldı: %s (%d Verts, %d Tris)"), *Sec.MeshName, Sec.Vertices.Num(), Sec.Triangles.Num() / 3);
        }
    }

    return (OutVisual.Num() > 0 || OutUCX.Num() > 0);
}

// =========================================================================================
// [AŞAMA 3] HİYERARŞİK BAĞLAMA, GÖRSEL ÇİZİM (Visual) VE COLLISION AKTİFLEŞTİRME (UCX)
// =========================================================================================
void APiSimModelImporter::BuildAndSpawnRobotHierarchy(float Scale)
{
    ClearSpawnedComponents();

    FString FbxPath = FPaths::ProjectSavedDir() / TEXT("Robots/Cache/robot_import_test.fbx");
    if (!ParseBinaryFbxFile(FbxPath, VisualSections, UCXSections, Scale))
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 8.0f, FColor::Red,
                FString::Printf(TEXT(">>> [PiSimModelImporter HATA] %s okunamadi! <<<"), *FbxPath));
        }
        return;
    }

    UMaterialInterface* DefaultMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

    // -----------------------------------------------------------------------------------------
    // 1) GÖRSEL LİSTEDEKİ PARÇALARI SAHNEYE OLUŞTUR, KEMİKLERE BAĞLA VE GÖRÜNÜR KIL
    // -----------------------------------------------------------------------------------------
    for (int32 i = 0; i < VisualSections.Num(); ++i)
    {
        FName CompName = *FString::Printf(TEXT("VisualSubMesh_%d_%s"), i, *VisualSections[i].MeshName);
        UProceduralMeshComponent* VisComp = NewObject<UProceduralMeshComponent>(this, CompName);
        VisComp->SetMobility(EComponentMobility::Movable);

        // Hiyerarşik Kemik Bağlantısı (Parent-Child)
        int32 ParentIdx = VisualSections[i].ParentSectionIndex;
        if (ParentIdx >= 0 && VisualMeshComponents.IsValidIndex(ParentIdx) && VisualMeshComponents[ParentIdx])
        {
            VisComp->SetupAttachment(VisualMeshComponents[ParentIdx]);
            VisComp->SetRelativeLocation(VisualSections[i].PivotPoint - VisualSections[ParentIdx].PivotPoint);
        }
        else
        {
            VisComp->SetupAttachment(SceneRootComponent);
            VisComp->SetRelativeLocation(VisualSections[i].PivotPoint);
        }

        VisComp->RegisterComponent();

        TArray<FVector2D> UV0;
        TArray<FLinearColor> VertexColors;
        TArray<FProcMeshTangent> Tangents;

        // Görsel Modeli Çiz
        VisComp->CreateMeshSection_LinearColor(0, VisualSections[i].Vertices, VisualSections[i].Triangles, VisualSections[i].Normals, UV0, VertexColors, Tangents, false);
        if (DefaultMat) VisComp->SetMaterial(0, DefaultMat);

        // GÖRÜNÜR KIL (Render Açık, Çarpışma Kapalı - Çarpışmayı UCX yapacak!)
        VisComp->SetVisibility(true);
        VisComp->SetHiddenInGame(false);
        VisComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        VisComp->SetCollisionResponseToAllChannels(ECR_Ignore);

        VisualMeshComponents.Add(VisComp);
    }

    // -----------------------------------------------------------------------------------------
    // 2) UCX LİSTESİNDEKİ PARÇALARI İLGİLİ GÖRSEL KEMİĞE BAĞLA, GÖRÜNMEZ YAP VE COLLISION AKTİFLEŞTİR
    // -----------------------------------------------------------------------------------------
    for (int32 j = 0; j < UCXSections.Num(); ++j)
    {
        FName CollCompName = *FString::Printf(TEXT("UCXCollision_%d_%s"), j, *UCXSections[j].MeshName);
        UProceduralMeshComponent* CollComp = NewObject<UProceduralMeshComponent>(this, CollCompName);
        CollComp->SetMobility(EComponentMobility::Movable);

        // Hedef görsel parçayı bul (Örn: UCX_Chassis -> Chassis)
        FString TargetName = UCXSections[j].MeshName;
        TargetName.RemoveFromStart(TEXT("UCX_"), ESearchCase::IgnoreCase);
        TargetName.RemoveFromStart(TEXT("UBX_"), ESearchCase::IgnoreCase);
        TargetName.RemoveFromStart(TEXT("USP_"), ESearchCase::IgnoreCase);

        UProceduralMeshComponent* TargetVisComp = (VisualMeshComponents.Num() > 0) ? VisualMeshComponents[0] : nullptr;
        for (int32 v = 0; v < VisualSections.Num(); ++v)
        {
            if (VisualSections[v].MeshName.Equals(TargetName, ESearchCase::IgnoreCase) ||
                VisualSections[v].MeshName.Contains(TargetName, ESearchCase::IgnoreCase) ||
                TargetName.Contains(VisualSections[v].MeshName, ESearchCase::IgnoreCase))
            {
                TargetVisComp = VisualMeshComponents[v];
                break;
            }
        }

        if (TargetVisComp)
        {
            CollComp->SetupAttachment(TargetVisComp);
            CollComp->SetRelativeLocation(FVector::ZeroVector);
        }
        else
        {
            CollComp->SetupAttachment(SceneRootComponent);
            CollComp->SetRelativeLocation(UCXSections[j].PivotPoint);
        }

        CollComp->RegisterComponent();

        // ÇARPIŞMAYI AKTİFLEŞTİR (Chaos Convex Hull)
        CollComp->ClearCollisionConvexMeshes();
        CollComp->AddCollisionConvexMesh(UCXSections[j].Vertices);
        CollComp->bUseComplexAsSimpleCollision = false;
        CollComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        CollComp->SetCollisionObjectType(ECC_WorldDynamic);
        CollComp->SetCollisionResponseToAllChannels(ECR_Block);
        CollComp->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
        CollComp->RecreatePhysicsState();
        CollComp->UpdateBounds();

        // GÖRÜNTÜYÜ GÖRÜNMEZ YAP (Render Kapalı)
        CollComp->SetVisibility(false);
        CollComp->SetHiddenInGame(true);

        CollisionMeshComponents.Add(CollComp);
    }

    // -----------------------------------------------------------------------------------------
    // 3) EĞER HİÇ UCX YOKSA: GÖRSEL PARÇALARIN KENDİSİNE COLLISION VER (Güvenlik Sigortası)
    // -----------------------------------------------------------------------------------------
    if (UCXSections.Num() == 0)
    {
        for (int32 i = 0; i < VisualMeshComponents.Num(); ++i)
        {
            VisualMeshComponents[i]->ClearCollisionConvexMeshes();
            VisualMeshComponents[i]->AddCollisionConvexMesh(VisualSections[i].Vertices);
            VisualMeshComponents[i]->bUseComplexAsSimpleCollision = false;
            VisualMeshComponents[i]->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            VisualMeshComponents[i]->SetCollisionObjectType(ECC_WorldDynamic);
            VisualMeshComponents[i]->SetCollisionResponseToAllChannels(ECR_Block);
            VisualMeshComponents[i]->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
            VisualMeshComponents[i]->RecreatePhysicsState();
            VisualMeshComponents[i]->UpdateBounds();
        }
    }

    // Robot parçalarının kendi kendine çarpışmasını engelle (Self-Collision Filtering)
    for (int32 i = 0; i < VisualMeshComponents.Num(); ++i)
    {
        for (int32 j = i + 1; j < VisualMeshComponents.Num(); ++j)
        {
            if (VisualMeshComponents[i] && VisualMeshComponents[j])
            {
                VisualMeshComponents[i]->IgnoreComponentWhenMoving(VisualMeshComponents[j], true);
                VisualMeshComponents[j]->IgnoreComponentWhenMoving(VisualMeshComponents[i], true);
            }
        }
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Green,
            FString::Printf(TEXT(">>> [PiSimModelImporter] %d GÖRSEL VE %d UCX ÇARPIŞMA PARÇASI YÜKLENDİ (Ölçek: %.2fX) <<<"),
                VisualMeshComponents.Num(), CollisionMeshComponents.Num(), Scale));
    }
}

// =========================================================================================
// [AŞAMA 4] FİZİK VE YERÇEKİMİNİ AKTİFLEŞTİRME / KAPATMA (Simulate Physics)
// =========================================================================================
void APiSimModelImporter::SetPhysicsSimulationActive(bool bActive)
{
    TArray<UProceduralMeshComponent*>& TargetComps = (CollisionMeshComponents.Num() > 0) ? CollisionMeshComponents : VisualMeshComponents;

    for (int32 i = 0; i < TargetComps.Num(); ++i)
    {
        UProceduralMeshComponent* Comp = TargetComps[i];
        if (!Comp) continue;

        if (bActive)
        {
            Comp->SetMobility(EComponentMobility::Movable);
            Comp->SetSimulatePhysics(true);
            Comp->SetEnableGravity(true);
            float Mass = (i == 0 ? 30.0f : 2.5f);
            Comp->SetMassOverrideInKg(NAME_None, Mass, true);
            Comp->SetLinearDamping(0.8f);
            Comp->SetAngularDamping(2.0f);
            Comp->WakeRigidBody();
        }
        else
        {
            Comp->SetSimulatePhysics(false);
            Comp->SetEnableGravity(false);
        }
    }

    if (GEngine)
    {
        FColor MsgColor = bActive ? FColor::Emerald : FColor::Orange;
        GEngine->AddOnScreenDebugMessage(-1, 10.0f, MsgColor,
            FString::Printf(TEXT(">>> [PiSimModelImporter] FİZİK SİMÜLASYONU: %s <<<"), bActive ? TEXT("AKTİF (Açık)") : TEXT("DEVRE DIŞI (Kapalı)")));
    }
}
