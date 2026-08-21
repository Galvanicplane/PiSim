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

    TArray<uint8> FileBytes;
    if (!FFileHelper::LoadFileToArray(FileBytes, *FilePath) || FileBytes.Num() < 64)
    {
        UE_LOG(LogTemp, Error, TEXT("[PiSimModelImporter] FBX dosyasi okunamadi: %s"), *FilePath);
        return false;
    }

    int32 FileSize = FileBytes.Num();
    int32 Pos = 27;

    struct FRawFbxGeom
    {
        FString Label;
        TArray<FVector> Vertices;
        TArray<int32> Polygons;
    };

    struct FRawFbxSubDef
    {
        FString BoneLabel;
        TArray<int32> Indices;
    };

    TMap<uint64, FString> ModelMap;
    TMap<uint64, FRawFbxGeom> GeomMap;
    TMap<uint64, FString> DeformerMap;
    TMap<uint64, FRawFbxSubDef> SubDeformerMap;

    TMap<uint64, TArray<uint64>> ChildToParents;
    TMap<uint64, TArray<uint64>> ParentToChildren;

    // 1) FBX Node Parser Helper
    auto ParseNodeHeader = [&](int32 P, uint32& OutEnd, uint32& OutNumProps, uint32& OutPropLen, FString& OutName, int32& OutPropsStart, int32& OutChildStart) -> bool
    {
        if (P + 13 > FileSize) return false;
        OutEnd = *reinterpret_cast<const uint32*>(&FileBytes[P]);
        OutNumProps = *reinterpret_cast<const uint32*>(&FileBytes[P + 4]);
        OutPropLen = *reinterpret_cast<const uint32*>(&FileBytes[P + 8]);
        uint8 NLen = FileBytes[P + 12];
        if (OutEnd == 0 || OutEnd > (uint32)FileSize || P + 13 + NLen > FileSize) return false;
        OutName = FString(NLen, (const ANSICHAR*)&FileBytes[P + 13]);
        OutPropsStart = P + 13 + NLen;
        OutChildStart = OutPropsStart + OutPropLen;
        return true;
    };

    // 2) Top-Level Iterate: Find Objects and Connections
    int32 ObjectsStart = 0, ObjectsEnd = 0;
    int32 ConnsStart = 0, ConnsEnd = 0;

    while (Pos + 13 < FileSize)
    {
        uint32 NEnd, NNumProps, NPropLen;
        FString NName;
        int32 NPropsStart, NChildStart;
        if (!ParseNodeHeader(Pos, NEnd, NNumProps, NPropLen, NName, NPropsStart, NChildStart)) break;

        if (NName.Equals(TEXT("Objects")))
        {
            ObjectsStart = NChildStart;
            ObjectsEnd = (int32)NEnd;
        }
        else if (NName.Equals(TEXT("Connections")))
        {
            ConnsStart = NChildStart;
            ConnsEnd = (int32)NEnd;
        }
        Pos = (int32)NEnd;
    }

    // 3) Parse Connections (OO, ChildID, ParentID)
    int32 CP = ConnsStart;
    while (CP + 13 < ConnsEnd)
    {
        uint32 CEnd, CNumProps, CPropLen;
        FString CName;
        int32 CPropsStart, CChildStart;
        if (!ParseNodeHeader(CP, CEnd, CNumProps, CPropLen, CName, CPropsStart, CChildStart)) break;

        if (CName.Equals(TEXT("C")) && CPropLen >= 19)
        {
            uint32 StrLen = *reinterpret_cast<const uint32*>(&FileBytes[CPropsStart + 1]);
            int32 Off = CPropsStart + 5 + StrLen;
            if (Off + 18 <= (int32)CEnd)
            {
                uint64 ChildID = *reinterpret_cast<const uint64*>(&FileBytes[Off + 1]);
                Off += 9;
                uint64 ParentID = *reinterpret_cast<const uint64*>(&FileBytes[Off + 1]);

                ChildToParents.FindOrAdd(ChildID).Add(ParentID);
                ParentToChildren.FindOrAdd(ParentID).Add(ChildID);
            }
        }
        CP = (int32)CEnd;
    }

    // 4) Parse Objects (Model, Geometry, Deformer)
    int32 OP = ObjectsStart;
    while (OP + 13 < ObjectsEnd)
    {
        uint32 OEnd, ONumProps, OPropLen;
        FString OName;
        int32 OPropsStart, OChildStart;
        if (!ParseNodeHeader(OP, OEnd, ONumProps, OPropLen, OName, OPropsStart, OChildStart)) break;

        uint64 ObjID = 0;
        if (OPropsStart + 9 <= OChildStart && FileBytes[OPropsStart] == 'L')
        {
            ObjID = *reinterpret_cast<const uint64*>(&FileBytes[OPropsStart + 1]);
        }

        // Extract Label String
        FString LabelStr = TEXT("");
        for (int32 off = OPropsStart; off + 5 < OChildStart && off < OPropsStart + 60; ++off)
        {
            if (FileBytes[off] == 'S')
            {
                uint32 sLen = *reinterpret_cast<const uint32*>(&FileBytes[off + 1]);
                if (off + 5 + (int32)sLen <= OChildStart)
                {
                    LabelStr = FString(sLen, (const ANSICHAR*)&FileBytes[off + 5]);
                    int32 NullIdx;
                    if (LabelStr.FindChar('\0', NullIdx)) LabelStr = LabelStr.Left(NullIdx);
                    break;
                }
            }
        }

        if (OName.Equals(TEXT("Model")))
        {
            ModelMap.Add(ObjID, LabelStr);
        }
        else if (OName.Equals(TEXT("Geometry")))
        {
            FRawFbxGeom Geom;
            Geom.Label = LabelStr;

            int32 GP = OChildStart;
            while (GP + 13 < (int32)OEnd)
            {
                uint32 GEnd, GNumProps, GPropLen;
                FString GName;
                int32 GPropsStart, GChildStart;
                if (!ParseNodeHeader(GP, GEnd, GNumProps, GPropLen, GName, GPropsStart, GChildStart)) break;

                if (GName.Equals(TEXT("Vertices")) && GPropLen > 12)
                {
                    uint8 TypeCode = FileBytes[GPropsStart];
                    uint32 ArrayLen = *reinterpret_cast<const uint32*>(&FileBytes[GPropsStart + 1]);
                    uint32 Encoding = *reinterpret_cast<const uint32*>(&FileBytes[GPropsStart + 5]);
                    uint32 CompLen = *reinterpret_cast<const uint32*>(&FileBytes[GPropsStart + 9]);
                    int32 DataOffset = GPropsStart + 13;

                    int32 ElemSize = (TypeCode == 'd' ? 8 : 4);
                    int32 UncompSize = ArrayLen * ElemSize;
                    TArray<uint8> UncompBuf;
                    const uint8* DataPtr = nullptr;

                    if (Encoding == 0 && DataOffset + UncompSize <= (int32)GEnd)
                    {
                        DataPtr = &FileBytes[DataOffset];
                    }
                    else if (Encoding == 1 && CompLen > 0 && DataOffset + (int32)CompLen <= (int32)GEnd)
                    {
                        UncompBuf.AddUninitialized(UncompSize);
                        if (FCompression::UncompressMemory(NAME_Zlib, (void*)UncompBuf.GetData(), (int64)UncompSize, (const void*)&FileBytes[DataOffset], (int64)CompLen))
                        {
                            DataPtr = UncompBuf.GetData();
                        }
                    }

                    if (DataPtr && ArrayLen > 0)
                    {
                        // Blender to Unreal coordinate conversion: (X, -Y, Z)
                        if (TypeCode == 'd')
                        {
                            const double* VData = reinterpret_cast<const double*>(DataPtr);
                            for (uint32 v = 0; v + 2 < ArrayLen; v += 3)
                            {
                                Geom.Vertices.Add(FVector(VData[v] * Scale, -VData[v + 1] * Scale, VData[v + 2] * Scale));
                            }
                        }
                        else if (TypeCode == 'f')
                        {
                            const float* VData = reinterpret_cast<const float*>(DataPtr);
                            for (uint32 v = 0; v + 2 < ArrayLen; v += 3)
                            {
                                Geom.Vertices.Add(FVector(VData[v] * Scale, -VData[v + 1] * Scale, VData[v + 2] * Scale));
                            }
                        }
                    }
                }
                else if (GName.Equals(TEXT("PolygonVertexIndex")) && GPropLen > 12)
                {
                    uint32 ArrayLen = *reinterpret_cast<const uint32*>(&FileBytes[GPropsStart + 1]);
                    uint32 Encoding = *reinterpret_cast<const uint32*>(&FileBytes[GPropsStart + 5]);
                    uint32 CompLen = *reinterpret_cast<const uint32*>(&FileBytes[GPropsStart + 9]);
                    int32 DataOffset = GPropsStart + 13;

                    int32 UncompSize = ArrayLen * 4;
                    TArray<uint8> UncompBuf;
                    const uint8* DataPtr = nullptr;

                    if (Encoding == 0 && DataOffset + UncompSize <= (int32)GEnd)
                    {
                        DataPtr = &FileBytes[DataOffset];
                    }
                    else if (Encoding == 1 && CompLen > 0 && DataOffset + (int32)CompLen <= (int32)GEnd)
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
                            int32 RealIdx = bIsLast ? (-Val - 1) : Val;
                            PolyLoop.Add(RealIdx);

                            if (bIsLast)
                            {
                                if (PolyLoop.Num() >= 3)
                                {
                                    // Reverse winding order (0, p+1, p) because Y is inverted (-Y) for Unreal Engine!
                                    for (int32 p = 1; p < PolyLoop.Num() - 1; ++p)
                                    {
                                        Geom.Polygons.Add(PolyLoop[0]);
                                        Geom.Polygons.Add(PolyLoop[p + 1]);
                                        Geom.Polygons.Add(PolyLoop[p]);
                                    }
                                }
                                PolyLoop.Empty();
                            }
                        }
                    }
                }
                GP = (int32)GEnd;
            }
            GeomMap.Add(ObjID, Geom);
        }
        else if (OName.Equals(TEXT("Deformer")))
        {
            int32 DP = OChildStart;
            TArray<int32> SubIndices;

            while (DP + 13 < (int32)OEnd)
            {
                uint32 DEnd, DNumProps, DPropLen;
                FString DName;
                int32 DPropsStart, DChildStart;
                if (!ParseNodeHeader(DP, DEnd, DNumProps, DPropLen, DName, DPropsStart, DChildStart)) break;

                if (DName.Equals(TEXT("Indexes")) && DPropLen > 12)
                {
                    uint32 ArrayLen = *reinterpret_cast<const uint32*>(&FileBytes[DPropsStart + 1]);
                    uint32 Encoding = *reinterpret_cast<const uint32*>(&FileBytes[DPropsStart + 5]);
                    uint32 CompLen = *reinterpret_cast<const uint32*>(&FileBytes[DPropsStart + 9]);
                    int32 DataOffset = DPropsStart + 13;

                    int32 UncompSize = ArrayLen * 4;
                    TArray<uint8> UncompBuf;
                    const uint8* DataPtr = nullptr;

                    if (Encoding == 0 && DataOffset + UncompSize <= (int32)DEnd)
                    {
                        DataPtr = &FileBytes[DataOffset];
                    }
                    else if (Encoding == 1 && CompLen > 0 && DataOffset + (int32)CompLen <= (int32)DEnd)
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
                        for (uint32 idx = 0; idx < ArrayLen; ++idx)
                        {
                            SubIndices.Add(PIndices[idx]);
                        }
                    }
                }
                DP = (int32)DEnd;
            }

            if (SubIndices.Num() > 0)
            {
                FRawFbxSubDef SubDef;
                SubDef.BoneLabel = LabelStr;
                SubDef.Indices = SubIndices;
                SubDeformerMap.Add(ObjID, SubDef);
            }
            else
            {
                DeformerMap.Add(ObjID, LabelStr);
            }
        }
        OP = (int32)OEnd;
    }

    // 5) Extract and Classify Each Geometry per Model with Local Centering and Smoothed Normals
    for (const auto& GeomPair : GeomMap)
    {
        uint64 GeomID = GeomPair.Key;
        const FRawFbxGeom& Geom = GeomPair.Value;

        // Find Parent Model
        FString ModelName = Geom.Label;
        if (const TArray<uint64>* Parents = ChildToParents.Find(GeomID))
        {
            for (uint64 PID : *Parents)
            {
                if (const FString* MName = ModelMap.Find(PID))
                {
                    ModelName = *MName;
                    break;
                }
            }
        }

        bool bIsUCXModel = ModelName.StartsWith(TEXT("UCX_"), ESearchCase::IgnoreCase) ||
                           ModelName.StartsWith(TEXT("UBX_"), ESearchCase::IgnoreCase) ||
                           ModelName.StartsWith(TEXT("USP_"), ESearchCase::IgnoreCase) ||
                           ModelName.StartsWith(TEXT("UCX"), ESearchCase::IgnoreCase) ||
                           ModelName.Contains(TEXT("UCX"), ESearchCase::IgnoreCase);

        // Find Skin Deformers connected to this Geometry
        TArray<uint64> GeomSkinDeformers;
        if (const TArray<uint64>* Children = ParentToChildren.Find(GeomID))
        {
            for (uint64 CID : *Children)
            {
                if (DeformerMap.Contains(CID)) GeomSkinDeformers.Add(CID);
            }
        }

        if (GeomSkinDeformers.Num() > 0)
        {
            for (uint64 SkinDefID : GeomSkinDeformers)
            {
                if (const TArray<uint64>* SubDefs = ParentToChildren.Find(SkinDefID))
                {
                    for (uint64 SubDefID : *SubDefs)
                    {
                        if (const FRawFbxSubDef* SubDef = SubDeformerMap.Find(SubDefID))
                        {
                            TSet<int32> BoneVertSet(SubDef->Indices);
                            TArray<FVector> SubVerts;
                            TArray<int32> SubTris;
                            TMap<int32, int32> VertMap;

                            for (int32 p = 0; p + 2 < Geom.Polygons.Num(); p += 3)
                            {
                                int32 V0 = Geom.Polygons[p];
                                int32 V1 = Geom.Polygons[p + 1];
                                int32 V2 = Geom.Polygons[p + 2];

                                if (BoneVertSet.Contains(V0) && BoneVertSet.Contains(V1) && BoneVertSet.Contains(V2))
                                {
                                    if (!VertMap.Contains(V0)) { VertMap.Add(V0, SubVerts.Num()); SubVerts.Add(Geom.Vertices[V0]); }
                                    if (!VertMap.Contains(V1)) { VertMap.Add(V1, SubVerts.Num()); SubVerts.Add(Geom.Vertices[V1]); }
                                    if (!VertMap.Contains(V2)) { VertMap.Add(V2, SubVerts.Num()); SubVerts.Add(Geom.Vertices[V2]); }

                                    SubTris.Add(VertMap[V0]);
                                    SubTris.Add(VertMap[V1]);
                                    SubTris.Add(VertMap[V2]);
                                }
                            }

                            if (SubVerts.Num() > 0 && SubTris.Num() > 0)
                            {
                                FImporterMeshSection Sec;
                                Sec.MeshName = bIsUCXModel ? FString::Printf(TEXT("%s_%s"), *ModelName, *SubDef->BoneLabel) : SubDef->BoneLabel;

                                // 1) Calculate Exact Pivot Point (Centroid in World Space)
                                FVector Center = FVector::ZeroVector;
                                for (const FVector& V : SubVerts) Center += V;
                                Sec.PivotPoint = Center / (float)SubVerts.Num();

                                // 2) Center Vertices around local origin (0,0,0) for component
                                for (FVector& V : SubVerts)
                                {
                                    V = V - Sec.PivotPoint;
                                }

                                Sec.Vertices = SubVerts;
                                Sec.Triangles = SubTris;

                                // 3) Compute Outward-Facing Smoothed Vertex Normals
                                Sec.Normals.Init(FVector::ZeroVector, SubVerts.Num());
                                for (int32 t = 0; t + 2 < SubTris.Num(); t += 3)
                                {
                                    int32 i0 = SubTris[t], i1 = SubTris[t + 1], i2 = SubTris[t + 2];
                                    FVector TriNormal = ((SubVerts[i1] - SubVerts[i0]) ^ (SubVerts[i2] - SubVerts[i0])).GetSafeNormal();
                                    Sec.Normals[i0] += TriNormal;
                                    Sec.Normals[i1] += TriNormal;
                                    Sec.Normals[i2] += TriNormal;
                                }
                                for (FVector& Norm : Sec.Normals)
                                {
                                    Norm = Norm.GetSafeNormal();
                                    if (Norm.IsNearlyZero()) Norm = FVector::UpVector;
                                }

                                if (bIsUCXModel)
                                {
                                    OutUCX.Add(Sec);
                                }
                                else
                                {
                                    OutVisual.Add(Sec);
                                }
                            }
                        }
                    }
                }
            }
        }
        else if (Geom.Vertices.Num() > 0 && Geom.Polygons.Num() > 0)
        {
            // Static unskinned geometry
            FImporterMeshSection Sec;
            Sec.MeshName = ModelName;

            FVector Center = FVector::ZeroVector;
            for (const FVector& V : Geom.Vertices) Center += V;
            Sec.PivotPoint = Center / (float)Geom.Vertices.Num();

            TArray<FVector> CenteredVerts = Geom.Vertices;
            for (FVector& V : CenteredVerts) V = V - Sec.PivotPoint;

            Sec.Vertices = CenteredVerts;
            Sec.Triangles = Geom.Polygons;

            Sec.Normals.Init(FVector::ZeroVector, CenteredVerts.Num());
            for (int32 t = 0; t + 2 < Geom.Polygons.Num(); t += 3)
            {
                int32 i0 = Geom.Polygons[t], i1 = Geom.Polygons[t + 1], i2 = Geom.Polygons[t + 2];
                FVector TriNormal = ((CenteredVerts[i1] - CenteredVerts[i0]) ^ (CenteredVerts[i2] - CenteredVerts[i0])).GetSafeNormal();
                Sec.Normals[i0] += TriNormal;
                Sec.Normals[i1] += TriNormal;
                Sec.Normals[i2] += TriNormal;
            }
            for (FVector& Norm : Sec.Normals)
            {
                Norm = Norm.GetSafeNormal();
                if (Norm.IsNearlyZero()) Norm = FVector::UpVector;
            }

            if (bIsUCXModel) OutUCX.Add(Sec);
            else OutVisual.Add(Sec);
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

        // Hiyerarşik Kemik Bağlantısı: Gövde dışındaki parçalar gövdeye bağlanır
        if (i == 0)
        {
            VisComp->SetupAttachment(SceneRootComponent);
            VisComp->SetRelativeLocation(VisualSections[i].PivotPoint);
        }
        else
        {
            if (VisualMeshComponents.IsValidIndex(0) && VisualMeshComponents[0])
            {
                VisComp->SetupAttachment(VisualMeshComponents[0]);
                // Gövdeye göre bağıl konum: (TekerlekPivot - GövdePivot)
                VisComp->SetRelativeLocation(VisualSections[i].PivotPoint - VisualSections[0].PivotPoint);
            }
            else
            {
                VisComp->SetupAttachment(SceneRootComponent);
                VisComp->SetRelativeLocation(VisualSections[i].PivotPoint);
            }
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
    // 2) UCX LİSTESİNDEKİ PARÇALARI İLGİLİ GÖRSEL KEMİĞE BAĞLA VE STATİK/DİNAMİK COLLISION VER
    // -----------------------------------------------------------------------------------------
    for (int32 j = 0; j < UCXSections.Num(); ++j)
    {
        FName CollCompName = *FString::Printf(TEXT("UCXCollision_%d_%s"), j, *UCXSections[j].MeshName);
        UProceduralMeshComponent* CollComp = NewObject<UProceduralMeshComponent>(this, CollCompName);
        CollComp->SetMobility(EComponentMobility::Movable);

        // Hedef görsel parçayı bul (Örn: UCX_Chassis_wheel_FR -> wheel_FR veya UCX_Chassis_chassis -> chassis)
        FString UcxName = UCXSections[j].MeshName;
        UProceduralMeshComponent* TargetVisComp = (VisualMeshComponents.Num() > 0) ? VisualMeshComponents[0] : nullptr;
        int32 TargetVisIdx = 0;

        for (int32 v = 0; v < VisualSections.Num(); ++v)
        {
            if (UcxName.Contains(VisualSections[v].MeshName, ESearchCase::IgnoreCase))
            {
                TargetVisComp = VisualMeshComponents[v];
                TargetVisIdx = v;
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

        // Görsel Render Gizli
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

    // 1) Ana Gövdeyi Kök Bileşen Yap
    if (VisualMeshComponents.IsValidIndex(0) && VisualMeshComponents[0])
    {
        VisualMeshComponents[0]->SetMobility(EComponentMobility::Movable);
        VisualMeshComponents[0]->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
        SetRootComponent(VisualMeshComponents[0]);
    }

    // 2) Her Parçaya Kütle, Zırh ve Dinamik Fizik Ver
    for (int32 i = 0; i < VisualMeshComponents.Num(); ++i)
    {
        UProceduralMeshComponent* VisComp = VisualMeshComponents[i];
        if (!VisComp || !VisualSections.IsValidIndex(i)) continue;

        float Mass = (i == 0) ? 30.0f : 2.5f;

        // İlgili UCX Convex Hull'unu aktar (veya kendi geometrisi)
        VisComp->ClearCollisionConvexMeshes();
        bool bFoundUCX = false;
        for (const FImporterMeshSection& UcxSec : UCXSections)
        {
            if (UcxSec.MeshName.Contains(VisualSections[i].MeshName, ESearchCase::IgnoreCase))
            {
                VisComp->AddCollisionConvexMesh(UcxSec.Vertices);
                bFoundUCX = true;
                break;
            }
        }
        if (!bFoundUCX)
        {
            VisComp->AddCollisionConvexMesh(VisualSections[i].Vertices);
        }

        VisComp->bUseComplexAsSimpleCollision = false;
        VisComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        VisComp->SetCollisionObjectType(ECC_WorldDynamic);
        VisComp->SetCollisionResponseToAllChannels(ECR_Block);
        VisComp->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block); // Zeminle çarpış
        VisComp->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
        VisComp->RecreatePhysicsState();
        VisComp->UpdateBounds();

        // Dinamik Fiziği Aç
        VisComp->SetMobility(EComponentMobility::Movable);
        VisComp->SetSimulatePhysics(true);
        VisComp->SetEnableGravity(true);
        VisComp->SetMassOverrideInKg(NAME_None, Mass, true);
        VisComp->SetLinearDamping(0.8f);
        VisComp->SetAngularDamping(1.5f);
        VisComp->WakeRigidBody();

        UE_LOG(LogTemp, Warning, TEXT("[FİZİK LOG] '%s' bileşenine CANLI DİNAMİK FİZİK verildi | Kütle: %.1f kg | Yerçekimi: AÇIK"),
            *VisualSections[i].MeshName, Mass);

        // 3) Gövde Dışındaki Tekerlekler İçin Fiziksel Eklem (Constraint) Bağla
        if (i > 0 && VisualMeshComponents[0])
        {
            FName ConstraintName = *FString::Printf(TEXT("PhysicsJoint_%d_%s"), i, *VisualSections[i].MeshName);
            UPhysicsConstraintComponent* Constraint = NewObject<UPhysicsConstraintComponent>(this, ConstraintName);
            Constraint->SetupAttachment(VisualMeshComponents[0]);
            Constraint->SetRelativeLocation(VisualSections[i].PivotPoint - VisualSections[0].PivotPoint);
            Constraint->RegisterComponent();

            Constraint->SetConstrainedComponents(VisualMeshComponents[0], NAME_None, VisComp, NAME_None);
            Constraint->SetAngularTwistLimit(EAngularConstraintMotion::ACM_Free, 0.0f); // Serbest tekerlek dönüşü
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
            FString::Printf(TEXT("🚀 >>> [CHAOS FİZİK AKTİF!] %d Parça Canlı Simüle Ediliyor <<<"), VisualMeshComponents.Num()));
        
        GEngine->AddOnScreenDebugMessage(802, 10.0f, FColor::Emerald,
            FString::Printf(TEXT("🛡️ >>> [GÖVDE: 30.0 KG | YERÇEKİMİ: AÇIK] Zemin Çarpışması Aktif <<<")));

        GEngine->AddOnScreenDebugMessage(803, 10.0f, FColor::Yellow,
            FString::Printf(TEXT("⚙️ >>> [%d ADET EKLEM KISITLAMASI BAĞLANDI] Tekerlekler Serbest Dönüyor! <<<"), JointConstraints.Num()));
    }
}
