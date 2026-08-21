// PiSimModelImporter.cpp
// Full-Featured Pawn for GameMode with 360 Orbit Camera, Mouse Controls, Interactive Screen UI, and Strict Visual vs UCX Collision Separation.

#include "PiSimModelImporter.h"
#include "PiSimModelImporterWidget.h"
#include "PiSimGarageRobot.h"
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
        }

        // Create and Add Dedicated HUD Widget to Viewport (Z-Order: 100)
        if (!ImporterWidget)
        {
            ImporterWidget = CreateWidget<UPiSimModelImporterWidget>(GetWorld(), UPiSimModelImporterWidget::StaticClass());
            if (ImporterWidget)
            {
                ImporterWidget->TargetImporter = this;
                ImporterWidget->AddToViewport(100);
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

        // Direct Key Binds for Mouse Scroll Wheel Zoom
        PlayerInputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &APiSimModelImporter::ZoomIn);
        PlayerInputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &APiSimModelImporter::ZoomOut);
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

void APiSimModelImporter::ZoomIn()
{
    if (OrbitSpringArm)
    {
        OrbitSpringArm->TargetArmLength = FMath::Clamp(OrbitSpringArm->TargetArmLength - 35.0f, 30.0f, 2500.0f);
    }
}

void APiSimModelImporter::ZoomOut()
{
    if (OrbitSpringArm)
    {
        OrbitSpringArm->TargetArmLength = FMath::Clamp(OrbitSpringArm->TargetArmLength + 35.0f, 30.0f, 2500.0f);
    }
}

void APiSimModelImporter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Mouse Orbit & Pan Camera Dragging
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

        // =====================================================================================
        // UCX İSİM AYIKLAMA (UCX_, UBX_, USP_, UCX veya Cube.001)
        // =====================================================================================
        bool bIsUCX = Sec.MeshName.StartsWith(TEXT("UCX_"), ESearchCase::IgnoreCase) ||
                      Sec.MeshName.StartsWith(TEXT("UBX_"), ESearchCase::IgnoreCase) ||
                      Sec.MeshName.StartsWith(TEXT("USP_"), ESearchCase::IgnoreCase) ||
                      Sec.MeshName.StartsWith(TEXT("UCX"), ESearchCase::IgnoreCase) ||
                      Sec.MeshName.Contains(TEXT("UCX_"), ESearchCase::IgnoreCase) ||
                      Sec.MeshName.Contains(TEXT("Cube.00"), ESearchCase::IgnoreCase);

        if (bIsUCX)
        {
            OutUCX.Add(ImpSec);
        }
        else
        {
            OutVisual.Add(ImpSec);
        }
    }

    // =========================================================================================
    // HEM LOGA HEM DE EKRANA HER İKİ LİSTEYİ DE DETAYLICA YAZDIR!
    // =========================================================================================
    UE_LOG(LogTemp, Warning, TEXT("===================================================================="));
    UE_LOG(LogTemp, Warning, TEXT(">>> [PiSimModelImporter] FBX AYRIŞTIRMA RAPORU (%s) <<<"), *FilePath);
    UE_LOG(LogTemp, Warning, TEXT("🎨 GÖRSEL PARÇA LİSTESİ (Toplam: %d Adet):"), OutVisual.Num());
    for (int32 v = 0; v < OutVisual.Num(); ++v)
    {
        UE_LOG(LogTemp, Warning, TEXT("   [%d] VisualMesh: '%s' | Vertices: %d | Triangles: %d | Pivot: %s"),
            v, *OutVisual[v].MeshName, OutVisual[v].Vertices.Num(), OutVisual[v].Triangles.Num() / 3, *OutVisual[v].PivotPoint.ToString());
    }

    UE_LOG(LogTemp, Warning, TEXT("🛡️ UCX COLLISION PARÇA LİSTESİ (Toplam: %d Adet):"), OutUCX.Num());
    for (int32 u = 0; u < OutUCX.Num(); ++u)
    {
        UE_LOG(LogTemp, Warning, TEXT("   [%d] UCXMesh: '%s' | Vertices: %d | Triangles: %d | Pivot: %s"),
            u, *OutUCX[u].MeshName, OutUCX[u].Vertices.Num(), OutUCX[u].Triangles.Num() / 3, *OutUCX[u].PivotPoint.ToString());
    }
    UE_LOG(LogTemp, Warning, TEXT("===================================================================="));

    // Canlı Ekrana Renkli Bildirimler Bas
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(501, 15.0f, FColor::Cyan,
            FString::Printf(TEXT("🎨 [GÖRSEL LİSTE: %d PARÇA]"), OutVisual.Num()));
        for (int32 v = 0; v < FMath::Min(OutVisual.Num(), 5); ++v)
        {
            GEngine->AddOnScreenDebugMessage(510 + v, 15.0f, FColor::White,
                FString::Printf(TEXT("   • Visual [%d]: %s (%d Verts, %d Tris)"), v, *OutVisual[v].MeshName, OutVisual[v].Vertices.Num(), OutVisual[v].Triangles.Num() / 3));
        }

        GEngine->AddOnScreenDebugMessage(530, 15.0f, FColor::Yellow,
            FString::Printf(TEXT("🛡️ [UCX COLLISION LİSTE: %d PARÇA]"), OutUCX.Num()));
        for (int32 u = 0; u < FMath::Min(OutUCX.Num(), 5); ++u)
        {
            GEngine->AddOnScreenDebugMessage(540 + u, 15.0f, FColor::Orange,
                FString::Printf(TEXT("   • UCX [%d]: %s (%d Verts)"), u, *OutUCX[u].MeshName, OutUCX[u].Vertices.Num()));
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
    //    (KESİNLİKLE COLLISION YOK - PURE RENDER!)
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

        // GÖRÜNÜR KIL (Render Açık, Çarpışma TAMAMEN KAPALI!)
        VisComp->SetVisibility(true);
        VisComp->SetHiddenInGame(false);
        VisComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        VisComp->SetCollisionResponseToAllChannels(ECR_Ignore);

        VisualMeshComponents.Add(VisComp);
    }

    // -----------------------------------------------------------------------------------------
    // 2) UCX LİSTESİNDEKİ PARÇALARI İLGİLİ GÖRSEL KEMİĞE BAĞLA, GÖRÜNMEZ YAP VE STATİK/DİNAMİK COLLISION VER
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
        TargetName.RemoveFromStart(TEXT("UCX"), ESearchCase::IgnoreCase);

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

        // ÇARPIŞMAYI AKTİFLEŞTİR (Chaos Convex Hull - Başlangıçtan İtibaren Aktif!)
        CollComp->ClearCollisionConvexMeshes();
        CollComp->AddCollisionConvexMesh(UCXSections[j].Vertices);
        CollComp->bUseComplexAsSimpleCollision = false;
        CollComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        CollComp->SetCollisionObjectType(ECC_WorldDynamic);
        CollComp->SetCollisionResponseToAllChannels(ECR_Block);
        CollComp->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block); // Zeminle çarpış
        CollComp->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
        CollComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
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
}

