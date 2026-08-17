// PiSimGarageRobot.cpp
#include "PiSimGarageRobot.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/Engine.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "TextureResource.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Compression.h"
#include "Serialization/BufferArchive.h"
#include "ProceduralMeshComponent.h"


#include "KismetProceduralMeshLibrary.h"
#include "Materials/MaterialInterface.h"
#include "Serialization/JsonSerializer.h"
#include "JsonObjectConverter.h"
#include "PiSimHUD.h"
#include "Components/InputComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"

#pragma pack(push, 1)
struct FGLBHeader
{
    uint32 Magic;
    uint32 Version;
    uint32 Length;
};

struct FGLBChunkHeader
{
    uint32 ChunkLength;
    uint32 ChunkType;
};
#pragma pack(pop)

APiSimGarageRobot::APiSimGarageRobot()

{
    PrimaryActorTick.bCanEverTick = true;
    AutoPossessPlayer = EAutoReceiveInput::Player0;

    // Create Physics Collision Skeletal Mesh Component (robot_collision.fbx)
    CollisionSkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CollisionSkeletalMeshComponent"));
    CollisionSkeletalMeshComponent->SetupAttachment(RootComponent);
    CollisionSkeletalMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    CollisionSkeletalMeshComponent->SetCollisionObjectType(ECC_WorldDynamic);

    // Create High-Poly Visual Skeletal Mesh Component (robot_visual.fbx - Syncs pose with Leader Component)
    VisualSkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("VisualSkeletalMeshComponent"));
    VisualSkeletalMeshComponent->SetupAttachment(CollisionSkeletalMeshComponent);
    VisualSkeletalMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    VisualSkeletalMeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);

    // Create SpaceX Configurator 360 Orbit Camera System
    ConfiguratorSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("ConfiguratorSpringArm"));
    ConfiguratorSpringArm->SetupAttachment(RootComponent);
    ConfiguratorSpringArm->TargetArmLength = 350.0f; // 3.5 meters framing
    ConfiguratorSpringArm->SetRelativeRotation(FRotator(-20.0f, 45.0f, 0.0f));
    ConfiguratorSpringArm->bUsePawnControlRotation = false;
    ConfiguratorSpringArm->bDoCollisionTest = false;
    ConfiguratorSpringArm->bEnableCameraLag = true;
    ConfiguratorSpringArm->CameraLagSpeed = 12.0f;

    ConfiguratorOrbitCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ConfiguratorOrbitCamera"));
    ConfiguratorOrbitCamera->SetupAttachment(ConfiguratorSpringArm, USpringArmComponent::SocketName);

    // Attach FPV Camera Capture Component
    FpvCameraComponent = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("FpvCameraComponent"));
    FpvCameraComponent->SetupAttachment(RootComponent);
    FpvCameraComponent->SetRelativeLocation(FVector(60.0f, 0.0f, 10.0f));
    FpvCameraComponent->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));
    FpvCameraComponent->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
    FpvCameraComponent->bCaptureEveryFrame = true;
    FpvCameraComponent->bCaptureOnMovement = false;

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> InvisMatFinder(TEXT("/Engine/Engine_MI_Shaders/Instances/M_Shader_SimpleTranslucent_Invis.M_Shader_SimpleTranslucent_Invis"));
    if (InvisMatFinder.Succeeded())
    {
        TranslucentMaterial = InvisMatFinder.Object;
        InvisibleMaterial = InvisMatFinder.Object;
    }
    else
    {
        static ConstructorHelpers::FObjectFinder<UMaterialInterface> GameInvisFinder(TEXT("/Game/SimBlank/Materials/M_PiSim_İnvis.M_PiSim_İnvis"));
        if (GameInvisFinder.Succeeded())
        {
            TranslucentMaterial = GameInvisFinder.Object;
            InvisibleMaterial = GameInvisFinder.Object;
        }
    }
}

static bool ParseGlbAllBinaryMeshes(const FString& GlbFilePath, TArray<FGLBMeshSection>& OutSections, float ScaleMultiplier)
{
    TArray<uint8> FileData;
    if (!FFileHelper::LoadFileToArray(FileData, *GlbFilePath) || FileData.Num() < sizeof(FGLBHeader) + sizeof(FGLBChunkHeader))
    {
        return false;
    }

    const FGLBHeader* Header = reinterpret_cast<const FGLBHeader*>(FileData.GetData());
    if (Header->Magic != 0x46546C67) // "glTF"
    {
        return false;
    }

    const uint8* Ptr = FileData.GetData() + sizeof(FGLBHeader);
    const FGLBChunkHeader* JsonChunk = reinterpret_cast<const FGLBChunkHeader*>(Ptr);
    Ptr += sizeof(FGLBChunkHeader);

    if (JsonChunk->ChunkType != 0x4E4F534A) // "JSON"
    {
        return false;
    }

    FString JsonStr = FString(JsonChunk->ChunkLength, reinterpret_cast<const UTF8CHAR*>(Ptr));
    Ptr += JsonChunk->ChunkLength;

    if (Ptr >= FileData.GetData() + FileData.Num())
    {
        return false;
    }

    const FGLBChunkHeader* BinChunk = reinterpret_cast<const FGLBChunkHeader*>(Ptr);
    Ptr += sizeof(FGLBChunkHeader);

    const uint8* BinBufferData = Ptr;

    // Parse JSON metadata
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* Accessors = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* BufferViews = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* Meshes = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;

    if (!JsonObject->TryGetArrayField(TEXT("accessors"), Accessors) || 
        !JsonObject->TryGetArrayField(TEXT("bufferViews"), BufferViews) ||
        !JsonObject->TryGetArrayField(TEXT("meshes"), Meshes) ||
        Meshes->Num() == 0)
    {
        return false;
    }

    JsonObject->TryGetArrayField(TEXT("nodes"), Nodes);

    // Map Node Index -> Parent Node Index & Node Index -> Mesh Index across ALL hierarchy levels
    TMap<int32, int32> NodeParents; // ChildNodeIdx -> ParentNodeIdx
    TMap<int32, int32> NodeToMesh;  // NodeIdx -> MeshIdx
    TMap<int32, FVector> NodeTranslations;

    if (Nodes)
    {
        for (int32 NodeIdx = 0; NodeIdx < Nodes->Num(); ++NodeIdx)
        {
            TSharedPtr<FJsonObject> NodeObj = (*Nodes)[NodeIdx]->AsObject();

            if (NodeObj->HasField(TEXT("mesh")))
            {
                int32 MeshIdx = NodeObj->GetIntegerField(TEXT("mesh"));
                NodeToMesh.Add(NodeIdx, MeshIdx);
            }

            FVector Trans(0.0f, 0.0f, 0.0f);
            const TArray<TSharedPtr<FJsonValue>>* TransArr = nullptr;
            if (NodeObj->TryGetArrayField(TEXT("translation"), TransArr) && TransArr->Num() >= 3)
            {
                Trans.X = (*TransArr)[0]->AsNumber();
                Trans.Y = (*TransArr)[1]->AsNumber();
                Trans.Z = (*TransArr)[2]->AsNumber();
            }
            NodeTranslations.Add(NodeIdx, Trans);

            const TArray<TSharedPtr<FJsonValue>>* ChildrenArr = nullptr;
            if (NodeObj->TryGetArrayField(TEXT("children"), ChildrenArr))
            {
                for (int32 c = 0; c < ChildrenArr->Num(); ++c)
                {
                    int32 ChildNodeIdx = (*ChildrenArr)[c]->AsNumber();
                    NodeParents.Add(ChildNodeIdx, NodeIdx);
                }
            }
        }
    }

    // Map MeshIdx -> ParentMeshIdx by traversing UP through multi-level ancestor nodes
    TMap<int32, int32> MeshParents; // ChildMeshIdx -> ParentMeshIdx
    TMap<int32, FVector> AccumulatedWorldNodeTranslations;

    for (const TPair<int32, int32>& NodeMeshPair : NodeToMesh)
    {
        int32 NodeIdx = NodeMeshPair.Key;
        int32 MeshIdx = NodeMeshPair.Value;

        // Compute ACCUMULATED WORLD TRANSLATION by walking UP through all ancestor nodes
        FVector WorldTrans = FVector::ZeroVector;
        int32 CurrNode = NodeIdx;
        while (CurrNode >= 0)
        {
            if (NodeTranslations.Contains(CurrNode))
            {
                WorldTrans += NodeTranslations[CurrNode];
            }
            CurrNode = NodeParents.Contains(CurrNode) ? NodeParents[CurrNode] : -1;
        }
        AccumulatedWorldNodeTranslations.Add(MeshIdx, WorldTrans);

        // Find parent mesh index
        CurrNode = NodeIdx;
        while (NodeParents.Contains(CurrNode))
        {
            int32 ParentNodeIdx = NodeParents[CurrNode];
            if (NodeToMesh.Contains(ParentNodeIdx))
            {
                int32 ParentMeshIdx = NodeToMesh[ParentNodeIdx];
                MeshParents.Add(MeshIdx, ParentMeshIdx);
                break; // Found nearest ancestor mesh!
            }
            CurrNode = ParentNodeIdx;
        }
    }

    TMap<int32, FString> NodeNamesForMeshes;
    if (Nodes)
    {
        for (int32 NodeIdx = 0; NodeIdx < Nodes->Num(); ++NodeIdx)
        {
            TSharedPtr<FJsonObject> NodeObj = (*Nodes)[NodeIdx]->AsObject();
            if (NodeObj->HasField(TEXT("mesh")) && NodeObj->HasField(TEXT("name")))
            {
                int32 MIdx = NodeObj->GetIntegerField(TEXT("mesh"));
                FString NName = NodeObj->GetStringField(TEXT("name"));
                if (!NName.IsEmpty() && (!NodeNamesForMeshes.Contains(MIdx) || NName.StartsWith(TEXT("CM_")) || NName.StartsWith(TEXT("S_"))))
                {
                    NodeNamesForMeshes.Add(MIdx, NName);
                }
            }
        }
    }

    TMap<int32, int32> MeshToSectionMap; // MeshIdx -> SectionIndex

    // Iterate over ALL meshes in the GLB hierarchy
    for (int32 MeshIdx = 0; MeshIdx < Meshes->Num(); ++MeshIdx)
    {
        TSharedPtr<FJsonObject> MeshObj = (*Meshes)[MeshIdx]->AsObject();
        FString MeshName = NodeNamesForMeshes.Contains(MeshIdx) ? NodeNamesForMeshes[MeshIdx] :
                           (MeshObj->HasField(TEXT("name")) ? MeshObj->GetStringField(TEXT("name")) : FString::Printf(TEXT("Mesh_%d"), MeshIdx));

        const TArray<TSharedPtr<FJsonValue>>* Primitives = nullptr;
        if (!MeshObj->TryGetArrayField(TEXT("primitives"), Primitives) || Primitives->Num() == 0)
        {
            continue;
        }

        // Use ACCUMULATED WORLD TRANSLATION for pristine initial CAD position alignment
        FVector WorldTrans = AccumulatedWorldNodeTranslations.Contains(MeshIdx) ? AccumulatedWorldNodeTranslations[MeshIdx] : FVector::ZeroVector;

        for (int32 PrimIdx = 0; PrimIdx < Primitives->Num(); ++PrimIdx)
        {
            TSharedPtr<FJsonObject> PrimObj = (*Primitives)[PrimIdx]->AsObject();
            const TSharedPtr<FJsonObject>* AttributesObj = nullptr;
            if (!PrimObj->TryGetObjectField(TEXT("attributes"), AttributesObj))
            {
                continue;
            }

            int32 PosAccessorIdx = -1;
            if ((*AttributesObj)->HasField(TEXT("POSITION")))
            {
                PosAccessorIdx = (*AttributesObj)->GetIntegerField(TEXT("POSITION"));
            }
            if (PosAccessorIdx < 0 || PosAccessorIdx >= Accessors->Num())
            {
                continue;
            }

            int32 IndexAccessorIdx = -1;
            if (PrimObj->HasField(TEXT("indices")))
            {
                IndexAccessorIdx = PrimObj->GetIntegerField(TEXT("indices"));
            }

            TArray<FVector> RawVertices;
            TArray<int32> RawTriangles;

            // Extract Raw Vertices (Converting glTF Right-Handed Y-Up to UE5 Left-Handed Z-Up)
            TSharedPtr<FJsonObject> PosAcc = (*Accessors)[PosAccessorIdx]->AsObject();
            int32 VertexCount = PosAcc->GetIntegerField(TEXT("count"));
            int32 PosBVIdx = PosAcc->GetIntegerField(TEXT("bufferView"));
            int32 PosAccOffset = PosAcc->HasField(TEXT("byteOffset")) ? PosAcc->GetIntegerField(TEXT("byteOffset")) : 0;

            TSharedPtr<FJsonObject> PosBV = (*BufferViews)[PosBVIdx]->AsObject();
            int32 PosBVOffset = PosBV->HasField(TEXT("byteOffset")) ? PosBV->GetIntegerField(TEXT("byteOffset")) : 0;

            const float* PosPtr = reinterpret_cast<const float*>(BinBufferData + PosBVOffset + PosAccOffset);
            for (int32 i = 0; i < VertexCount; ++i)
            {
                float RawX = PosPtr[i * 3 + 0] + WorldTrans.X;
                float RawY = PosPtr[i * 3 + 1] + WorldTrans.Y;
                float RawZ = PosPtr[i * 3 + 2] + WorldTrans.Z;

                // EXACT glTF -> UE5 Handedness Coordinate Mapping: UE_X = -RawZ, UE_Y = RawX, UE_Z = RawY
                float X = -RawZ * ScaleMultiplier;
                float Y = RawX * ScaleMultiplier;
                float Z = RawY * ScaleMultiplier;
                RawVertices.Add(FVector(X, Y, Z));
            }

            // Extract Triangles Indices
            if (IndexAccessorIdx >= 0 && IndexAccessorIdx < Accessors->Num())
            {
                TSharedPtr<FJsonObject> IndexAcc = (*Accessors)[IndexAccessorIdx]->AsObject();
                int32 IndexCount = IndexAcc->GetIntegerField(TEXT("count"));
                int32 IndexBVIdx = IndexAcc->GetIntegerField(TEXT("bufferView"));
                int32 IndexComponentType = IndexAcc->GetIntegerField(TEXT("componentType"));
                int32 IndexAccOffset = IndexAcc->HasField(TEXT("byteOffset")) ? IndexAcc->GetIntegerField(TEXT("byteOffset")) : 0;

                TSharedPtr<FJsonObject> IndexBV = (*BufferViews)[IndexBVIdx]->AsObject();
                int32 IndexBVOffset = IndexBV->HasField(TEXT("byteOffset")) ? IndexBV->GetIntegerField(TEXT("byteOffset")) : 0;

                const uint8* IndexDataPtr = BinBufferData + IndexBVOffset + IndexAccOffset;

                for (int32 i = 0; i < IndexCount; ++i)
                {
                    int32 IndexVal = -1;
                    if (IndexComponentType == 5121) // uint8
                    {
                        IndexVal = static_cast<int32>(reinterpret_cast<const uint8*>(IndexDataPtr)[i]);
                    }
                    else if (IndexComponentType == 5123) // uint16
                    {
                        IndexVal = static_cast<int32>(reinterpret_cast<const uint16*>(IndexDataPtr)[i]);
                    }
                    else if (IndexComponentType == 5125) // uint32
                    {
                        IndexVal = static_cast<int32>(reinterpret_cast<const uint32*>(IndexDataPtr)[i]);
                    }

                    if (IndexVal >= 0 && IndexVal < RawVertices.Num())
                    {
                        RawTriangles.Add(IndexVal);
                    }
                }

                int32 Extra = RawTriangles.Num() % 3;
                if (Extra > 0)
                {
                    RawTriangles.RemoveAt(RawTriangles.Num() - Extra, Extra);
                }
            }
            else
            {
                for (int32 i = 0; i < RawVertices.Num(); ++i)
                {
                    RawTriangles.Add(i);
                }
            }

            // UNROLL TRIANGLES FOR PER-FACE GEOMETRIC NORMAL & FACE ORIENTATION CALCULATION
            FGLBMeshSection Section;
            Section.MeshName = FString::Printf(TEXT("%s_Prim%d"), *MeshName, PrimIdx);
            Section.PivotPoint = FVector(-WorldTrans.Z * ScaleMultiplier, WorldTrans.X * ScaleMultiplier, WorldTrans.Y * ScaleMultiplier);

            for (int32 i = 0; i + 2 < RawTriangles.Num(); i += 3)
            {
                int32 i0 = RawTriangles[i];
                int32 i1 = RawTriangles[i + 2]; // SWAP INDICES FOR LEFT-HANDED COORDINATE WINDING CORRECTION
                int32 i2 = RawTriangles[i + 1];

                if (RawVertices.IsValidIndex(i0) && RawVertices.IsValidIndex(i1) && RawVertices.IsValidIndex(i2))
                {
                    FVector V0 = RawVertices[i0];
                    FVector V1 = RawVertices[i1];
                    FVector V2 = RawVertices[i2];

                    // CALCULATE EXACT PER-FACE GEOMETRIC NORMAL FROM TRIANGLE EDGES
                    FVector FaceNormal = FVector::CrossProduct(V1 - V0, V2 - V0).GetSafeNormal();
                    if (FaceNormal.IsNearlyZero())
                    {
                        FaceNormal = FVector(0, 0, 1);
                    }

                    int32 BaseIdx = Section.Vertices.Num();

                    Section.Vertices.Add(V0);
                    Section.Vertices.Add(V1);
                    Section.Vertices.Add(V2);

                    Section.Normals.Add(FaceNormal);
                    Section.Normals.Add(FaceNormal);
                    Section.Normals.Add(FaceNormal);

                    Section.Triangles.Add(BaseIdx + 0);
                    Section.Triangles.Add(BaseIdx + 1);
                    Section.Triangles.Add(BaseIdx + 2);
                }
            }

            if (Section.Vertices.Num() > 0 && Section.Triangles.Num() >= 3)
            {
                int32 NewSecIdx = OutSections.Num();
                OutSections.Add(Section);
                MeshToSectionMap.Add(MeshIdx, NewSecIdx);
            }
        }
    }

    // Link Parent-Child Hierarchy Relationships across ALL ancestor levels
    for (const TPair<int32, int32>& MeshParentPair : MeshParents)
    {
        int32 ChildMeshIdx = MeshParentPair.Key;
        int32 ParentMeshIdx = MeshParentPair.Value;

        if (MeshToSectionMap.Contains(ChildMeshIdx) && MeshToSectionMap.Contains(ParentMeshIdx))
        {
            int32 ChildSecIdx = MeshToSectionMap[ChildMeshIdx];
            int32 ParentSecIdx = MeshToSectionMap[ParentMeshIdx];

            if (OutSections.IsValidIndex(ChildSecIdx))
            {
                OutSections[ChildSecIdx].ParentSectionIndex = ParentSecIdx;
            }
        }
    }

    // Calculate Hierarchy DepthLevel for each section
    for (int32 i = 0; i < OutSections.Num(); ++i)
    {
        int32 Depth = 0;
        int32 ParentIdx = OutSections[i].ParentSectionIndex;
        while (ParentIdx >= 0 && OutSections.IsValidIndex(ParentIdx) && Depth < 20)
        {
            Depth++;
            ParentIdx = OutSections[ParentIdx].ParentSectionIndex;
        }
        OutSections[i].DepthLevel = Depth;
    }

    // Reorder OutSections topologically so root meshes (DepthLevel == 0, Main Chassis / Parent Mesh)
    // are ALWAYS at index 0, followed by children and grandchildren in top-down tree order!
    TArray<FGLBMeshSection> SortedSections;
    TMap<int32, int32> OldToNewSecMap;

    for (int32 TargetDepth = 0; TargetDepth <= 20; ++TargetDepth)
    {
        for (int32 oldI = 0; oldI < OutSections.Num(); ++oldI)
        {
            if (OutSections[oldI].DepthLevel == TargetDepth)
            {
                int32 newI = SortedSections.Num();
                OldToNewSecMap.Add(oldI, newI);
                SortedSections.Add(OutSections[oldI]);
            }
        }
    }

    for (int32 i = 0; i < SortedSections.Num(); ++i)
    {
        int32 OldParent = SortedSections[i].ParentSectionIndex;
        if (OldParent >= 0 && OldToNewSecMap.Contains(OldParent))
        {
            SortedSections[i].ParentSectionIndex = OldToNewSecMap[OldParent];
        }
        else
        {
            SortedSections[i].ParentSectionIndex = -1;
        }
    }

    OutSections = SortedSections;

    return OutSections.Num() > 0;
}

