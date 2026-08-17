#include "PiSimFbxPreCooker.h"
#include "PiSimGarageRobot.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Serialization/BufferArchive.h"
#include "Engine/Engine.h"


APiSimFbxPreCooker::APiSimFbxPreCooker()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);
}

void APiSimFbxPreCooker::BakeFbxRobotToStaticMesh()
{
    FString CollisionFbxPath = FPaths::ProjectSavedDir() / TEXT("Robots/Cache/robot_collision.fbx");
    FString VisualFbxPath = FPaths::ProjectSavedDir() / TEXT("Robots/Cache/robot_visual.fbx");

    // Fallback to robot.fbx if single file mode is used
    if (!FPaths::FileExists(CollisionFbxPath))
    {
        CollisionFbxPath = FPaths::ProjectSavedDir() / TEXT("Robots/Cache/robot.fbx");
    }
    if (!FPaths::FileExists(VisualFbxPath))
    {
        VisualFbxPath = FPaths::ProjectSavedDir() / TEXT("Robots/Cache/robot.fbx");
    }

    BakeTwoFbxRobotFiles(CollisionFbxPath, VisualFbxPath);
}

bool APiSimFbxPreCooker::BakeTwoFbxRobotFiles(const FString& CollisionFbxPath, const FString& VisualFbxPath)
{
    if (!FPaths::FileExists(CollisionFbxPath) && !FPaths::FileExists(VisualFbxPath))
    {
        UE_LOG(LogTemp, Warning, TEXT("[PRE-COOKER LOG] Neither robot_collision.fbx nor robot_visual.fbx exists in Saved/Robots/Cache/!"));
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red,
                TEXT(">>> [PRE-COOKER HATA] Saved/Robots/Cache/ İÇİNDE FBX BULUNAMADI! <<<"));
        }
        return false;
    }

    // 1) Bake Collision FBX
    TArray<FGLBMeshSection> CollisionSections;
    bool bCollisionOk = false;
    if (FPaths::FileExists(CollisionFbxPath))
    {
        bCollisionOk = APiSimGarageRobot::ParseFbxAllBinaryMeshes(CollisionFbxPath, CollisionSections, 0.1f);
    }

    // 2) Bake Visual FBX
    TArray<FGLBMeshSection> VisualSections;
    bool bVisualOk = false;
    if (FPaths::FileExists(VisualFbxPath))
    {
        bVisualOk = APiSimGarageRobot::ParseFbxAllBinaryMeshes(VisualFbxPath, VisualSections, 0.1f);
    }

    // 3) Save pre-cooked binary caches to Saved/Robots/Baked/
    FString BakedDir = FPaths::ProjectSavedDir() / TEXT("Robots/Baked");
    IFileManager::Get().MakeDirectory(*BakedDir, true);

    if (bCollisionOk && CollisionSections.Num() > 0)
    {
        FBufferArchive Ar;
        int32 SecCount = CollisionSections.Num();
        Ar << SecCount;

        for (FGLBMeshSection& Sec : CollisionSections)
        {
            Ar << Sec.MeshName;
            Ar << Sec.Vertices;
            Ar << Sec.Triangles;
            Ar << Sec.Normals;
            Ar << Sec.PivotPoint;
        }

        FString CollisionBakedFilePath = BakedDir / TEXT("robot_collision_baked.bin");
        FFileHelper::SaveArrayToFile(Ar, *CollisionBakedFilePath);
        UE_LOG(LogTemp, Warning, TEXT("[PRE-COOKER SUCCESS] Saved Collision Baked Cache: %s (%d sections)"), *CollisionBakedFilePath, CollisionSections.Num());
    }

    if (bVisualOk && VisualSections.Num() > 0)
    {
        FBufferArchive Ar;
        int32 SecCount = VisualSections.Num();
        Ar << SecCount;

        for (FGLBMeshSection& Sec : VisualSections)
        {
            Ar << Sec.MeshName;
            Ar << Sec.Vertices;
            Ar << Sec.Triangles;
            Ar << Sec.Normals;
            Ar << Sec.PivotPoint;
        }

        FString VisualBakedFilePath = BakedDir / TEXT("robot_visual_baked.bin");
        FFileHelper::SaveArrayToFile(Ar, *VisualBakedFilePath);
        UE_LOG(LogTemp, Warning, TEXT("[PRE-COOKER SUCCESS] Saved Visual Baked Cache: %s (%d sections)"), *VisualBakedFilePath, VisualSections.Num());
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Green,
            FString::Printf(TEXT(">>> [PRE-COOKER BAŞARILI] BULUT UYUMLU 2 FBX PİŞİRİLDİ! (Collision: %d parça, Visual: %d parça) <<<"), CollisionSections.Num(), VisualSections.Num()));
    }

    return true;
}
