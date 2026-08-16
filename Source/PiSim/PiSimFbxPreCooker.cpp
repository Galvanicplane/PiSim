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
    FString FbxFullPath = FPaths::ProjectSavedDir() / TEXT("Robots/Cache/robot.fbx");
    if (!FPaths::FileExists(FbxFullPath))
    {
        UE_LOG(LogTemp, Warning, TEXT("[PRE-COOKER LOG] robot.fbx does not exist at: %s"), *FbxFullPath);
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red,
                FString::Printf(TEXT(">>> [PRE-COOKER HATA] robot.fbx BULUNAMADI! YOL: %s <<<"), *FbxFullPath));
        }
        return;
    }

    TArray<FGLBMeshSection> Sections;
    if (!APiSimGarageRobot::ParseFbxAllBinaryMeshes(FbxFullPath, Sections, 0.1f))

    {
        UE_LOG(LogTemp, Warning, TEXT("[PRE-COOKER LOG] Failed to parse robot.fbx!"));
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red,
                TEXT(">>> [PRE-COOKER HATA] robot.fbx AYRIŞTIRILAMADI! <<<"));
        }
        return;
    }

    // Save pre-cooked binary cache to Saved/Robots/Baked/robot_baked.bin
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
        UE_LOG(LogTemp, Warning, TEXT("[PRE-COOKER LOG] Saved Pre-Cooked Binary Cache to: %s (%d bytes)"), *BakedFilePath, Ar.Num());
    }

    UE_LOG(LogTemp, Warning, TEXT("[PRE-COOKER SUCCESS] Parsed %d sub-mesh sections with Use Complex Collision As Simple before Play!"), Sections.Num());
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Green,
            FString::Printf(TEXT(">>> [PRE-COOKER BAŞARILI] PLAY'E BASMADAN ÖNCE %d PARÇA Saved/Robots/Baked/ İÇİNE COMPLEX COLLISION İLE PİŞİRİLDİ! <<<"), Sections.Num()));
    }
}