void APiSimGarageRobot::BeginPlay()
{
    Super::BeginPlay();

    // Ensure APiSimHUD is spawned automatically for UI overlays
    if (GetWorld())
    {
        APlayerController* PC = GetWorld()->GetFirstPlayerController();
        if (PC && !Cast<APiSimHUD>(PC->MyHUD))
        {
            APiSimHUD* NewHUD = GetWorld()->SpawnActor<APiSimHUD>(APiSimHUD::StaticClass());
            if (NewHUD)
            {
                PC->MyHUD = NewHUD;
            }
        }
    }

    // Load or generate robot_config.json
    LoadConfig();

    // Keep robot fixed in air for clean joint testing
    if (ChassisMeshComponent)
    {
        ChassisMeshComponent->SetMobility(EComponentMobility::Movable);
        ChassisMeshComponent->SetSimulatePhysics(false);
        ChassisMeshComponent->SetEnableGravity(false);
    }

    // Check disk paths for FBX and GLB files in Saved/Robots/Cache/
    FString FbxFullPath = FPaths::ProjectSavedDir() / TEXT("Robots/Cache/robot_collision.fbx");
    if (!FPaths::FileExists(FbxFullPath))
    {
        FbxFullPath = FPaths::ProjectSavedDir() / TEXT("Robots/Cache/robot.fbx");
    }
    FString GlbFullPath = FPaths::ProjectSavedDir() / TEXT("Robots/Cache/robot.glb");

    bool bFbxExists = FPaths::FileExists(FbxFullPath);
    bool bGlbExists = FPaths::FileExists(GlbFullPath);

    TArray<FGLBMeshSection> Sections;
    bool bLoadedDiskModel = false;

    // 0) PRE-COOKED BINARY CACHE LOADER: Fast 0.001s Instant Loading from Saved/Robots/Baked/ (Auto-invalidated if FBX is newer)
    FString BakedFilePath = FPaths::ProjectSavedDir() / TEXT("Robots/Baked/robot_collision_baked.bin");
    if (!FPaths::FileExists(BakedFilePath))
    {
        BakedFilePath = FPaths::ProjectSavedDir() / TEXT("Robots/Baked/robot_baked.bin");
    }
    bool bBakedValid = FPaths::FileExists(BakedFilePath);
    if (bBakedValid && bFbxExists)
    {
        FDateTime FbxTime = FPlatformFileManager::Get().GetPlatformFile().GetTimeStamp(*FbxFullPath);
        FDateTime BakedTime = FPlatformFileManager::Get().GetPlatformFile().GetTimeStamp(*BakedFilePath);
        if (FbxTime > BakedTime)
        {
            bBakedValid = false; // Invalidate stale baked file!
            IFileManager::Get().Delete(*BakedFilePath);
            UE_LOG(LogTemp, Warning, TEXT("[BeginPlay] Detected NEWER FBX model file! Deleted stale baked cache."));
        }
    }

    if (bBakedValid)
    {
        TArray<uint8> BakedBytes;
        if (FFileHelper::LoadFileToArray(BakedBytes, *BakedFilePath))
        {
            FMemoryReader Ar(BakedBytes, true);
            int32 SecCount = 0;
            Ar << SecCount;

            for (int32 s = 0; s < SecCount; ++s)
            {
                FGLBMeshSection Sec;
                Ar << Sec.MeshName;
                Ar << Sec.ParentSectionIndex;
                Ar << Sec.DepthLevel;
                Ar << Sec.PivotPoint;
                Ar << Sec.MassKg;
                Ar << Sec.Friction;
                Ar << Sec.JointType;
                Ar << Sec.Vertices;
                Ar << Sec.Triangles;
                Ar << Sec.Normals;
                Sections.Add(Sec);
            }

            if (Sections.Num() > 0)
            {
                bLoadedDiskModel = true;
                LoadedModelFormatName = TEXT("Pre-Cooked Binary Cache (.bin) [Use Complex Collision As Simple]");
                if (GEngine)
                {
                    GEngine->AddOnScreenDebugMessage(-1, 20.0f, FColor::Green,
                        FString::Printf(TEXT(">>> [PRE-COOKED CACHE AKTİF!] Saved/Robots/Baked/robot_baked.bin İLE %d ALT PARÇA 0.001sn ANINDA YÜKLENDİ! <<<"), Sections.Num()));
                }
            }

        }
    }

    // 1) DISK FBX LOADER: Check if robot.fbx exists in Saved/Robots/Cache/
    if (!bLoadedDiskModel && bFbxExists)
    {
        if (ParseFbxAllBinaryMeshes(FbxFullPath, Sections, CadUnitScaleMultiplier))
        {
            bLoadedDiskModel = true;
            LoadedModelFormatName = TEXT("FBX (.fbx) [Saved/Robots/Cache/robot.fbx]");
            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Green,
                    TEXT(">>> [DISK FBX LOADER] Saved/Robots/Cache/robot.fbx DISKTEN OKUNDU VE YÜKLENDİ! <<<"));
            }
        }
    }


    // 2) DISK GLB FALLBACK LOADER: If robot.fbx does not exist, check robot.glb
    if (!bLoadedDiskModel && bGlbExists)
    {
        if (ParseGlbAllBinaryMeshes(GlbFullPath, Sections, CadUnitScaleMultiplier))
        {
            bLoadedDiskModel = true;
            LoadedModelFormatName = TEXT("GLB (.glb) [Saved/Robots/Cache/robot.glb]");
            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Cyan,
                    TEXT(">>> [DISK GLB LOADER] Saved/Robots/Cache/robot.glb DISKTEN YÜKLENDİ! <<<"));
            }
        }
    }

    // 3) UNREAL STATIC MESH ASSET FALLBACK
    if (!bLoadedDiskModel)
    {
        if (!FbxRobotMeshAsset) FbxRobotMeshAsset = RobotMeshAsset;
        if (!FbxRobotMeshAsset) FbxRobotMeshAsset = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Robots/robot_fbx.robot_fbx"));
        if (FbxRobotMeshAsset)
        {
            ChassisMeshComponent->SetStaticMesh(FbxRobotMeshAsset);
            ChassisMeshComponent->SetVisibility(true);
            ChassisMeshComponent->SetHiddenInGame(false);
            ChassisMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            ChassisMeshComponent->SetCollisionObjectType(ECC_WorldDynamic);
            ChassisMeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
            ChassisMeshComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);

            bLoadedDiskModel = true;
            LoadedModelFormatName = FString::Printf(TEXT("FBX ASSET (%s) [Unreal Content]"), *FbxRobotMeshAsset->GetName());
        }
    }


    if (bLoadedDiskModel && Sections.Num() > 0)
    {
        UMaterialInterface* DefaultMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

        int32 TotalVerts = 0;
        int32 TotalTris = 0;


            LoadedMeshSections = Sections;
            SubMeshComponents.Empty();
            SubMeshNames.Empty();
            OriginalSubMeshVertices.Empty();
            OriginalSubMeshNormals.Empty();
            ParentJointIndices.Empty();
            DepthLevels.Empty();
            JointPivotPoints.Empty();
            JointLimitsList.Empty();

            for (int32 SecIdx = 0; SecIdx < Sections.Num(); ++SecIdx)
            {
                FName CompName = *FString::Printf(TEXT("CADSubMesh_%d_%s"), SecIdx, *Sections[SecIdx].MeshName);
                UProceduralMeshComponent* SubComp = NewObject<UProceduralMeshComponent>(this, CompName);

                SubComp->SetMobility(EComponentMobility::Movable);

                int32 ParentIdx = Sections[SecIdx].ParentSectionIndex;
                if (ParentIdx >= 0 && SubMeshComponents.IsValidIndex(ParentIdx) && SubMeshComponents[ParentIdx])
                {
                    SubComp->SetupAttachment(SubMeshComponents[ParentIdx]);
                    SubComp->SetRelativeLocation(Sections[SecIdx].PivotPoint - Sections[ParentIdx].PivotPoint);
                }
                else
                {
                    SubComp->SetupAttachment(RootComponent);
                    SubComp->SetRelativeLocation(Sections[SecIdx].PivotPoint);
                }

                SubComp->RegisterComponent();



                FString MeshName = Sections[SecIdx].MeshName;
                bool bIsCMOnly = MeshName.StartsWith(TEXT("CM_"), ESearchCase::IgnoreCase);
                bool bIsStructural = bIsCMOnly || MeshName.StartsWith(TEXT("COG"), ESearchCase::IgnoreCase) || MeshName.StartsWith(TEXT("COL"), ESearchCase::IgnoreCase) || MeshName.StartsWith(TEXT("Cube"), ESearchCase::IgnoreCase) || MeshName.StartsWith(TEXT("Stick"), ESearchCase::IgnoreCase) || MeshName.StartsWith(TEXT("Buckett"), ESearchCase::IgnoreCase) || bLoadedDiskModel;

                TArray<FVector2D> UV0;
                TArray<FLinearColor> VertexColors;
                TArray<FProcMeshTangent> Tangents;

                // Configure Procedural Mesh collision and rendering settings directly during GLTF/FBX Model Import
                SubComp->CastShadow = true;
                SubComp->bCastDynamicShadow = true;
                SubComp->bAffectDistanceFieldLighting = false;

                SubComp->CreateMeshSection_LinearColor(0, Sections[SecIdx].Vertices, Sections[SecIdx].Triangles, Sections[SecIdx].Normals, UV0, VertexColors, Tangents, true);
                if (DefaultMat)
                {
                    SubComp->SetMaterial(0, DefaultMat);
                }

                if (bIsCMOnly || bIsStructural)
                {
                    SubComp->ClearCollisionConvexMeshes();
                    SubComp->AddCollisionConvexMesh(Sections[SecIdx].Vertices);
                    SubComp->bUseComplexAsSimpleCollision = false; // Simple Collision uses Convex Hulls (FKConvexElem)
                    SubComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
                    SubComp->SetCollisionObjectType(ECC_WorldDynamic);
                    SubComp->SetCollisionResponseToAllChannels(ECR_Block);
                    SubComp->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
                    SubComp->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
                    SubComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
                }
                else
                {
                    SubComp->bUseComplexAsSimpleCollision = false;
                    SubComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                    SubComp->SetCollisionResponseToAllChannels(ECR_Ignore);
                }

                SubComp->RecreatePhysicsState();
                SubComp->UpdateBounds();

                SubComp->SetVisibility(true);
                SubComp->SetHiddenInGame(false);

                SubMeshComponents.Add(SubComp);
                SubMeshNames.Add(Sections[SecIdx].MeshName);
                OriginalSubMeshVertices.Add(Sections[SecIdx].Vertices); // Store unrotated base vertices!
                OriginalSubMeshNormals.Add(Sections[SecIdx].Normals);   // Store unrotated base face normals!
                ParentJointIndices.Add(Sections[SecIdx].ParentSectionIndex);
                DepthLevels.Add(Sections[SecIdx].DepthLevel);
                JointPivotPoints.Add(Sections[SecIdx].PivotPoint);      // Store exact Accumulated Blender Node Origin / Pivot Point!
                MeshCategories.Add(bIsStructural ? EMeshCategoryType::Structural : EMeshCategoryType::Visual);

                FPiSimJointLimits Limits;
                Limits.MinAngle = -90.0f;
                Limits.MaxAngle = 90.0f;
                Limits.CurrentAngle = 0.0f;
                Limits.RotationAxis = FVector(0, 1, 0);
                Limits.RotationAxisName = TEXT("Y");
                Limits.bInvertAxis = false;
                JointLimitsList.Add(Limits);


                TotalVerts += Sections[SecIdx].Vertices.Num();
                TotalTris += Sections[SecIdx].Triangles.Num() / 3;
            }

            // Disable self-collision between internal sub-mesh components so they interact ONLY with external environment/floors/pawns!
            for (int32 i = 0; i < SubMeshComponents.Num(); ++i)
            {
                for (int32 j = i + 1; j < SubMeshComponents.Num(); ++j)
                {
                    if (SubMeshComponents[i] && SubMeshComponents[j])
                    {
                        SubMeshComponents[i]->IgnoreComponentWhenMoving(SubMeshComponents[j], true);
                        SubMeshComponents[j]->IgnoreComponentWhenMoving(SubMeshComponents[i], true);
                    }
                }
            }

            if (ChassisMeshComponent)
            {
                ChassisMeshComponent->SetVisibility(false);
                ChassisMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                ChassisMeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
            }






            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 20.0f, FColor::Green,
                    FString::Printf(TEXT(">>> [GARAJ PAWN] %d BAĞIMSIZ ALT PARÇA SAHNEYE EKLENDİ! (Accumulated World Node Transformations) <<<"),
                        SubMeshComponents.Num()));
            }
        }

    if (!bLoadedDiskModel && RobotMeshAsset)

    {
        ChassisMeshComponent->SetStaticMesh(RobotMeshAsset);
        ChassisMeshComponent->SetVisibility(true);
        UE_LOG(LogTemp, Warning, TEXT("[APiSimGarageRobot] Custom 3D Robot Mesh Asset '%s' SPAWNED ON SCENE!"), *RobotMeshAsset->GetName());
    }


    // Initialize autonomous runtime Render Target if not assigned
    if (!VideoRenderTarget)
    {
        VideoRenderTarget = NewObject<UTextureRenderTarget2D>(this);
        VideoRenderTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
        VideoRenderTarget->ClearColor = FLinearColor::Black;
        VideoRenderTarget->TargetGamma = 2.2f;
        VideoRenderTarget->bAutoGenerateMips = false;
        VideoRenderTarget->InitCustomFormat(320, 240, PF_B8G8R8A8, false);
        VideoRenderTarget->UpdateResourceImmediate(true);
    }

    if (FpvCameraComponent)
    {
        FpvCameraComponent->TextureTarget = VideoRenderTarget;
        FpvCameraComponent->bCaptureEveryFrame = true;
    }

    if (VisualSkeletalMeshComponent && CollisionSkeletalMeshComponent)
    {
        VisualSkeletalMeshComponent->SetLeaderPoseComponent(CollisionSkeletalMeshComponent);
    }

    // Initialize UDP Network Manager for FPV stream & JSON broadcast
    UDPManager = MakeUnique<FPiSimUDPManager>();
    UDPManager->ReserveVideoSocket(5000);

    // Broadcast JSON config payload over UDP Port 7402 to Pi 5
    FString JsonStr = UPiSimConfigManager::RobotConfigToJsonString(RobotConfig);
    if (!JsonStr.IsEmpty() && UDPManager)
    {
        FTCHARToUTF8 Converter(*JsonStr);
        TArray<uint8> JsonBytes;
        JsonBytes.Append(reinterpret_cast<const uint8*>(Converter.Get()), Converter.Length());
        UDPManager->SendControlData(JsonBytes, TEXT("192.168.1.20"), 7402);
    }

    InitialChassisLocation = GetActorLocation();
    InitialChassisRotation = GetActorRotation();
    ClassifySubMeshes();
    SetGarageViewMode(EGarageViewMode::Visual);
    SetPhysicsTestMode(EPhysicsTestMode::None);
}

