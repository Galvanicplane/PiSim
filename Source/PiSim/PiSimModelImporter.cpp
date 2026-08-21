// PiSimModelImporter.cpp
// Clean, Dedicated FBX Importer with strict Visual vs UCX Collision Separation, Hierarchy Linking, Scale Control, and Chaos Physics Toggle.

#include "PiSimModelImporter.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/Compression.h"
#include "HAL/PlatformFileManager.h"
#include "Materials/MaterialInterface.h"
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

    OrbitCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("OrbitCamera"));
    OrbitCamera->SetupAttachment(OrbitSpringArm, USpringArmComponent::SocketName);

    ImportScaleMultiplier = 1.0f; // Pure 1:1 scale by default
    bIsPhysicsSimulating = false;
}

void APiSimModelImporter::BeginPlay()
{
    Super::BeginPlay();
    BuildAndSpawnRobotHierarchy(ImportScaleMultiplier);
}

void APiSimModelImporter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void APiSimModelImporter::ClearSpawnedComponents()
{
    // Destroy previous visual components
    for (UProceduralMeshComponent* VisComp : VisualMeshComponents)
    {
        if (VisComp) VisComp->DestroyComponent();
    }
    VisualMeshComponents.Empty();

    // Destroy previous collision components
    for (UProceduralMeshComponent* CollComp : CollisionMeshComponents)
    {
        if (CollComp) CollComp->DestroyComponent();
    }
    CollisionMeshComponents.Empty();

    // Destroy physics constraints
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
        UE_LOG(LogTemp, Error, TEXT("[PiSimModelImporter] FBX dosyasi bulunamadi: %s"), *FilePath);
        return false;
    }

    TArray<uint8> FileBytes;
    if (!FFileHelper::LoadFileToArray(FileBytes, *FilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("[PiSimModelImporter] FBX dosya baytlari okunamadi: %s"), *FilePath);
        return false;
    }

    int32 FileSize = FileBytes.Num();
    if (FileSize < 23) return false;

    // Header check
    FString HeaderStr = FString(20, (const ANSICHAR*)FileBytes.GetData());
    if (!HeaderStr.StartsWith(TEXT("Kaydara FBX Binary")))
    {
        UE_LOG(LogTemp, Warning, TEXT("[PiSimModelImporter] Binary FBX degil!"));
        return false;
    }

    // Unpack Skin Clusters / Geometry Nodes
    int32 Pos = 27;
    TArray<FVector> AllVerts;
    TArray<int32> AllPolys;
    TMap<FString, TArray<int32>> Clusters;

    while (Pos + 13 < FileSize)
    {
        uint32 NodeEnd = *reinterpret_cast<const uint32*>(&FileBytes[Pos]);
        uint32 NumProps = *reinterpret_cast<const uint32*>(&FileBytes[Pos + 4]);
        uint32 PropLen = *reinterpret_cast<const uint32*>(&FileBytes[Pos + 8]);
        uint8 NameLen = FileBytes[Pos + 12];

        if (NodeEnd == 0 || NodeEnd > (uint32)FileSize || Pos + 13 + NameLen > FileSize) break;

        FString NodeName = FString(NameLen, (const ANSICHAR*)&FileBytes[Pos + 13]);
        int32 PropsStart = Pos + 13 + NameLen;
        int32 ChildrenStart = PropsStart + PropLen;

        if (NodeName.Equals(TEXT("Objects")))
        {
            int32 Sub = ChildrenStart;
            while (Sub + 13 < (int32)NodeEnd)
            {
                uint32 SubEnd = *reinterpret_cast<const uint32*>(&FileBytes[Sub]);
                uint32 SubNumProps = *reinterpret_cast<const uint32*>(&FileBytes[Sub + 4]);
                uint32 SubPropLen = *reinterpret_cast<const uint32*>(&FileBytes[Sub + 8]);
                uint8 SubNameLen = FileBytes[Sub + 12];

                if (SubEnd == 0 || SubEnd > NodeEnd || Sub + 13 + SubNameLen > (int32)NodeEnd) break;

                FString SubName = FString(SubNameLen, (const ANSICHAR*)&FileBytes[Sub + 13]);
                int32 SubPropsStart = Sub + 13 + SubNameLen;
                int32 SubChildrenStart = SubPropsStart + SubPropLen;

                if (SubName.Equals(TEXT("Geometry")))
                {
                    int32 ChildP = SubChildrenStart;
                    while (ChildP + 13 < (int32)SubEnd)
                    {
                        uint32 CEnd = *reinterpret_cast<const uint32*>(&FileBytes[ChildP]);
                        uint32 CNumProps = *reinterpret_cast<const uint32*>(&FileBytes[ChildP + 4]);
                        uint32 CPropLen = *reinterpret_cast<const uint32*>(&FileBytes[ChildP + 8]);
                        uint8 CNameLen = FileBytes[ChildP + 12];

                        if (CEnd == 0 || CEnd > SubEnd || ChildP + 13 + CNameLen > (int32)SubEnd) break;

                        FString CName = FString(CNameLen, (const ANSICHAR*)&FileBytes[ChildP + 13]);
                        int32 CPropsStart = ChildP + 13 + CNameLen;

                        if (CName.Equals(TEXT("Vertices")) && CPropsStart + 13 <= (int32)CEnd)
                        {
                            uint32 ArrayLen = *reinterpret_cast<const uint32*>(&FileBytes[CPropsStart]);
                            uint32 Encoding = *reinterpret_cast<const uint32*>(&FileBytes[CPropsStart + 4]);
                            uint32 CompLen = *reinterpret_cast<const uint32*>(&FileBytes[CPropsStart + 8]);
                            int32 DataOffset = CPropsStart + 12;
                            uint32 UncompSize = ArrayLen * sizeof(double);

                            const uint8* DataPtr = nullptr;
                            TArray<uint8> UncompBuf;
                            if (Encoding == 0 && DataOffset + (int32)UncompSize <= (int32)CEnd)
                            {
                                DataPtr = &FileBytes[DataOffset];
                            }
                            else if (Encoding == 1 && CompLen > 0 && DataOffset + (int32)CompLen <= (int32)CEnd)
                            {
                                UncompBuf.AddUninitialized(UncompSize);
                                if (FCompression::UncompressMemory(NAME_Zlib, (void*)UncompBuf.GetData(), (int64)UncompSize, (const void*)&FileBytes[DataOffset], (int64)CompLen))
                                {
                                    DataPtr = UncompBuf.GetData();
                                }
                            }

                            if (DataPtr && ArrayLen > 0)
                            {
                                const double* DblData = reinterpret_cast<const double*>(DataPtr);
                                for (uint32 v = 0; v + 2 < ArrayLen; v += 3)
                                {
                                    // Coordinate conversion: Right-handed to Unreal Left-handed (Y inverted)
                                    AllVerts.Add(FVector((float)DblData[v] * Scale, -(float)DblData[v + 1] * Scale, (float)DblData[v + 2] * Scale));
                                }
                            }
                        }
                        else if (CName.Equals(TEXT("PolygonVertexIndex")) && CPropsStart + 13 <= (int32)CEnd)
                        {
                            uint32 ArrayLen = *reinterpret_cast<const uint32*>(&FileBytes[CPropsStart]);
                            uint32 Encoding = *reinterpret_cast<const uint32*>(&FileBytes[CPropsStart + 4]);
                            uint32 CompLen = *reinterpret_cast<const uint32*>(&FileBytes[CPropsStart + 8]);
                            int32 DataOffset = CPropsStart + 12;
                            uint32 UncompSize = ArrayLen * sizeof(int32);

                            const uint8* DataPtr = nullptr;
                            TArray<uint8> UncompBuf;
                            if (Encoding == 0 && DataOffset + (int32)UncompSize <= (int32)CEnd)
                            {
                                DataPtr = &FileBytes[DataOffset];
                            }
                            else if (Encoding == 1 && CompLen > 0 && DataOffset + (int32)CompLen <= (int32)CEnd)
                            {
                                UncompBuf.AddUninitialized(UncompSize);
                                if (FCompression::UncompressMemory(NAME_Zlib, (void*)UncompBuf.GetData(), (int64)UncompSize, (const void*)&FileBytes[DataOffset], (int64)CompLen))
                                {
                                    DataPtr = UncompBuf.GetData();
                                }
                            }

                            if (DataPtr && ArrayLen > 0)
                            {
                                const int32* PIndices = reinterpret_cast<const int32*>(DataPtr);
                                TArray<int32> PolyLoop;
                                for (uint32 idx = 0; idx < ArrayLen; ++idx)
                                {
                                    int32 Val = PIndices[idx];
                                    bool bIsLast = (Val < 0);
                                    int32 RealVal = bIsLast ? (-Val - 1) : Val;
                                    PolyLoop.Add(RealVal);

                                    if (bIsLast)
                                    {
                                        int32 LoopCount = PolyLoop.Num();
                                        if (LoopCount >= 3)
                                        {
                                            for (int32 p = 1; p < LoopCount - 1; ++p)
                                            {
                                                AllPolys.Add(PolyLoop[0]);
                                                AllPolys.Add(PolyLoop[p]);
                                                AllPolys.Add(PolyLoop[p + 1]);
                                            }
                                        }
                                        PolyLoop.Empty();
                                    }
                                }
                            }
                        }

                        ChildP = CEnd;
                    }
                }
                else if (SubName.Equals(TEXT("Deformer")))
                {
                    int32 PPos = SubPropsStart;
                    int32 StrPos = PPos + 9;
                    FString DefLabel = TEXT("");
                    if (StrPos < SubChildrenStart && FileBytes[StrPos] == 'S' && StrPos + 5 <= SubChildrenStart)
                    {
                        uint32 SLen = *reinterpret_cast<const uint32*>(&FileBytes[StrPos + 1]);
                        if (StrPos + 5 + (int32)SLen <= SubChildrenStart)
                        {
                            DefLabel = FString(SLen, (const ANSICHAR*)&FileBytes[StrPos + 5]);
                            int32 NullIdx = -1;
                            if (DefLabel.FindChar('\0', NullIdx)) DefLabel = DefLabel.Left(NullIdx);
                            int32 ColonIdx = -1;
                            if (DefLabel.FindChar(':', ColonIdx)) DefLabel = DefLabel.Mid(ColonIdx + 1);
                        }
                    }

                    int32 CCurr = SubChildrenStart;
                    while (CCurr + 13 < (int32)SubEnd)
                    {
                        uint32 CEnd = *reinterpret_cast<const uint32*>(&FileBytes[CCurr]);
                        uint32 CNumProps = *reinterpret_cast<const uint32*>(&FileBytes[CCurr + 4]);
                        uint32 CPropLen = *reinterpret_cast<const uint32*>(&FileBytes[CCurr + 8]);
                        uint8 CNameLen = FileBytes[CCurr + 12];

                        if (CEnd == 0 || CEnd > SubEnd || CCurr + 13 + CNameLen > (int32)SubEnd) break;

                        FString CName = FString(CNameLen, (const ANSICHAR*)&FileBytes[CCurr + 13]);
                        int32 CPropsStart = CCurr + 13 + CNameLen;

                        if (CName.Equals(TEXT("Indexes")) && CPropsStart + 13 <= (int32)CEnd)
                        {
                            uint32 ArrayLen = *reinterpret_cast<const uint32*>(&FileBytes[CPropsStart]);
                            uint32 Encoding = *reinterpret_cast<const uint32*>(&FileBytes[CPropsStart + 4]);
                            uint32 CompLen = *reinterpret_cast<const uint32*>(&FileBytes[CPropsStart + 8]);
                            int32 DataOffset = CPropsStart + 12;
                            uint32 UncompSize = ArrayLen * sizeof(int32);

                            const uint8* DataPtr = nullptr;
                            TArray<uint8> UncompBuf;
                            if (Encoding == 0 && DataOffset + (int32)UncompSize <= (int32)CEnd)
                            {
                                DataPtr = &FileBytes[DataOffset];
                            }
                            else if (Encoding == 1 && CompLen > 0 && DataOffset + (int32)CompLen <= (int32)CEnd)
                            {
                                UncompBuf.AddUninitialized(UncompSize);
                                if (FCompression::UncompressMemory(NAME_Zlib, (void*)UncompBuf.GetData(), (int64)UncompSize, (const void*)&FileBytes[DataOffset], (int64)CompLen))
                                {
                                    DataPtr = UncompBuf.GetData();
                                }
                            }

                            if (DataPtr && ArrayLen > 0 && !DefLabel.IsEmpty())
                            {
                                const int32* IdxData = reinterpret_cast<const int32*>(DataPtr);
                                TArray<int32>& ClusterIndices = Clusters.FindOrAdd(DefLabel);
                                for (uint32 i = 0; i < ArrayLen; ++i)
                                {
                                    ClusterIndices.Add(IdxData[i]);
                                }
                            }
                        }
                        CCurr = CEnd;
                    }
                }

                Sub = SubEnd;
            }
        }
        Pos = NodeEnd;
    }

    if (Clusters.Num() == 0 || AllVerts.Num() == 0 || AllPolys.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[PiSimModelImporter] FBX Skin cluster bulunamadi!"));
        return false;
    }

    TArray<FString> ClusterKeys;
    Clusters.GetKeys(ClusterKeys);
    ClusterKeys.Sort([](const FString& A, const FString& B) {
        bool bAChassis = A.Contains(TEXT("chassis"), ESearchCase::IgnoreCase);
        bool bBChassis = B.Contains(TEXT("chassis"), ESearchCase::IgnoreCase);
        if (bAChassis != bBChassis) return bAChassis;
        return A < B;
    });

    for (int32 c = 0; c < ClusterKeys.Num(); ++c)
    {
        const FString& CName = ClusterKeys[c];
        const TArray<int32>& IdxArr = Clusters[CName];
        TSet<int32> VertSet(IdxArr);

        FImporterMeshSection MeshSec;
        MeshSec.MeshName = CName;
        MeshSec.ParentSectionIndex = (c == 0 ? -1 : 0);
        MeshSec.DepthLevel = (c == 0 ? 0 : 1);
        MeshSec.MassKg = (c == 0 ? 30.0f : 2.5f);
        MeshSec.Friction = 0.85f;
        MeshSec.MinAngle = -90.0f;
        MeshSec.MaxAngle = 90.0f;
        MeshSec.RotationAxis = FVector(0.0f, 1.0f, 0.0f); // Wheel spin Y-axis

        TMap<int32, int32> VertMap;
        for (int32 p = 0; p + 2 < AllPolys.Num(); p += 3)
        {
            int32 v0 = AllPolys[p];
            int32 v1 = AllPolys[p + 1];
            int32 v2 = AllPolys[p + 2];

            if (VertSet.Contains(v0) && VertSet.Contains(v1) && VertSet.Contains(v2))
            {
                int32 nv0 = VertMap.FindOrAdd(v0, MeshSec.Vertices.Num());
                if (nv0 == MeshSec.Vertices.Num()) { MeshSec.Vertices.Add(AllVerts[v0]); MeshSec.Normals.Add(FVector(0, 0, 1)); }

                int32 nv1 = VertMap.FindOrAdd(v1, MeshSec.Vertices.Num());
                if (nv1 == MeshSec.Vertices.Num()) { MeshSec.Vertices.Add(AllVerts[v1]); MeshSec.Normals.Add(FVector(0, 0, 1)); }

                int32 nv2 = VertMap.FindOrAdd(v2, MeshSec.Vertices.Num());
                if (nv2 == MeshSec.Vertices.Num()) { MeshSec.Vertices.Add(AllVerts[v2]); MeshSec.Normals.Add(FVector(0, 0, 1)); }

                MeshSec.Triangles.Add(nv0);
                MeshSec.Triangles.Add(nv1);
                MeshSec.Triangles.Add(nv2);
            }
        }

        if (MeshSec.Vertices.Num() > 0)
        {
            FVector Centroid = FVector::ZeroVector;
            for (const FVector& V : MeshSec.Vertices) Centroid += V;
            Centroid /= (float)MeshSec.Vertices.Num();
            MeshSec.PivotPoint = Centroid;

            for (FVector& V : MeshSec.Vertices) V -= Centroid;

            // =================================================================================
            // [İSTEK 2] ADINDA UCX GEÇENLERİ UCXSections'A, GEÇMEYENLERİ VisualSections'A AT
            // =================================================================================
            if (MeshSec.MeshName.StartsWith(TEXT("UCX_"), ESearchCase::IgnoreCase) ||
                MeshSec.MeshName.StartsWith(TEXT("UBX_"), ESearchCase::IgnoreCase) ||
                MeshSec.MeshName.StartsWith(TEXT("USP_"), ESearchCase::IgnoreCase))
            {
                OutUCX.Add(MeshSec);
                UE_LOG(LogTemp, Warning, TEXT("[PiSimModelImporter] UCX Çarpışma Parçası Yakalandı: %s (%d Vertices)"),
                    *MeshSec.MeshName, MeshSec.Vertices.Num());
            }
            else
            {
                OutVisual.Add(MeshSec);
                UE_LOG(LogTemp, Warning, TEXT("[PiSimModelImporter] Görsel Parça Yakalandı: %s (%d Vertices, %d Tris)"),
                    *MeshSec.MeshName, MeshSec.Vertices.Num(), MeshSec.Triangles.Num() / 3);
            }
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
    // Hedef bileşenler: Varsa CollisionMeshComponents, yoksa VisualMeshComponents
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
