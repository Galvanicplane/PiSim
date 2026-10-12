// MotorDebug.cpp

#include "MotorDebug.h"
#include "Vehicle.h"
#include "ProceduralMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"

UPiSimMotorDebugComponent::UPiSimMotorDebugComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostPhysics; // Fizik sonrası güncel konum ve açıyı yakala
}

void UPiSimMotorDebugComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    DrawBoneDiagnostics();
}

void UPiSimMotorDebugComponent::DrawBoneDiagnostics()
{
    APiSimVehicle* Vehicle = Cast<APiSimVehicle>(GetOwner());
    if (!Vehicle || !GetWorld())
    {
        return;
    }

    UWorld* World = GetWorld();
    float CurrentTime = World->GetTimeSeconds();
    bool bShouldLog = (CurrentTime - LastLogTime >= LogIntervalSec);
    if (bShouldLog)
    {
        LastLogTime = CurrentTime;
    }

    int32 BoneIdx = 0;
    for (const auto& Pair : Vehicle->BoneSceneComponents)
    {
        FName BoneName = Pair.Key;
        USceneComponent* BoneComp = Pair.Value;
        if (!BoneComp)
        {
            continue;
        }

        FTransform BoneTransform = BoneComp->GetComponentTransform();
        FVector BoneLoc = BoneTransform.GetLocation();
        FRotator RelRot = BoneComp->GetRelativeRotation();

        // 1) KEMİK EBEVEYN-ÇOCUK HİYERARŞİ ÇİZGİSİ (Sarı Çizgi)
        if (bShowBoneHierarchyLines)
        {
            if (USceneComponent* ParentComp = BoneComp->GetAttachParent())
            {
                FVector ParentLoc = ParentComp->GetComponentLocation();
                DrawDebugLine(World, ParentLoc, BoneLoc, FColor::Yellow, false, -1.0f, 0, 2.5f);
            }
        }

        // 2) KEMİK KOORDİNAT EKSENLERİ VE YÖNELİM OKLARI (RGB)
        if (bShowBoneAxes)
        {
            FVector ForwardVec = BoneTransform.GetUnitAxis(EAxis::X);
            FVector RightVec   = BoneTransform.GetUnitAxis(EAxis::Y);
            FVector UpVec      = BoneTransform.GetUnitAxis(EAxis::Z);

            // X Eksen (İleri): Kırmızı
            DrawDebugDirectionalArrow(World, BoneLoc, BoneLoc + ForwardVec * AxisLength, 10.0f, FColor::Red, false, -1.0f, 0, 3.0f);
            // Y Eksen (Sağ): Yeşil
            DrawDebugDirectionalArrow(World, BoneLoc, BoneLoc + RightVec * (AxisLength * 0.7f), 8.0f, FColor::Green, false, -1.0f, 0, 2.0f);
            // Z Eksen (Yukarı): Mavi
            DrawDebugDirectionalArrow(World, BoneLoc, BoneLoc + UpVec * (AxisLength * 0.7f), 8.0f, FColor::Blue, false, -1.0f, 0, 2.0f);
            // Mafsal Noktası
            DrawDebugPoint(World, BoneLoc, 8.0f, FColor::Magenta, false, -1.0f, 0);
        }

        // 3) KANAL BAZLI LINE TRACE (Kemik yönünde ışın at ve çarpışmayı göster)
        if (bShowLineTraces)
        {
            FVector TraceStart = BoneLoc;
            FVector TraceEnd = BoneLoc + BoneTransform.GetUnitAxis(EAxis::X) * TraceDistance;

            FHitResult Hit;
            FCollisionQueryParams Params(SCENE_QUERY_STAT(PiSimMotorDebugTrace), false, Vehicle);
            Params.AddIgnoredActor(Vehicle);

            bool bHit = World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params);
            if (bHit)
            {
                DrawDebugLine(World, TraceStart, Hit.Location, FColor::Red, false, -1.0f, 0, 2.5f);
                DrawDebugPoint(World, Hit.Location, 10.0f, FColor::Red, false, -1.0f, 0);
            }
            else
            {
                DrawDebugLine(World, TraceStart, TraceEnd, FColor::Cyan, false, -1.0f, 0, 1.5f);
            }
        }

        // 4) VISUAL MESH VE UCX VERTEX SAYILARINI HESAPLA
        int32 TotalVisVerts = 0;
        FString VisNames = TEXT("");
        int32 TotalUcxVerts = 0;
        FString UcxNames = TEXT("");

        // A) Doğrudan bu kemiğin altında (Child) olan Procedural Mesh'ler
        for (int32 c = 0; c < BoneComp->GetNumChildrenComponents(); ++c)
        {
            if (UProceduralMeshComponent* ProcMesh = Cast<UProceduralMeshComponent>(BoneComp->GetChildComponent(c)))
            {
                int32 MeshVerts = 0;
                for (int32 s = 0; s < ProcMesh->GetNumSections(); ++s)
                {
                    if (FProcMeshSection* Sec = ProcMesh->GetProcMeshSection(s))
                    {
                        MeshVerts += Sec->ProcVertexBuffer.Num();
                    }
                }

                if (Vehicle->VisualMeshComponents.Contains(ProcMesh))
                {
                    TotalVisVerts += MeshVerts;
                    VisNames += FString::Printf(TEXT("%s(%d) "), *ProcMesh->GetName(), MeshVerts);
                }
                else if (Vehicle->CollisionMeshComponents.Contains(ProcMesh))
                {
                    TotalUcxVerts += MeshVerts;
                    UcxNames += FString::Printf(TEXT("%s(%d) "), *ProcMesh->GetName(), MeshVerts);
                }
            }
        }

        // B) Stored Sections üzerinden de tara
        FString BoneStrLower = BoneName.ToString().ToLower();
        for (const auto& VSec : Vehicle->StoredVisualSections)
        {
            if (VSec.MeshName.ToLower().Equals(BoneStrLower))
            {
                if (TotalVisVerts == 0)
                {
                    TotalVisVerts = VSec.Vertices.Num();
                    VisNames = FString::Printf(TEXT("%s(%d)"), *VSec.MeshName, VSec.Vertices.Num());
                }
            }
        }
        for (const auto& USec : Vehicle->StoredUCXSections)
        {
            if (USec.MeshName.ToLower().Contains(BoneStrLower))
            {
                if (TotalUcxVerts == 0)
                {
                    TotalUcxVerts += USec.Vertices.Num();
                    UcxNames += FString::Printf(TEXT("%s(%d)"), *USec.MeshName, USec.Vertices.Num());
                }
            }
        }

        // 5) DÜNYADA KEMİK ÜZERİNE 3D BİLGİ METNİ YAZ
        if (bShow3DText)
        {
            FString Diag3D = FString::Printf(
                TEXT("🦴 %s\nRot: Yaw=%+.1f° (Rel: P=%.0f, Y=%.0f, R=%.0f)\n👁️ Vis: %d verts %s\n🛡️ UCX: %d verts %s"),
                *BoneName.ToString(),
                RelRot.Yaw, RelRot.Pitch, RelRot.Yaw, RelRot.Roll,
                TotalVisVerts, VisNames.IsEmpty() ? TEXT("(Mesh Yok)") : *VisNames,
                TotalUcxVerts, UcxNames.IsEmpty() ? TEXT("(UCX Yok)") : *UcxNames
            );
            DrawDebugString(World, BoneLoc + FVector(0.0f, 0.0f, 18.0f), Diag3D, nullptr, FColor::White, -1.0f, false, 1.15f);
        }

        // 6) EKRANA CANLI DEBUG BİLGİSİ
        if (GEngine && BoneIdx < 12)
        {
            GEngine->AddOnScreenDebugMessage(960 + BoneIdx, 0.15f, FColor::Emerald,
                FString::Printf(TEXT("🦴 [KEMİK: %s] Yaw: %+.1f° | Vis: %d verts | UCX: %d verts | %s"),
                    *BoneName.ToString(), RelRot.Yaw, TotalVisVerts, TotalUcxVerts,
                    BoneComp->GetAttachParent() ? *FString::Printf(TEXT("Ebeveyn: %s"), *BoneComp->GetAttachParent()->GetName()) : TEXT("KÖK KEMİK")));
        }

        if (bShouldLog)
        {
            UE_LOG(LogTemp, Warning, TEXT("  🦴 Kemik: '%-18s' | Yaw: %+6.1f° | VisVerts: %5d [%s] | UcxVerts: %4d [%s] | Konum: %s"),
                *BoneName.ToString(), RelRot.Yaw, TotalVisVerts, *VisNames, TotalUcxVerts, *UcxNames, *BoneLoc.ToString());
        }

        BoneIdx++;
    }
}

