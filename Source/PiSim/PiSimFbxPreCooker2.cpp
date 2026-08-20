#include "PiSimFbxPreCooker2.h"
#include "PiSimGarageRobot.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Serialization/BufferArchive.h"
#include "Engine/Engine.h"

APiSimFbxPreCooker2::APiSimFbxPreCooker2()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);
}

void APiSimFbxPreCooker2::BakeSingleFbx()
{
    FString TargetPath = CustomFbxPath;

    if (TargetPath.IsEmpty())
    {
        TargetPath = FPaths::ProjectSavedDir() / TEXT("Robots/Cache") / SourceFbxFileName;
    }

    // Fallback search if rbot1.fbx is in project root, Saved, or fallback to robot.fbx
    if (!FPaths::FileExists(TargetPath))
    {
        FString AltPath1 = FPaths::ProjectDir() / SourceFbxFileName;
        FString AltPath2 = FPaths::ProjectSavedDir() / SourceFbxFileName;
        FString AltFallback = FPaths::ProjectSavedDir() / TEXT("Robots/Cache/robot.fbx");

        if (FPaths::FileExists(AltPath1))
        {
            TargetPath = AltPath1;
        }
        else if (FPaths::FileExists(AltPath2))
        {
            TargetPath = AltPath2;
        }
        else if (FPaths::FileExists(AltFallback))
        {
            TargetPath = AltFallback;
        }
    }

    BakeSingleFbxFile(TargetPath);
}

bool APiSimFbxPreCooker2::BakeSingleFbxFile(const FString& FbxFilePath)
{
    if (!FPaths::FileExists(FbxFilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("[PRECOOKER 2 HATA] FBX Dosyasi bulunamadi: %s"), *FbxFilePath);
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 8.0f, FColor::Red,
                FString::Printf(TEXT(">>> [PRECOOKER 2 HATA] FBX BULUNAMADI: %s <<<"), *FbxFilePath));
        }
        return false;
    }

    UE_LOG(LogTemp, Warning, TEXT("[PRECOOKER 2] FBX Okunuyor: %s (AutoGenerateCollision: %s)"),
        *FbxFilePath, bAutoGenerateCollision ? TEXT("ACIK") : TEXT("KAPALI"));

    // 1) Parse single FBX file normally with user-configured ImportScale
    TArray<FGLBMeshSection> MeshSections;
    bool bParsed = APiSimGarageRobot::ParseFbxAllBinaryMeshes(FbxFilePath, MeshSections, ImportScale);

    if (!bParsed || MeshSections.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("[PRECOOKER 2 HATA] FBX icinden geometri cikartilamadi!"));
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 8.0f, FColor::Red,
                TEXT(">>> [PRECOOKER 2 HATA] FBX İÇİNDEN GEOMETRİ OKUNAMADI! <<<"));
        }
        return false;
    }

    // 2) Auto Generate Collision is explicitly disabled (bAutoGenerateCollision = false)

    // 3) Serialize to Saved/Robots/Baked/
    FString BakedDir = FPaths::ProjectSavedDir() / TEXT("Robots/Baked");
    IFileManager::Get().MakeDirectory(*BakedDir, true);

    FBufferArchive Ar;
    int32 SecCount = MeshSections.Num();
    Ar << SecCount;

    for (FGLBMeshSection& Sec : MeshSections)
    {
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
        Ar << Sec.CollisionConvexVertices;
        Ar << Sec.bHasCustomUCXCollision;
    }

    FString RobotBakedFilePath = BakedDir / TEXT("robot_baked.bin");
    FFileHelper::SaveArrayToFile(Ar, *RobotBakedFilePath);

    FString Rbot1BakedFilePath = BakedDir / TEXT("rbot1_baked.bin");
    FFileHelper::SaveArrayToFile(Ar, *Rbot1BakedFilePath);

    UE_LOG(LogTemp, Warning, TEXT("[PRECOOKER 2 SUCCESS] %d sections baked from %s to %s"),
        MeshSections.Num(), *FbxFilePath, *RobotBakedFilePath);

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Green,
            FString::Printf(TEXT(">>> [PRECOOKER 2] %s BAŞARIYLA PİŞİRİLDİ! (%d Parça, Auto Collision: KAPALI) <<<"),
                *FPaths::GetCleanFilename(FbxFilePath), MeshSections.Num()));
    }

    return true;
}