// =========================================================================================
// [AŞAMA 4] FİZİK VE YERÇEKİMİNİ AKTİFLEŞTİRME / KAPATMA (Simulate Physics)
// =========================================================================================
void APiSimModelImporter::SetPhysicsSimulationActive(bool bActive)
{
    if (VisualMeshComponents.Num() == 0)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT(">>> [HATA] Sahnede yüklü parça bulunamadı! <<<"));
        }
        return;
    }

    if (!bActive)
    {
        // -------------------------------------------------------------------------------------
        // FİZİĞİ DURDUR (Statik Garaj Moduna Dön)
        // -------------------------------------------------------------------------------------
        for (UPhysicsConstraintComponent* Constraint : JointConstraints)
        {
            if (Constraint) Constraint->DestroyComponent();
        }
        JointConstraints.Empty();

        for (int32 i = 0; i < VisualMeshComponents.Num(); ++i)
        {
            if (VisualMeshComponents[i])
            {
                VisualMeshComponents[i]->SetSimulatePhysics(false);
                VisualMeshComponents[i]->SetEnableGravity(false);
            }
        }

        for (int32 j = 0; j < CollisionMeshComponents.Num(); ++j)
        {
            if (CollisionMeshComponents[j])
            {
                CollisionMeshComponents[j]->SetSimulatePhysics(false);
                CollisionMeshComponents[j]->SetEnableGravity(false);
            }
        }

        // Hiyerarşiyi ve konumu yeniden sıfırla
        BuildAndSpawnRobotHierarchy(ImportScaleMultiplier);

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(801, 8.0f, FColor::Orange, TEXT("🛑 >>> [FİZİK SİMÜLASYONU DURDURULDU] Robot Statik Moda Alındı <<<"));
        }
        UE_LOG(LogTemp, Warning, TEXT("[PiSimModelImporter LOG] Fizik simülasyonu durduruldu."));
        return;
    }

    // -----------------------------------------------------------------------------------------
    // FİZİĞİ AKTİFLEŞTİR (Canlı Chaos Multi-Body Gazebo Simülasyonu)
    // -----------------------------------------------------------------------------------------
    UE_LOG(LogTemp, Warning, TEXT("===================================================================="));
    UE_LOG(LogTemp, Warning, TEXT("[PiSimModelImporter LOG] >>> CANLI CHAOS FİZİK SİMÜLASYONU BAŞLATILIYOR <<<"));
    UE_LOG(LogTemp, Warning, TEXT("===================================================================="));

    // 1) Ana Gövdeyi Kök Yap
    if (VisualMeshComponents.IsValidIndex(0) && VisualMeshComponents[0])
    {
        VisualMeshComponents[0]->SetMobility(EComponentMobility::Movable);
        VisualMeshComponents[0]->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
        SetRootComponent(VisualMeshComponents[0]);
    }

    // Hedef fizik bileşenleri: Varsa UCX (CollisionMeshComponents), yoksa VisualMeshComponents
    bool bHasUCX = (CollisionMeshComponents.Num() > 0);
    TArray<UProceduralMeshComponent*>& PhysComps = bHasUCX ? CollisionMeshComponents : VisualMeshComponents;

    for (int32 i = 0; i < PhysComps.Num(); ++i)
    {
        UProceduralMeshComponent* Comp = PhysComps[i];
        if (!Comp) continue;

        FString CompName = Comp->GetName();
        float Mass = (i == 0) ? 30.0f : 2.5f;

        // Dinamik Fiziği Aç
        Comp->SetMobility(EComponentMobility::Movable);
        Comp->SetSimulatePhysics(true);
        Comp->SetEnableGravity(true);
        Comp->SetMassOverrideInKg(NAME_None, Mass, true);
        Comp->SetLinearDamping(0.8f);
        Comp->SetAngularDamping(1.5f);
        Comp->WakeRigidBody();

        UE_LOG(LogTemp, Warning, TEXT("[FİZİK LOG] '%s' bileşenine CANLI DİNAMİK FİZİK verildi | Kütle: %.1f kg | Yerçekimi: AÇIK"),
            *CompName, Mass);

        // Gövde Dışındaki Tekerlekler İçin Fiziksel Eklem (Constraint)
        if (i > 0 && PhysComps[0])
        {
            FName ConstraintName = *FString::Printf(TEXT("PhysicsJoint_%d"), i);
            UPhysicsConstraintComponent* Constraint = NewObject<UPhysicsConstraintComponent>(this, ConstraintName);
            Constraint->SetupAttachment(PhysComps[0]);
            Constraint->RegisterComponent();

            Constraint->SetConstrainedComponents(PhysComps[0], NAME_None, Comp, NAME_None);
            Constraint->SetAngularTwistLimit(EAngularConstraintMotion::ACM_Free, 0.0f);
            Constraint->SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Locked, 0.0f);
            Constraint->SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Locked, 0.0f);
            Constraint->SetLinearXLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);
            Constraint->SetLinearYLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);
            Constraint->SetLinearZLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);

            JointConstraints.Add(Constraint);
        }
    }

    // Ekran Bildirimleri
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(801, 10.0f, FColor::Cyan,
            FString::Printf(TEXT("🚀 >>> [CHAOS FİZİK AKTİF!] %d Parçaya Canlı Dinamik Fizik Verildi <<<"), PhysComps.Num()));
        
        GEngine->AddOnScreenDebugMessage(802, 10.0f, FColor::Emerald,
            FString::Printf(TEXT("🛡️ >>> [GÖVDE: 30.0 KG | YERÇEKİMİ: AÇIK] Zemin Çarpışması Aktif <<<")));

        GEngine->AddOnScreenDebugMessage(803, 10.0f, FColor::Yellow,
            FString::Printf(TEXT("⚙️ >>> [%d ADET EKLEM KISITLAMASI BAĞLANDI] Tekerlekler Serbest Dönüyor! <<<"), JointConstraints.Num()));
    }
}