void APiSimGarageRobot::SetJointAngleClamped(int32 SectionIndex, float TargetAngleDegrees, FVector RotationAxis, bool bInvertAxis)
{
    if (!SubMeshComponents.IsValidIndex(SectionIndex) || !SubMeshComponents[SectionIndex] || !OriginalSubMeshVertices.IsValidIndex(SectionIndex))
    {
        return;
    }

    if (JointLimitsList.IsValidIndex(SectionIndex))
    {
        JointLimitsList[SectionIndex].CurrentAngle = TargetAngleDegrees;
        JointLimitsList[SectionIndex].RotationAxis = RotationAxis;
        JointLimitsList[SectionIndex].bInvertAxis = bInvertAxis;
    }

    // RECALCULATE FORWARD KINEMATICS TRANSFORMATIONS FOR ALL SUB-MESHES ACROSS ALL HIERARCHY LEVELS
    int32 SubMeshCount = SubMeshComponents.Num();
    for (int32 i = 0; i < SubMeshCount; ++i)
    {
        if (!SubMeshComponents[i] || !OriginalSubMeshVertices.IsValidIndex(i))
        {
            continue;
        }

        // Build ancestor chain from root down to sub-mesh i
        TArray<int32> Ancestors;
        int32 Curr = i;
        while (Curr >= 0 && SubMeshComponents.IsValidIndex(Curr) && Ancestors.Num() < 30)
        {
            if (Ancestors.Contains(Curr)) break;
            Ancestors.Insert(Curr, 0);
            Curr = (ParentJointIndices.IsValidIndex(Curr)) ? ParentJointIndices[Curr] : -1;
        }


        const TArray<FVector>& BaseVerts = OriginalSubMeshVertices[i];
        const TArray<FVector>& BaseNorms = OriginalSubMeshNormals.IsValidIndex(i) ? OriginalSubMeshNormals[i] : BaseVerts;

        TArray<FVector> NewVerts = BaseVerts;
        TArray<FVector> NewNorms = BaseNorms;

        // Apply joint rotations down the ancestor chain using DYNAMICALLY TRANSFORMED Pivot Points
        for (int32 JointIdx : Ancestors)
        {
            if (JointLimitsList.IsValidIndex(JointIdx))
            {
                float Angle = JointLimitsList[JointIdx].CurrentAngle;
                if (FMath::Abs(Angle) > 0.001f)
                {
                    FVector Axis = JointLimitsList[JointIdx].RotationAxis.GetSafeNormal();
                    if (JointLimitsList[JointIdx].bInvertAxis)
                    {
                        Angle = -Angle;
                    }

                    FQuat RotQuat(Axis, FMath::DegreesToRadians(Angle));

                    // DYNAMIC PIVOT TRANSFORMATION: Compute JointIdx's current 3D pivot point transformed by earlier ancestors
                    FVector CurrentJointPivot = JointPivotPoints.IsValidIndex(JointIdx) ? JointPivotPoints[JointIdx] : FVector::ZeroVector;

                    for (int32 PrevIdx : Ancestors)
                    {
                        if (PrevIdx == JointIdx)
                        {
                            break; // Stop at JointIdx
                        }

                        if (JointLimitsList.IsValidIndex(PrevIdx))
                        {
                            float PrevAngle = JointLimitsList[PrevIdx].CurrentAngle;
                            if (FMath::Abs(PrevAngle) > 0.001f)
                            {
                                FVector PrevAxis = JointLimitsList[PrevIdx].RotationAxis.GetSafeNormal();
                                if (JointLimitsList[PrevIdx].bInvertAxis)
                                {
                                    PrevAngle = -PrevAngle;
                                }

                                FQuat PrevRotQuat(PrevAxis, FMath::DegreesToRadians(PrevAngle));
                                FVector PrevPivot = JointPivotPoints.IsValidIndex(PrevIdx) ? JointPivotPoints[PrevIdx] : FVector::ZeroVector;

                                FVector LocalJointPivotDelta = CurrentJointPivot - PrevPivot;
                                CurrentJointPivot = PrevPivot + PrevRotQuat.RotateVector(LocalJointPivotDelta);
                            }
                        }
                    }

                    // Rotate vertices around the DYNAMIC TRANSFORMED PIVOT POINT!
                    for (int32 v = 0; v < NewVerts.Num(); ++v)
                    {
                        FVector Offset = NewVerts[v] - CurrentJointPivot;
                        NewVerts[v] = CurrentJointPivot + RotQuat.RotateVector(Offset);
                        NewNorms[v] = RotQuat.RotateVector(NewNorms[v]);
                    }
                }
            }
        }

        // Update mesh section
        FProcMeshSection* Section = SubMeshComponents[i]->GetProcMeshSection(0);
        if (Section && Section->ProcVertexBuffer.Num() > 0)
        {
            TArray<FVector2D> UVs;
            TArray<FLinearColor> Colors;
            TArray<FProcMeshTangent> Tangents;

            for (int32 p = 0; p < Section->ProcVertexBuffer.Num(); ++p)
            {
                UVs.Add(Section->ProcVertexBuffer[p].UV0);
                Colors.Add(Section->ProcVertexBuffer[p].Color);
                Tangents.Add(Section->ProcVertexBuffer[p].Tangent);
            }

            SubMeshComponents[i]->UpdateMeshSection_LinearColor(0, NewVerts, NewNorms, UVs, Colors, Tangents);
        }
    }
}

void APiSimGarageRobot::SetJointEulerAngles(int32 SectionIndex, FVector EulerDeg)
{
    float Angle = EulerDeg.Size();
    FVector Axis = (Angle > 0.001f) ? EulerDeg.GetSafeNormal() : FVector(0, 1, 0);
    SetJointAngleClamped(SectionIndex, Angle, Axis, false);
}

void APiSimGarageRobot::ToggleFlipMeshNormals()

{
    bFlipMeshNormals = !bFlipMeshNormals;

    for (int32 i = 0; i < SubMeshComponents.Num(); ++i)
    {
        if (OriginalSubMeshNormals.IsValidIndex(i))
        {
            for (FVector& N : OriginalSubMeshNormals[i])
            {
                N = -N;
            }
            float CurrA = JointLimitsList.IsValidIndex(i) ? JointLimitsList[i].CurrentAngle : 0.0f;
            FVector AxisV = JointLimitsList.IsValidIndex(i) ? JointLimitsList[i].RotationAxis : FVector(0, 1, 0);
            bool bInv = JointLimitsList.IsValidIndex(i) ? JointLimitsList[i].bInvertAxis : false;
            SetJointAngleClamped(i, CurrA, AxisV, bInv);
        }
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Yellow,
            FString::Printf(TEXT(">>> [NORMALLER %s] MESH NORMALLERİ CANLI FLIP EDİLDİ! <<<"),
                bFlipMeshNormals ? TEXT("FLİP EDİLDİ (ON)") : TEXT("NORMAL (OFF)")));
    }
}

void APiSimGarageRobot::StartJointMinMaxSweep(int32 JointIndex)
{
    if (JointLimitsList.IsValidIndex(JointIndex))
    {
        bIsSweepingJoint = true;
        SweepJointIndex = JointIndex;
        SweepTimer = 0.0f;

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Cyan,
                FString::Printf(TEXT(">>> [MIN-MAX TEST SÜPÜRME BAŞLADI] EKLEM %d: %.1f° --> %.1f° SÜPÜRÜLÜYOR... <<<"),
                    JointIndex, JointLimitsList[JointIndex].MinAngle, JointLimitsList[JointIndex].MaxAngle));
        }
    }
}

void APiSimGarageRobot::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UDPManager)
    {
        UDPManager->Shutdown();
        UDPManager.Reset();
    }

    Super::EndPlay(EndPlayReason);
}

void APiSimGarageRobot::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Min-Max Smooth Joint Sweep Probing Animation
    if (bIsSweepingJoint && JointLimitsList.IsValidIndex(SweepJointIndex))
    {
        SweepTimer += DeltaTime;
        float Duration = 2.4f; // 2.4 second full cycle
        if (SweepTimer >= Duration)
        {
            bIsSweepingJoint = false;
            SweepTimer = 0.0f;
        }
        else
        {
            float Progress = (FMath::Sin((SweepTimer / Duration) * 2.0f * PI - (PI * 0.5f)) + 1.0f) * 0.5f;
            float MinA = JointLimitsList[SweepJointIndex].MinAngle;
            float MaxA = JointLimitsList[SweepJointIndex].MaxAngle;
            float TargetA = FMath::Lerp(MinA, MaxA, Progress);

            FVector AxisV = JointLimitsList[SweepJointIndex].RotationAxis;
            bool bInv = JointLimitsList[SweepJointIndex].bInvertAxis;
            SetJointAngleClamped(SweepJointIndex, TargetA, AxisV, bInv);
        }
    }

    // Ensure 360 Orbit Camera and FPV Camera are attached to Main Mesh (SubMeshComponents[0])
    if (SubMeshComponents.Num() > 0 && SubMeshComponents[0])
    {
        if (ConfiguratorSpringArm && ConfiguratorSpringArm->GetAttachParent() != SubMeshComponents[0])
        {
            ConfiguratorSpringArm->AttachToComponent(SubMeshComponents[0], FAttachmentTransformRules::SnapToTargetNotIncludingScale);
            ConfiguratorSpringArm->SetRelativeLocation(FVector(0.0f, 0.0f, 15.0f));
            ConfiguratorSpringArm->SetRelativeRotation(FRotator(-20.0f, 45.0f, 0.0f));
        }
        if (FpvCameraComponent && FpvCameraComponent->GetAttachParent() != SubMeshComponents[0])
        {
            FpvCameraComponent->AttachToComponent(SubMeshComponents[0], FAttachmentTransformRules::KeepRelativeTransform);
        }
    }

    // Dynamic 360 Orbit Camera Mouse Rotation & Zooming
    APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (PC && ConfiguratorSpringArm)
    {
        if (PC->IsInputKeyDown(EKeys::RightMouseButton))
        {
            float MouseX = 0.0f;
            float MouseY = 0.0f;
            PC->GetInputMouseDelta(MouseX, MouseY);

            if (FMath::Abs(MouseX) > 0.01f || FMath::Abs(MouseY) > 0.01f)
            {
                FRotator Rot = ConfiguratorSpringArm->GetRelativeRotation();
                Rot.Yaw = FRotator::NormalizeAxis(Rot.Yaw + (MouseX * 1.5f));
                Rot.Pitch = FMath::Clamp(Rot.Pitch + (MouseY * 1.5f), -85.0f, 85.0f);
                ConfiguratorSpringArm->SetRelativeRotation(Rot);
            }
        }

        if (PC->WasInputKeyJustPressed(EKeys::MouseScrollUp))
        {
            ConfiguratorSpringArm->TargetArmLength = FMath::Clamp(ConfiguratorSpringArm->TargetArmLength - 35.0f, 60.0f, 1500.0f);
        }
        else if (PC->WasInputKeyJustPressed(EKeys::MouseScrollDown))
        {
            ConfiguratorSpringArm->TargetArmLength = FMath::Clamp(ConfiguratorSpringArm->TargetArmLength + 35.0f, 60.0f, 1500.0f);
        }
    }

    // Video Streaming at 20 FPS (only if bEnableVideoStream is enabled)
    if (bEnableVideoStream)
    {
        VideoTimer += DeltaTime;
        if (VideoTimer >= 0.05f)
        {
            VideoTimer = 0.0f;
            CaptureAndSendVideoFrame();
        }
    }

    if (CurrentViewMode == EGarageViewMode::Sensor)
    {
        UpdateSensorRaycasts();
    }

    // Direct Hardware Key Polling for DevKit Numpad (0..9) and Top-Row Digit Keys (0..9)
    APlayerController* DevPC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (DevPC && SubMeshComponents.Num() > 0 && SubMeshComponents[0])
    {
        int32 TargetMeshIdx = 0;
        if (DevPC->MyHUD)
        {
            APiSimHUD* HUD = Cast<APiSimHUD>(DevPC->MyHUD);
            if (HUD && HUD->ActiveGarageWidget)
            {
                TargetMeshIdx = HUD->ActiveGarageWidget->SelectedJointIndex;
            }
        }

        // Numpad 0 / Digit 0 -> Toggle Servo Axis
        if (DevPC->WasInputKeyJustPressed(EKeys::NumPadZero) || DevPC->WasInputKeyJustPressed(EKeys::Zero))
        {
            ToggleDevKitServoAxis();
        }
        // Numpad 1 / Digit 1 -> Servo Min (-1.0)
        else if (DevPC->WasInputKeyJustPressed(EKeys::NumPadOne) || DevPC->WasInputKeyJustPressed(EKeys::One))
        {
            SetDevKitServoPosition(TargetMeshIdx, -1.0f);
        }
        // Numpad 2 / Digit 2 -> Servo Zero (0.0)
        else if (DevPC->WasInputKeyJustPressed(EKeys::NumPadTwo) || DevPC->WasInputKeyJustPressed(EKeys::Two))
        {
            SetDevKitServoPosition(TargetMeshIdx, 0.0f);
        }
        // Numpad 3 / Digit 3 -> Servo Max (+1.0)
        else if (DevPC->WasInputKeyJustPressed(EKeys::NumPadThree) || DevPC->WasInputKeyJustPressed(EKeys::Three))
        {
            SetDevKitServoPosition(TargetMeshIdx, 1.0f);
        }
        // Numpad 4 / Digit 4 -> X-RPM +1
        else if (DevPC->WasInputKeyJustPressed(EKeys::NumPadFour) || DevPC->WasInputKeyJustPressed(EKeys::Four))
        {
            AddDevKitRpm(TargetMeshIdx, 0, 1.0f);
        }
        // Numpad 5 / Digit 5 -> Y-RPM +1
        else if (DevPC->WasInputKeyJustPressed(EKeys::NumPadFive) || DevPC->WasInputKeyJustPressed(EKeys::Five))
        {
            AddDevKitRpm(TargetMeshIdx, 1, 1.0f);
        }
        // Numpad 6 / Digit 6 -> Z-RPM +1
        else if (DevPC->WasInputKeyJustPressed(EKeys::NumPadSix) || DevPC->WasInputKeyJustPressed(EKeys::Six))
        {
            AddDevKitRpm(TargetMeshIdx, 2, 1.0f);
        }
        // Numpad 7 / Digit 7 -> X-Force +10 N
        else if (DevPC->WasInputKeyJustPressed(EKeys::NumPadSeven) || DevPC->WasInputKeyJustPressed(EKeys::Seven))
        {
            AddDevKitForceN(TargetMeshIdx, 0, 10.0f);
        }
        // Numpad 8 / Digit 8 -> Y-Force +10 N
        else if (DevPC->WasInputKeyJustPressed(EKeys::NumPadEight) || DevPC->WasInputKeyJustPressed(EKeys::Eight))
        {
            AddDevKitForceN(TargetMeshIdx, 1, 10.0f);
        }
        // Numpad 9 / Digit 9 -> Z-Force +10 N
        else if (DevPC->WasInputKeyJustPressed(EKeys::NumPadNine) || DevPC->WasInputKeyJustPressed(EKeys::Nine))
        {
            AddDevKitForceN(TargetMeshIdx, 2, 10.0f);
        }

        // Apply Forces & Torques during Physics Simulation
        if (CurrentPhysicsTestMode != EPhysicsTestMode::None && SubMeshComponents.IsValidIndex(TargetMeshIdx) && SubMeshComponents[TargetMeshIdx])
        {
            FVector TargetLoc = SubMeshComponents[TargetMeshIdx]->GetComponentLocation();

            // 1) Apply Numpad 7, 8, 9 Forces along X, Y, Z axes
            if (!DevKitAppliedForceN.IsNearlyZero(0.01f))
            {
                FVector LocalForceUE = DevKitAppliedForceN * 100.0f; // 1 N = 100 g*cm/s^2 force unit in Unreal Engine
                FVector WorldForce = SubMeshComponents[TargetMeshIdx]->GetComponentTransform().TransformVector(LocalForceUE);

                SubMeshComponents[0]->AddForceAtLocation(WorldForce, TargetLoc);

                if (GetWorld())
                {
                    FVector EndArrow = TargetLoc + WorldForce.GetSafeNormal() * (40.0f + FMath::Min(DevKitAppliedForceN.Size() * 2.0f, 150.0f));
                    DrawDebugDirectionalArrow(GetWorld(), TargetLoc, EndArrow, 30.0f, FColor(255, 128, 0), false, -1.0f, 0, 4.0f);
                    DrawDebugSphere(GetWorld(), TargetLoc, 12.0f, 16, FColor::Yellow, false, -1.0f);
                }
            }

            // 2) PURE CHAOS PHYSICS WHEEL DRIVE (100% Native Ground Friction Mechanics)
            if (!DevKitAppliedRpm.IsNearlyZero(0.01f))
            {
                // A) VISUAL REVOLUTION (360° Real-time Wheel Mesh Spin on Axle)
                FVector LocalAxleDir = DevKitAppliedRpm.GetSafeNormal();
                float ScalarRpm = DevKitAppliedRpm.Size();
                float DeltaAngleDeg = (ScalarRpm * 360.0f / 60.0f) * DeltaTime;

                float CurrentSpin = SubMeshSpinAngles.Contains(TargetMeshIdx) ? SubMeshSpinAngles[TargetMeshIdx] : 0.0f;
                CurrentSpin += DeltaAngleDeg;
                SubMeshSpinAngles.Add(TargetMeshIdx, CurrentSpin);

                // GAZEBO REVOLUTE JOINT MOTOR DRIVE (Pure Chaos Physics Angular Velocity Drive)
                FVector LocalAngVelRadSec = (DevKitAppliedRpm * 2.0f * PI) / 60.0f;
                FVector WorldAngVel = SubMeshComponents[TargetMeshIdx]->GetComponentTransform().TransformVector(LocalAngVelRadSec);

                SubMeshComponents[TargetMeshIdx]->SetPhysicsAngularVelocityInRadians(WorldAngVel);

                if (GetWorld())
                {
                    FVector EndRpmArrow = TargetLoc + WorldAngVel.GetSafeNormal() * 60.0f;
                    DrawDebugDirectionalArrow(GetWorld(), TargetLoc, EndRpmArrow, 25.0f, FColor::Green, false, -1.0f, 0, 3.0f);
                }
            }
        }
    }
}

