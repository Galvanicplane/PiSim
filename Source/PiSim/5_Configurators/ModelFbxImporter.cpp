// ModelFbxImporter.cpp
// FBX İthalatçısı: İkili FBX ayrıştırma motoru, UCX/Visual/Sensör sınıflandırması ve APiSimVehicle aktörüne montajı.

#include "ModelFbxImporter.h"
#include "Vehicle.h"
#include "MotorConfigurator.h"
#include "Components/SkeletalMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/SpringArmComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Compression.h"
#include "HAL/PlatformFileManager.h"
#include "Engine/SkeletalMesh.h"

static TMap<FString, FVector> G_FbxBoneWorldLocations;

UPiSimModelFbxImporter::UPiSimModelFbxImporter()
{
}

// @state: WIP - Diskteki Saved/Robots/Cache klasöründeki tüm geçerli FBX dosyalarını tarar
TArray<FString> UPiSimModelFbxImporter::GetAvailableCacheModels()
{
    TArray<FString> FoundModels;
    FString CacheDir = FPaths::ProjectSavedDir() / TEXT("Robots/Cache");

    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.DirectoryExists(*CacheDir))
    {
        return FoundModels;
    }

    class FModelVisitor : public IPlatformFile::FDirectoryVisitor
    {
    public:
        TArray<FString>& OutList;
        FModelVisitor(TArray<FString>& InList) : OutList(InList) {}
        virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
        {
            if (!bIsDirectory)
            {
                FString File = FilenameOrDirectory;
                if (File.EndsWith(TEXT(".fbx"), ESearchCase::IgnoreCase))
                {
                    OutList.Add(FPaths::GetCleanFilename(File));
                }
            }
            return true;
        }
    };

    FModelVisitor Visitor(FoundModels);
    PlatformFile.IterateDirectory(*CacheDir, Visitor);

    FoundModels.Sort();
    return FoundModels;
}