void UPiSimMotorDebugComponent::DumpDiagnosticsToLog()
{
    LastLogTime = 0.0f; // Hemen bas
}

// IPiSimInspectableModule Entegrasyonu
void UPiSimMotorDebugComponent::GetInspectableProperties(TArray<FPiSimPropertyDescriptor>& OutProperties)
{
    FPiSimPropertyDescriptor AxesDesc;
    AxesDesc.PropertyId = TEXT("ShowBoneAxes");
    AxesDesc.DisplayName = TEXT("3D Kemik Eksenleri (0/1)");
    AxesDesc.PropertyType = EPiSimPropertyType::Slider;
    AxesDesc.MinValue = 0.0f;
    AxesDesc.MaxValue = 1.0f;
    AxesDesc.CurrentFloatValue = bShowBoneAxes ? 1.0f : 0.0f;
    AxesDesc.bContinuousTickUpdate = false;
    OutProperties.Add(AxesDesc);

    FPiSimPropertyDescriptor HierDesc;
    HierDesc.PropertyId = TEXT("ShowBoneHierarchyLines");
    HierDesc.DisplayName = TEXT("Kemik Hiyerarşi Çizgileri (0/1)");
    HierDesc.PropertyType = EPiSimPropertyType::Slider;
    HierDesc.MinValue = 0.0f;
    HierDesc.MaxValue = 1.0f;
    HierDesc.CurrentFloatValue = bShowBoneHierarchyLines ? 1.0f : 0.0f;
    HierDesc.bContinuousTickUpdate = false;
    OutProperties.Add(HierDesc);

    FPiSimPropertyDescriptor TraceDesc;
    TraceDesc.PropertyId = TEXT("ShowLineTraces");
    TraceDesc.DisplayName = TEXT("Kanal Bazlı Line Trace Işınları (0/1)");
    TraceDesc.PropertyType = EPiSimPropertyType::Slider;
    TraceDesc.MinValue = 0.0f;
    TraceDesc.MaxValue = 1.0f;
    TraceDesc.CurrentFloatValue = bShowLineTraces ? 1.0f : 0.0f;
    TraceDesc.bContinuousTickUpdate = false;
    OutProperties.Add(TraceDesc);

    FPiSimPropertyDescriptor TextDesc;
    TextDesc.PropertyId = TEXT("Show3DText");
    TextDesc.DisplayName = TEXT("3D Dünyada Bilgi Metni (0/1)");
    TextDesc.PropertyType = EPiSimPropertyType::Slider;
    TextDesc.MinValue = 0.0f;
    TextDesc.MaxValue = 1.0f;
    TextDesc.CurrentFloatValue = bShow3DText ? 1.0f : 0.0f;
    TextDesc.bContinuousTickUpdate = false;
    OutProperties.Add(TextDesc);

    FPiSimPropertyDescriptor LenDesc;
    LenDesc.PropertyId = TEXT("AxisLength");
    LenDesc.DisplayName = TEXT("Eksen Uzunluğu (cm)");
    LenDesc.PropertyType = EPiSimPropertyType::Slider;
    LenDesc.MinValue = 5.0f;
    LenDesc.MaxValue = 100.0f;
    LenDesc.CurrentFloatValue = AxisLength;
    LenDesc.bContinuousTickUpdate = false;
    OutProperties.Add(LenDesc);
}

void UPiSimMotorDebugComponent::SetInspectablePropertyFloat(FName PropertyId, float NewValue)
{
    if (PropertyId == TEXT("ShowBoneAxes"))
    {
        bShowBoneAxes = (NewValue > 0.5f);
    }
    else if (PropertyId == TEXT("ShowBoneHierarchyLines"))
    {
        bShowBoneHierarchyLines = (NewValue > 0.5f);
    }
    else if (PropertyId == TEXT("ShowLineTraces"))
    {
        bShowLineTraces = (NewValue > 0.5f);
    }
    else if (PropertyId == TEXT("Show3DText"))
    {
        bShow3DText = (NewValue > 0.5f);
    }
    else if (PropertyId == TEXT("AxisLength"))
    {
        AxisLength = FMath::Clamp(NewValue, 5.0f, 200.0f);
    }
}

void UPiSimMotorDebugComponent::SetInspectablePropertyString(FName PropertyId, const FString& NewValue)
{
}

void UPiSimMotorDebugComponent::TriggerInspectableAction(FName ActionId)
{
    if (ActionId == TEXT("DumpDiagnostics"))
    {
        DumpDiagnosticsToLog();
    }
}