bool APiSimGarageRobot::LoadConfig(const FString& ConfigFilePath)
{
    bool bResult = UPiSimConfigManager::LoadRobotConfigFromFile(ConfigFilePath, RobotConfig);
    if (bResult)
    {
        UE_LOG(LogTemp, Log, TEXT("[APiSimGarageRobot] Loaded config for robot: %s"), *RobotConfig.RobotName);
    }
    return bResult;
}

bool APiSimGarageRobot::SaveConfig(const FString& ConfigFilePath)
{
    return UPiSimConfigManager::SaveRobotConfigToFile(RobotConfig, ConfigFilePath);
}

void APiSimGarageRobot::SetupDynamicConvexCollision()
{
    if (!ChassisMeshComponent)
    {
        return;
    }

    ChassisMeshComponent->SetMobility(EComponentMobility::Movable);
    ChassisMeshComponent->SetSimulatePhysics(false);
    ChassisMeshComponent->SetEnableGravity(false);
}

void APiSimGarageRobot::CaptureAndSendVideoFrame()
{
    if (!VideoRenderTarget || !UDPManager)
    {
        return;
    }

    FTextureRenderTargetResource* Resource = VideoRenderTarget->GameThread_GetRenderTargetResource();
    if (!Resource)
    {
        return;
    }

    int32 Width = VideoRenderTarget->SizeX;
    int32 Height = VideoRenderTarget->SizeY;
    if (Width <= 0 || Height <= 0)
    {
        return;
    }

    TArray<FColor> RawPixels;
    FReadSurfaceDataFlags ReadPixelFlags(RCM_UNorm);
    ReadPixelFlags.SetLinearToGamma(false);

    if (!Resource->ReadPixels(RawPixels, ReadPixelFlags) || RawPixels.Num() == 0)
    {
        return;
    }

    for (FColor& Pixel : RawPixels)
    {
        Pixel.A = 255;
    }

    IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
    TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);
    if (!ImageWrapper.IsValid())
    {
        return;
    }

    if (!ImageWrapper->SetRaw(RawPixels.GetData(), RawPixels.Num() * sizeof(FColor), Width, Height, ERGBFormat::BGRA, 8))
    {
        return;
    }

    TArray64<uint8> CompressedJpeg64 = ImageWrapper->GetCompressed(50);
    if (CompressedJpeg64.Num() == 0)
    {
        return;
    }

    TArray<uint8> CompressedJpeg;
    CompressedJpeg.Append(CompressedJpeg64.GetData(), CompressedJpeg64.Num());

    static uint16 FrameSeq = 0;
    FrameSeq++;

    const int32 MAX_CHUNK_SIZE = 1000;
    int32 TotalBytes = CompressedJpeg.Num();
    uint8 TotalChunks = static_cast<uint8>((TotalBytes + MAX_CHUNK_SIZE - 1) / MAX_CHUNK_SIZE);

    for (uint8 ChunkIdx = 0; ChunkIdx < TotalChunks; ++ChunkIdx)
    {
        int32 StartOffset = ChunkIdx * MAX_CHUNK_SIZE;
        int32 ChunkLength = FMath::Min(MAX_CHUNK_SIZE, TotalBytes - StartOffset);

        TArray<uint8> Packet;
        Packet.Reserve(4 + ChunkLength);

        Packet.Add(static_cast<uint8>((FrameSeq >> 8) & 0xFF));
        Packet.Add(static_cast<uint8>(FrameSeq & 0xFF));
        Packet.Add(ChunkIdx);
        Packet.Add(TotalChunks);

        Packet.Append(CompressedJpeg.GetData() + StartOffset, ChunkLength);

        UDPManager->SendVideoData(Packet, TEXT("192.168.1.20"), 5000);
    }
}

void APiSimGarageRobot::ClassifySubMeshes()
{
    MeshCategories.Empty();
    StructuralPropsList.Empty();
    SensorsList.Empty();
    bFoundCOG = false;
    bFoundCOL = false;

    int32 Count = SubMeshComponents.Num();
    for (int32 i = 0; i < Count; ++i)
    {
        FString Name = SubMeshNames.IsValidIndex(i) ? SubMeshNames[i] : TEXT("Mesh");

        FPiSimStructuralProperties StructProp;
        StructProp.MassKg = 1.0f;
        StructProp.AirDragCoeff = 0.25f;
        StructProp.GroundFriction = 0.8f;
        StructProp.LiftCoefficient = 0.0f;
        StructProp.bIsChassisGroup = (i == 0);

        if (LoadedMeshSections.IsValidIndex(i))
        {
            if (LoadedMeshSections[i].MassKg > 0.0f)
            {
                StructProp.MassKg = LoadedMeshSections[i].MassKg;
            }
            if (LoadedMeshSections[i].Friction > 0.0f)
            {
                StructProp.GroundFriction = LoadedMeshSections[i].Friction;
            }
        }
        else if (Name.StartsWith(TEXT("CM_"), ESearchCase::IgnoreCase))
        {
            StructProp.MassKg = 5.0f;
        }

        if (Name.StartsWith(TEXT("CM_"), ESearchCase::IgnoreCase))
        {
            MeshCategories.Add(EMeshCategoryType::Structural);
        }
        else if (Name.Equals(TEXT("COG"), ESearchCase::IgnoreCase) || Name.StartsWith(TEXT("COG_"), ESearchCase::IgnoreCase))
        {
            bFoundCOG = true;
            if (SubMeshComponents.IsValidIndex(i) && SubMeshComponents[i])
            {
                COGLocation = SubMeshComponents[i]->GetComponentLocation();
            }
            MeshCategories.Add(EMeshCategoryType::Structural);
        }
        else if (Name.Equals(TEXT("COL"), ESearchCase::IgnoreCase) || Name.StartsWith(TEXT("COL_"), ESearchCase::IgnoreCase))
        {
            bFoundCOL = true;
            if (SubMeshComponents.IsValidIndex(i) && SubMeshComponents[i])
            {
                COLLocation = SubMeshComponents[i]->GetComponentLocation();
            }
            MeshCategories.Add(EMeshCategoryType::Structural);
        }
        else if (Name.StartsWith(TEXT("S_"), ESearchCase::IgnoreCase))
        {
            MeshCategories.Add(EMeshCategoryType::Sensor);

            FPiSimSensorDetail Detail;
            Detail.SensorName = Name;
            Detail.MeshIndex = i;
            Detail.bSensorActive = true;

            if (Name.Contains(TEXT("Cam"), ESearchCase::IgnoreCase))
            {
                Detail.SensorType = TEXT("Camera (RGB 1080p 60fps)");
                Detail.LiveDataStream = TEXT("CAM_FPS: 60 | FOV: 90° | RES: 1920x1080 | STATUS: STREAMING");
            }
            else if (Name.Contains(TEXT("Gyro"), ESearchCase::IgnoreCase) || Name.Contains(TEXT("Imu"), ESearchCase::IgnoreCase))
            {
                Detail.SensorType = TEXT("IMU / Gyroscope (MPU6050)");
                Detail.LiveDataStream = TEXT("GYRO: (0.02, -0.01, 9.81) m/s² | ANG_VEL: (0.0, 0.0, 0.0) rad/s");
            }
            else if (Name.Contains(TEXT("Lidar"), ESearchCase::IgnoreCase))
            {
                Detail.SensorType = TEXT("LiDAR Sensor (360° Scan)");
                Detail.LiveDataStream = TEXT("LIDAR_PTS: 360 | RANGE: 12.4m | MIN_DIST: 0.85m");
            }
            else
            {
                Detail.SensorType = TEXT("Generic Telemetry Sensor");
                Detail.LiveDataStream = TEXT("DATA: ACTIVE | VOLTAGE: 5.0V | CURRENT: 120mA");
            }

            SensorsList.Add(Detail);
        }
        else
        {
            MeshCategories.Add(EMeshCategoryType::Visual);
        }

        if (SubMeshComponents.IsValidIndex(i) && SubMeshComponents[i])
        {
            FString MeshName = SubMeshNames.IsValidIndex(i) ? SubMeshNames[i] : TEXT("");
            bool bIsCMOnly = MeshName.StartsWith(TEXT("CM_"), ESearchCase::IgnoreCase);

            if (bIsCMOnly)
            {
                SubMeshComponents[i]->bUseComplexAsSimpleCollision = true;
                SubMeshComponents[i]->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
                SubMeshComponents[i]->SetCollisionObjectType(ECC_WorldDynamic);
                SubMeshComponents[i]->SetCollisionResponseToAllChannels(ECR_Block);
                SubMeshComponents[i]->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
                SubMeshComponents[i]->SetLinearDamping(StructProp.AirDragCoeff);
                SubMeshComponents[i]->SetAngularDamping(StructProp.AirDragCoeff * 0.5f);
            }
            else
            {
                SubMeshComponents[i]->bUseComplexAsSimpleCollision = false;
                SubMeshComponents[i]->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                SubMeshComponents[i]->SetCollisionResponseToAllChannels(ECR_Ignore);
            }
        }

        StructuralPropsList.Add(StructProp);
    }

    if (SubMeshComponents.Num() > 0 && ChassisMeshComponent)
    {
        ChassisMeshComponent->SetVisibility(false);
        ChassisMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        ChassisMeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
    }






    if (ActuatorsList.Num() == 0 && Count > 0)
    {
        FPiSimMotorActuator SpinMotor;
        SpinMotor.MotorName = TEXT("Left_Drive_Motor");
        SpinMotor.MotorType = EMotorBehaviorType::ContinuousSpin;
        SpinMotor.MaxTorqueNm = 12.5f;
        SpinMotor.TargetMeshIndex = FMath::Min(1, Count - 1);
        SpinMotor.TargetMeshName = SubMeshNames.IsValidIndex(SpinMotor.TargetMeshIndex) ? SubMeshNames[SpinMotor.TargetMeshIndex] : TEXT("WheelLeft");
        SpinMotor.bAffectsRotation = true;
        SpinMotor.bAffectsLiftForce = false;
        SpinMotor.AppliedTorqueAxis = FVector(0, 1, 0);
        ActuatorsList.Add(SpinMotor);

        FPiSimMotorActuator ServoActuator;
        ServoActuator.MotorName = TEXT("Steering_Servo");
        ServoActuator.MotorType = EMotorBehaviorType::Servo;
        ServoActuator.MaxTorqueNm = 5.0f;
        ServoActuator.TargetMeshIndex = FMath::Min(2, Count - 1);
        ServoActuator.TargetMeshName = SubMeshNames.IsValidIndex(ServoActuator.TargetMeshIndex) ? SubMeshNames[ServoActuator.TargetMeshIndex] : TEXT("SteeringArm");
        ServoActuator.bAffectsRotation = true;
        ServoActuator.bAffectsLiftForce = false;
        ServoActuator.AppliedTorqueAxis = FVector(0, 0, 1);
        ActuatorsList.Add(ServoActuator);

        FPiSimMotorActuator ThrusterActuator;
        ThrusterActuator.MotorName = TEXT("Vertical_Lift_Thruster");
        ThrusterActuator.MotorType = EMotorBehaviorType::Thruster;
        ThrusterActuator.MaxThrustNewton = 80.0f;
        ThrusterActuator.TargetMeshIndex = 0;
        ThrusterActuator.TargetMeshName = SubMeshNames.IsValidIndex(0) ? SubMeshNames[0] : TEXT("Chassis");
        ThrusterActuator.bAffectsRotation = false;
        ThrusterActuator.bAffectsLiftForce = true;
        ThrusterActuator.AppliedForceAtMin = FVector(0, 0, -80.0f);
        ThrusterActuator.AppliedForceAtZero = FVector(0, 0, 0.0f);
        ThrusterActuator.AppliedForceAtMax = FVector(0, 0, 80.0f);
        ActuatorsList.Add(ThrusterActuator);
    }
}