// @state: WIP - İkili FBX dosyasını bayt bayt okur; kemik pivotları ve smooth/flat normalleri ile ayrıştırır
bool UPiSimModelFbxImporter::ParseBinaryFbxFile(
    const FString& FilePath,
    TArray<FPiSimMeshSection>& OutVisual,
    TArray<FPiSimMeshSection>& OutUCX,
    TArray<FPiSimSensorSection>& OutSensors,
    TArray<FString>& OutPureBones,
    TArray<FPiSimBoneHierarchyNode>& OutBoneHierarchy,
    float Scale,
    bool bEnableSmoothNormals)
{
    OutVisual.Empty();
    OutUCX.Empty();
    OutSensors.Empty();
    OutPureBones.Empty();
    OutBoneHierarchy.Empty();
    G_FbxBoneWorldLocations.Empty();

    TArray<uint8> FileBytes;
    if (!FFileHelper::LoadFileToArray(FileBytes, *FilePath) || FileBytes.Num() < 64)
    {
        UE_LOG(LogTemp, Error, TEXT("[ModelFbxImporter] FBX dosyasi okunamadi veya boyutu gecersiz: %s"), *FilePath);
        return false;
    }

    int32 FileSize = FileBytes.Num();
    int32 Pos = 27; // FBX Binary sihirli başlığını (Magic) atla

    struct FRawFbxGeom
    {
        FString Label;
        TArray<FVector> Vertices;
        TArray<int32> Polygons;
        TArray<int32> TriangleMatSlots;
    };

    struct FRawFbxSubDef
    {
        FString BoneLabel;
        TArray<int32> Indices;
        FVector BonePosition = FVector::ZeroVector;
        bool bHasBonePosition = false;
        FVector BoneDirection = FVector(0.0f, 1.0f, 0.0f);
    };

    struct FRawFbxMaterial
    {
        FLinearColor Color = FLinearColor::White;
        float Metallic = 0.0f;
        float Roughness = 0.5f;
        float Specular = 0.5f;
    };

    TMap<uint64, FString> ModelMap;
    TMap<uint64, FRawFbxGeom> GeomMap;
    TMap<uint64, FString> DeformerMap;
    TMap<uint64, FRawFbxSubDef> SubDeformerMap;
    TMap<uint64, FRawFbxMaterial> MaterialMap;

    TMap<uint64, TArray<uint64>> ChildToParents;
    TMap<uint64, TArray<uint64>> ParentToChildren;

    // 1) FBX Node Header Okuyucu
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

    // 2) Objects ve Connections Kök Bloklarını Bul
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

    // 3) Bağlantıları (Connections) Ayrıştır
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

    // 4) Nesneleri (Objects: Model, Geometry, Deformer, Material) Ayrıştır
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
                    int32 ColonIdx;
                    if (LabelStr.FindLastChar(':', ColonIdx)) LabelStr = LabelStr.Mid(ColonIdx + 1);
                    int32 PipeIdx;
                    if (LabelStr.FindLastChar('|', PipeIdx)) LabelStr = LabelStr.Mid(PipeIdx + 1);
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
            TArray<int32> RawPolyIndices;
            TArray<int32> RawPolyMatIndices;

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
                        // Blender'dan Unreal koordinat dönüşümü: X=İleri(Blender Y), Y=Sağ(Blender X), Z=Yukarı(Blender Z)
                        // Eğer vertexler metre cinsindeyse (örneğin genlik < 10) ve Scale == 1.0f ise, Unreal cm dönüşümü (100x) uygula!
                        float EffectiveScale = Scale;
                        if (TypeCode == 'd')
                        {
                            const double* VData = reinterpret_cast<const double*>(DataPtr);
                            double MaxVal = 0.0;
                            for (uint32 v = 0; v < FMath::Min(ArrayLen, 30u); ++v)
                            {
                                MaxVal = FMath::Max(MaxVal, FMath::Abs(VData[v]));
                            }
                            if (Scale <= 1.0f && MaxVal > 0.01 && MaxVal < 10.0)
                            {
                                EffectiveScale = Scale * 100.0f;
                            }
                            for (uint32 v = 0; v + 2 < ArrayLen; v += 3)
                            {
                                Geom.Vertices.Add(FVector(VData[v + 1] * EffectiveScale, VData[v] * EffectiveScale, VData[v + 2] * EffectiveScale));
                            }
                        }
                        else if (TypeCode == 'f')
                        {
                            const float* VData = reinterpret_cast<const float*>(DataPtr);
                            float MaxVal = 0.0f;
                            for (uint32 v = 0; v < FMath::Min(ArrayLen, 30u); ++v)
                            {
                                MaxVal = FMath::Max(MaxVal, FMath::Abs(VData[v]));
                            }
                            if (Scale <= 1.0f && MaxVal > 0.01f && MaxVal < 10.0f)
                            {
                                EffectiveScale = Scale * 100.0f;
                            }
                            for (uint32 v = 0; v + 2 < ArrayLen; v += 3)
                            {
                                Geom.Vertices.Add(FVector(VData[v + 1] * EffectiveScale, VData[v] * EffectiveScale, VData[v + 2] * EffectiveScale));
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
                        RawPolyIndices.Append(PIndices, ArrayLen);
                    }
                }
                else if (GName.Equals(TEXT("LayerElementMaterial")))
                {
                    int32 LMP = GChildStart;
                    while (LMP + 13 < (int32)GEnd)
                    {
                        uint32 LMEnd, LMNumProps, LMPropLen;
                        FString LMName;
                        int32 LMPropsStart, LMChildStart;
                        if (!ParseNodeHeader(LMP, LMEnd, LMNumProps, LMPropLen, LMName, LMPropsStart, LMChildStart)) break;

                        if (LMName.Equals(TEXT("Materials")) && LMPropLen > 12)
                        {
                            uint32 ArrayLen = *reinterpret_cast<const uint32*>(&FileBytes[LMPropsStart + 1]);
                            uint32 Encoding = *reinterpret_cast<const uint32*>(&FileBytes[LMPropsStart + 5]);
                            uint32 CompLen = *reinterpret_cast<const uint32*>(&FileBytes[LMPropsStart + 9]);
                            int32 DataOffset = LMPropsStart + 13;

                            int32 UncompSize = ArrayLen * 4;
                            TArray<uint8> UncompBuf;
                            const uint8* DataPtr = nullptr;

                            if (Encoding == 0 && DataOffset + UncompSize <= (int32)LMEnd)
                            {
                                DataPtr = &FileBytes[DataOffset];
                            }
                            else if (Encoding == 1 && CompLen > 0 && DataOffset + (int32)CompLen <= (int32)LMEnd)
                            {
                                UncompBuf.AddUninitialized(UncompSize);
                                if (FCompression::UncompressMemory(NAME_Zlib, (void*)UncompBuf.GetData(), (int64)UncompSize, (const void*)&FileBytes[DataOffset], (int64)CompLen))
                                {
                                    DataPtr = UncompBuf.GetData();
                                }
                            }

                            if (DataPtr && ArrayLen > 0)
                            {
                                const int32* MIndices = reinterpret_cast<const int32*>(DataPtr);
                                RawPolyMatIndices.Append(MIndices, ArrayLen);
                            }
                        }
                        LMP = (int32)LMEnd;
                    }
                }
                GP = (int32)GEnd;
            }

            // N-gen Poligonları Üçgenlere Böl ve Saat Yönünü Tersle
            if (RawPolyIndices.Num() > 0)
            {
                TArray<int32> PolyLoop;
                int32 CurrentPolyIndex = 0;
                for (int32 Val : RawPolyIndices)
                {
                    bool bIsLast = (Val < 0);
                    int32 RealIdx = bIsLast ? (-Val - 1) : Val;
                    PolyLoop.Add(RealIdx);

                    if (bIsLast)
                    {
                        int32 MatSlot = RawPolyMatIndices.IsValidIndex(CurrentPolyIndex) ? RawPolyMatIndices[CurrentPolyIndex] : 0;
                        if (PolyLoop.Num() >= 3)
                        {
                            for (int32 p = 1; p < PolyLoop.Num() - 1; ++p)
                            {
                                Geom.Polygons.Add(PolyLoop[0]);
                                Geom.Polygons.Add(PolyLoop[p + 1]);
                                Geom.Polygons.Add(PolyLoop[p]);
                                Geom.TriangleMatSlots.Add(MatSlot);
                            }
                        }
                        PolyLoop.Empty();
                        CurrentPolyIndex++;
                    }
                }
            }
            GeomMap.Add(ObjID, Geom);
        }
        else if (OName.Equals(TEXT("Deformer")))
        {
            int32 DP = OChildStart;
            TArray<int32> SubIndices;
            FVector SubBoneDir = FVector(0.0f, 1.0f, 0.0f);
            FVector SubBonePos = FVector::ZeroVector;
            bool bSubHasBonePos = false;

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
                else if (DName.Equals(TEXT("TransformLink")) && DPropLen > 12)
                {
                    uint32 ArrayLen = *reinterpret_cast<const uint32*>(&FileBytes[DPropsStart + 1]);
                    uint32 Encoding = *reinterpret_cast<const uint32*>(&FileBytes[DPropsStart + 5]);
                    uint32 CompLen = *reinterpret_cast<const uint32*>(&FileBytes[DPropsStart + 9]);
                    int32 DataOffset = DPropsStart + 13;

                    int32 UncompSize = ArrayLen * 8;
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

                    if (DataPtr && ArrayLen >= 16)
                    {
                        const double* Mat = reinterpret_cast<const double*>(DataPtr);
                        // FBX TransformLink matrix (Armature -90 pitch koordinat dönüşümü):
                        // Mat[12] = Blender X (Sağ) -> Unreal Y
                        // Mat[13] = Blender -Z (Aşağı) -> Unreal -Z
                        // Mat[14] = -Blender Y (-İleri) -> Unreal -X (Front is -168cm, so -Mat[14] = +168cm Forward)
                        float MatScale = FVector(Mat[0], Mat[1], Mat[2]).Size();
                        double MaxBoneCoord = FMath::Max3(FMath::Abs(Mat[12]), FMath::Abs(Mat[13]), FMath::Abs(Mat[14]));
                        float PosScale = Scale;
                        if (Scale <= 1.0f && MatScale < 10.0f && MaxBoneCoord > 0.01 && MaxBoneCoord < 10.0)
                        {
                            PosScale = Scale * 100.0f;
                        }
                        SubBonePos = FVector(-Mat[14] * PosScale, Mat[12] * PosScale, -Mat[13] * PosScale);
                        bSubHasBonePos = true;

                        FVector Dir(-Mat[6], Mat[4], -Mat[5]);
                        if (!Dir.IsNearlyZero())
                        {
                            SubBoneDir = Dir.GetSafeNormal();
                        }

                        G_FbxBoneWorldLocations.Add(LabelStr, SubBonePos);
                        FString CleanLabel = LabelStr;
                        int32 SepIdx;
                        if (CleanLabel.FindLastChar(':', SepIdx)) CleanLabel = CleanLabel.Mid(SepIdx + 1);
                        if (CleanLabel.FindLastChar('|', SepIdx)) CleanLabel = CleanLabel.Mid(SepIdx + 1);
                        G_FbxBoneWorldLocations.Add(CleanLabel, SubBonePos);
                        FString NoPre = CleanLabel;
                        NoPre.RemoveFromStart(TEXT("M_"), ESearchCase::IgnoreCase);
                        NoPre.RemoveFromStart(TEXT("B_"), ESearchCase::IgnoreCase);
                        G_FbxBoneWorldLocations.Add(NoPre, SubBonePos);
                        UE_LOG(LogTemp, Warning, TEXT("🦴 [FBX KEMİK KONUMU] '%s' -> %s"), *LabelStr, *SubBonePos.ToString());
                    }
                }
                DP = (int32)DEnd;
            }

            if (SubIndices.Num() > 0)
            {
                FRawFbxSubDef SubDef;
                SubDef.BoneLabel = LabelStr;
                SubDef.Indices = SubIndices;
                SubDef.BonePosition = SubBonePos;
                SubDef.bHasBonePosition = bSubHasBonePos;
                SubDef.BoneDirection = SubBoneDir;
                SubDeformerMap.Add(ObjID, SubDef);
            }
            else
            {
                DeformerMap.Add(ObjID, LabelStr);
            }
        }
        else if (OName.Equals(TEXT("Material")))
        {
            FRawFbxMaterial Mat;
            MaterialMap.Add(ObjID, Mat);
        }
        OP = (int32)OEnd;
    }

    // 5) Geometrileri Kemiklerine Göre Parçala ve Sınıflandır
    for (const auto& GeomPair : GeomMap)
    {
        uint64 GeomID = GeomPair.Key;
        const FRawFbxGeom& Geom = GeomPair.Value;

        FString ModelName = Geom.Label;
        uint64 ParentModelID = 0;
        if (const TArray<uint64>* Parents = ChildToParents.Find(GeomID))
        {
            for (uint64 PID : *Parents)
            {
                if (const FString* MName = ModelMap.Find(PID))
                {
                    ModelName = *MName;
                    ParentModelID = PID;
                    break;
                }
            }
        }

        bool bIsSensorMesh = ModelName.StartsWith(TEXT("S_"), ESearchCase::IgnoreCase) ||
                             ModelName.StartsWith(TEXT("Sensor_"), ESearchCase::IgnoreCase);

        bool bIsUCXModel = !bIsSensorMesh && (
            ModelName.StartsWith(TEXT("UCX_"), ESearchCase::IgnoreCase) ||
            ModelName.StartsWith(TEXT("UBX_"), ESearchCase::IgnoreCase) ||
            ModelName.StartsWith(TEXT("USP_"), ESearchCase::IgnoreCase) ||
            ModelName.StartsWith(TEXT("UCX"), ESearchCase::IgnoreCase) ||
            ModelName.Contains(TEXT("UCX"), ESearchCase::IgnoreCase) ||
            ModelName.StartsWith(TEXT("COL_"), ESearchCase::IgnoreCase));

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
                            TArray<int32> SubTriMatSlots;
                            TMap<int32, int32> VertMap;

                            for (int32 p = 0; p + 2 < Geom.Polygons.Num(); p += 3)
                            {
                                int32 triIdx = p / 3;
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

                                    int32 Slot = Geom.TriangleMatSlots.IsValidIndex(triIdx) ? Geom.TriangleMatSlots[triIdx] : 0;
                                    SubTriMatSlots.Add(Slot);
                                }
                            }

                            if (SubVerts.Num() > 0)
                            {
                                FString MeshLabel = SubDef->BoneLabel.IsEmpty() ? ModelName : SubDef->BoneLabel;
                                bool bBoneIsSensor = MeshLabel.StartsWith(TEXT("S_"), ESearchCase::IgnoreCase) ||
                                                     MeshLabel.StartsWith(TEXT("Sensor_"), ESearchCase::IgnoreCase) ||
                                                     bIsSensorMesh;

                                // 1) Calculate Exact Pivot Point: KEMİK ÜSTÜNDÜR! Kemik varsa mesh pivotunu kemikten alır!
                                FVector Center = FVector::ZeroVector;
                                for (const FVector& V : SubVerts) Center += V;
                                FVector Pivot = SubDef->bHasBonePosition ? SubDef->BonePosition : (Center / (float)SubVerts.Num());

                                // Köşeleri kemik pivotuna göre yerelleştir (Kemik üstündür, kemik pivotunu meshlere verir)
                                for (FVector& V : SubVerts)
                                {
                                    V -= Pivot;
                                }

                                // [KURAL 2]: Normal Hesabı (Smooth vs Flat)
                                TArray<FVector> SubNormals;
                                SubNormals.SetNumZeroed(SubVerts.Num());

                                for (int32 t = 0; t + 2 < SubTris.Num(); t += 3)
                                {
                                    int32 I0 = SubTris[t];
                                    int32 I1 = SubTris[t + 1];
                                    int32 I2 = SubTris[t + 2];

                                    FVector TriNormal = FVector::CrossProduct(SubVerts[I1] - SubVerts[I0], SubVerts[I2] - SubVerts[I0]).GetSafeNormal();

                                    if (bEnableSmoothNormals && !bIsUCXModel)
                                    {
                                        SubNormals[I0] += TriNormal;
                                        SubNormals[I1] += TriNormal;
                                        SubNormals[I2] += TriNormal;
                                    }
                                    else
                                    {
                                        SubNormals[I0] = TriNormal;
                                        SubNormals[I1] = TriNormal;
                                        SubNormals[I2] = TriNormal;
                                    }
                                }

                                bool bIsUCX = bIsUCXModel ||
                                              MeshLabel.StartsWith(TEXT("UCX_"), ESearchCase::IgnoreCase) ||
                                              MeshLabel.StartsWith(TEXT("UBX_"), ESearchCase::IgnoreCase) ||
                                              MeshLabel.StartsWith(TEXT("USP_"), ESearchCase::IgnoreCase) ||
                                              MeshLabel.StartsWith(TEXT("UCX"), ESearchCase::IgnoreCase) ||
                                              MeshLabel.Contains(TEXT("UCX"), ESearchCase::IgnoreCase) ||
                                              MeshLabel.StartsWith(TEXT("COL_"), ESearchCase::IgnoreCase);

                                if (bEnableSmoothNormals && !bIsUCX)
                                {
                                    for (FVector& N : SubNormals)
                                    {
                                        N = N.GetSafeNormal();
                                    }
                                }

                                FPiSimMeshSection Section;
                                Section.MeshName = bIsUCX ? (bIsUCXModel ? FString::Printf(TEXT("%s_%s"), *ModelName, *MeshLabel) : MeshLabel) : MeshLabel;
                                Section.PivotPoint = Pivot;
                                Section.BoneDirection = SubDef->BoneDirection;
                                Section.Vertices = SubVerts;
                                Section.Triangles = SubTris;
                                Section.Normals = SubNormals;

                                FPiSimImporterMaterialSlot DefaultSlot;
                                DefaultSlot.Triangles = SubTris;
                                Section.MaterialSlots.Add(DefaultSlot);

                                if (bBoneIsSensor)
                                {
                                    FPiSimSensorSection SensorSec;
                                    SensorSec.SensorName = MeshLabel;
                                    SensorSec.PivotPoint = Pivot;
                                    SensorSec.Rotation = FRotator::ZeroRotator;
                                    OutSensors.Add(SensorSec);
                                }
                                else if (bIsUCX)
                                {
                                    OutUCX.Add(Section);
                                }
                                else
                                {
                                    OutVisual.Add(Section);
                                }
                            }
                        }
                    }
                }
            }
        }
        else if (Geom.Vertices.Num() > 0)
        {
            if (bIsSensorMesh)
            {
                FVector Center = FVector::ZeroVector;
                for (const FVector& V : Geom.Vertices) Center += V;
                Center /= (float)Geom.Vertices.Num();

                FPiSimSensorSection SensorSec;
                SensorSec.SensorName = ModelName;
                SensorSec.PivotPoint = Center;
                OutSensors.Add(SensorSec);
            }
            else if (bIsUCXModel)
            {
                FVector Center = FVector::ZeroVector;
                for (const FVector& V : Geom.Vertices) Center += V;
                Center /= (float)Geom.Vertices.Num();

                TArray<FVector> LocalVerts = Geom.Vertices;
                for (FVector& V : LocalVerts) V -= Center;

                TArray<FVector> Normals;
                Normals.SetNumZeroed(LocalVerts.Num());
                for (int32 t = 0; t + 2 < Geom.Polygons.Num(); t += 3)
                {
                    int32 I0 = Geom.Polygons[t];
                    int32 I1 = Geom.Polygons[t + 1];
                    int32 I2 = Geom.Polygons[t + 2];
                    FVector TriN = FVector::CrossProduct(LocalVerts[I1] - LocalVerts[I0], LocalVerts[I2] - LocalVerts[I0]).GetSafeNormal();
                    Normals[I0] = TriN; Normals[I1] = TriN; Normals[I2] = TriN;
                }

                FPiSimMeshSection Section;
                Section.MeshName = ModelName;
                Section.PivotPoint = Center;
                Section.Vertices = LocalVerts;
                Section.Triangles = Geom.Polygons;
                Section.Normals = Normals;

                FPiSimImporterMaterialSlot DefaultSlot;
                DefaultSlot.Triangles = Geom.Polygons;
                Section.MaterialSlots.Add(DefaultSlot);
                OutUCX.Add(Section);
            }
            else
            {
                // Görsel Geometri: Ayrı mesh adaları (Disconnected Components - örn. Gövde + Tekerlekler) var mı kontrol et
                int32 NumVerts = Geom.Vertices.Num();
                TArray<int32> DisjointParent;
                DisjointParent.SetNumUninitialized(NumVerts);
                for (int32 vi = 0; vi < NumVerts; ++vi) DisjointParent[vi] = vi;

                auto FindRoot = [&](int32 x) -> int32
                {
                    if (x < 0 || x >= NumVerts) return -1;
                    while (x != DisjointParent[x])
                    {
                        DisjointParent[x] = DisjointParent[DisjointParent[x]];
                        x = DisjointParent[x];
                    }
                    return x;
                };

                auto UnionVerts = [&](int32 a, int32 b)
                {
                    int32 ra = FindRoot(a);
                    int32 rb = FindRoot(b);
                    if (ra != -1 && rb != -1 && ra != rb)
                    {
                        DisjointParent[ra] = rb;
                    }
                };

                for (int32 t = 0; t + 2 < Geom.Polygons.Num(); t += 3)
                {
                    int32 I0 = Geom.Polygons[t];
                    int32 I1 = Geom.Polygons[t + 1];
                    int32 I2 = Geom.Polygons[t + 2];
                    if (I0 >= 0 && I0 < NumVerts && I1 >= 0 && I1 < NumVerts && I2 >= 0 && I2 < NumVerts)
                    {
                        UnionVerts(I0, I1);
                        UnionVerts(I1, I2);
                    }
                }

                TMap<int32, TArray<int32>> RootToTris;
                for (int32 t = 0; t + 2 < Geom.Polygons.Num(); t += 3)
                {
                    int32 I0 = Geom.Polygons[t];
                    int32 Root = FindRoot(I0);
                    if (Root != -1)
                    {
                        RootToTris.FindOrAdd(Root).Add(t / 3);
                    }
                }

                // Tekerlek kemikleri var mı? Sadece ana tekerlek kemiklerini (M_Wheel_FR vb.) al
                TArray<FString> AvailableWheelBones;
                for (const auto& LocPair : G_FbxBoneWorldLocations)
                {
                    FString BName = LocPair.Key;
                    if ((BName.StartsWith(TEXT("M_Wheel_"), ESearchCase::IgnoreCase) ||
                         (BName.Contains(TEXT("wheel"), ESearchCase::IgnoreCase) && !BName.StartsWith(TEXT("Wheel_")))) &&
                        !BName.Contains(TEXT("steer"), ESearchCase::IgnoreCase))
                    {
                        AvailableWheelBones.AddUnique(BName);
                    }
                }

                if (RootToTris.Num() > 1 && AvailableWheelBones.Num() > 0)
                {
                    // Çoklu adalar var: Her adanın merkezini en yakın tekerlek kemiğiyle eşleştir
                    TSet<FString> UsedWheelBones;
                    TArray<int32> UnmatchedRoots;

                    for (const auto& Pair : RootToTris)
                    {
                        int32 Root = Pair.Key;
                        const TArray<int32>& CompTris = Pair.Value;

                        // Adanın köşe noktalarını ve merkezini bul
                        TSet<int32> CompVertIndices;
                        FVector IslandCenter = FVector::ZeroVector;
                        for (int32 triIdx : CompTris)
                        {
                            CompVertIndices.Add(Geom.Polygons[triIdx * 3]);
                            CompVertIndices.Add(Geom.Polygons[triIdx * 3 + 1]);
                            CompVertIndices.Add(Geom.Polygons[triIdx * 3 + 2]);
                        }
                        for (int32 vi : CompVertIndices)
                        {
                            if (vi >= 0 && vi < NumVerts)
                            {
                                IslandCenter += Geom.Vertices[vi];
                            }
                        }
                        if (CompVertIndices.Num() > 0)
                        {
                            IslandCenter /= (float)CompVertIndices.Num();
                        }

                        // En yakın boş tekerlek kemiğini ara (Sadece tekerlek adaları için, en az 80 köşe)
                        FString MatchedBone = TEXT("");
                        if (CompVertIndices.Num() >= 80)
                        {
                            float BestDistSq = FMath::Square(50.0f); // 50 cm tolerans
                            for (const FString& WB : AvailableWheelBones)
                            {
                                if (UsedWheelBones.Contains(WB)) continue;
                                if (const FVector* BonePosPtr = G_FbxBoneWorldLocations.Find(WB))
                                {
                                    float DistSq = FVector::DistSquared(IslandCenter, *BonePosPtr);
                                    if (DistSq < BestDistSq)
                                    {
                                        BestDistSq = DistSq;
                                        MatchedBone = WB;
                                    }
                                }
                            }
                        }

                        if (!MatchedBone.IsEmpty())
                        {
                            UsedWheelBones.Add(MatchedBone);
                            FVector BonePivot = G_FbxBoneWorldLocations[MatchedBone];

                            // Bu ada için görsel section oluştur
                            TArray<FVector> SubVerts;
                            TArray<int32> SubTris;
                            TMap<int32, int32> LocalVertMap;

                            for (int32 triIdx : CompTris)
                            {
                                int32 V0 = Geom.Polygons[triIdx * 3];
                                int32 V1 = Geom.Polygons[triIdx * 3 + 1];
                                int32 V2 = Geom.Polygons[triIdx * 3 + 2];

                                if (!LocalVertMap.Contains(V0)) { LocalVertMap.Add(V0, SubVerts.Num()); SubVerts.Add(Geom.Vertices[V0] - BonePivot); }
                                if (!LocalVertMap.Contains(V1)) { LocalVertMap.Add(V1, SubVerts.Num()); SubVerts.Add(Geom.Vertices[V1] - BonePivot); }
                                if (!LocalVertMap.Contains(V2)) { LocalVertMap.Add(V2, SubVerts.Num()); SubVerts.Add(Geom.Vertices[V2] - BonePivot); }

                                SubTris.Add(LocalVertMap[V0]);
                                SubTris.Add(LocalVertMap[V1]);
                                SubTris.Add(LocalVertMap[V2]);
                            }

                            TArray<FVector> SubNormals;
                            SubNormals.SetNumZeroed(SubVerts.Num());
                            for (int32 t = 0; t + 2 < SubTris.Num(); t += 3)
                            {
                                int32 I0 = SubTris[t], I1 = SubTris[t + 1], I2 = SubTris[t + 2];
                                FVector TriN = FVector::CrossProduct(SubVerts[I1] - SubVerts[I0], SubVerts[I2] - SubVerts[I0]).GetSafeNormal();
                                SubNormals[I0] += TriN; SubNormals[I1] += TriN; SubNormals[I2] += TriN;
                            }
                            for (FVector& N : SubNormals) N = N.GetSafeNormal();

                            FPiSimMeshSection Sec;
                            Sec.MeshName = MatchedBone;
                            Sec.PivotPoint = BonePivot;
                            Sec.Vertices = SubVerts;
                            Sec.Triangles = SubTris;
                            Sec.Normals = SubNormals;

                            FPiSimImporterMaterialSlot Slot;
                            Slot.Triangles = SubTris;
                            Sec.MaterialSlots.Add(Slot);

                            OutVisual.Add(Sec);
                            UE_LOG(LogTemp, Warning, TEXT("🚗 [VİSUAL TEKERLEK KÜMESİ BULUNDU] '%s' mesh adasından '%s' kemiğine atandı (Verts: %d)"),
                                *ModelName, *MatchedBone, SubVerts.Num());
                        }
                        else
                        {
                            UnmatchedRoots.Add(Root);
                        }
                    }

                    // Eşleşmeyen kalan tüm adalar (ana gövde parçaları) birleştirilerek şasiye atanır
                    if (UnmatchedRoots.Num() > 0)
                    {
                        FVector ChassisCenter = FVector::ZeroVector;
                        int32 TotalBodyVerts = 0;
                        for (int32 Root : UnmatchedRoots)
                        {
                            for (int32 triIdx : RootToTris[Root])
                            {
                                ChassisCenter += Geom.Vertices[Geom.Polygons[triIdx * 3]];
                                ChassisCenter += Geom.Vertices[Geom.Polygons[triIdx * 3 + 1]];
                                ChassisCenter += Geom.Vertices[Geom.Polygons[triIdx * 3 + 2]];
                                TotalBodyVerts += 3;
                            }
                        }
                        if (TotalBodyVerts > 0) ChassisCenter /= (float)TotalBodyVerts;

                        FVector ChassisPivot = G_FbxBoneWorldLocations.Contains(TEXT("chassis")) ? G_FbxBoneWorldLocations[TEXT("chassis")] : ChassisCenter;

                        TArray<FVector> BodyVerts;
                        TArray<int32> BodyTris;
                        TMap<int32, int32> BodyVertMap;

                        for (int32 Root : UnmatchedRoots)
                        {
                            for (int32 triIdx : RootToTris[Root])
                            {
                                int32 V0 = Geom.Polygons[triIdx * 3];
                                int32 V1 = Geom.Polygons[triIdx * 3 + 1];
                                int32 V2 = Geom.Polygons[triIdx * 3 + 2];

                                if (!BodyVertMap.Contains(V0)) { BodyVertMap.Add(V0, BodyVerts.Num()); BodyVerts.Add(Geom.Vertices[V0] - ChassisPivot); }
                                if (!BodyVertMap.Contains(V1)) { BodyVertMap.Add(V1, BodyVerts.Num()); BodyVerts.Add(Geom.Vertices[V1] - ChassisPivot); }
                                if (!BodyVertMap.Contains(V2)) { BodyVertMap.Add(V2, BodyVerts.Num()); BodyVerts.Add(Geom.Vertices[V2] - ChassisPivot); }

                                BodyTris.Add(BodyVertMap[V0]);
                                BodyTris.Add(BodyVertMap[V1]);
                                BodyTris.Add(BodyVertMap[V2]);
                            }
                        }

                        TArray<FVector> BodyNormals;
                        BodyNormals.SetNumZeroed(BodyVerts.Num());
                        for (int32 t = 0; t + 2 < BodyTris.Num(); t += 3)
                        {
                            int32 I0 = BodyTris[t], I1 = BodyTris[t + 1], I2 = BodyTris[t + 2];
                            FVector TriN = FVector::CrossProduct(BodyVerts[I1] - BodyVerts[I0], BodyVerts[I2] - BodyVerts[I0]).GetSafeNormal();
                            BodyNormals[I0] += TriN; BodyNormals[I1] += TriN; BodyNormals[I2] += TriN;
                        }
                        for (FVector& N : BodyNormals) N = N.GetSafeNormal();

                        FPiSimMeshSection BodySec;
                        BodySec.MeshName = ModelName.Contains(TEXT("chassis"), ESearchCase::IgnoreCase) ? TEXT("chassis") : ModelName;
                        BodySec.PivotPoint = ChassisPivot;
                        BodySec.Vertices = BodyVerts;
                        BodySec.Triangles = BodyTris;
                        BodySec.Normals = BodyNormals;

                        FPiSimImporterMaterialSlot DefaultSlot;
                        DefaultSlot.Triangles = BodyTris;
                        BodySec.MaterialSlots.Add(DefaultSlot);

                        // Şasiyi en başa ekle (Index 0 olması için)
                        OutVisual.Insert(BodySec, 0);
                        UE_LOG(LogTemp, Warning, TEXT("🚗 [VİSUAL ŞASİ GÖVDESİ BİRLEŞTİRİLDİ] '%s' (Toplam Verts: %d)"), *BodySec.MeshName, BodyVerts.Num());
                    }
                }
                else
                {
                    // Tek parça statik geometri
                    FVector Center = FVector::ZeroVector;
                    for (const FVector& V : Geom.Vertices) Center += V;
                    Center /= (float)Geom.Vertices.Num();

                    TArray<FVector> LocalVerts = Geom.Vertices;
                    for (FVector& V : LocalVerts) V -= Center;

                    TArray<FVector> Normals;
                    Normals.SetNumZeroed(LocalVerts.Num());
                    for (int32 t = 0; t + 2 < Geom.Polygons.Num(); t += 3)
                    {
                        int32 I0 = Geom.Polygons[t];
                        int32 I1 = Geom.Polygons[t + 1];
                        int32 I2 = Geom.Polygons[t + 2];
                        FVector TriN = FVector::CrossProduct(LocalVerts[I1] - LocalVerts[I0], LocalVerts[I2] - LocalVerts[I0]).GetSafeNormal();
                        Normals[I0] = TriN; Normals[I1] = TriN; Normals[I2] = TriN;
                    }

                    FPiSimMeshSection Section;
                    Section.MeshName = ModelName;
                    Section.PivotPoint = Center;
                    Section.Vertices = LocalVerts;
                    Section.Triangles = Geom.Polygons;
                    Section.Normals = Normals;

                    FPiSimImporterMaterialSlot DefaultSlot;
                    DefaultSlot.Triangles = Geom.Polygons;
                    Section.MaterialSlots.Add(DefaultSlot);
                    OutVisual.Add(Section);
                }
            }
        }
    }

    // Geometrisi Olmayan Saf Kemikleri (Pure Bones: M_Steer_FR, M_Steer_FL vb.) ModelMap ve SubDeformer'dan Eksiksiz Çıkar
    OutPureBones.Empty();

    // 1) FBX ModelMap üzerindeki tüm nesneleri tara (Eski koddaki birebir yöntem)
    for (const auto& Pair : ModelMap)
    {
        FString MName = Pair.Value;
        int32 ColonIdx;
        if (MName.FindLastChar(':', ColonIdx)) MName = MName.Mid(ColonIdx + 1);

        FString Lower = MName.ToLower();
        if (Lower.Contains(TEXT("armature")) || Lower.Contains(TEXT("root")) || Lower.StartsWith(TEXT("ucx")) || Lower.StartsWith(TEXT("s_")))
        {
            continue; // Armature, root, collision ve s_ ile başlayan sensörler elenir
        }

        bool bAlreadyInVisual = false;
        for (const auto& Sec : OutVisual)
        {
            if (Sec.MeshName.Equals(MName, ESearchCase::IgnoreCase))
            {
                bAlreadyInVisual = true;
                break;
            }
        }

        if (!bAlreadyInVisual && (MName.StartsWith(TEXT("M_"), ESearchCase::IgnoreCase) ||
                                  MName.StartsWith(TEXT("B_"), ESearchCase::IgnoreCase) ||
                                  Lower.Contains(TEXT("wing")) ||
                                  Lower.Contains(TEXT("steer")) ||
                                  Lower.Contains(TEXT("servo")) ||
                                  Lower.Contains(TEXT("joint")) ||
                                  Lower.Contains(TEXT("cam")) ||
                                  Lower.Contains(TEXT("sensor")) ||
                                  Lower.Contains(TEXT("fpv"))))
        {
            OutPureBones.AddUnique(MName);
        }
    }

    // 2) SubDeformerMap kontrolü (Ek güvence)
    for (const auto& SubDefPair : SubDeformerMap)
    {
        const FRawFbxSubDef& SubDef = SubDefPair.Value;
        bool bHasGeo = false;
        for (const auto& Sec : OutVisual)
        {
            if (Sec.MeshName.Equals(SubDef.BoneLabel, ESearchCase::IgnoreCase)) { bHasGeo = true; break; }
        }
        if (!bHasGeo && !SubDef.BoneLabel.IsEmpty())
        {
            FString Lower = SubDef.BoneLabel.ToLower();
            if (!Lower.Contains(TEXT("armature")) && !Lower.Contains(TEXT("root")) && !Lower.StartsWith(TEXT("ucx")) && !Lower.StartsWith(TEXT("s_")))
            {
                OutPureBones.AddUnique(SubDef.BoneLabel);
            }
        }
    }

    // 3) HİYERARŞİK KEMİK AĞACINI OLUŞTUR (Connections ve ModelMap üzerinden)
    // Sensörler (s_ veya S_) KESİNLİKLE dahil edilmez!
    TMap<FString, FString> BoneParentMap; // BoneName -> ParentBoneName
    for (const auto& Pair : ModelMap)
    {
        uint64 ObjID = Pair.Key;
        FString ChildName = Pair.Value;
        int32 CIdx;
        if (ChildName.FindLastChar(':', CIdx)) ChildName = ChildName.Mid(CIdx + 1);

        FString ChildLower = ChildName.ToLower();
        if (ChildLower.StartsWith(TEXT("s_")) || ChildLower.StartsWith(TEXT("ucx")) || ChildLower.Contains(TEXT("armature")))
        {
            continue;
        }

        if (const TArray<uint64>* Parents = ChildToParents.Find(ObjID))
        {
            for (uint64 PID : *Parents)
            {
                if (const FString* PNamePtr = ModelMap.Find(PID))
                {
                    FString PName = *PNamePtr;
                    if (PName.FindLastChar(':', CIdx)) PName = PName.Mid(CIdx + 1);
                    FString PLower = PName.ToLower();
                    if (!PLower.StartsWith(TEXT("ucx")) && !PLower.Contains(TEXT("armature")) && !PLower.Contains(TEXT("root")))
                    {
                        BoneParentMap.Add(ChildName, PName);
                        break;
                    }
                }
            }
        }
    }

    // Tüm geçerli kemikleri topla (Visual parçalar + Saf kemikler, sensörler HARİÇ)
    TArray<FString> CombinedBoneNames;
    for (const auto& Sec : OutVisual)
    {
        if (!Sec.MeshName.StartsWith(TEXT("s_"), ESearchCase::IgnoreCase))
        {
            CombinedBoneNames.AddUnique(Sec.MeshName);
        }
    }
    for (const auto& PureBone : OutPureBones)
    {
        if (!PureBone.StartsWith(TEXT("s_"), ESearchCase::IgnoreCase))
        {
            CombinedBoneNames.AddUnique(PureBone);
        }
    }

    // Derinlik hesaplama fonksiyonu (Özyinelemeli)
    auto GetDepthLevel = [&](const FString& Bone, auto& Self, int32 CurDepth) -> int32
    {
        if (CurDepth > 8) return CurDepth; // Döngüsel koruma
        if (const FString* Parent = BoneParentMap.Find(Bone))
        {
            if (CombinedBoneNames.Contains(*Parent))
            {
                return Self(*Parent, Self, CurDepth + 1);
            }
        }
        return CurDepth;
    };

    // Hiyerarşik düğümleri oluştur
    for (const FString& Bone : CombinedBoneNames)
    {
        FPiSimBoneHierarchyNode Node;
        Node.BoneName = FName(*Bone);
        if (const FString* Parent = BoneParentMap.Find(Bone))
        {
            Node.ParentBoneName = FName(**Parent);
        }
        else
        {
            Node.ParentBoneName = NAME_None;
        }
        Node.IndentLevel = GetDepthLevel(Bone, GetDepthLevel, 0);
        OutBoneHierarchy.Add(Node);
    }

    UE_LOG(LogTemp, Warning, TEXT("===================================================================="));
    UE_LOG(LogTemp, Warning, TEXT(">>> [ModelFbxImporter] FBX AYRIŞTIRMA RAPORU (%s) <<<"), *FilePath);
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
    UE_LOG(LogTemp, Warning, TEXT("📡 SENSÖR LİSTESİ (Toplam: %d Adet):"), OutSensors.Num());
    for (int32 s = 0; s < OutSensors.Num(); ++s)
    {
        UE_LOG(LogTemp, Warning, TEXT("   [%d] Sensor: '%s' | Pivot: %s"),
            s, *OutSensors[s].SensorName, *OutSensors[s].PivotPoint.ToString());
    }
    UE_LOG(LogTemp, Warning, TEXT("🦴 SAF KEMİK LİSTESİ (Toplam: %d Adet):"), OutPureBones.Num());
    for (int32 b = 0; b < OutPureBones.Num(); ++b)
    {
        UE_LOG(LogTemp, Warning, TEXT("   [%d] PureBone: '%s'"), b, *OutPureBones[b]);
    }
    UE_LOG(LogTemp, Warning, TEXT("===================================================================="));

    UE_LOG(LogTemp, Log, TEXT("[ModelFbxImporter] FBX Ayrıştırma Tamamlandı: Görsel: %d, UCX: %d, Sensör: %d, Saf Kemik: %d, Hiyerarşi Düğüm: %d"),
        OutVisual.Num(), OutUCX.Num(), OutSensors.Num(), OutPureBones.Num(), OutBoneHierarchy.Num());

    return (OutVisual.Num() > 0 || OutUCX.Num() > 0);
}

// @state: WIP - FBX dosyasını APiSimVehicle aktörüne bağlar ve kemik ağacını kurar
bool UPiSimModelFbxImporter::ImportFbxFileToVehicle(
    APiSimVehicle* TargetVehicle,
    const FString& FilePath,
    float Scale,
    bool bEnableSmoothNormals)
{
    if (!TargetVehicle)
    {
        return false;
    }

    if (!FPaths::FileExists(FilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("[ModelFbxImporter] Belirtilen FBX dosyasi bulunamadi: %s. Otomatik atama yapilmadi!"), *FilePath);
        return false;
    }

    TArray<FPiSimMeshSection> VisualSections;
    TArray<FPiSimMeshSection> UCXSections;
    TArray<FPiSimSensorSection> SensorSections;
    TArray<FString> PureBones;
    TArray<FPiSimBoneHierarchyNode> BoneHierarchy;

    if (!ParseBinaryFbxFile(FilePath, VisualSections, UCXSections, SensorSections, PureBones, BoneHierarchy, Scale, bEnableSmoothNormals))
    {
        UE_LOG(LogTemp, Error, TEXT("[ModelFbxImporter] FBX dosyasi ayrilamadi: %s"), *FilePath);
        return false;
    }

    // Aracı Temizle
    TargetVehicle->ClearVehicleModel();

    // =========================================================================
    // ⚠️ KESİNLİKLE DEĞİŞTİRİLEMEZ KURAL: EĞER MODELDE UCX YOK İSE HATA MESAJI VER!
    // VİSUAL MESH KESİNLİKLE COLLİSİON OLARAK ATANAMAZ VE FİZİK SİMÜLE EDEMEZ!
    // =========================================================================
    if (UCXSections.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("=================================================================================="));
        UE_LOG(LogTemp, Error, TEXT("❌ [ModelFbxImporter HATA] '%s' modelinde HİÇBİR UCX ÇARPIŞMA ZIRHI BULUNAMADI!"), *FilePath);
        UE_LOG(LogTemp, Error, TEXT("⚠️ GÜVENLİK GEREĞİ: Visual Mesh ASLA collision olarak kullanılamaz ve fizik simüle edemez!"));
        UE_LOG(LogTemp, Error, TEXT("=================================================================================="));
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, TEXT("❌ [HATA] FBX içinde UCX Çarpışma Zırhı YOK! Görsel mesh ASLA collision alamaz!"));
        }
    }

    UMaterialInterface* DefaultMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

    FVector BaseChassisPivot = (VisualSections.Num() > 0) ? VisualSections[0].PivotPoint : FVector::ZeroVector;
    if (BaseChassisPivot.IsZero() && UCXSections.Num() > 0)
    {
        BaseChassisPivot = UCXSections[0].PivotPoint;
    }

    USceneComponent* MainParentComp = TargetVehicle->GetRootComponent();

    // =========================================================================
    // AŞAMA 1: UCX ÇARPIŞMA ZIRHINI (COLLISION) KUR
    // =========================================================================
    // AŞAMA 1: GÖRSEL MODELLERİ (VISUAL MESH) KUR (22e300b pattern)
    // VisualMeshComponents[0] ana şasidir, diğer parçalar buna bağlanır
    // =========================================================================
    TMap<int32, USceneComponent*> WheelToSteerCompMap;

    auto RegisterCleanAliases = [&](const FString& RawName, USceneComponent* Comp)
    {
        if (!Comp || RawName.IsEmpty()) return;
        TargetVehicle->RegisterBoneSceneComponent(FName(*RawName), Comp);
        FString Clean = RawName;
        int32 Idx;
        if (Clean.FindLastChar(':', Idx)) Clean = Clean.Mid(Idx + 1);
        if (Clean.FindLastChar('|', Idx)) Clean = Clean.Mid(Idx + 1);
        TargetVehicle->RegisterBoneSceneComponent(FName(*Clean), Comp);
        TargetVehicle->RegisterBoneSceneComponent(FName(*Clean.ToLower()), Comp);

        FString NoPrefix = Clean;
        NoPrefix.RemoveFromStart(TEXT("M_"), ESearchCase::IgnoreCase);
        NoPrefix.RemoveFromStart(TEXT("B_"), ESearchCase::IgnoreCase);
        if (!NoPrefix.Equals(Clean))
        {
            TargetVehicle->RegisterBoneSceneComponent(FName(*NoPrefix), Comp);
            TargetVehicle->RegisterBoneSceneComponent(FName(*NoPrefix.ToLower()), Comp);
        }
    };

    // 0) Önceki parçaları temizle
    for (UProceduralMeshComponent* Comp : TargetVehicle->VisualMeshComponents)
    {
        if (Comp) Comp->DestroyComponent();
    }
    TargetVehicle->VisualMeshComponents.Empty();

    for (UProceduralMeshComponent* Comp : TargetVehicle->CollisionMeshComponents)
    {
        if (Comp) Comp->DestroyComponent();
    }
    TargetVehicle->CollisionMeshComponents.Empty();
    TargetVehicle->BoneSceneComponents.Empty();
    TargetVehicle->WheelToSteerBoneMap.Empty();

    for (UPhysicsConstraintComponent* Constraint : TargetVehicle->JointConstraints)
    {
        if (Constraint && !Constraint->IsBeingDestroyed())
        {
            Constraint->DestroyComponent();
        }
    }
    TargetVehicle->JointConstraints.Empty();
    TargetVehicle->SectionConstraintMap.Empty();

    // =========================================================================
    // AŞAMA 1: KEMİK HİYERARŞİSİNİ KUR (Blender Kemik Ağacı Birebir)
    // =========================================================================
    TMap<FName, USceneComponent*> CreatedBoneComponents;

    // Tüm kemikleri (BoneHierarchy + PureBones + Visual Parçalar) topla
    TArray<FName> AllBonesToCreate;
    for (const FPiSimBoneHierarchyNode& Node : BoneHierarchy)
    {
        AllBonesToCreate.AddUnique(Node.BoneName);
    }
    for (const FString& PureBone : PureBones)
    {
        AllBonesToCreate.AddUnique(FName(*PureBone));
    }
    for (const auto& Sec : VisualSections)
    {
        AllBonesToCreate.AddUnique(FName(*Sec.MeshName));
    }

    // 1.1) Her kemik için USceneComponent oluştur ve konumunu ayarla
    for (const FName& BoneName : AllBonesToCreate)
    {
        FName BoneCompName = MakeUniqueObjectName(TargetVehicle, USceneComponent::StaticClass(), FName(*FString::Printf(TEXT("BoneComp_%s"), *BoneName.ToString())));
        USceneComponent* BoneComp = NewObject<USceneComponent>(TargetVehicle, BoneCompName);
        BoneComp->SetMobility(EComponentMobility::Movable);

        FVector BoneLoc = FVector::ZeroVector;
        FString BoneStr = BoneName.ToString();
        if (G_FbxBoneWorldLocations.Contains(BoneStr))
        {
            BoneLoc = G_FbxBoneWorldLocations[BoneStr] - BaseChassisPivot;
        }
        else
        {
            for (const auto& Sec : VisualSections)
            {
                if (Sec.MeshName.Equals(BoneStr, ESearchCase::IgnoreCase))
                {
                    BoneLoc = Sec.PivotPoint - BaseChassisPivot;
                    break;
                }
            }
        }

        BoneComp->SetRelativeLocation(BoneLoc);
        BoneComp->SetRelativeRotation(FRotator::ZeroRotator);
        CreatedBoneComponents.Add(BoneName, BoneComp);
        TargetVehicle->RegisterBoneSceneComponent(BoneName, BoneComp);
        RegisterCleanAliases(BoneStr, BoneComp);
    }

    // 1.2) Blender Ebeveyn-Çocuk İlişkisine Göre Kemikleri Birbirine Bağla
    USceneComponent* RootBoneComp = nullptr;
    for (const FPiSimBoneHierarchyNode& Node : BoneHierarchy)
    {
        USceneComponent* ChildBone = CreatedBoneComponents.FindRef(Node.BoneName);
        if (!ChildBone) continue;

        USceneComponent* ParentBone = (!Node.ParentBoneName.IsNone()) ? CreatedBoneComponents.FindRef(Node.ParentBoneName) : nullptr;
        if (ParentBone)
        {
            FVector ChildWorld = ChildBone->GetRelativeLocation();
            FVector ParentWorld = ParentBone->GetRelativeLocation();
            ChildBone->AttachToComponent(ParentBone, FAttachmentTransformRules::KeepRelativeTransform);
            ChildBone->SetRelativeLocation(ChildWorld - ParentWorld);
            ChildBone->SetRelativeRotation(FRotator::ZeroRotator);
            UE_LOG(LogTemp, Warning, TEXT("🔗 [BLENDER HİYERARŞİSİ] Kemik: '%s' -> Ebeveyn: '%s' (RelLoc: %s)"),
                *Node.BoneName.ToString(), *Node.ParentBoneName.ToString(), *(ChildWorld - ParentWorld).ToString());
        }
        else
        {
            FVector ChildWorld = ChildBone->GetRelativeLocation();
            ChildBone->AttachToComponent(MainParentComp, FAttachmentTransformRules::KeepRelativeTransform);
            ChildBone->SetRelativeLocation(ChildWorld);
            ChildBone->SetRelativeRotation(FRotator::ZeroRotator);
            if (!RootBoneComp) RootBoneComp = ChildBone;
            UE_LOG(LogTemp, Warning, TEXT("👑 [BLENDER KÖK KEMİK] '%s' doğrudan Pawn köküne bağlandı."), *Node.BoneName.ToString());
        }
        ChildBone->RegisterComponent();
    }

    // Hiyerarşide listelenmemiş kalan kemikleri ana köke bağla
    for (const auto& Pair : CreatedBoneComponents)
    {
        if (!Pair.Value->GetAttachParent())
        {
            Pair.Value->AttachToComponent(RootBoneComp ? RootBoneComp : MainParentComp, FAttachmentTransformRules::KeepRelativeTransform);
            Pair.Value->RegisterComponent();
        }
    }

    // =========================================================================
    // AŞAMA 2: UCX ÇARPIŞMA ZIRHINI (COLLISION) KUR
    // Fiziği ve çarpışmayı simüle edecek TEK bileşenler UCX zırhıdır!
    // =========================================================================
    for (int32 j = 0; j < UCXSections.Num(); ++j)
    {
        FName CollCompName = MakeUniqueObjectName(TargetVehicle, UProceduralMeshComponent::StaticClass(), FName(*FString::Printf(TEXT("UcxMesh_%d_%s"), j, *UCXSections[j].MeshName)));
        UProceduralMeshComponent* CollMesh = NewObject<UProceduralMeshComponent>(TargetVehicle, CollCompName);
        CollMesh->SetMobility(EComponentMobility::Movable);

        // İlgili kemiği bul ve ona bağla
        USceneComponent* AttachBone = nullptr;
        for (const auto& BonePair : CreatedBoneComponents)
        {
            FString BStr = BonePair.Key.ToString();
            if (UCXSections[j].MeshName.Contains(BStr, ESearchCase::IgnoreCase))
            {
                AttachBone = BonePair.Value;
                break;
            }
        }
        if (!AttachBone)
        {
            AttachBone = RootBoneComp ? RootBoneComp : MainParentComp;
        }

        CollMesh->AttachToComponent(AttachBone, FAttachmentTransformRules::KeepRelativeTransform);
        CollMesh->SetRelativeLocation(FVector::ZeroVector);
        CollMesh->SetRelativeRotation(FRotator::ZeroRotator);
        CollMesh->RegisterComponent();

        TArray<FVector2D> UV0;
        TArray<FLinearColor> VertexColors;
        TArray<FProcMeshTangent> Tangents;
        CollMesh->CreateMeshSection_LinearColor(0, UCXSections[j].Vertices, UCXSections[j].Triangles, UCXSections[j].Normals, UV0, VertexColors, Tangents, false);
        if (DefaultMat) CollMesh->SetMaterial(0, DefaultMat);

        // UCX Convex Hull zırhını bu parçaya fizik gövdesi olarak doğrudan uygula
        CollMesh->ClearCollisionConvexMeshes();
        CollMesh->AddCollisionConvexMesh(UCXSections[j].Vertices);

        CollMesh->bUseComplexAsSimpleCollision = false;
        CollMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        CollMesh->SetCollisionObjectType(ECC_WorldDynamic);
        CollMesh->SetCollisionResponseToAllChannels(ECR_Block);
        CollMesh->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
        CollMesh->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
        CollMesh->RecreatePhysicsState();

        // Normalde görsel olarak gizlidir, kullanıcı "UCX ZIRH" butonuna basınca görünür
        CollMesh->SetVisibility(false);
        CollMesh->SetHiddenInGame(true);

        TargetVehicle->CollisionMeshComponents.Add(CollMesh);
        RegisterCleanAliases(UCXSections[j].MeshName, CollMesh);
    }

    // =========================================================================
    // AŞAMA 3: GÖRSEL MODELLERİ (VISUAL MESH) KUR
    // ⚠️ KESİNLİKLE DEĞİŞTİRİLEMEZ KURAL: VİSUAL MESH KESİNLİKLE FİZİK SİMÜLE ETMEZ,
    // COLLİSİON ALMAZ! DOĞRUDAN BLENDER'DAKİ KEMİĞİNİN CHILDI OLARAK ONU TAKİP EDER!
    // =========================================================================
    for (int32 i = 0; i < VisualSections.Num(); ++i)
    {
        FName CompName = MakeUniqueObjectName(TargetVehicle, UProceduralMeshComponent::StaticClass(), FName(*FString::Printf(TEXT("VisMesh_%d_%s"), i, *VisualSections[i].MeshName)));
        UProceduralMeshComponent* VisComp = NewObject<UProceduralMeshComponent>(TargetVehicle, CompName);
        VisComp->SetMobility(EComponentMobility::Movable);

        // İlgili kemiği bul ve ona bağla
        USceneComponent* AttachBone = CreatedBoneComponents.FindRef(FName(*VisualSections[i].MeshName));
        if (!AttachBone)
        {
            for (const auto& BonePair : CreatedBoneComponents)
            {
                FString BStr = BonePair.Key.ToString();
                if (VisualSections[i].MeshName.Contains(BStr, ESearchCase::IgnoreCase))
                {
                    AttachBone = BonePair.Value;
                    break;
                }
            }
        }
        if (!AttachBone)
        {
            AttachBone = RootBoneComp ? RootBoneComp : MainParentComp;
        }

        VisComp->AttachToComponent(AttachBone, FAttachmentTransformRules::KeepRelativeTransform);
        VisComp->SetRelativeLocation(FVector::ZeroVector);
        VisComp->SetRelativeRotation(FRotator::ZeroRotator);
        VisComp->RegisterComponent();

        TArray<FVector2D> UV0;
        TArray<FLinearColor> VertexColors;
        TArray<FProcMeshTangent> Tangents;

        UV0.Reserve(VisualSections[i].Vertices.Num());
        for (const FVector& V : VisualSections[i].Vertices)
        {
            UV0.Add(FVector2D(V.X * 0.01f, V.Y * 0.01f));
        }

        // Görsel Modeli Çiz (Render)
        UMaterialInterface* BaseMat = DefaultMat;
        if (VisualSections[i].MaterialSlots.Num() > 1)
        {
            for (int32 s = 0; s < VisualSections[i].MaterialSlots.Num(); ++s)
            {
                const FPiSimImporterMaterialSlot& Slot = VisualSections[i].MaterialSlots[s];
                if (Slot.Triangles.Num() > 0)
                {
                    VisComp->CreateMeshSection_LinearColor(s, VisualSections[i].Vertices, Slot.Triangles, VisualSections[i].Normals, UV0, VertexColors, Tangents, false);
                    if (BaseMat)
                    {
                        UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(BaseMat, TargetVehicle);
                        if (DynMat)
                        {
                            DynMat->SetVectorParameterValue(TEXT("Color"), Slot.Color);
                            DynMat->SetVectorParameterValue(TEXT("BaseColor"), Slot.Color);
                            DynMat->SetVectorParameterValue(TEXT("Tint"), Slot.Color);
                            DynMat->SetScalarParameterValue(TEXT("Metallic"), Slot.Metallic);
                            DynMat->SetScalarParameterValue(TEXT("Roughness"), Slot.Roughness);
                            DynMat->SetScalarParameterValue(TEXT("Specular"), Slot.Specular);
                            VisComp->SetMaterial(s, DynMat);
                        }
                        else
                        {
                            VisComp->SetMaterial(s, BaseMat);
                        }
                    }
                }
            }
        }
        else
        {
            VisComp->CreateMeshSection_LinearColor(0, VisualSections[i].Vertices, VisualSections[i].Triangles, VisualSections[i].Normals, UV0, VertexColors, Tangents, false);
            if (BaseMat)
            {
                UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(BaseMat, TargetVehicle);
                if (DynMat)
                {
                    DynMat->SetVectorParameterValue(TEXT("Color"), VisualSections[i].MaterialColor);
                    DynMat->SetVectorParameterValue(TEXT("BaseColor"), VisualSections[i].MaterialColor);
                    DynMat->SetVectorParameterValue(TEXT("Tint"), VisualSections[i].MaterialColor);
                    DynMat->SetScalarParameterValue(TEXT("Metallic"), VisualSections[i].Metallic);
                    DynMat->SetScalarParameterValue(TEXT("Roughness"), VisualSections[i].Roughness);
                    DynMat->SetScalarParameterValue(TEXT("Specular"), VisualSections[i].Specular);
                    VisComp->SetMaterial(0, DynMat);
                }
                else
                {
                    VisComp->SetMaterial(0, BaseMat);
                }
            }
        }

        VisComp->SetVisibility(true);
        VisComp->SetHiddenInGame(false);

        // =========================================================================
        // 🔒 İLGİLİ UCX CONVEX HULL'UNU BUL VE PARÇAYA ZIRH OLARAK GİYDİR
        // (Spagetti Kod - PiSimModelImporter.cpp lines 2848-2915 Birebir Entegrasyon)
        // =========================================================================
        VisComp->ClearCollisionConvexMeshes();
        bool bHasCollision = false;

        // 1) Bu görsel parçayla doğrudan eşleşen UCX'leri ekle
        for (int32 j = 0; j < UCXSections.Num(); ++j)
        {
            FString UcxLower = UCXSections[j].MeshName.ToLower();
            FString VisLower = VisualSections[i].MeshName.ToLower();
            bool bMatch = UcxLower.Contains(VisLower);
            if (!bMatch && VisLower.Contains(TEXT("chassis")) && UcxLower.Contains(TEXT("chassis")))
            {
                bMatch = true;
            }
            if (bMatch)
            {
                VisComp->AddCollisionConvexMesh(UCXSections[j].Vertices);
                bHasCollision = true;
            }
        }

        // 2) Eğer bu ana gövde (i == 0) ise ve ayrı alt görsel parçalara atanmamış kanat/gövde UCX'leri varsa:
        // Bunları ana şasiye göre ofsetleyerek ana fizik gövdesine ekle!
        if (i == 0 && UCXSections.Num() > 0)
        {
            for (int32 j = 0; j < UCXSections.Num(); ++j)
            {
                bool bBelongsToOtherVisual = false;
                for (int32 other = 1; other < VisualSections.Num(); ++other)
                {
                    FString OtherLower = VisualSections[other].MeshName.ToLower();
                    if (UCXSections[j].MeshName.ToLower().Contains(OtherLower))
                    {
                        bBelongsToOtherVisual = true;
                        break;
                    }
                }

                if (!bBelongsToOtherVisual)
                {
                    FVector Offset = UCXSections[j].PivotPoint - VisualSections[0].PivotPoint;
                    TArray<FVector> OffsetVerts = UCXSections[j].Vertices;
                    for (FVector& V : OffsetVerts)
                    {
                        V += Offset;
                    }
                    VisComp->AddCollisionConvexMesh(OffsetVerts);
                    bHasCollision = true;
                    UE_LOG(LogTemp, Warning, TEXT("🛡️ [UCX ZIRHI ŞASİYE MONTE EDİLDİ] '%s' şasiye eklendi (Ofset: %s, Verts: %d)"),
                        *UCXSections[j].MeshName, *Offset.ToString(), OffsetVerts.Num());
                }
            }
        }

        // Çarpışma durumu: SADECE UCX varsa collision aktif, UCX yoksa NO COLLISION!
        if (bHasCollision)
        {
            VisComp->bUseComplexAsSimpleCollision = false;
            VisComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            VisComp->SetCollisionObjectType(ECC_WorldDynamic);
            VisComp->SetCollisionResponseToAllChannels(ECR_Block);
            VisComp->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block); // Zemini bloklar
            VisComp->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
            VisComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
        }
        else
        {
            VisComp->bUseComplexAsSimpleCollision = false;
            VisComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }
        VisComp->SetSimulatePhysics(false);
        VisComp->RecreatePhysicsState();
        VisComp->UpdateBounds();

        TargetVehicle->VisualMeshComponents.Add(VisComp);
        RegisterCleanAliases(VisualSections[i].MeshName, VisComp);
    }

    // ⚠️ DİKKAT: SetRootComponent(VisualMeshComponents[0]) çağrılmaz!
    // Kök bileşen VehicleSkeletalMesh kalır; parçalar kemik hiyerarşisine bağlıdır.
    // Çocuğu kök yapmak cyclic attachment döngüsü oluşturup PIE'de oyunu donduruyordu.

    // -----------------------------------------------------------------------------------------
    // EĞER HİÇ UCX YOKSA: GÖRSEL PARÇALARIN KENDİSİNE COLLISION VER (Güvenlik Sigortası - Spagetti kod satır 3158)
    // -----------------------------------------------------------------------------------------
    if (UCXSections.Num() == 0)
    {
        for (int32 v = 0; v < TargetVehicle->VisualMeshComponents.Num(); ++v)
        {
            if (TargetVehicle->VisualMeshComponents[v] && VisualSections.IsValidIndex(v))
            {
                TargetVehicle->VisualMeshComponents[v]->ClearCollisionConvexMeshes();
                TargetVehicle->VisualMeshComponents[v]->AddCollisionConvexMesh(VisualSections[v].Vertices);
                TargetVehicle->VisualMeshComponents[v]->bUseComplexAsSimpleCollision = false;
                TargetVehicle->VisualMeshComponents[v]->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
                TargetVehicle->VisualMeshComponents[v]->SetCollisionObjectType(ECC_WorldDynamic);
                TargetVehicle->VisualMeshComponents[v]->SetCollisionResponseToAllChannels(ECR_Block);
                TargetVehicle->VisualMeshComponents[v]->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
                TargetVehicle->VisualMeshComponents[v]->RecreatePhysicsState();
                TargetVehicle->VisualMeshComponents[v]->UpdateBounds();
            }
        }
    }

    // =========================================================================
    // AŞAMA 4: MOVİNG İKEN ÇAKIŞMA FİLTRELEME (Self-Collision Filtering)
    // ⚠️ ÇAKIŞAN FİZİKLER: Hareket eden parçalar birbirini itip patlatmasın
    // =========================================================================
    TArray<UPrimitiveComponent*> AllVehiclePrimitives;
    for (UProceduralMeshComponent* C : TargetVehicle->CollisionMeshComponents)
    {
        if (C) AllVehiclePrimitives.Add(C);
    }
    for (UProceduralMeshComponent* V : TargetVehicle->VisualMeshComponents)
    {
        if (V) AllVehiclePrimitives.Add(V);
    }
    if (TargetVehicle->VehicleSkeletalMesh)
    {
        AllVehiclePrimitives.Add(TargetVehicle->VehicleSkeletalMesh);
    }

    for (int32 a = 0; a < AllVehiclePrimitives.Num(); ++a)
    {
        for (int32 b = a + 1; b < AllVehiclePrimitives.Num(); ++b)
        {
            AllVehiclePrimitives[a]->IgnoreComponentWhenMoving(AllVehiclePrimitives[b], true);
            AllVehiclePrimitives[b]->IgnoreComponentWhenMoving(AllVehiclePrimitives[a], true);
        }
    }

    int32 TotalUcxTris = 0;
    int32 TotalUcxVerts = 0;
    for (const auto& Sec : UCXSections)
    {
        TotalUcxTris += Sec.Triangles.Num() / 3;
        TotalUcxVerts += Sec.Vertices.Num();
    }
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(829, 8.0f, FColor::Cyan,
            FString::Printf(TEXT("🛡️ [TOPLAM UCX COLLISION] %d Zırh Parçası | %d Toplam Üçgen | %d Toplam Verts"),
                UCXSections.Num(), TotalUcxTris, TotalUcxVerts));
    }
    UE_LOG(LogTemp, Warning, TEXT("🛡️ [TOPLAM UCX COLLISION] %d Zırh Parçası | %d Toplam Üçgen | %d Toplam Verts"),
        UCXSections.Num(), TotalUcxTris, TotalUcxVerts);

    // Kesitleri araçta sakla (Canlı Chaos simülasyonu için)
    TargetVehicle->StoredVisualSections = VisualSections;
    TargetVehicle->StoredUCXSections = UCXSections;

    // =========================================================================
    // AŞAMA 4: SENSÖR SOKETLERİ VE KEMİK LİSTELERİNİ KAYDET
    // =========================================================================
    for (const FPiSimSensorSection& Sensor : SensorSections)
    {
        FTransform SensorT;
        SensorT.SetLocation(Sensor.PivotPoint);
        SensorT.SetRotation(FQuat(Sensor.Rotation));
        TargetVehicle->SensorSocketTransforms.Add(Sensor.SensorName, SensorT);
    }

    // Kemik isimlerini ve hiyerarşik ağacı kaydet
    TArray<FName> AllBoneNames;
    for (const auto& Sec : VisualSections)
    {
        if (!Sec.MeshName.StartsWith(TEXT("s_"), ESearchCase::IgnoreCase))
        {
            AllBoneNames.AddUnique(FName(*Sec.MeshName));
        }
    }
    for (const auto& PureBone : PureBones)
    {
        if (!PureBone.StartsWith(TEXT("s_"), ESearchCase::IgnoreCase))
        {
            AllBoneNames.AddUnique(FName(*PureBone));
        }
    }

    TargetVehicle->RegisterImportedBones(AllBoneNames);
    TargetVehicle->RegisterImportedBoneHierarchy(BoneHierarchy);

    // =========================================================================
    // 🔒 STEER <-> TEKERLEK BAĞLANTI EŞLEŞTİRMESİ (PiSimModelImporter lines 2921-2977)
    // Direksiyon kemiklerini (M_Steer_FR, M_Steer_FL) ilgili tekerlek mesh indeksine bağla
    // =========================================================================
    TargetVehicle->WheelToSteerBoneMap.Empty();

    // 1) PureBones ve AllBoneNames içindeki Steer kemiklerini isim token'ı ile eşleştir
    for (const FString& PureBoneName : PureBones)
    {
        FString LowerPure = PureBoneName.ToLower();
        if (LowerPure.Contains(TEXT("steer")) || LowerPure.Contains(TEXT("servo")) || LowerPure.StartsWith(TEXT("m_steer")))
        {
            FString Clean = PureBoneName;
            Clean.RemoveFromStart(TEXT("M_Steer_"), ESearchCase::IgnoreCase);
            Clean.RemoveFromStart(TEXT("Steer_"), ESearchCase::IgnoreCase);
            Clean.RemoveFromStart(TEXT("M_"), ESearchCase::IgnoreCase);
            FString CleanLower = Clean.ToLower();

            int32 MatchedWheelIdx = INDEX_NONE;
            for (int32 w = 1; w < VisualSections.Num(); ++w)
            {
                FString LowerW = VisualSections[w].MeshName.ToLower();

                // Sadece tekerlek mesh'leri ile eşleştir — elevon, aero yüzeyleri ASLA
                bool bIsWheelMesh = LowerW.Contains(TEXT("wheel")) ||
                                    LowerW.Contains(TEXT("teker")) ||
                                    LowerW.StartsWith(TEXT("w_"));
                if (!bIsWheelMesh) continue;

                bool bSuffixMatch = LowerW.EndsWith(TEXT("_") + CleanLower) ||
                                    LowerW.Contains(TEXT("_") + CleanLower + TEXT("_")) ||
                                    LowerW.Equals(CleanLower);

                if (bSuffixMatch)
                {
                    MatchedWheelIdx = w;
                    break;
                }
            }

            if (MatchedWheelIdx != INDEX_NONE)
            {
                TargetVehicle->WheelToSteerBoneMap.Add(MatchedWheelIdx, PureBoneName);
                UE_LOG(LogTemp, Warning, TEXT("🎯 [STEER MAFSALI KURULDU] '%s' steer mafsalı tekerleğe bağlandı: [%d] %s"),
                    *PureBoneName, MatchedWheelIdx, *VisualSections[MatchedWheelIdx].MeshName);
            }
        }
    }

    // 2) Blender BoneHierarchy hiyerarşik ilişkisinden de eşleştir (Ebeveyn steer kemiği ise)
    for (const FPiSimBoneHierarchyNode& Node : BoneHierarchy)
    {
        FString ChildStr = Node.BoneName.ToString();
        FString ParentStr = Node.ParentBoneName.ToString();
        if (ParentStr.Contains(TEXT("steer"), ESearchCase::IgnoreCase) || ParentStr.Contains(TEXT("servo"), ESearchCase::IgnoreCase))
        {
            for (int32 w = 1; w < VisualSections.Num(); ++w)
            {
                if (VisualSections[w].MeshName.Equals(ChildStr, ESearchCase::IgnoreCase))
                {
                    TargetVehicle->WheelToSteerBoneMap.Add(w, ParentStr);
                    UE_LOG(LogTemp, Warning, TEXT("🎯 [HİYERARŞİK STEER BAĞLANTISI] '%s' ebeveyn steer kemiği '%s' tekerleğine kilitlendi (Bölüm: %d)"),
                        *ParentStr, *ChildStr, w);
                    break;
                }
            }
        }
    }

    // Otomatik Motor Rol Ataması Yap
    UPiSimMotorConfigurator::AutoAssignMotorsByBoneNames(TargetVehicle);

    // =========================================================================
    // AŞAMA 4: KAMERAYI ARACIN MERKEZİNE KİLİTLE (SPACEX ORBIT FRAMING)
    // =========================================================================
    if (TargetVehicle->CameraSpringArm)
    {
        USceneComponent* FocusComp = RootBoneComp ? RootBoneComp : TargetVehicle->GetRootComponent();

        TargetVehicle->CameraSpringArm->AttachToComponent(FocusComp, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
        TargetVehicle->CameraSpringArm->SetRelativeLocation(FVector(0.0f, 0.0f, 60.0f));
        TargetVehicle->CameraSpringArm->TargetArmLength = 350.0f;
    }

    UE_LOG(LogTemp, Log, TEXT("[ModelFbxImporter] Model araca basariyla yuklendi! Toplam Kemik: %d, Hiyerarsi: %d"), AllBoneNames.Num(), BoneHierarchy.Num());
    return true;
}

// @state: WIP - İskelet ağını araca bağlar, ölçek ve rotasyonu uygular
bool UPiSimModelFbxImporter::ApplySkeletalMeshToVehicle(
    APiSimVehicle* TargetVehicle,
    USkeletalMesh* InSkeletalMesh,
    float InScaleMultiplier,
    FRotator InRotationOffset)
{
    if (!TargetVehicle || !InSkeletalMesh)
    {
        return false;
    }

    if (TargetVehicle->VehicleSkeletalMesh)
    {
        TargetVehicle->VehicleSkeletalMesh->SetSkeletalMesh(InSkeletalMesh);
        TargetVehicle->VehicleSkeletalMesh->SetRelativeScale3D(FVector(InScaleMultiplier));
        TargetVehicle->VehicleSkeletalMesh->SetRelativeRotation(InRotationOffset);
        return true;
    }

    return false;
}