void APiSimGarageRobot::SetGarageViewMode(EGarageViewMode NewMode)
{
    CurrentViewMode = NewMode;

    UMaterialInterface* DefaultMatToApply = DefaultMaterial ? DefaultMaterial : LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

    int32 VisualCount = 0;
    for (int32 i = 0; i < MeshCategories.Num(); ++i)
    {
        if (MeshCategories[i] == EMeshCategoryType::Visual) VisualCount++;
    }

    int32 Count = SubMeshComponents.Num();
    for (int32 i = 0; i < Count; ++i)
    {
        if (!SubMeshComponents[i]) continue;

        EMeshCategoryType Cat = MeshCategories.IsValidIndex(i) ? MeshCategories[i] : EMeshCategoryType::Visual;
        bool bShow = false;

        if (NewMode == EGarageViewMode::Visual)
        {
            bShow = (Cat == EMeshCategoryType::Visual) || (VisualCount == 0);
        }
        else if (NewMode == EGarageViewMode::Structural)
        {
            bShow = (Cat == EMeshCategoryType::Structural);
        }
        else if (NewMode == EGarageViewMode::Sensor)
        {
            bShow = (Cat == EMeshCategoryType::Sensor);
        }

        // ONLY CM_ meshes have active collision! Non-CM_ meshes remain NoCollision!
        FString MeshName = SubMeshNames.IsValidIndex(i) ? SubMeshNames[i] : TEXT("");
        bool bIsCMOnly = MeshName.StartsWith(TEXT("CM_"), ESearchCase::IgnoreCase);

        SubMeshComponents[i]->SetVisibility(true, false);
        SubMeshComponents[i]->SetHiddenInGame(false, false);

        if (bIsCMOnly)
        {
            SubMeshComponents[i]->bUseComplexAsSimpleCollision = false; // Simple Collision uses Convex Hulls (FKConvexElem)
            SubMeshComponents[i]->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            SubMeshComponents[i]->SetCollisionObjectType(ECC_WorldDynamic);
            SubMeshComponents[i]->SetCollisionResponseToAllChannels(ECR_Block);
            SubMeshComponents[i]->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
            SubMeshComponents[i]->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
            SubMeshComponents[i]->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
        }
        else
        {
            SubMeshComponents[i]->bUseComplexAsSimpleCollision = false;
            SubMeshComponents[i]->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            SubMeshComponents[i]->SetCollisionResponseToAllChannels(ECR_Ignore);
        }

        if (bShow)
        {
            if (DefaultMatToApply)
            {
                SubMeshComponents[i]->SetMaterial(0, DefaultMatToApply);
            }
            if (bIsCMOnly)
            {
                SubMeshComponents[i]->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
            }
        }
        else
        {
            UMaterialInterface* TargetMat = TranslucentMaterial ? TranslucentMaterial : InvisibleMaterial;
            if (!TargetMat)
            {
                TargetMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/Engine_MI_Shaders/Instances/M_Shader_SimpleTranslucent_Invis.M_Shader_SimpleTranslucent_Invis"));
            }
            if (!TargetMat)
            {
                TargetMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/SimBlank/Materials/M_PiSim_İnvis.M_PiSim_İnvis"));
            }
            if (!TargetMat)
            {
                TargetMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/M_Shader_SimpleTranslucent_Invis.M_Shader_SimpleTranslucent_Invis"));
            }
            if (!TargetMat)
            {
                TargetMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/M_Shader_SimpleTranslucent_Invis.M_Shader_SimpleTranslucent_Invis"));
            }
            if (!TargetMat)
            {
                UMaterialInterface* InvisBaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/Widget3DMaterial_Translucent.Widget3DMaterial_Translucent"));
                if (!InvisBaseMat) InvisBaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/DefaultText.DefaultText"));

                if (InvisBaseMat)
                {
                    UMaterialInstanceDynamic* DynInvisMat = UMaterialInstanceDynamic::Create(InvisBaseMat, this);
                    if (DynInvisMat)
                    {
                        DynInvisMat->SetScalarParameterValue(TEXT("Opacity"), 0.0f);
                        DynInvisMat->SetScalarParameterValue(TEXT("OpacityMask"), 0.0f);
                        DynInvisMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
                        TargetMat = DynInvisMat;
                    }
                    else
                    {
                        TargetMat = InvisBaseMat;
                    }
                }
            }

            if (TargetMat)
            {
                SubMeshComponents[i]->SetMaterial(0, TargetMat);
            }
            SubMeshComponents[i]->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
        }
    }

    if (ConfiguratorOrbitCamera)
    {
        ConfiguratorOrbitCamera->SetHiddenInGame(true, true);
        ConfiguratorOrbitCamera->SetVisibility(false, true);
    }
    if (ConfiguratorSpringArm)
    {
        ConfiguratorSpringArm->SetHiddenInGame(true, true);
        ConfiguratorSpringArm->SetVisibility(false, true);
    }
    if (FpvCameraComponent)
    {
        FpvCameraComponent->SetHiddenInGame(true, true);
        FpvCameraComponent->SetVisibility(false, true);
    }

    if (GEngine)
    {
        const TCHAR* ModeNames[3] = { TEXT("🎨 GÖRSEL MOD (VISUAL)"), TEXT("🦴 YAPISAL & FİZİK MODU (STRUCTURAL - CM_)"), TEXT("📡 SENSÖR MODU (S_ PLACEHOLDERS)") };
        int32 ModeIdx = static_cast<int32>(NewMode);
        GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor(0, 240, 255),
            FString::Printf(TEXT(">>> [GARAJ GÖRÜNÜM MODU DEĞİŞTİRİLDİ] : %s <<<"), ModeNames[FMath::Clamp(ModeIdx, 0, 2)]));
    }
}

void APiSimGarageRobot::ReimportCadModel()
{
    // Delete stale pre-cooked baked binary cache
    FString BakedFilePath1 = FPaths::ProjectSavedDir() / TEXT("Robots/Baked/robot_collision_baked.bin");
    FString BakedFilePath2 = FPaths::ProjectSavedDir() / TEXT("Robots/Baked/robot_baked.bin");
    if (FPaths::FileExists(BakedFilePath1)) IFileManager::Get().Delete(*BakedFilePath1);
    if (FPaths::FileExists(BakedFilePath2)) IFileManager::Get().Delete(*BakedFilePath2);

    FString FbxFullPath = FPaths::ProjectSavedDir() / TEXT("Robots/Cache/robot_collision.fbx");
    if (!FPaths::FileExists(FbxFullPath))
    {
        FbxFullPath = FPaths::ProjectSavedDir() / TEXT("Robots/Cache/robot.fbx");
    }
    FString GlbFullPath = FPaths::ProjectSavedDir() / TEXT("Robots/Cache/robot.glb");

    TArray<FGLBMeshSection> Sections;
    bool bSuccess = false;

    if (FPaths::FileExists(FbxFullPath))
    {
        bSuccess = ParseFbxAllBinaryMeshes(FbxFullPath, Sections, CadUnitScaleMultiplier);
        if (bSuccess)
        {
            LoadedModelFormatName = FString::Printf(TEXT("FBX (.fbx) [%s]"), *FPaths::GetCleanFilename(FbxFullPath));
        }
    }
    else if (FPaths::FileExists(GlbFullPath))
    {
        bSuccess = ParseGlbAllBinaryMeshes(GlbFullPath, Sections, CadUnitScaleMultiplier);
        if (bSuccess)
        {
            LoadedModelFormatName = TEXT("GLB (.glb) [Saved/Robots/Cache/robot.glb]");
        }
    }

    if (bSuccess && Sections.Num() > 0)
    {
        for (UProceduralMeshComponent* SubComp : SubMeshComponents)
        {
            if (SubComp)
            {
                SubComp->DestroyComponent();
            }
        }
        SubMeshComponents.Empty();
        SubMeshNames.Empty();
        OriginalSubMeshVertices.Empty();
        OriginalSubMeshNormals.Empty();
        ParentJointIndices.Empty();
        DepthLevels.Empty();

        UMaterialInterface* DefaultMat = DefaultMaterial ? DefaultMaterial : LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

        for (int32 SecIdx = 0; SecIdx < Sections.Num(); ++SecIdx)
        {
            FName CompName = *FString::Printf(TEXT("CADSubMesh_%d_%s"), SecIdx, *Sections[SecIdx].MeshName);
            UProceduralMeshComponent* SubComp = NewObject<UProceduralMeshComponent>(this, CompName);

            SubComp->SetMobility(EComponentMobility::Movable);

            int32 ParentIdx = Sections[SecIdx].ParentSectionIndex;
            if (ParentIdx >= 0 && SubMeshComponents.IsValidIndex(ParentIdx) && SubMeshComponents[ParentIdx])
            {
                SubComp->SetupAttachment(SubMeshComponents[ParentIdx]);
                SubComp->SetRelativeLocation(Sections[SecIdx].PivotPoint - Sections[ParentIdx].PivotPoint);
            }
            else
            {
                SubComp->SetupAttachment(RootComponent);
                SubComp->SetRelativeLocation(Sections[SecIdx].PivotPoint);
            }

            SubComp->RegisterComponent();

            InitialSubMeshRelativeRotations.Add(SecIdx, SubComp->GetRelativeRotation().Quaternion());
            SubMeshSpinAngles.Add(SecIdx, 0.0f);

            FString MeshName = Sections[SecIdx].MeshName;
            bool bIsCMOnly = MeshName.StartsWith(TEXT("CM_"), ESearchCase::IgnoreCase);

            TArray<FVector2D> UV0;
            TArray<FLinearColor> VertexColors;
            TArray<FProcMeshTangent> Tangents;

            SubComp->CastShadow = true;
            SubComp->bCastDynamicShadow = true;
            SubComp->bAffectDistanceFieldLighting = false;

            if (bIsCMOnly)
            {
                SubComp->bUseComplexAsSimpleCollision = true;
                SubComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
                SubComp->SetCollisionObjectType(ECC_WorldDynamic);
                SubComp->SetCollisionResponseToAllChannels(ECR_Block);
                SubComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
                SubComp->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
                SubComp->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
            }
            else
            {
                SubComp->bUseComplexAsSimpleCollision = false;
                SubComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                SubComp->SetCollisionResponseToAllChannels(ECR_Ignore);
            }

            SubComp->CreateMeshSection_LinearColor(0, Sections[SecIdx].Vertices, Sections[SecIdx].Triangles, Sections[SecIdx].Normals, UV0, VertexColors, Tangents, bIsCMOnly);
            if (DefaultMat)
            {
                SubComp->SetMaterial(0, DefaultMat);
            }

            SubComp->RecreatePhysicsState();
            SubComp->SetVisibility(true);
            SubComp->SetHiddenInGame(false);

            SubMeshComponents.Add(SubComp);
            SubMeshNames.Add(Sections[SecIdx].MeshName);
            OriginalSubMeshVertices.Add(Sections[SecIdx].Vertices);
            OriginalSubMeshNormals.Add(Sections[SecIdx].Normals);
            ParentJointIndices.Add(Sections[SecIdx].ParentSectionIndex);
            DepthLevels.Add(Sections[SecIdx].DepthLevel);
        }

        ClassifySubMeshes();
        SetGarageViewMode(CurrentViewMode);
        ResetRobotPose();

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 8.0f, FColor::Green,
                FString::Printf(TEXT(">>> [CAD MODELİ RE-IMPORT EDİLDİ] %d PARÇA YENİDEN YÜKLENDİ (ÖLÇEK: %.3f) <<<"), Sections.Num(), CadUnitScaleMultiplier));
        }
    }
}

void APiSimGarageRobot::ApplyActuatorTestValue(int32 ActuatorIndex, float SliderVal)
{
    if (!ActuatorsList.IsValidIndex(ActuatorIndex)) return;

    FPiSimMotorActuator& Actuator = ActuatorsList[ActuatorIndex];
    Actuator.TestSliderValue = FMath::Clamp(SliderVal, -1.0f, 1.0f);

    int32 TargetIdx = Actuator.TargetMeshIndex;
    if (Actuator.bAffectsRotation && JointLimitsList.IsValidIndex(TargetIdx))
    {
        float TargetAngle = 0.0f;
        if (SliderVal < 0.0f)
        {
            TargetAngle = FMath::Lerp(0.0f, JointLimitsList[TargetIdx].MinAngle, FMath::Abs(SliderVal));
        }
        else
        {
            TargetAngle = FMath::Lerp(0.0f, JointLimitsList[TargetIdx].MaxAngle, SliderVal);
        }
        SetJointAngleClamped(TargetIdx, TargetAngle, JointLimitsList[TargetIdx].RotationAxis, JointLimitsList[TargetIdx].bInvertAxis);
    }

    if (!SubMeshComponents.IsValidIndex(TargetIdx) || !SubMeshComponents[TargetIdx]) return;

    FVector Origin = SubMeshComponents[TargetIdx]->GetComponentLocation();

    FVector ForceVec = FVector::ZeroVector;
    if (Actuator.TestSliderValue < 0.0f)
    {
        ForceVec = FMath::Lerp(Actuator.AppliedForceAtZero, Actuator.AppliedForceAtMin, FMath::Abs(Actuator.TestSliderValue));
    }
    else
    {
        ForceVec = FMath::Lerp(Actuator.AppliedForceAtZero, Actuator.AppliedForceAtMax, Actuator.TestSliderValue);
    }

    if (GetWorld())
    {
        FColor ArrowColor = (Actuator.TestSliderValue >= 0.0f) ? FColor::Green : FColor::Red;
        FVector EndPos = Origin + ForceVec * 2.0f;
        DrawDebugDirectionalArrow(GetWorld(), Origin, EndPos, 25.0f, ArrowColor, false, 0.1f, 0, 4.0f);
        DrawDebugSphere(GetWorld(), Origin, 8.0f, 12, FColor::Yellow, false, 0.1f);
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(301, 1.5f, FColor::Yellow,
            FString::Printf(TEXT(">>> [MOTOR ETKİ TESTİ] '%s' (%.2f) -> UYGULANAN KUVVET: (%.1f, %.1f, %.1f) N <<<"),
                *Actuator.MotorName, Actuator.TestSliderValue, ForceVec.X, ForceVec.Y, ForceVec.Z));
    }
}

void APiSimGarageRobot::UpdateSensorRaycasts()
{
    if (!GetWorld()) return;

    for (FPiSimSensorDetail& Sensor : SensorsList)
    {
        if (!Sensor.bSensorActive || !SubMeshComponents.IsValidIndex(Sensor.MeshIndex) || !SubMeshComponents[Sensor.MeshIndex])
        {
            continue;
        }

        FVector StartPos = SubMeshComponents[Sensor.MeshIndex]->GetComponentLocation();
        FVector ForwardDir = SubMeshComponents[Sensor.MeshIndex]->GetForwardVector();
        FVector EndPos = StartPos + ForwardDir * 600.0f;

        FHitResult Hit;
        FCollisionQueryParams Params;
        Params.AddIgnoredActor(this);

        bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, StartPos, EndPos, ECC_Visibility, Params);
        if (bHit && Hit.GetActor())
        {
            Sensor.RaycastHitDistance = Hit.Distance;
            Sensor.RaycastHitActorName = Hit.GetActor()->GetName();
            DrawDebugLine(GetWorld(), StartPos, Hit.Location, FColor::Green, false, -1.0f, 0, 3.0f);
            DrawDebugSphere(GetWorld(), Hit.Location, 10.0f, 12, FColor::Yellow, false, -1.0f);
        }
        else
        {
            Sensor.RaycastHitDistance = -1.0f;
            Sensor.RaycastHitActorName = TEXT("Hiçbir Şey (Boşluk)");
            DrawDebugLine(GetWorld(), StartPos, EndPos, FColor(0, 200, 255), false, -1.0f, 0, 2.0f);
        }

        Sensor.LookAtDirection = ForwardDir;
    }
}

void APiSimGarageRobot::ToggleDevKitServoAxis()
{
    DevKitServoAxis = (DevKitServoAxis + 1) % 3;
    const TCHAR* AxisNames[3] = { TEXT("X-EKSENİ"), TEXT("Y-EKSENİ"), TEXT("Z-EKSENİ") };
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(901, 3.0f, FColor(0, 240, 255),
            FString::Printf(TEXT(">>> [DEVKIT NUMPAD 0] AKTİF SERVO EKSENİ DEĞİŞTİRİLDİ: [%s] <<<"), AxisNames[DevKitServoAxis]));
    }
}

void APiSimGarageRobot::SetDevKitServoPosition(int32 TargetMeshIdx, float PositionState)
{
    if (!SubMeshComponents.IsValidIndex(TargetMeshIdx) || !SubMeshComponents[TargetMeshIdx]) return;

    FVector RotAxis = FVector::UpVector;
    if (DevKitServoAxis == 0) RotAxis = FVector::ForwardVector;
    else if (DevKitServoAxis == 1) RotAxis = FVector::RightVector;

    float MinAng = JointLimitsList.IsValidIndex(TargetMeshIdx) ? JointLimitsList[TargetMeshIdx].MinAngle : -90.0f;
    float MaxAng = JointLimitsList.IsValidIndex(TargetMeshIdx) ? JointLimitsList[TargetMeshIdx].MaxAngle : 90.0f;

    float TargetAngle = 0.0f;
    if (PositionState < 0.0f) TargetAngle = MinAng;
    else if (PositionState > 0.0f) TargetAngle = MaxAng;
    else TargetAngle = 0.0f;

    SetJointAngleClamped(TargetMeshIdx, TargetAngle, RotAxis, false);

    const TCHAR* PosNames[3] = { TEXT("MIN (-1.0)"), TEXT("ZERO (0.0)"), TEXT("MAX (+1.0)") };
    int32 PosIdx = FMath::Clamp((int32)PositionState + 1, 0, 2);

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(902, 3.0f, FColor::Yellow,
            FString::Printf(TEXT(">>> [DEVKIT NUMPAD %d] SERVO KONUMU ATANDI: [%s] -> %.1f° <<<"), PosIdx + 1, PosNames[PosIdx], TargetAngle));
    }
}

void APiSimGarageRobot::AddDevKitRpm(int32 TargetMeshIdx, int32 AxisIndex, float DeltaRpm)
{
    if (AxisIndex == 0) DevKitAppliedRpm.X += DeltaRpm;
    else if (AxisIndex == 1) DevKitAppliedRpm.Y += DeltaRpm;
    else if (AxisIndex == 2) DevKitAppliedRpm.Z += DeltaRpm;

    const TCHAR* Axes[3] = { TEXT("X"), TEXT("Y"), TEXT("Z") };
    int32 NumpadKey = AxisIndex + 4;
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(903, 3.0f, FColor::Green,
            FString::Printf(TEXT(">>> [DEVKIT NUMPAD %d] DÖNDÜRME RPM ARTIRILDI (+%.0f RPM): %s-RPM = %.0f <<<"), NumpadKey, DeltaRpm, Axes[FMath::Clamp(AxisIndex, 0, 2)], (AxisIndex == 0 ? DevKitAppliedRpm.X : (AxisIndex == 1 ? DevKitAppliedRpm.Y : DevKitAppliedRpm.Z))));
    }
}

void APiSimGarageRobot::AddDevKitForceN(int32 TargetMeshIdx, int32 AxisIndex, float DeltaForceN)
{
    if (AxisIndex == 0) DevKitAppliedForceN.X += DeltaForceN;
    else if (AxisIndex == 1) DevKitAppliedForceN.Y += DeltaForceN;
    else if (AxisIndex == 2) DevKitAppliedForceN.Z += DeltaForceN;

    const TCHAR* Axes[3] = { TEXT("X"), TEXT("Y"), TEXT("Z") };
    int32 NumpadKey = AxisIndex + 7;
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(904, 3.0f, FColor(255, 128, 0),
            FString::Printf(TEXT(">>> [DEVKIT NUMPAD %d] UYGULANAN KUVVET ARTIRILDI (+%.0f N): %s-KUVVET = %.0f N <<<"), NumpadKey, DeltaForceN, Axes[FMath::Clamp(AxisIndex, 0, 2)], (AxisIndex == 0 ? DevKitAppliedForceN.X : (AxisIndex == 1 ? DevKitAppliedForceN.Y : DevKitAppliedForceN.Z))));
    }
}

void APiSimGarageRobot::ResetDevKitTestValues()
{
    DevKitAppliedRpm = FVector::ZeroVector;
    DevKitAppliedForceN = FVector::ZeroVector;
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(905, 3.0f, FColor::White, TEXT(">>> [DEVKIT TEST DEĞERLERİ SIFIRLANDI] <<<"));
    }
}

void APiSimGarageRobot::SetPhysicsTestMode(EPhysicsTestMode TestMode)
{
    CurrentPhysicsTestMode = TestMode;

    if (!ChassisMeshComponent || SubMeshComponents.Num() == 0 || !SubMeshComponents[0]) return;

    if (TestMode == EPhysicsTestMode::None)
    {
        for (int32 i = 0; i < SubMeshComponents.Num(); ++i)
        {
            if (SubMeshComponents[i])
            {
                SubMeshComponents[i]->SetSimulatePhysics(false);
            }
        }
        
        // Clear old joint physics constraints
        for (UPhysicsConstraintComponent* Constraint : JointPhysicsConstraints)
        {
            if (Constraint)
            {
                Constraint->DestroyComponent();
            }
        }
        JointPhysicsConstraints.Empty();
        
        SetGarageViewMode(CurrentViewMode);
        ResetRobotPose();
        return;
    }

    // 1) Configure Gazebo-Style Links (Each CM_ component is an independent simulated Rigid Body)
    for (int32 i = 0; i < SubMeshComponents.Num(); ++i)
    {
        if (!SubMeshComponents[i]) continue;

        FString MeshName = SubMeshNames.IsValidIndex(i) ? SubMeshNames[i] : TEXT("");
        bool bIsCMOnly = MeshName.StartsWith(TEXT("CM_"), ESearchCase::IgnoreCase);

        if (bIsCMOnly && OriginalSubMeshVertices.IsValidIndex(i) && OriginalSubMeshVertices[i].Num() > 0)
        {
            SubMeshComponents[i]->bUseComplexAsSimpleCollision = false;
            SubMeshComponents[i]->ClearCollisionConvexMeshes();
            SubMeshComponents[i]->AddCollisionConvexMesh(OriginalSubMeshVertices[i]);

            SubMeshComponents[i]->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            SubMeshComponents[i]->SetCollisionObjectType(ECC_WorldDynamic);
            SubMeshComponents[i]->SetCollisionResponseToAllChannels(ECR_Block);
            SubMeshComponents[i]->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block); // Block floor plane!

            SubMeshComponents[i]->RecreatePhysicsState();

            // EVERY LINK SIMULATES PHYSICS NATIVELY LIKE GAZEBO (100% IMPOSSIBLE TO PENETRATE THE FLOOR!)
            SubMeshComponents[i]->SetSimulatePhysics(true);
            SubMeshComponents[i]->SetEnableGravity(TestMode != EPhysicsTestMode::HoldInAir);
            SubMeshComponents[i]->SetMassOverrideInKg(NAME_None, (i == 0 ? 30.0f : 2.5f), true);
            SubMeshComponents[i]->SetLinearDamping(0.8f);
            SubMeshComponents[i]->SetAngularDamping(2.0f);
        }
        else
        {
            SubMeshComponents[i]->SetSimulatePhysics(false);
        }

        // 2) Create Gazebo-Style Joint Constraint (<joint type="revolute">)
        if (i > 0)
        {
            int32 ParentIdx = ParentJointIndices.IsValidIndex(i) ? ParentJointIndices[i] : 0;
            UPrimitiveComponent* ParentComp = (ParentIdx >= 0 && SubMeshComponents.IsValidIndex(ParentIdx)) ? SubMeshComponents[ParentIdx] : SubMeshComponents[0];
            if (ParentComp)
            {
                // CRITICAL FIX: Detach simulating child component from scene graph to prevent SceneGraph vs ChaosPhysics conflict!
                SubMeshComponents[i]->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);

                FName ConstraintName = *FString::Printf(TEXT("GazeboJoint_%d"), i);
                UPhysicsConstraintComponent* ConstraintComp = NewObject<UPhysicsConstraintComponent>(this, ConstraintName);
                ConstraintComp->SetupAttachment(RootComponent);
                ConstraintComp->SetWorldLocation(SubMeshComponents[i]->GetComponentLocation());
                ConstraintComp->RegisterComponent();

                ConstraintComp->SetConstrainedComponents(ParentComp, NAME_None, SubMeshComponents[i], NAME_None);

                // GAZEBO PARENT-CHILD COLLISION FILTERING: DISABLE SELF-COLLISION BETWEEN CONNECTED LINKS!
                ConstraintComp->SetDisableCollision(true);

                // Lock Linear Motion (Wheel/Link CANNOT detach from axle origin)
                ConstraintComp->SetLinearXLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);
                ConstraintComp->SetLinearYLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);
                ConstraintComp->SetLinearZLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);

                // Revolute Joint: Twist is Free (Axle Spin), Swings are Locked
                ConstraintComp->SetAngularTwistLimit(EAngularConstraintMotion::ACM_Free, 0.0f);
                ConstraintComp->SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Locked, 0.0f);
                ConstraintComp->SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Locked, 0.0f);

                JointPhysicsConstraints.Add(ConstraintComp);
            }
        }
    }

    ResetDevKitTestValues();
    SubMeshComponents[0]->SetPhysicsLinearVelocity(FVector::ZeroVector);
    SubMeshComponents[0]->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
    SubMeshComponents[0]->SetLinearDamping(0.8f);
    SubMeshComponents[0]->SetAngularDamping(2.0f);

    SubMeshComponents[0]->SetSimulatePhysics(true);
    SubMeshComponents[0]->SetEnableGravity(TestMode != EPhysicsTestMode::HoldInAir);
    SubMeshComponents[0]->SetMassOverrideInKg(NAME_None, 35.0f, true);

    if (TestMode == EPhysicsTestMode::HoldInAir)
    {
        SubMeshComponents[0]->BodyInstance.bLockXTranslation = true;
        SubMeshComponents[0]->BodyInstance.bLockYTranslation = true;
        SubMeshComponents[0]->BodyInstance.bLockZTranslation = true;
        SubMeshComponents[0]->BodyInstance.bLockXRotation = false;
        SubMeshComponents[0]->BodyInstance.bLockYRotation = false;
        SubMeshComponents[0]->BodyInstance.bLockZRotation = false;
    }
    else if (TestMode == EPhysicsTestMode::LockRotation)
    {
        SubMeshComponents[0]->BodyInstance.bLockXTranslation = false;
        SubMeshComponents[0]->BodyInstance.bLockYTranslation = false;
        SubMeshComponents[0]->BodyInstance.bLockZTranslation = false;
        SubMeshComponents[0]->BodyInstance.bLockXRotation = true;
        SubMeshComponents[0]->BodyInstance.bLockYRotation = true;
        SubMeshComponents[0]->BodyInstance.bLockZRotation = true;
    }
    else if (TestMode == EPhysicsTestMode::FreeSim)
    {
        SubMeshComponents[0]->BodyInstance.bLockXTranslation = false;
        SubMeshComponents[0]->BodyInstance.bLockYTranslation = false;
        SubMeshComponents[0]->BodyInstance.bLockZTranslation = false;
        SubMeshComponents[0]->BodyInstance.bLockXRotation = false;
        SubMeshComponents[0]->BodyInstance.bLockYRotation = false;
        SubMeshComponents[0]->BodyInstance.bLockZRotation = false;
    }

    if (GEngine)
    {
        const TCHAR* ModeNames[4] = { TEXT("EDİT MODU (TEST KAPALI)"), TEXT("✈️ HAVADA SABİT TUT (HOLD IN AIR)"), TEXT("🔒 ROTASYONU SABİT TUT"), TEXT("🌊 HEPSİNİ SERBEST BIRAK (6-DOF)") };
        int32 ModeIdx = static_cast<int32>(TestMode);
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow,
            FString::Printf(TEXT(">>> [FİZİK TEST MODU] : %s <<<"), ModeNames[FMath::Clamp(ModeIdx, 0, 3)]));
    }
}

void APiSimGarageRobot::ResetRobotPose()
{
    SetActorLocationAndRotation(InitialChassisLocation, InitialChassisRotation);
    if (ChassisMeshComponent)
    {
        ChassisMeshComponent->SetPhysicsLinearVelocity(FVector::ZeroVector);
        ChassisMeshComponent->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan,
            TEXT(">>> [ROBOT POZİSYONU SIRIFLANDI] BAŞLANGIÇ KONUMUNA DÖNÜLDÜ! <<<"));
    }
}

void APiSimGarageRobot::ToggleChassisGroup(int32 MeshIndex)
{
    if (!StructuralPropsList.IsValidIndex(MeshIndex)) return;

    StructuralPropsList[MeshIndex].bIsChassisGroup = !StructuralPropsList[MeshIndex].bIsChassisGroup;

    if (GEngine)
    {
        FString StateStr = StructuralPropsList[MeshIndex].bIsChassisGroup ? TEXT("ANA GÖVDE GRUBUNA EKLENDİ (MAIN CHASSIS GROUP)") : TEXT("ANA GÖVDEDEN ÇIKARILDI (AYRI PARÇA)");
        GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Green,
            FString::Printf(TEXT(">>> [PARÇA GRUPLANMASI] EKLEM [%d] -> %s <<<"), MeshIndex, *StateStr));
    }
}

void APiSimGarageRobot::AddNewMotorActuator()
{
    FPiSimMotorActuator NewMotor;
    int32 NewIdx = ActuatorsList.Num();
    NewMotor.MotorName = FString::Printf(TEXT("Motor_%d"), NewIdx);
    NewMotor.MotorType = EMotorBehaviorType::Servo;
    NewMotor.MaxTorqueNm = 15.0f;
    NewMotor.MaxThrustNewton = 50.0f;
    NewMotor.TargetMeshIndex = 0;
    NewMotor.TargetMeshName = SubMeshNames.IsValidIndex(0) ? SubMeshNames[0] : TEXT("Chassis");
    NewMotor.bAffectsRotation = true;
    NewMotor.bAffectsLiftForce = false;
    NewMotor.AssignedPwmChannel = NewIdx % 16;
    NewMotor.DriverProfileName = TEXT("Standard Servo (1000 - 2000 us)");

    ActuatorsList.Add(NewMotor);

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Yellow,
            FString::Printf(TEXT(">>> [YENİ MOTOR EKLENDİ] : '%s' <<<"), *NewMotor.MotorName));
    }
}

void APiSimGarageRobot::BindSelectedMotorToMesh(int32 ActuatorIndex, int32 MeshIndex)
{
    if (!ActuatorsList.IsValidIndex(ActuatorIndex) || !SubMeshNames.IsValidIndex(MeshIndex)) return;

    ActuatorsList[ActuatorIndex].TargetMeshIndex = MeshIndex;
    ActuatorsList[ActuatorIndex].TargetMeshName = SubMeshNames[MeshIndex];

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Green,
            FString::Printf(TEXT(">>> [MOTOR HEDEFİ GÜNCELLENDİ] Motor '%s' -> Hedef Parça: [%d] '%s' <<<"),
                *ActuatorsList[ActuatorIndex].MotorName, MeshIndex, *SubMeshNames[MeshIndex]));
    }
}

static void CollectFbxModelNamesAndConnections(const TArray<uint8>& FileBytes, int32 StartOffset, int32 EndOffset, TMap<uint64, FString>& OutModelNames, TMap<uint64, uint64>& OutGeomToModelMap)
{
    int32 Pos = StartOffset;
    while (Pos + 13 < EndOffset)
    {
        uint32 NodeEnd = *reinterpret_cast<const uint32*>(&FileBytes[Pos]);
        uint32 NumProps = *reinterpret_cast<const uint32*>(&FileBytes[Pos + 4]);
        uint32 PropLen = *reinterpret_cast<const uint32*>(&FileBytes[Pos + 8]);
        uint8 NameLen = FileBytes[Pos + 12];

        if (NodeEnd == 0 || NodeEnd > (uint32)EndOffset || Pos + 13 + NameLen > EndOffset)
        {
            break;
        }

        FString NodeName = FString(NameLen, (const ANSICHAR*)&FileBytes[Pos + 13]);
        int32 PropsStart = Pos + 13 + NameLen;
        int32 ChildrenStart = PropsStart + PropLen;

        if (NodeName.Equals(TEXT("Model")) && NumProps >= 2)
        {
            uint64 ModelID = 0;
            if (FileBytes[PropsStart] == 'L')
            {
                ModelID = *reinterpret_cast<const uint64*>(&FileBytes[PropsStart + 1]);
            }
            int32 P1 = PropsStart + 9;
            if (P1 < ChildrenStart && FileBytes[P1] == 'S' && P1 + 5 <= ChildrenStart)
            {
                uint32 StrLen = *reinterpret_cast<const uint32*>(&FileBytes[P1 + 1]);
                if (P1 + 5 + (int32)StrLen <= ChildrenStart)
                {
                    FString FullPropStr = FString(StrLen, (const ANSICHAR*)&FileBytes[P1 + 5]);
                    int32 NullIdx = -1;
                    FString CleanName = FullPropStr.FindChar('\0', NullIdx) ? FullPropStr.Left(NullIdx) : FullPropStr;
                    CleanName.RemoveFromStart(TEXT("Model::"));
                    OutModelNames.Add(ModelID, CleanName);
                }
            }
        }
        else if (NodeName.Equals(TEXT("Connections")))
        {
            int32 ChildP = ChildrenStart;
            while (ChildP + 13 < (int32)NodeEnd)
            {
                uint32 CEnd = *reinterpret_cast<const uint32*>(&FileBytes[ChildP]);
                uint32 CNumProps = *reinterpret_cast<const uint32*>(&FileBytes[ChildP + 4]);
                uint32 CPropLen = *reinterpret_cast<const uint32*>(&FileBytes[ChildP + 8]);
                uint8 CNameLen = FileBytes[ChildP + 12];

                if (CEnd == 0 || CEnd > NodeEnd || ChildP + 13 + CNameLen > (int32)NodeEnd) break;

                FString CName = FString(CNameLen, (const ANSICHAR*)&FileBytes[ChildP + 13]);
                int32 CPropsStart = ChildP + 13 + CNameLen;

                if (CName.Equals(TEXT("C")) && CNumProps >= 3)
                {
                    int32 P0 = CPropsStart;
                    if (FileBytes[P0] == 'S')
                    {
                        uint32 SLen = *reinterpret_cast<const uint32*>(&FileBytes[P0 + 1]);
                        int32 P1 = P0 + 5 + SLen;
                        if (P1 + 9 <= CPropsStart + (int32)CPropLen && FileBytes[P1] == 'L')
                        {
                            uint64 ChildID = *reinterpret_cast<const uint64*>(&FileBytes[P1 + 1]);
                            int32 P2 = P1 + 9;
                            if (P2 + 9 <= CPropsStart + (int32)CPropLen && FileBytes[P2] == 'L')
                            {
                                uint64 ParentID = *reinterpret_cast<const uint64*>(&FileBytes[P2 + 1]);
                                OutGeomToModelMap.Add(ChildID, ParentID);
                            }
                        }
                    }
                }
                ChildP = CEnd;
            }
        }
        else if (ChildrenStart < (int32)NodeEnd)
        {
            CollectFbxModelNamesAndConnections(FileBytes, ChildrenStart, NodeEnd, OutModelNames, OutGeomToModelMap);
        }

        Pos = NodeEnd;
    }
}

static void ParseFbxBinaryNodeTreeHelper(const TArray<uint8>& FileBytes, int32 StartOffset, int32 EndOffset, TMap<uint64, FString>& ModelNames, TMap<uint64, uint64>& GeomToModelMap, TArray<FGLBMeshSection>& OutSections, float ScaleMultiplier)

{
    int32 Pos = StartOffset;
    while (Pos + 13 < EndOffset)
    {
        uint32 NodeEnd = *reinterpret_cast<const uint32*>(&FileBytes[Pos]);
        uint32 NumProps = *reinterpret_cast<const uint32*>(&FileBytes[Pos + 4]);
        uint32 PropLen = *reinterpret_cast<const uint32*>(&FileBytes[Pos + 8]);
        uint8 NameLen = FileBytes[Pos + 12];

        if (NodeEnd == 0 || NodeEnd > (uint32)EndOffset || Pos + 13 + NameLen > EndOffset)
        {
            break;
        }

        FString NodeName = FString(NameLen, (const ANSICHAR*)&FileBytes[Pos + 13]);
        int32 PropsStart = Pos + 13 + NameLen;
        int32 ChildrenStart = PropsStart + PropLen;

        if (NodeName.Equals(TEXT("Geometry")))
        {
            uint64 GeomID = 0;
            if (NumProps >= 1 && PropLen >= 9)
            {
                if (FileBytes[PropsStart] == 'L')
                {
                    GeomID = *reinterpret_cast<const uint64*>(&FileBytes[PropsStart + 1]);
                }
            }

            FString MeshName = TEXT("CM_SubMesh");
            if (GeomToModelMap.Contains(GeomID))
            {
                uint64 ModelID = GeomToModelMap[GeomID];
                if (ModelNames.Contains(ModelID))
                {
                    MeshName = ModelNames[ModelID];
                }
            }

            FGLBMeshSection MeshSec;
            MeshSec.MeshName = MeshName;

            int32 ChildP = ChildrenStart;
            while (ChildP + 13 < (int32)NodeEnd)
            {
                uint32 CEnd = *reinterpret_cast<const uint32*>(&FileBytes[ChildP]);
                uint32 CNumProps = *reinterpret_cast<const uint32*>(&FileBytes[ChildP + 4]);
                uint32 CPropLen = *reinterpret_cast<const uint32*>(&FileBytes[ChildP + 8]);
                uint8 CNameLen = FileBytes[ChildP + 12];

                if (CEnd == 0 || CEnd > NodeEnd || ChildP + 13 + CNameLen > (int32)NodeEnd) break;

                FString CName = FString(CNameLen, (const ANSICHAR*)&FileBytes[ChildP + 13]);
                int32 CPropsStart = ChildP + 13 + CNameLen;

                if (CName.Equals(TEXT("Vertices")) && CPropLen > 12)
                {
                    uint8 TypeCode = FileBytes[CPropsStart];
                    uint32 ArrayLen = *reinterpret_cast<const uint32*>(&FileBytes[CPropsStart + 1]);
                    uint32 Encoding = *reinterpret_cast<const uint32*>(&FileBytes[CPropsStart + 5]);
                    uint32 CompLen = *reinterpret_cast<const uint32*>(&FileBytes[CPropsStart + 9]);
                    int32 DataOffset = CPropsStart + 13;

                    int32 ElemSize = (TypeCode == 'd' ? 8 : 4);
                    int32 UncompSize = ArrayLen * ElemSize;
                    TArray<uint8> UncompBuf;
                    const uint8* DataPtr = nullptr;

                    if (Encoding == 0 && DataOffset + UncompSize <= (int32)NodeEnd)
                    {
                        DataPtr = &FileBytes[DataOffset];
                    }
                    else if (Encoding == 1 && CompLen > 0 && DataOffset + (int32)CompLen <= (int32)NodeEnd)
                    {
                        UncompBuf.AddUninitialized(UncompSize);
                        if (FCompression::UncompressMemory(NAME_Zlib, (void*)UncompBuf.GetData(), (int64)UncompSize, (const void*)&FileBytes[DataOffset], (int64)CompLen))
                        {
                            DataPtr = UncompBuf.GetData();
                        }
                    }

                    if (DataPtr && ArrayLen > 0)
                    {
                        if (TypeCode == 'd')
                        {
                            const double* VData = reinterpret_cast<const double*>(DataPtr);
                            for (uint32 v = 0; v + 2 < ArrayLen; v += 3)
                            {
                                MeshSec.Vertices.Add(FVector(VData[v] * ScaleMultiplier, -VData[v + 1] * ScaleMultiplier, VData[v + 2] * ScaleMultiplier));
                                MeshSec.Normals.Add(FVector(0, 0, 1));
                            }
                        }
                        else if (TypeCode == 'f')
                        {
                            const float* VData = reinterpret_cast<const float*>(DataPtr);
                            for (uint32 v = 0; v + 2 < ArrayLen; v += 3)
                            {
                                MeshSec.Vertices.Add(FVector(VData[v] * ScaleMultiplier, -VData[v + 1] * ScaleMultiplier, VData[v + 2] * ScaleMultiplier));
                                MeshSec.Normals.Add(FVector(0, 0, 1));
                            }
                        }
                    }
                }
                else if (CName.Equals(TEXT("PolygonVertexIndex")) && CPropLen > 12)
                {
                    uint8 TypeCode = FileBytes[CPropsStart];
                    uint32 ArrayLen = *reinterpret_cast<const uint32*>(&FileBytes[CPropsStart + 1]);
                    uint32 Encoding = *reinterpret_cast<const uint32*>(&FileBytes[CPropsStart + 5]);
                    uint32 CompLen = *reinterpret_cast<const uint32*>(&FileBytes[CPropsStart + 9]);
                    int32 DataOffset = CPropsStart + 13;

                    int32 UncompSize = ArrayLen * 4;
                    TArray<uint8> UncompBuf;
                    const uint8* DataPtr = nullptr;

                    if (Encoding == 0 && DataOffset + UncompSize <= (int32)NodeEnd)
                    {
                        DataPtr = &FileBytes[DataOffset];
                    }
                    else if (Encoding == 1 && CompLen > 0 && DataOffset + (int32)CompLen <= (int32)NodeEnd)
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
                                        MeshSec.Triangles.Add(PolyLoop[0]);
                                        MeshSec.Triangles.Add(PolyLoop[p]);
                                        MeshSec.Triangles.Add(PolyLoop[p + 1]);
                                    }
                                }
                                PolyLoop.Empty();
                            }
                        }
                    }
                }

                ChildP = CEnd;
            }

            if (MeshSec.Triangles.Num() == 0 && MeshSec.Vertices.Num() >= 3)
            {
                for (int32 v = 0; v + 2 < MeshSec.Vertices.Num(); v += 3)
                {
                    MeshSec.Triangles.Add(v);
                    MeshSec.Triangles.Add(v + 1);
                    MeshSec.Triangles.Add(v + 2);
                }
            }

            if (MeshSec.Vertices.Num() > 0 && MeshSec.Triangles.Num() > 0)
            {
                // Calculate Centroid Origin & Subtract Centroid for Local Mesh Offsets
                FVector Centroid = FVector::ZeroVector;
                for (const FVector& Vert : MeshSec.Vertices)
                {
                    Centroid += Vert;
                }
                Centroid /= (float)MeshSec.Vertices.Num();

                MeshSec.PivotPoint = Centroid;
                for (FVector& Vert : MeshSec.Vertices)
                {
                    Vert -= Centroid;
                }

                OutSections.Add(MeshSec);
                UE_LOG(LogTemp, Warning, TEXT("[FBX LOADER LOG] Extracted FBX Geometry Section '%s' (GeomID: %llu): %d Vertices, %d Triangles | Centroid: %s"),
                    *MeshSec.MeshName, GeomID, MeshSec.Vertices.Num(), MeshSec.Triangles.Num() / 3, *Centroid.ToString());
            }
        }
        else if (ChildrenStart < (int32)NodeEnd)
        {
            ParseFbxBinaryNodeTreeHelper(FileBytes, ChildrenStart, NodeEnd, ModelNames, GeomToModelMap, OutSections, ScaleMultiplier);
        }

        Pos = NodeEnd;
    }
}

static bool ParseFbxSkinClusters(const TArray<uint8>& FileBytes, const FString& FilePath, TArray<FGLBMeshSection>& OutSections, float ScaleMultiplier)
{
    int32 FileSize = FileBytes.Num();
    int32 Pos = 27;
    TArray<FVector> AllVerts;
    TArray<int32> AllPolys;
    TMap<FString, TArray<int32>> Clusters;
    TMap<FString, float> NodeMassMap;
    TMap<FString, float> NodeFrictionMap;
    TMap<FString, FString> NodeJointTypeMap;

    bool bIsCollisionFbx = FilePath.Contains(TEXT("collision"), ESearchCase::IgnoreCase) || FilePath.Contains(TEXT("CM_"), ESearchCase::IgnoreCase);

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

                        if (CName.Equals(TEXT("Vertices")) && CPropLen > 12)
                        {
                            uint8 TypeCode = FileBytes[CPropsStart];
                            uint32 ArrayLen = *reinterpret_cast<const uint32*>(&FileBytes[CPropsStart + 1]);
                            uint32 Encoding = *reinterpret_cast<const uint32*>(&FileBytes[CPropsStart + 5]);
                            uint32 CompLen = *reinterpret_cast<const uint32*>(&FileBytes[CPropsStart + 9]);
                            int32 DataOffset = CPropsStart + 13;

                            int32 ElemSize = (TypeCode == 'd' ? 8 : 4);
                            int32 UncompSize = ArrayLen * ElemSize;
                            TArray<uint8> UncompBuf;
                            const uint8* DataPtr = nullptr;

                            if (Encoding == 0 && DataOffset + UncompSize <= (int32)SubEnd)
                            {
                                DataPtr = &FileBytes[DataOffset];
                            }
                            else if (Encoding == 1 && CompLen > 0 && DataOffset + (int32)CompLen <= (int32)SubEnd)
                            {
                                UncompBuf.AddUninitialized(UncompSize);
                                if (FCompression::UncompressMemory(NAME_Zlib, (void*)UncompBuf.GetData(), (int64)UncompSize, (const void*)&FileBytes[DataOffset], (int64)CompLen))
                                {
                                    DataPtr = UncompBuf.GetData();
                                }
                            }

                            if (DataPtr && ArrayLen > 0)
                            {
                                if (TypeCode == 'd')
                                {
                                    const double* VData = reinterpret_cast<const double*>(DataPtr);
                                    for (uint32 v = 0; v + 2 < ArrayLen; v += 3)
                                    {
                                        AllVerts.Add(FVector(VData[v] * ScaleMultiplier, -VData[v + 1] * ScaleMultiplier, VData[v + 2] * ScaleMultiplier));
                                    }
                                }
                                else if (TypeCode == 'f')
                                {
                                    const float* VData = reinterpret_cast<const float*>(DataPtr);
                                    for (uint32 v = 0; v + 2 < ArrayLen; v += 3)
                                    {
                                        AllVerts.Add(FVector(VData[v] * ScaleMultiplier, -VData[v + 1] * ScaleMultiplier, VData[v + 2] * ScaleMultiplier));
                                    }
                                }
                            }
                        }
                        else if (CName.Equals(TEXT("PolygonVertexIndex")) && CPropLen > 12)
                        {
                            uint8 TypeCode = FileBytes[CPropsStart];
                            uint32 ArrayLen = *reinterpret_cast<const uint32*>(&FileBytes[CPropsStart + 1]);
                            uint32 Encoding = *reinterpret_cast<const uint32*>(&FileBytes[CPropsStart + 5]);
                            uint32 CompLen = *reinterpret_cast<const uint32*>(&FileBytes[CPropsStart + 9]);
                            int32 DataOffset = CPropsStart + 13;

                            int32 UncompSize = ArrayLen * 4;
                            TArray<uint8> UncompBuf;
                            const uint8* DataPtr = nullptr;

                            if (Encoding == 0 && DataOffset + UncompSize <= (int32)SubEnd)
                            {
                                DataPtr = &FileBytes[DataOffset];
                            }
                            else if (Encoding == 1 && CompLen > 0 && DataOffset + (int32)CompLen <= (int32)SubEnd)
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
                                        if (PolyLoop.Num() >= 3)
                                        {
                                            for (int32 t = 1; t < PolyLoop.Num() - 1; ++t)
                                            {
                                                AllPolys.Add(PolyLoop[0]);
                                                AllPolys.Add(PolyLoop[t]);
                                                AllPolys.Add(PolyLoop[t + 1]);
                                            }
                                        }
                                        PolyLoop.Empty();
                                    }
                                }
                            }
                        }
                        CCurr = CEnd;
                    }
                }
                else if (SubName.Equals(TEXT("NodeAttribute")))
                {
                    int32 PPos = SubPropsStart;
                    int32 StrPos = PPos + 9;
                    FString AttrLabel = TEXT("");
                    if (StrPos < SubChildrenStart && FileBytes[StrPos] == 'S' && StrPos + 5 <= SubChildrenStart)
                    {
                        uint32 SLen = *reinterpret_cast<const uint32*>(&FileBytes[StrPos + 1]);
                        if (StrPos + 5 + (int32)SLen <= SubChildrenStart)
                        {
                            AttrLabel = FString(SLen, (const ANSICHAR*)&FileBytes[StrPos + 5]);
                            int32 SpaceIdx = -1;
                            if (AttrLabel.FindChar(' ', SpaceIdx)) AttrLabel = AttrLabel.Left(SpaceIdx);
                            AttrLabel.RemoveFromStart(TEXT("NodeAttribute::"));
                        }
                    }

                    int32 PropsSub = SubChildrenStart;
                    while (PropsSub + 13 < (int32)SubEnd)
                    {
                        uint32 PEnd = *reinterpret_cast<const uint32*>(&FileBytes[PropsSub]);
                        uint32 PNumProps = *reinterpret_cast<const uint32*>(&FileBytes[PropsSub + 4]);
                        uint32 PPropLen = *reinterpret_cast<const uint32*>(&FileBytes[PropsSub + 8]);
                        uint8 PNameLen = FileBytes[PropsSub + 12];

                        if (PEnd == 0 || PEnd > SubEnd || PropsSub + 13 + PNameLen > (int32)SubEnd) break;

                        FString PNodename = FString(PNameLen, (const ANSICHAR*)&FileBytes[PropsSub + 13]);
                        if (PNodename.Equals(TEXT("Properties70")))
                        {
                            int32 PCurr = PropsSub + 13 + PNameLen + PPropLen;
                            while (PCurr + 13 < (int32)PEnd)
                            {
                                uint32 PEnd2 = *reinterpret_cast<const uint32*>(&FileBytes[PCurr]);
                                uint8 PNL2 = FileBytes[PCurr + 12];
                                if (PEnd2 == 0 || PEnd2 > PEnd || PCurr + 13 + PNL2 > (int32)PEnd) break;

                                FString PName2 = FString(PNL2, (const ANSICHAR*)&FileBytes[PCurr + 13]);
                                if (PName2.Equals(TEXT("P")))
                                {
                                    int32 CPos = PCurr + 13 + PNL2;
                                    if (CPos + 5 <= (int32)PEnd2 && FileBytes[CPos] == 'S')
                                    {
                                        uint32 KLen = *reinterpret_cast<const uint32*>(&FileBytes[CPos + 1]);
                                        if (CPos + 5 + (int32)KLen <= (int32)PEnd2)
                                        {
                                            FString PropKey = FString(KLen, (const ANSICHAR*)&FileBytes[CPos + 5]);
                                            CPos += 5 + KLen;

                                            for (int32 s = 0; s < 3; ++s)
                                            {
                                                if (CPos + 5 <= (int32)PEnd2 && (FileBytes[CPos] == 'S' || FileBytes[CPos] == 'R'))
                                                {
                                                    uint32 TLen = *reinterpret_cast<const uint32*>(&FileBytes[CPos + 1]);
                                                    CPos += 5 + TLen;
                                                }
                                            }

                                            if (CPos < (int32)PEnd2)
                                            {
                                                uint8 VType = FileBytes[CPos];
                                                if (VType == 'D' && CPos + 9 <= (int32)PEnd2)
                                                {
                                                    double DVal = *reinterpret_cast<const double*>(&FileBytes[CPos + 1]);
                                                    if (PropKey.Equals(TEXT("mass"), ESearchCase::IgnoreCase) || PropKey.Equals(TEXT("mas"), ESearchCase::IgnoreCase))
                                                    {
                                                        NodeMassMap.Add(AttrLabel, (float)DVal);
                                                    }
                                                    else if (PropKey.Equals(TEXT("friction"), ESearchCase::IgnoreCase))
                                                    {
                                                        NodeFrictionMap.Add(AttrLabel, (float)DVal);
                                                    }
                                                }
                                                else if (VType == 'F' && CPos + 5 <= (int32)PEnd2)
                                                {
                                                    float FVal = *reinterpret_cast<const float*>(&FileBytes[CPos + 1]);
                                                    if (PropKey.Equals(TEXT("mass"), ESearchCase::IgnoreCase) || PropKey.Equals(TEXT("mas"), ESearchCase::IgnoreCase))
                                                    {
                                                        NodeMassMap.Add(AttrLabel, FVal);
                                                    }
                                                    else if (PropKey.Equals(TEXT("friction"), ESearchCase::IgnoreCase))
                                                    {
                                                        NodeFrictionMap.Add(AttrLabel, FVal);
                                                    }
                                                }
                                                else if ((VType == 'S' || VType == 'R') && CPos + 5 <= (int32)PEnd2)
                                                {
                                                    uint32 VLen = *reinterpret_cast<const uint32*>(&FileBytes[CPos + 1]);
                                                    if (CPos + 5 + (int32)VLen <= (int32)PEnd2)
                                                    {
                                                        FString SVal = FString(VLen, (const ANSICHAR*)&FileBytes[CPos + 5]);
                                                        if (PropKey.Equals(TEXT("joint_type"), ESearchCase::IgnoreCase))
                                                        {
                                                            NodeJointTypeMap.Add(AttrLabel, SVal);
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                                PCurr = PEnd2;
                            }
                        }
                        PropsSub = PEnd;
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
                            int32 SpaceIdx = -1;
                            if (DefLabel.FindChar(' ', SpaceIdx)) DefLabel = DefLabel.Left(SpaceIdx);
                            DefLabel.RemoveFromStart(TEXT("SubDeformer::"));
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

                        if (CName.Equals(TEXT("Indexes")) && CPropLen > 12)
                        {
                            uint8 TypeCode = FileBytes[CPropsStart];
                            uint32 ArrayLen = *reinterpret_cast<const uint32*>(&FileBytes[CPropsStart + 1]);
                            uint32 Encoding = *reinterpret_cast<const uint32*>(&FileBytes[CPropsStart + 5]);
                            uint32 CompLen = *reinterpret_cast<const uint32*>(&FileBytes[CPropsStart + 9]);
                            int32 DataOffset = CPropsStart + 13;

                            int32 UncompSize = ArrayLen * 4;
                            TArray<uint8> UncompBuf;
                            const uint8* DataPtr = nullptr;

                            if (Encoding == 0 && DataOffset + UncompSize <= (int32)SubEnd)
                            {
                                DataPtr = &FileBytes[DataOffset];
                            }
                            else if (Encoding == 1 && CompLen > 0 && DataOffset + (int32)CompLen <= (int32)SubEnd)
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

        FGLBMeshSection MeshSec;
        MeshSec.MeshName = (bIsCollisionFbx ? TEXT("CM_") : TEXT("")) + CName;
        MeshSec.ParentSectionIndex = (c == 0 ? -1 : 0);
        MeshSec.DepthLevel = (c == 0 ? 0 : 1);

        if (NodeMassMap.Contains(CName)) MeshSec.MassKg = NodeMassMap[CName];
        else MeshSec.MassKg = (c == 0 ? 31.0f : 2.7f);

        if (NodeFrictionMap.Contains(CName)) MeshSec.Friction = NodeFrictionMap[CName];
        else MeshSec.Friction = 0.86f;

        if (NodeJointTypeMap.Contains(CName)) MeshSec.JointType = NodeJointTypeMap[CName];
        else MeshSec.JointType = (c == 0 ? TEXT("fixed") : TEXT("revolute"));

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

            OutSections.Add(MeshSec);
            UE_LOG(LogTemp, Warning, TEXT("[FBX CLUSTER LOADER] Extracted Skin Bone Section '%s' (Sec %d): %d Verts, %d Tris | Mass: %.2f kg | Fric: %.2f | Joint: %s | Centroid: %s"),
                *MeshSec.MeshName, c, MeshSec.Vertices.Num(), MeshSec.Triangles.Num() / 3, MeshSec.MassKg, MeshSec.Friction, *MeshSec.JointType, *Centroid.ToString());
        }
    }

    return OutSections.Num() > 0;
}

bool APiSimGarageRobot::ParseFbxAllBinaryMeshes(const FString& FilePath, TArray<FGLBMeshSection>& OutSections, float ScaleMultiplier)
{
    OutSections.Empty();
    if (!FPaths::FileExists(FilePath))
    {
        UE_LOG(LogTemp, Warning, TEXT("[FBX LOADER LOG] FBX File does NOT exist at path: %s"), *FilePath);
        return false;
    }

    TArray<uint8> FileBytes;
    if (!FFileHelper::LoadFileToArray(FileBytes, *FilePath))
    {
        UE_LOG(LogTemp, Warning, TEXT("[FBX LOADER LOG] Failed to load FBX File bytes from: %s"), *FilePath);
        return false;
    }

    int32 FileSize = FileBytes.Num();
    UE_LOG(LogTemp, Warning, TEXT("[FBX LOADER LOG] Reading FBX File: %s (%d bytes)"), *FilePath, FileSize);

    bool bIsBinaryFbx = false;
    if (FileSize >= 23)
    {
        FString HeaderStr = FString(20, (const ANSICHAR*)FileBytes.GetData());
        if (HeaderStr.StartsWith(TEXT("Kaydara FBX Binary")))
        {
            bIsBinaryFbx = true;
            UE_LOG(LogTemp, Warning, TEXT("[FBX LOADER LOG] Detected BINARY FBX Header (Kaydara FBX Binary)!"));
        }
    }

    if (!bIsBinaryFbx)
    {
        UE_LOG(LogTemp, Warning, TEXT("[FBX LOADER LOG] Detected ASCII FBX format. Parsing text lines..."));

        FString FbxText;
        FFileHelper::BufferToString(FbxText, FileBytes.GetData(), FileSize);

        TArray<FString> Lines;
        FbxText.ParseIntoArrayLines(Lines);

        FGLBMeshSection CurrentSection;
        bool bInVertices = false;
        bool bInPolygonIndices = false;
        FString CurrentNodeName = TEXT("CM_Chassis");

        for (const FString& Line : Lines)
        {
            FString Trimmed = Line.TrimStartAndEnd();
            if (Trimmed.StartsWith(TEXT("Model:")))
            {
                if (CurrentSection.Vertices.Num() > 0 && CurrentSection.Triangles.Num() > 0)
                {
                    CurrentSection.MeshName = CurrentNodeName;
                    OutSections.Add(CurrentSection);
                    UE_LOG(LogTemp, Warning, TEXT("[FBX LOADER LOG] Parsed ASCII FBX Section '%s': %d Vertices, %d Triangles"),
                        *CurrentSection.MeshName, CurrentSection.Vertices.Num(), CurrentSection.Triangles.Num() / 3);
                    CurrentSection = FGLBMeshSection();
                }
                int32 FirstQuote = -1;
                if (Trimmed.FindChar('"', FirstQuote))
                {
                    FString Rest = Trimmed.Mid(FirstQuote + 1);
                    int32 SecondQuote = -1;
                    if (Rest.FindChar('"', SecondQuote))
                    {
                        CurrentNodeName = Rest.Left(SecondQuote);
                        CurrentNodeName.RemoveFromStart(TEXT("Model::"));
                    }
                }
            }
            else if (Trimmed.StartsWith(TEXT("Vertices:")))
            {
                bInVertices = true;
                bInPolygonIndices = false;
            }
            else if (Trimmed.StartsWith(TEXT("PolygonVertexIndex:")))
            {
                bInVertices = false;
                bInPolygonIndices = true;
            }
            else if (Trimmed.Contains(TEXT("}")) || Trimmed.StartsWith(TEXT("LayerElement")))
            {
                bInVertices = false;
                bInPolygonIndices = false;
            }
            else if (bInVertices)
            {
                TArray<FString> Tokens;
                Trimmed.ParseIntoArray(Tokens, TEXT(","), true);
                TArray<float> Vals;
                for (const FString& Tok : Tokens)
                {
                    FString ClearTok = Tok.TrimStartAndEnd();
                    if (!ClearTok.IsEmpty())
                    {
                        Vals.Add(FCString::Atof(*ClearTok));
                    }
                }
                for (int32 v = 0; v + 2 < Vals.Num(); v += 3)
                {
                    CurrentSection.Vertices.Add(FVector(Vals[v] * ScaleMultiplier, -Vals[v + 1] * ScaleMultiplier, Vals[v + 2] * ScaleMultiplier));
                    CurrentSection.Normals.Add(FVector(0, 0, 1));
                }
            }
            else if (bInPolygonIndices)
            {
                TArray<FString> Tokens;
                Trimmed.ParseIntoArray(Tokens, TEXT(","), true);
                for (const FString& Tok : Tokens)
                {
                    FString ClearTok = Tok.TrimStartAndEnd();
                    if (!ClearTok.IsEmpty())
                    {
                        int32 Idx = FCString::Atoi(*ClearTok);
                        if (Idx < 0)
                        {
                            Idx = -Idx - 1;
                        }
                        CurrentSection.Triangles.Add(Idx);
                    }
                }
            }
        }

        if (CurrentSection.Vertices.Num() > 0)
        {
            CurrentSection.MeshName = CurrentNodeName;
            OutSections.Add(CurrentSection);
            UE_LOG(LogTemp, Warning, TEXT("[FBX LOADER LOG] Parsed Final ASCII FBX Section '%s': %d Vertices, %d Triangles"),
                *CurrentSection.MeshName, CurrentSection.Vertices.Num(), CurrentSection.Triangles.Num() / 3);
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[FBX LOADER LOG] Binary FBX format detected (%d bytes). Attempting Skin Bone Cluster Extraction..."), FileSize);
        if (ParseFbxSkinClusters(FileBytes, FilePath, OutSections, ScaleMultiplier))
        {
            UE_LOG(LogTemp, Warning, TEXT("[FBX LOADER SUCCESS] Successfully extracted %d Skin Bone SubMesh Sections from FBX Clusters!"), OutSections.Num());
        }
        else
        {
            TMap<uint64, FString> ModelNames;
            TMap<uint64, uint64> GeomToModelMap;
            CollectFbxModelNamesAndConnections(FileBytes, 27, FileSize, ModelNames, GeomToModelMap);
            UE_LOG(LogTemp, Warning, TEXT("[FBX LOADER LOG] Found %d Model Names and %d Connections! Traversing FBX Node Tree..."), ModelNames.Num(), GeomToModelMap.Num());
            ParseFbxBinaryNodeTreeHelper(FileBytes, 27, FileSize, ModelNames, GeomToModelMap, OutSections, ScaleMultiplier);

            // Resolve ParentSectionIndex for each extracted section using FBX parent model connections
            TMap<uint64, int32> ModelToSecIdxMap;
            for (int32 s = 0; s < OutSections.Num(); ++s)
            {
                for (const auto& Pair : ModelNames)
                {
                    if (Pair.Value.Equals(OutSections[s].MeshName, ESearchCase::IgnoreCase))
                    {
                        ModelToSecIdxMap.Add(Pair.Key, s);
                        break;
                    }
                }
            }

            for (int32 s = 0; s < OutSections.Num(); ++s)
            {
                OutSections[s].ParentSectionIndex = -1;
                for (const auto& Pair : ModelNames)
                {
                    if (Pair.Value.Equals(OutSections[s].MeshName, ESearchCase::IgnoreCase))
                    {
                        uint64 ModelID = Pair.Key;
                        if (GeomToModelMap.Contains(ModelID))
                        {
                            uint64 ParentModelID = GeomToModelMap[ModelID];
                            if (ModelToSecIdxMap.Contains(ParentModelID))
                            {
                                OutSections[s].ParentSectionIndex = ModelToSecIdxMap[ParentModelID];
                                UE_LOG(LogTemp, Warning, TEXT("[FBX HIERARCHY LOG] SubMesh '%s' (Sec %d) ATTACHED TO PARENT '%s' (Sec %d)"),
                                    *OutSections[s].MeshName, s, *OutSections[OutSections[s].ParentSectionIndex].MeshName, OutSections[s].ParentSectionIndex);
                            }
                        }
                        break;
                    }
                }
            }
        }
    }


    UE_LOG(LogTemp, Warning, TEXT("[FBX LOADER LOG] Total FBX Sections Parsed: %d"), OutSections.Num());
    return OutSections.Num() > 0;
}

void APiSimGarageRobot::BakeFbxRobotToStaticMesh()
{
    FString FbxFullPath = FPaths::ProjectSavedDir() / TEXT("Robots/Cache/robot.fbx");
    if (!FPaths::FileExists(FbxFullPath))
    {
        UE_LOG(LogTemp, Warning, TEXT("[BAKE FBX LOG] robot.fbx does not exist at: %s"), *FbxFullPath);
        return;
    }

    TArray<FGLBMeshSection> Sections;
    if (!ParseFbxAllBinaryMeshes(FbxFullPath, Sections, CadUnitScaleMultiplier))
    {
        UE_LOG(LogTemp, Warning, TEXT("[BAKE FBX LOG] Failed to parse robot.fbx!"));
        return;
    }

    // Save pre-cooked binary cache to Saved/Robots/Baked/robot_baked.bin for zero-delay Cloud Shipped runtime loading
    FString BakedDir = FPaths::ProjectSavedDir() / TEXT("Robots/Baked");
    IFileManager::Get().MakeDirectory(*BakedDir, true);
    FString BakedFilePath = BakedDir / TEXT("robot_baked.bin");

    FBufferArchive Ar;
    int32 SecCount = Sections.Num();
    Ar << SecCount;

    for (FGLBMeshSection& Sec : Sections)
    {
        Ar << Sec.MeshName;
        Ar << Sec.Vertices;
        Ar << Sec.Triangles;
        Ar << Sec.Normals;
        Ar << Sec.PivotPoint;
    }

    if (FFileHelper::SaveArrayToFile(Ar, *BakedFilePath))
    {
        UE_LOG(LogTemp, Warning, TEXT("[BAKE FBX LOG] Saved Pre-Cooked Binary Cache to: %s (%d bytes)"), *BakedFilePath, Ar.Num());
    }

    for (int32 i = 0; i < SubMeshComponents.Num(); ++i)
    {
        if (SubMeshComponents[i])
        {
            SubMeshComponents[i]->bUseComplexAsSimpleCollision = true;
            SubMeshComponents[i]->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            SubMeshComponents[i]->SetCollisionObjectType(ECC_WorldDynamic);
            SubMeshComponents[i]->SetCollisionResponseToAllChannels(ECR_Block);
            SubMeshComponents[i]->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
            SubMeshComponents[i]->RecreatePhysicsState();
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("[BAKE FBX LOG] Successfully pre-cooked %d sub-meshes with Use Complex Collision As Simple!"), Sections.Num());
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Green,
            FString::Printf(TEXT(">>> [BAKE FBX SUCCESS] %d PARÇA PİŞİRİLDİ VE Saved/Robots/Baked/ İÇİNE KAYDEDİLDİ! <<<"), Sections.Num()));
    }
}









