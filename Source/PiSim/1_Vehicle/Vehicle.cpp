#include "Vehicle.h"
#include "TelemetryGateway.h"
#include "TelemetryLogger.h"
#include "AutopilotBridge.h"
#include "Pi5Bridge.h"
#include "CameraSensor.h"
#include "ModelFbxImporter.h"
#include "GarageWidget.h"
#include "ImporterWidget.h"
#include "Wheel.h"
#include "Servo.h"
#include "Propeller.h"
#include "MotorDebug.h"
#include "Components/SkeletalMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Common/UdpSocketBuilder.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h" 

TArray<APiSimVehicle*> APiSimVehicle::ActiveFleet;
FCriticalSection APiSimVehicle::FleetLock;
int32 APiSimVehicle::GlobalVehicleSequence = 0;

// @state: WIP - Araç Pawn yapıcısı, iskelet, köprüler, logger ve kamera bileşenlerini kurar
APiSimVehicle::APiSimVehicle()
{
    PrimaryActorTick.bCanEverTick = true;

    VehicleSkeletalMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("VehicleSkeletalMesh"));
    SetRootComponent(VehicleSkeletalMesh);
    VehicleSkeletalMesh->SetSimulatePhysics(false);
    VehicleSkeletalMesh->SetCollisionProfileName(TEXT("BlockAll"));
    VehicleSkeletalMesh->bApplyImpulseOnDamage = false;

    VisualSkeletalMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("VisualSkeletalMesh"));
    VisualSkeletalMesh->SetupAttachment(VehicleSkeletalMesh);
    VisualSkeletalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    CameraSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraSpringArm"));
    CameraSpringArm->SetupAttachment(VehicleSkeletalMesh);
    CameraSpringArm->TargetArmLength = 350.0f;
    CameraSpringArm->bUsePawnControlRotation = false;
    CameraSpringArm->bDoCollisionTest = false;

    OrbitCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("OrbitCamera"));
    OrbitCamera->SetupAttachment(CameraSpringArm);

    TelemetryGateway = CreateDefaultSubobject<UPiSimTelemetryGateway>(TEXT("TelemetryGateway"));

    TelemetryLogger = CreateDefaultSubobject<UPiSimTelemetryLogger>(TEXT("TelemetryLogger"));

    AutopilotBridge = CreateDefaultSubobject<UPiSimModularAutopilotBridge>(TEXT("AutopilotBridge"));

    Pi5Bridge = CreateDefaultSubobject<UPiSimPi5Bridge>(TEXT("Pi5Bridge"));

    CameraSensor = CreateDefaultSubobject<UPiSimCameraSensor>(TEXT("CameraSensor"));

    bShowDebugVisualsAndMessages = false;
    MotorDebugComponent = CreateDefaultSubobject<UPiSimMotorDebugComponent>(TEXT("MotorDebugComponent"));
    if (MotorDebugComponent)
    {
        MotorDebugComponent->bEnableDebugDraw = false;
        MotorDebugComponent->bShow3DText = false;
        MotorDebugComponent->bShowBoneAxes = false;
        MotorDebugComponent->bShowBoneHierarchyLines = false;
        MotorDebugComponent->bShowLineTraces = false;
    }

    VehicleId = 0;
    VehicleCode = TEXT("");
    VehicleName = TEXT("");
}

// @state: WIP - Kemik ve motor teşhis görselleştirmelerini ve ekran mesajlarını açıp kapatır
void APiSimVehicle::SetDebugVisualsEnabled(bool bEnable)
{
    bShowDebugVisualsAndMessages = bEnable;
    if (MotorDebugComponent)
    {
        MotorDebugComponent->bEnableDebugDraw = bEnable;
        MotorDebugComponent->bShow3DText = bEnable;
        MotorDebugComponent->bShowBoneAxes = bEnable;
        MotorDebugComponent->bShowBoneHierarchyLines = bEnable;
        MotorDebugComponent->bShowLineTraces = bEnable;
    }
}

// @state: WIP - Araç kimliğini ve kodunu yapılandırır (Örn: rover_01_2710262359)
void APiSimVehicle::InitVehicleIdentity(int32 InId)
{
    // Eger arac zaten gecerli bir koda sahipse, garaja gidilse bile asla sifirlama!
    if (!VehicleCode.IsEmpty() && VehicleId > 0)
    {
        return;
    }

    if (InId <= 0)
    {
        InId = ++GlobalVehicleSequence;
    }
    else
    {
        GlobalVehicleSequence = FMath::Max(GlobalVehicleSequence, InId);
    }

    VehicleId = InId;

    // Farklı oturumlarda dahi ASLA tekrar etmeyen gerçek eşsiz kimlik (GUID + yüksek çözünürlüklü saat döngüsü)
    FGuid NewUid = FGuid::NewGuid();
    uint32 UniqueHash = (NewUid.A ^ NewUid.B ^ NewUid.C ^ NewUid.D ^ (uint32)FPlatformTime::Cycles());
    FString UniqueHex = FString::Printf(TEXT("%06X"), UniqueHash & 0xFFFFFF);

    VehicleCode = FString::Printf(TEXT("RV%02d-%s"), VehicleId, *UniqueHex);
    VehicleName = FString::Printf(TEXT("Rover %02d [%s]"), VehicleId, *UniqueHex);
}

// @state: WIP - Başlangıçta fare kontrollerini açar, hedef modeli arar ve uygun arayüzü ekrana basar
void APiSimVehicle::BeginPlay()
{
    // ÖNEMLİ: Bileşenlerin (Pi5Bridge, TelemetryLogger, AutopilotBridge) BeginPlay'i çalışmadan ÖNCE
    // aracın eşsiz kimliği (VehicleId, VehicleCode, VehicleName) ve filo kaydı tamamlanmalıdır!
    {
        FScopeLock Lock(&APiSimVehicle::FleetLock);
        APiSimVehicle::ActiveFleet.RemoveAll([](APiSimVehicle* V) { return !IsValid(V); });
        if (APiSimVehicle::ActiveFleet.Num() == 0)
        {
            GlobalVehicleSequence = 0;
        }
        APiSimVehicle::ActiveFleet.AddUnique(this);
    }

    if (VehicleCode.IsEmpty() || VehicleId <= 0)
    {
        InitVehicleIdentity(0);
    }

    Super::BeginPlay();

    // 1. Hedef Modeli Kontrol Et (SADECE açıkça hedeflenen dosya varsa yükle)
    FString DefaultModelPath = FPaths::ProjectSavedDir() / TEXT("Robots/Cache/robot_import_test.fbx");
    bool bModelLoaded = false;

    if (FPaths::FileExists(DefaultModelPath))
    {
        UE_LOG(LogTemp, Log, TEXT("[APiSimVehicle] Hedef model bulundu: %s. Yükleniyor..."), *DefaultModelPath);
        bModelLoaded = UPiSimModelFbxImporter::ImportFbxFileToVehicle(this, DefaultModelPath, 1.0f);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[APiSimVehicle] 'robot_import_test.fbx' bulunamadi. Güvenlik gereği otomatik model atanmadi, araç boş başlatildi."));
    }

    if (MotorDebugComponent)
    {
        AssignModuleToBone(TEXT("MotorDebug"), MotorDebugComponent);
    }

    if (bStartWithPhysics)
    {
        SetVehiclePhysicsEnabled(true);
    }

    InitCommandSocket();
}

// @state: WIP - Her frame araç durumunu günceller ve fare ile kamera orbit/pan/zoom kontrollerini yürütür
// @state: WIP - EndPlay: Araç seviyeden kaldırıldığında filodan temizler
void APiSimVehicle::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    {
        FScopeLock Lock(&APiSimVehicle::FleetLock);
        APiSimVehicle::ActiveFleet.Remove(this);
        APiSimVehicle::ActiveFleet.RemoveAll([](APiSimVehicle* V) { return !IsValid(V); });
        if (APiSimVehicle::ActiveFleet.Num() == 0)
        {
            GlobalVehicleSequence = 0;
        }
    }
    CloseCommandSocket();
    Super::EndPlay(EndPlayReason);
}

void APiSimVehicle::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    PollCommandSocket();
    UpdateKinematicsAndMotors(DeltaTime);
    UpdateLiveTelemetryState(DeltaTime);

    // =========================================================================
    // CANLI MOTOR / EYLEYİCİ DÖNÜŞ VE KONTROL BLOĞU (PiSimModelImporter pattern)
    // =========================================================================
    // 1) STEER VE SERVO AÇISINI HER TICK HİZALA (PiSimModelImporter lines 904-940)
    for (const auto& Pair : MotorTestValues)
    {
        FName BoneName = Pair.Key;
        float TestVal = Pair.Value;
        FString LowerName = BoneName.ToString().ToLower();

        bool bIsSteer = LowerName.Contains(TEXT("steer")) || LowerName.Contains(TEXT("servo")) ||
                        LowerName.Contains(TEXT("joint")) || LowerName.Contains(TEXT("arm")) ||
                        LowerName.Contains(TEXT("elevon")) || LowerName.Contains(TEXT("aileron")) ||
                        LowerName.Contains(TEXT("rudder")) || LowerName.Contains(TEXT("flap"));

        if (bIsSteer)
        {
            float TargetAngle = TestVal * 45.0f;
            if (USceneComponent* SteerComp = GetSceneComponentForBone(BoneName))
            {
                SteerComp->SetRelativeRotation(FRotator(0.0f, TargetAngle, 0.0f));
            }

            if (bIsPhysicsSimulating)
            {
                UPhysicsConstraintComponent* Constraint = FindConstraintForBone(BoneName);
                if (!Constraint)
                {
                    int32 TargetSection = FindVisualSectionForBone(BoneName);
                    Constraint = FindConstraintForSection(TargetSection);
                }
                if (Constraint)
                {
                    FVector RelLoc = Constraint->GetRelativeLocation();
                    int32 TargetSection = FindVisualSectionForBone(BoneName);
                    if (TargetSection > 0 && StoredVisualSections.IsValidIndex(TargetSection) && StoredVisualSections.IsValidIndex(0))
                    {
                        RelLoc = StoredVisualSections[TargetSection].PivotPoint - StoredVisualSections[0].PivotPoint;
                    }
                    FTransform SteerPose(FRotator(0.0f, TargetAngle, 0.0f), RelLoc);
                    Constraint->SetConstraintReferenceFrame(EConstraintFrame::Frame1, SteerPose);
                }
            }
        }
    }

    // 2) CANLI TEKERLEK SÜRÜŞÜ VE İTKİ FİZİĞİ (PiSimModelImporter lines 948-1020)
    if (bIsPhysicsSimulating)
    {
        TSet<UPrimitiveComponent*> DrivenWheels;

        for (const auto& Pair : MotorTestValues)
        {
            FName BoneName = Pair.Key;
            float TestVal = Pair.Value;
            if (FMath::Abs(TestVal) < 0.001f) continue;

            FString LowerName = BoneName.ToString().ToLower();
            bool bIsWheel = LowerName.Contains(TEXT("wheel")) || LowerName.Contains(TEXT("teker")) || LowerName.Contains(TEXT("caster"));
            bool bIsThruster = LowerName.Contains(TEXT("thrust")) || LowerName.Contains(TEXT("prop")) || LowerName.Contains(TEXT("rotor")) || LowerName.Contains(TEXT("motor"));

            if (bIsWheel)
            {
                UPrimitiveComponent* WheelPrim = GetPrimitiveComponentForBone(BoneName);
                if (WheelPrim && WheelPrim->IsSimulatingPhysics() && !DrivenWheels.Contains(WheelPrim))
                {
                    DrivenWheels.Add(WheelPrim);
                    float TotalRpm = TestVal * 1200.0f; // 1200 RPM
                    float AngularSpeedDegPerSec = TotalRpm * 6.0f; // 1 RPM = 6 deg/sec
                    FVector LocalAxle = FVector(1.0f, 0.0f, 0.0f); // Roll X axle
                    FVector WorldAxle = WheelPrim->GetComponentTransform().TransformVectorNoScale(LocalAxle).GetSafeNormal();

                    FVector CurrentAngVel = WheelPrim->GetPhysicsAngularVelocityInDegrees();
                    FVector NonRollVel = CurrentAngVel - (WorldAxle * (CurrentAngVel | WorldAxle));
                    FVector DesiredRollVel = WorldAxle * AngularSpeedDegPerSec;
                    WheelPrim->SetPhysicsAngularVelocityInDegrees(NonRollVel + DesiredRollVel, false);
                }
            }
            else if (bIsThruster)
            {
                float MaxThrustN = 25.0f;
                float ThrustNewtons = TestVal * MaxThrustN;
                FVector LocalThrustAxis = FVector(0.0f, 1.0f, 0.0f);
                int32 VisIdx = FindVisualSectionForBone(BoneName);
                if (VisIdx > 0 && StoredVisualSections.IsValidIndex(VisIdx) && !StoredVisualSections[VisIdx].BoneDirection.IsNearlyZero())
                {
                    LocalThrustAxis = StoredVisualSections[VisIdx].BoneDirection;
                }

                // Pervane görsel dönüşü
                if (USceneComponent* PropComp = GetSceneComponentForBone(BoneName))
                {
                    float PropRpm = TestVal * 3000.0f;
                    FQuat SpinQuat(LocalThrustAxis, FMath::DegreesToRadians(PropRpm * 6.0f * DeltaTime));
                    PropComp->AddLocalRotation(SpinQuat);
                }

                // Şasiye itki kuvveti uygula (1 Newton = 100 Unreal Force Units)
                if (VisualMeshComponents.IsValidIndex(0) && VisualMeshComponents[0])
                {
                    FVector WorldThrustAxis = VisualMeshComponents[0]->GetComponentTransform().TransformVectorNoScale(LocalThrustAxis).GetSafeNormal();
                    VisualMeshComponents[0]->AddForce(WorldThrustAxis * ThrustNewtons * 100.0f);
                }
            }
        }

        // Görsel Mesh'ler için ek kontrol (VisualMeshComponents[i], i > 0)
        for (int32 i = 1; i < VisualMeshComponents.Num(); ++i)
        {
            if (!VisualMeshComponents[i] || DrivenWheels.Contains(VisualMeshComponents[i]) || !StoredVisualSections.IsValidIndex(i)) continue;

            FName BoneName = FName(*StoredVisualSections[i].MeshName);
            float TestVal = MotorTestValues.FindRef(BoneName);
            if (FMath::Abs(TestVal) < 0.001f)
            {
                FString Clean = StoredVisualSections[i].MeshName;
                Clean.RemoveFromStart(TEXT("M_"), ESearchCase::IgnoreCase);
                TestVal = MotorTestValues.FindRef(FName(*Clean));
            }
            if (FMath::Abs(TestVal) < 0.001f) continue;

            FString LowerName = StoredVisualSections[i].MeshName.ToLower();
            bool bIsWheel = LowerName.Contains(TEXT("wheel")) || LowerName.Contains(TEXT("teker")) || LowerName.Contains(TEXT("caster"));
            if (bIsWheel && VisualMeshComponents[i]->IsSimulatingPhysics())
            {
                DrivenWheels.Add(VisualMeshComponents[i]);
                float TotalRpm = TestVal * 1200.0f;
                float AngularSpeedDegPerSec = TotalRpm * 6.0f;
                FVector LocalAxle = FVector(1.0f, 0.0f, 0.0f);
                FVector WorldAxle = VisualMeshComponents[i]->GetComponentTransform().TransformVectorNoScale(LocalAxle).GetSafeNormal();

                FVector CurrentAngVel = VisualMeshComponents[i]->GetPhysicsAngularVelocityInDegrees();
                FVector NonRollVel = CurrentAngVel - (WorldAxle * (CurrentAngVel | WorldAxle));
                FVector DesiredRollVel = WorldAxle * AngularSpeedDegPerSec;
                VisualMeshComponents[i]->SetPhysicsAngularVelocityInDegrees(NonRollVel + DesiredRollVel, false);
            }
        }
    }
    else
    {
        // STATİK GARAJ MODUNDA GÖRSEL DÖNÜŞ (Fizik kapalıyken)
        for (const auto& Pair : MotorTestValues)
        {
            FName BoneName = Pair.Key;
            float TestVal = Pair.Value;
            if (FMath::Abs(TestVal) < 0.001f) continue;

            FString LowerName = BoneName.ToString().ToLower();
            bool bIsWheel = LowerName.Contains(TEXT("wheel")) || LowerName.Contains(TEXT("teker")) || LowerName.Contains(TEXT("caster"));
            bool bIsThruster = LowerName.Contains(TEXT("thrust")) || LowerName.Contains(TEXT("prop")) || LowerName.Contains(TEXT("rotor")) || LowerName.Contains(TEXT("motor"));

            if (bIsWheel)
            {
                if (USceneComponent* Comp = GetSceneComponentForBone(BoneName))
                {
                    float TotalRpm = TestVal * 1200.0f;
                    Comp->AddLocalRotation(FRotator(TotalRpm * 0.1f * DeltaTime * 60.0f, 0.0f, 0.0f));
                }
            }
            else if (bIsThruster)
            {
                if (USceneComponent* Comp = GetSceneComponentForBone(BoneName))
                {
                    Comp->AddLocalRotation(FRotator(0.0f, TestVal * 3000.0f * 0.1f * DeltaTime * 60.0f, 0.0f));
                }
            }
        }
    }
}

void APiSimVehicle::AddOrbitDelta(float DeltaX, float DeltaY)
{
    if (!CameraSpringArm) return;
    FRotator ArmRot = CameraSpringArm->GetRelativeRotation();
    ArmRot.Yaw += DeltaX * 0.35f;
    ArmRot.Pitch = FMath::Clamp(ArmRot.Pitch - DeltaY * 0.35f, -80.0f, 80.0f);
    CameraSpringArm->SetRelativeRotation(ArmRot);
}

void APiSimVehicle::AddPanDelta(float DeltaX, float DeltaY)
{
    if (!CameraSpringArm) return;
    FVector ArmLoc = CameraSpringArm->GetRelativeLocation();
    ArmLoc.Y += DeltaX * 0.5f;
    ArmLoc.Z -= DeltaY * 0.5f;
    CameraSpringArm->SetRelativeLocation(ArmLoc);
}

void APiSimVehicle::AddZoomDelta(float Direction)
{
    if (!CameraSpringArm) return;
    CameraSpringArm->TargetArmLength = FMath::Clamp(CameraSpringArm->TargetArmLength + Direction * 45.0f, 30.0f, 2500.0f);
}

// @state: WIP - Görsel model ile UCX çarpışma zırhı arasındaki görünürlüğü değiştirir
void APiSimVehicle::SetCollisionMeshViewEnabled(bool bShowUCX)
{
    bShowCollisionView = bShowUCX;

    for (UProceduralMeshComponent* VisComp : VisualMeshComponents)
    {
        if (VisComp)
        {
            VisComp->SetVisibility(!bShowCollisionView);
            VisComp->SetHiddenInGame(bShowCollisionView);
        }
    }

    for (UProceduralMeshComponent* CollComp : CollisionMeshComponents)
    {
        if (CollComp)
        {
            CollComp->SetVisibility(bShowCollisionView);
            CollComp->SetHiddenInGame(!bShowCollisionView);
        }
    }

    if (VisualSkeletalMesh)
    {
        VisualSkeletalMesh->SetVisibility(!bShowCollisionView);
    }

    UE_LOG(LogTemp, Log, TEXT("[APiSimVehicle] Görünüm modu güncellendi: %s"), bShowCollisionView ? TEXT("🛡️ UCX ZIRH") : TEXT("🎨 GÖRSEL"));
}

// @state: WIP - Mevcut modeli ve alt parçaları temizler (Kök bileşen ve TypedElement emniyetli)
void APiSimVehicle::ClearVehicleModel()
{
    // 1. Kök bileşen emniyeti: Eğer RootComponent geçici bir parçaya atanmışsa, ana iskelete geri devret
    if (VehicleSkeletalMesh && GetRootComponent() != VehicleSkeletalMesh)
    {
        SetRootComponent(VehicleSkeletalMesh);
    }
    if (CameraSpringArm && VehicleSkeletalMesh)
    {
        CameraSpringArm->AttachToComponent(VehicleSkeletalMesh, FAttachmentTransformRules::KeepRelativeTransform);
    }

    // 2. Eklem kısıtlamalarını (Joint Constraints) güvenle yok et
    for (UPhysicsConstraintComponent* Constraint : JointConstraints)
    {
        if (Constraint && !Constraint->IsBeingDestroyed())
        {
            Constraint->DestroyComponent();
        }
    }
    JointConstraints.Empty();

    // 3. Görsel parçaları güvenle yok et
    for (UProceduralMeshComponent* VisComp : VisualMeshComponents)
    {
        if (VisComp && !VisComp->IsBeingDestroyed())
        {
            VisComp->DestroyComponent();
        }
    }
    VisualMeshComponents.Empty();

    // 4. UCX çarpışma zırhlarını güvenle yok et
    for (UProceduralMeshComponent* CollComp : CollisionMeshComponents)
    {
        if (CollComp && !CollComp->IsBeingDestroyed())
        {
            CollComp->DestroyComponent();
        }
    }
    CollisionMeshComponents.Empty();

    // 5. Dinamik kemik/mafsal bileşenlerini güvenle yok et
    for (auto& Pair : BoneSceneComponents)
    {
        if (Pair.Value && !Pair.Value->IsBeingDestroyed())
        {
            Pair.Value->DestroyComponent();
        }
    }
    BoneSceneComponents.Empty();
    VehicleBoneHierarchy.Empty();

    SensorSocketTransforms.Empty();
    VehicleBoneNames.Empty();
    StoredVisualSections.Empty();
    StoredUCXSections.Empty();
    MotorTestValues.Empty();
    SectionConstraintMap.Empty();
    bIsPhysicsSimulating = false;
}

// @state: WIP - İthal edilen kemik listesini kaydeder
void APiSimVehicle::RegisterImportedBones(const TArray<FName>& InBoneNames)
{
    VehicleBoneNames = InBoneNames;
}

// @state: WIP - Hiyerarşik kemik ağacını kaydeder
void APiSimVehicle::RegisterImportedBoneHierarchy(const TArray<FPiSimBoneHierarchyNode>& InHierarchy)
{
    VehicleBoneHierarchy = InHierarchy;
}

// @state: WIP - Kemik ile sahne bileşenini eşleştirir
void APiSimVehicle::RegisterBoneSceneComponent(FName BoneName, USceneComponent* SceneComp)
{
    if (!BoneName.IsNone() && SceneComp)
    {
        BoneSceneComponents.Add(BoneName, SceneComp);
    }
}

// @state: WIP - Kemiğe bağlı sahne bileşenini döner (SADECE Kemik Sahne Bileşenleri, Asla Görsel Mesh Değil!)
USceneComponent* APiSimVehicle::GetSceneComponentForBone(FName BoneName) const
{
    if (BoneName.IsNone()) return nullptr;

    // 1) Doğrudan hash map sorgusu (Birebir FName eşleşmesi)
    if (USceneComponent* Found = BoneSceneComponents.FindRef(BoneName))
    {
        return Found;
    }

    FString TargetStr = BoneName.ToString();

    // 2) Önek/Sonek ve ayraçları temizleyerek ara (Model::, RootNode|, M_, B_)
    FString CleanTarget = TargetStr;
    int32 SeparatorIdx;
    if (CleanTarget.FindLastChar(':', SeparatorIdx)) CleanTarget = CleanTarget.Mid(SeparatorIdx + 1);
    if (CleanTarget.FindLastChar('|', SeparatorIdx)) CleanTarget = CleanTarget.Mid(SeparatorIdx + 1);
    CleanTarget.RemoveFromStart(TEXT("M_"), ESearchCase::IgnoreCase);
    CleanTarget.RemoveFromStart(TEXT("B_"), ESearchCase::IgnoreCase);
    FString CleanTargetLower = CleanTarget.ToLower();

    // 3) Birebir temiz isim eşleşmesi (Exact Clean Match - Sadece BoneSceneComponents içinde)
    for (const auto& Pair : BoneSceneComponents)
    {
        if (!Pair.Value) continue;
        FString KeyStr = Pair.Key.ToString();
        FString KeyClean = KeyStr;
        if (KeyClean.FindLastChar(':', SeparatorIdx)) KeyClean = KeyClean.Mid(SeparatorIdx + 1);
        if (KeyClean.FindLastChar('|', SeparatorIdx)) KeyClean = KeyClean.Mid(SeparatorIdx + 1);
        KeyClean.RemoveFromStart(TEXT("M_"), ESearchCase::IgnoreCase);
        KeyClean.RemoveFromStart(TEXT("B_"), ESearchCase::IgnoreCase);
        FString KeyCleanLower = KeyClean.ToLower();

        if (KeyCleanLower.Equals(CleanTargetLower))
        {
            return Pair.Value;
        }
    }

    // 4) Alt dize / Token araması (Sadece BoneSceneComponents içinde)
    for (const auto& Pair : BoneSceneComponents)
    {
        if (!Pair.Value) continue;
        FString KeyLower = Pair.Key.ToString().ToLower();
        if (KeyLower.Contains(CleanTargetLower) || CleanTargetLower.Contains(KeyLower))
        {
            return Pair.Value;
        }
    }

    return nullptr;
}

// Kemiğe bağlı veya çocuğundaki fiziksel ilkel bileşeni döner (Dinamik tekerlek fizik itkisi için)
UPrimitiveComponent* APiSimVehicle::GetPrimitiveComponentForBone(FName BoneName) const
{
    if (BoneName.IsNone()) return nullptr;

    USceneComponent* SceneComp = GetSceneComponentForBone(BoneName);
    if (SceneComp)
    {
        if (UPrimitiveComponent* DirectPrim = Cast<UPrimitiveComponent>(SceneComp))
        {
            return DirectPrim;
        }
        for (int32 c = 0; c < SceneComp->GetNumChildrenComponents(); ++c)
        {
            if (UPrimitiveComponent* ChildPrim = Cast<UPrimitiveComponent>(SceneComp->GetChildComponent(c)))
            {
                return ChildPrim;
            }
        }
    }

    FString BoneStr = BoneName.ToString();
    FString Clean = BoneStr;
    Clean.RemoveFromStart(TEXT("M_"), ESearchCase::IgnoreCase);

    for (UProceduralMeshComponent* VisComp : VisualMeshComponents)
    {
        if (VisComp)
        {
            FString VName = VisComp->GetName();
            if (VName.Contains(BoneStr, ESearchCase::IgnoreCase) || VName.Contains(Clean, ESearchCase::IgnoreCase))
            {
                return VisComp;
            }
        }
    }

    return nullptr;
}

// @state: WIP - UI Slider'ından gelen motor test değerini uygular
void APiSimVehicle::SetMotorTestValue(FName BoneName, float Value)
{
    MotorTestValues.Add(BoneName, Value);

    UActorComponent* Mod = GetModuleAtBone(BoneName);
    FString ModStatus = TEXT("🔘 Modül Atanmamış");
    if (Mod)
    {
        if (UPiSimWheelComponent* Wheel = Cast<UPiSimWheelComponent>(Mod))
        {
            Wheel->bManualOverride = (FMath::Abs(Value) > 0.01f);
            Wheel->SetTargetRPM(Value * 1200.0f);
            ModStatus = FString::Printf(TEXT("🚗 Tekerlek (Hedef RPM: %.1f)"), Value * 1200.0f);
        }
        else if (UPiSimServoComponent* Servo = Cast<UPiSimServoComponent>(Mod))
        {
            Servo->bManualOverride = (FMath::Abs(Value) > 0.01f);
            Servo->SetTargetAngle(Value * 45.0f);
            ModStatus = FString::Printf(TEXT("🦾 Servo (Hedef Açı: %+.1f°)"), Value * 45.0f);
        }
        else if (UPiSimPropellerComponent* Prop = Cast<UPiSimPropellerComponent>(Mod))
        {
            Prop->bManualOverride = (FMath::Abs(Value) > 0.01f);
            Prop->SetThrottle(FMath::Max(0.0f, Value));
            ModStatus = FString::Printf(TEXT("🛸 Pervane (Gaz: %.1f%%)"), Value * 100.0f);
        }
    }

    USceneComponent* TargetComp = GetSceneComponentForBone(BoneName);
    FString CompName = TargetComp ? TargetComp->GetName() : TEXT("❌ Sahne Bileşeni Yok");

    // Steer / Servo mafsalı veya Tekerlek rotasyonu uygula
    FRotator PrevRot = TargetComp ? TargetComp->GetRelativeRotation() : FRotator::ZeroRotator;
    float TargetAngleDeg = 0.0f;
    bool bRotated = false;

    FString LowerName = BoneName.ToString().ToLower();
    bool bIsSteer = LowerName.Contains(TEXT("steer")) || LowerName.Contains(TEXT("servo")) ||
                    LowerName.Contains(TEXT("joint")) || LowerName.Contains(TEXT("arm")) ||
                    LowerName.Contains(TEXT("elevon")) || LowerName.Contains(TEXT("aileron")) ||
                    LowerName.Contains(TEXT("rudder")) || LowerName.Contains(TEXT("flap")) ||
                    (Mod && Mod->IsA(UPiSimServoComponent::StaticClass()));
    bool bIsWheel = LowerName.Contains(TEXT("wheel")) || LowerName.Contains(TEXT("teker"));

    if (bIsSteer)
    {
        TargetAngleDeg = Value * 45.0f;
        if (TargetComp)
        {
            TargetComp->SetRelativeRotation(FRotator(0.0f, TargetAngleDeg, 0.0f));
            bRotated = true;
        }

        // İlgili tekerlek mafsalı kısıtlamasını yönlendir
        if (bIsPhysicsSimulating)
        {
            UPhysicsConstraintComponent* Constraint = FindConstraintForBone(BoneName);
            if (!Constraint)
            {
                int32 TargetSection = FindVisualSectionForBone(BoneName);
                Constraint = FindConstraintForSection(TargetSection);
            }
            if (Constraint)
            {
                FTransform SteerPose(FRotator(0.0f, TargetAngleDeg, 0.0f), Constraint->GetRelativeLocation());
                Constraint->SetConstraintReferenceFrame(EConstraintFrame::Frame1, SteerPose);
            }
        }
    }
    else if (bIsWheel)
    {
        float IndividualRpm = Value * 1200.0f;
        if (bIsPhysicsSimulating)
        {
            UPrimitiveComponent* PrimComp = GetPrimitiveComponentForBone(BoneName);
            if (PrimComp && PrimComp->IsSimulatingPhysics())
            {
                float AngularSpeedDegPerSec = IndividualRpm * 6.0f;
                FVector LocalAxle = FVector(1.0f, 0.0f, 0.0f);
                FVector WorldAxle = PrimComp->GetComponentTransform().TransformVectorNoScale(LocalAxle).GetSafeNormal();
                FVector CurrentAngVel = PrimComp->GetPhysicsAngularVelocityInDegrees();
                FVector NonRollVel = CurrentAngVel - (WorldAxle * (CurrentAngVel | WorldAxle));
                FVector DesiredRollVel = WorldAxle * AngularSpeedDegPerSec;
                PrimComp->SetPhysicsAngularVelocityInDegrees(NonRollVel + DesiredRollVel, false);
                bRotated = true;
            }
        }
        else if (TargetComp)
        {
            TargetComp->AddLocalRotation(FRotator(IndividualRpm * 0.1f, 0.0f, 0.0f));
            bRotated = true;
        }
    }

    // =========================================================================================
    // 🔍 KULLANICI TALEBİ: KEMİK ATAMA VE İSKELET DÖNÜŞ KAPSAMLI DEBUG BİLGİSİ
    // =========================================================================
    FString ParentName = (TargetComp && TargetComp->GetAttachParent()) ? TargetComp->GetAttachParent()->GetName() : TEXT("❌ YOK (Bağsız/Kök)");
    int32 ChildCount = TargetComp ? TargetComp->GetNumChildrenComponents() : 0;
    FString ChildNames = TEXT("");
    if (TargetComp)
    {
        for (int32 c = 0; c < FMath::Min(ChildCount, 3); ++c)
        {
            USceneComponent* Child = TargetComp->GetChildComponent(c);
            if (Child) ChildNames += (c > 0 ? TEXT(", ") : TEXT("")) + Child->GetName();
        }
        if (ChildCount > 3) ChildNames += TEXT(", ...");
    }

    FString SkelStatus = TEXT("🏛️ Mimari: Dinamik Sahne Bileşeni Hiyerarşisi (FBX Prosedürel İskelet)");

    FRotator CompRelRot = TargetComp ? TargetComp->GetRelativeRotation() : FRotator::ZeroRotator;
    FRotator CompWorldRot = TargetComp ? TargetComp->GetComponentRotation() : FRotator::ZeroRotator;

    // 1) Canlı Ekrana Debug Mesajları (Sadece bShowDebugVisualsAndMessages aktifse)
    if (bShowDebugVisualsAndMessages && GEngine)
    {
        // Temel Test Logu
        GEngine->AddOnScreenDebugMessage(923, 3.0f, TargetComp ? FColor::Green : FColor::Red,
            FString::Printf(TEXT("🚀 [MOTOR TESTİ] Kemik: '%s' | Değer: %+.2f (%+.1f%%) | %s | Bileşen: %s"),
                *BoneName.ToString(), Value, Value * 100.0f, *ModStatus, *CompName));

        // [DEBUG 1]: İSKELET VE HİYERARŞİ ATAMA DEBUG'I
        GEngine->AddOnScreenDebugMessage(924, 3.0f, (TargetComp && TargetComp->GetAttachParent()) ? FColor::Cyan : FColor::Orange,
            FString::Printf(TEXT("🔗 [KEMİK ATAMA DEBUG] Kemik: '%s' | Ebeveyn: '%s' | Çocuk Sayısı: %d (%s) | %s"),
                *BoneName.ToString(), *ParentName, ChildCount, *ChildNames, *SkelStatus));

        // [DEBUG 2]: KEMİK DÖNÜŞ VE AÇI DEBUG'I
        GEngine->AddOnScreenDebugMessage(925, 3.0f, bRotated ? FColor::Yellow : FColor::White,
            FString::Printf(TEXT("🔄 [KEMİK DÖNÜŞ DEBUG] Hedef Açı: %+.1f° | Bağıl: (P=%.1f, Y=%.1f, R=%.1f) | Dünya: (P=%.1f, Y=%.1f, R=%.1f) | Döndü mü: %s"),
                TargetAngleDeg, CompRelRot.Pitch, CompRelRot.Yaw, CompRelRot.Roll,
                CompWorldRot.Pitch, CompWorldRot.Yaw, CompWorldRot.Roll,
                bRotated ? TEXT("✅ EVET") : TEXT("⏸️ Statik/RPM")));
    }

    UE_LOG(LogTemp, Warning, TEXT("🚀 [MOTOR TESTİ] Kemik: '%s' | Değer: %+.2f | %s | Bileşen: %s"),
        *BoneName.ToString(), Value, *ModStatus, *CompName);
    UE_LOG(LogTemp, Warning, TEXT("🔗 [KEMİK ATAMA DEBUG] Kemik: '%s' -> Ebeveyn: '%s' | Çocuk: %d (%s) | %s"),
        *BoneName.ToString(), *ParentName, ChildCount, *ChildNames, *SkelStatus);
    UE_LOG(LogTemp, Warning, TEXT("🔄 [KEMİK DÖNÜŞ DEBUG] Kemik: '%s' -> Hedef: %+.1f° | Bağıl: (P=%.1f, Y=%.1f, R=%.1f) | Dünya: (P=%.1f, Y=%.1f, R=%.1f) | Döndü: %s"),
        *BoneName.ToString(), TargetAngleDeg, CompRelRot.Pitch, CompRelRot.Yaw, CompRelRot.Roll,
        CompWorldRot.Pitch, CompWorldRot.Yaw, CompWorldRot.Roll, bRotated ? TEXT("EVET") : TEXT("STATIK/RPM"));
}

float APiSimVehicle::GetMotorTestValue(FName BoneName) const
{
    return MotorTestValues.FindRef(BoneName);
}

void APiSimVehicle::ResetAllMotorTestValues()
{
    MotorTestValues.Empty();
    for (auto& Pair : BoneSceneComponents)
    {
        if (Pair.Value && Pair.Key.ToString().Contains(TEXT("steer"), ESearchCase::IgnoreCase))
        {
            Pair.Value->SetRelativeRotation(FRotator::ZeroRotator);
        }
    }
    for (auto& Pair : BoneSlotRegistry)
    {
        if (Pair.Value)
        {
            if (UPiSimWheelComponent* Wheel = Cast<UPiSimWheelComponent>(Pair.Value))
            {
                Wheel->SetTargetRPM(0.0f);
                Wheel->bManualOverride = false;
            }
            else if (UPiSimServoComponent* Servo = Cast<UPiSimServoComponent>(Pair.Value))
            {
                Servo->SetTargetAngle(0.0f);
                Servo->bManualOverride = false;
            }
            else if (UPiSimPropellerComponent* Prop = Cast<UPiSimPropellerComponent>(Pair.Value))
            {
                Prop->SetThrottle(0.0f);
                Prop->bManualOverride = false;
            }
        }
    }
}

// @state: WIP - Canlı Chaos Multi-Body Gazebo Simülasyonunu Açıp Kapatır (22e300b pattern)
// UCX Convex Hull zırhını parçalara doğrudan giydirir ve tekerlekleri 1-DOF Roll kısıtlamasıyla şasiye bağlar!
void APiSimVehicle::SetVehiclePhysicsEnabled(bool bEnable)
{
    bIsPhysicsSimulating = bEnable;

    if (!bEnable)
    {
        // -------------------------------------------------------------------------------------
        // FİZİĞİ DURDUR (Statik Garaj Moduna Dön)
        // -------------------------------------------------------------------------------------
        for (UPhysicsConstraintComponent* Constraint : JointConstraints)
        {
            if (Constraint && !Constraint->IsBeingDestroyed())
            {
                Constraint->DestroyComponent();
            }
        }
        JointConstraints.Empty();
        SectionConstraintMap.Empty();
        BoneConstraintMap.Empty();

        for (int32 i = 0; i < VisualMeshComponents.Num(); ++i)
        {
            if (VisualMeshComponents[i])
            {
                VisualMeshComponents[i]->SetSimulatePhysics(false);
                VisualMeshComponents[i]->SetEnableGravity(false);
            }
        }

        for (UProceduralMeshComponent* CollMesh : CollisionMeshComponents)
        {
            if (CollMesh)
            {
                CollMesh->SetSimulatePhysics(false);
                CollMesh->SetEnableGravity(false);
                CollMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            }
        }

        // Hiyerarşiyi statik konuma sıfırla (PiSimModelImporter line 3473 mantığı)
        if (VehicleSkeletalMesh)
        {
            SetRootComponent(VehicleSkeletalMesh);
        }

        if (VisualMeshComponents.IsValidIndex(0) && VisualMeshComponents[0])
        {
            VisualMeshComponents[0]->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
            VisualMeshComponents[0]->SetRelativeLocation(FVector::ZeroVector);
            VisualMeshComponents[0]->SetRelativeRotation(FRotator::ZeroRotator);
        }

        // Steer mafsallarını şasiye ve doğru pivot ofsetine geri tak
        for (const auto& Pair : WheelToSteerBoneMap)
        {
            int32 WheelIdx = Pair.Key;
            FName SteerBoneName = FName(*Pair.Value);
            if (USceneComponent* SteerComp = GetSceneComponentForBone(SteerBoneName))
            {
                if (VisualMeshComponents.IsValidIndex(0) && VisualMeshComponents[0] && StoredVisualSections.IsValidIndex(WheelIdx))
                {
                    SteerComp->AttachToComponent(VisualMeshComponents[0], FAttachmentTransformRules::SnapToTargetNotIncludingScale);
                    SteerComp->SetRelativeLocation(StoredVisualSections[WheelIdx].PivotPoint - StoredVisualSections[0].PivotPoint);
                    SteerComp->SetRelativeRotation(FRotator::ZeroRotator);
                }
            }
        }

        for (int32 i = 1; i < VisualMeshComponents.Num(); ++i)
        {
            if (!VisualMeshComponents[i] || !StoredVisualSections.IsValidIndex(i)) continue;

            if (FString* SteerBoneName = WheelToSteerBoneMap.Find(i))
            {
                if (USceneComponent* SteerComp = GetSceneComponentForBone(FName(**SteerBoneName)))
                {
                    VisualMeshComponents[i]->AttachToComponent(SteerComp, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
                    VisualMeshComponents[i]->SetRelativeLocation(FVector::ZeroVector);
                    VisualMeshComponents[i]->SetRelativeRotation(FRotator::ZeroRotator);
                }
            }
            else if (VisualMeshComponents.IsValidIndex(0) && VisualMeshComponents[0])
            {
                VisualMeshComponents[i]->AttachToComponent(VisualMeshComponents[0], FAttachmentTransformRules::SnapToTargetNotIncludingScale);
                VisualMeshComponents[i]->SetRelativeLocation(StoredVisualSections[i].PivotPoint - StoredVisualSections[0].PivotPoint);
                VisualMeshComponents[i]->SetRelativeRotation(FRotator::ZeroRotator);
            }
        }

        if (CameraSpringArm && VisualMeshComponents.IsValidIndex(0) && VisualMeshComponents[0])
        {
            CameraSpringArm->AttachToComponent(VisualMeshComponents[0], FAttachmentTransformRules::SnapToTargetNotIncludingScale);
            CameraSpringArm->SetRelativeLocation(FVector(0.0f, 0.0f, 60.0f));
            CameraSpringArm->TargetArmLength = 350.0f;
        }

        if (bShowDebugVisualsAndMessages && GEngine)
        {
            GEngine->AddOnScreenDebugMessage(801, 8.0f, FColor::Orange, TEXT("🛑 >>> [FİZİK SİMÜLASYONU DURDURULDU] Robot Statik Moda Alındı <<<"));
        }
        UE_LOG(LogTemp, Warning, TEXT("[APiSimVehicle LOG] Fizik simülasyonu durduruldu."));
        return;
    }

    // -----------------------------------------------------------------------------------------
    // FİZİĞİ AKTİFLEŞTİR (Canlı Chaos Multi-Body Gazebo Simülasyonu - PiSimModelImporter lines 3485-3675)
    // -----------------------------------------------------------------------------------------
    UE_LOG(LogTemp, Warning, TEXT("===================================================================="));
    UE_LOG(LogTemp, Warning, TEXT("[APiSimVehicle LOG] >>> CANLI CHAOS FİZİK SİMÜLASYONU BAŞLATILIYOR <<<"));
    UE_LOG(LogTemp, Warning, TEXT("===================================================================="));

    // 1) Ana Gövdeyi Kök Bileşen Yap
    if (VisualMeshComponents.IsValidIndex(0) && VisualMeshComponents[0])
    {
        VisualMeshComponents[0]->SetMobility(EComponentMobility::Movable);
        VisualMeshComponents[0]->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
        SetRootComponent(VisualMeshComponents[0]);
    }
    if (CameraSpringArm && VisualMeshComponents.IsValidIndex(0) && VisualMeshComponents[0])
    {
        CameraSpringArm->AttachToComponent(VisualMeshComponents[0], FAttachmentTransformRules::KeepWorldTransform);
    }

    // 2) Steer Kemiklerini (Kinematik Mafsalları) Kontrol Et & Doğrula
    for (auto& Pair : BoneSceneComponents)
    {
        if (Pair.Value && Pair.Key.ToString().Contains(TEXT("steer"), ESearchCase::IgnoreCase))
        {
            Pair.Value->SetRelativeRotation(FRotator::ZeroRotator);
            UE_LOG(LogTemp, Warning, TEXT("🎯 [STEER KEMİĞİ HAZIR] '%s' Şasiye bağlı 1-Eksen (Yaw) Kinematik Mafsal! (Fizik Simülasyonu: KAPALI)"), *Pair.Key.ToString());
        }
    }

    // 3) Her Parçaya (Gövde ve Tüm Tekerleklere) Kütle, Zırh ve Dinamik Fizik Ver
    for (int32 i = 0; i < VisualMeshComponents.Num(); ++i)
    {
        UProceduralMeshComponent* VisComp = VisualMeshComponents[i];
        if (!VisComp || !StoredVisualSections.IsValidIndex(i)) continue;

        FString LowerName = StoredVisualSections[i].MeshName.ToLower();
        bool bIsWheel = (LowerName.Contains(TEXT("wheel")) || LowerName.Contains(TEXT("teker")) || LowerName.StartsWith(TEXT("w_")));
        bool bIsThruster = (LowerName.Contains(TEXT("thrust")) || LowerName.Contains(TEXT("prop")) || LowerName.Contains(TEXT("rotor")) || LowerName.Contains(TEXT("motor")));
        bool bIsAeroSurface = (LowerName.Contains(TEXT("elevon")) || LowerName.Contains(TEXT("aileron")) || LowerName.Contains(TEXT("flap")) || LowerName.Contains(TEXT("rudder")));

        bool bIsKinematic = (i > 0) && (bIsThruster || bIsAeroSurface || (!bIsWheel));
        if (bIsKinematic)
        {
            VisComp->SetSimulatePhysics(false);
            VisComp->SetEnableGravity(false);
            VisComp->SetMobility(EComponentMobility::Movable);
            if (VisualMeshComponents.IsValidIndex(0) && VisualMeshComponents[0])
            {
                VisComp->AttachToComponent(VisualMeshComponents[0], FAttachmentTransformRules::KeepWorldTransform);
            }
            UE_LOG(LogTemp, Warning, TEXT("🔒 [KİNEMATİK] [%d] '%s' → şasiye kinematik bağlandı"), i, *StoredVisualSections[i].MeshName);
            continue;
        }

        float Mass = 2.0f;
        if (i == 0)
        {
            Mass = FMath::Max(1.0f, TotalMassKg);
        }

        // İlgili UCX Convex Hull'unu aktar (UCX yoksa görsel köşe noktalarını kullan)
        VisComp->ClearCollisionConvexMeshes();
        bool bHasCollision = false;

        for (const FPiSimMeshSection& UcxSec : StoredUCXSections)
        {
            if (UcxSec.MeshName.Contains(StoredVisualSections[i].MeshName, ESearchCase::IgnoreCase))
            {
                VisComp->AddCollisionConvexMesh(UcxSec.Vertices);
                bHasCollision = true;
            }
        }

        // Kök gövde (i == 0) ise diğer parçalara atanmamış UCX'leri ofsetleyerek ekle
        if (i == 0 && StoredUCXSections.Num() > 0)
        {
            for (const FPiSimMeshSection& UcxSec : StoredUCXSections)
            {
                bool bBelongsToOtherVisual = false;
                for (int32 other = 1; other < StoredVisualSections.Num(); ++other)
                {
                    if (UcxSec.MeshName.Contains(StoredVisualSections[other].MeshName, ESearchCase::IgnoreCase))
                    {
                        bBelongsToOtherVisual = true;
                        break;
                    }
                }

                FString LowerUcx = UcxSec.MeshName.ToLower();
                if (LowerUcx.Contains(TEXT("wheel")) || LowerUcx.Contains(TEXT("teker")) || LowerUcx.Contains(TEXT("steer")))
                {
                    bBelongsToOtherVisual = true;
                }

                if (!bBelongsToOtherVisual)
                {
                    FVector Offset = UcxSec.PivotPoint - StoredVisualSections[0].PivotPoint;
                    TArray<FVector> OffsetVerts = UcxSec.Vertices;
                    for (FVector& V : OffsetVerts)
                    {
                        V += Offset;
                    }
                    VisComp->AddCollisionConvexMesh(OffsetVerts);
                    bHasCollision = true;
                    UE_LOG(LogTemp, Warning, TEXT("🛡️ [UCX ZIRHI ŞASİYE MONTE EDİLDİ] '%s' şasiye eklendi (Ofset: %s, Verts: %d)"),
                        *UcxSec.MeshName, *Offset.ToString(), OffsetVerts.Num());
                }
            }
        }

        // Güvenlik sigortası: UCX yoksa doğrudan görsel köşe noktalarını convex hull yap
        if (!bHasCollision)
        {
            VisComp->AddCollisionConvexMesh(StoredVisualSections[i].Vertices);
            bHasCollision = true;
        }

        if (bHasCollision)
        {
            VisComp->bUseComplexAsSimpleCollision = false;
            VisComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            VisComp->SetCollisionObjectType(ECC_WorldDynamic);
            VisComp->SetCollisionResponseToAllChannels(ECR_Block);
            VisComp->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block); // Zeminle çarpış
            VisComp->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
            VisComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
        }
        else
        {
            VisComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }
        VisComp->RecreatePhysicsState();

        // Dinamik Fiziği Aç
        VisComp->SetMobility(EComponentMobility::Movable);
        VisComp->SetSimulatePhysics(true);
        VisComp->SetEnableGravity(true);
        VisComp->SetMassOverrideInKg(NAME_None, Mass, true);
        VisComp->SetLinearDamping(0.8f);
        VisComp->SetAngularDamping(1.5f);
        VisComp->WakeRigidBody();

        UE_LOG(LogTemp, Warning, TEXT("[FİZİK LOG] '%s' bileşenine CANLI DİNAMİK FİZİK verildi | Kütle: %.1f kg | Yerçekimi: AÇIK"),
            *StoredVisualSections[i].MeshName, Mass);

        // 4) SADECE Tekerlekler için şasi ile fiziksel eklem (Constraint) bağla (PiSimModelImporter lines 3639-3670)
        if (i > 0 && bIsWheel && VisualMeshComponents.IsValidIndex(0) && VisualMeshComponents[0])
        {
            FName ConstraintName = MakeUniqueObjectName(this, UPhysicsConstraintComponent::StaticClass(), FName(*FString::Printf(TEXT("PhysicsJoint_%d_%s"), i, *StoredVisualSections[i].MeshName)));
            UPhysicsConstraintComponent* Constraint = NewObject<UPhysicsConstraintComponent>(this, ConstraintName);

            Constraint->SetupAttachment(VisualMeshComponents[0]);
            FVector WheelRelLoc = StoredVisualSections[i].PivotPoint - StoredVisualSections[0].PivotPoint;
            Constraint->SetRelativeLocation(WheelRelLoc);

            // Tüm tekerlekler için 1-DOF Roll kuralı:
            ConfigureConstraintDof(Constraint, i);
            Constraint->RegisterComponent();
            Constraint->SetConstrainedComponents(VisualMeshComponents[0], NAME_None, VisComp, NAME_None);

            if (FString* SteerBoneName = WheelToSteerBoneMap.Find(i))
            {
                FTransform InitialSteerPose(FRotator::ZeroRotator, WheelRelLoc);
                Constraint->SetConstraintReferenceFrame(EConstraintFrame::Frame1, InitialSteerPose);
                BoneConstraintMap.Add(FName(**SteerBoneName), Constraint);
                UE_LOG(LogTemp, Warning, TEXT("🔗 [DİREKSİYON BAĞLANTISI] '%s' tekerleği '%s' steer mafsalına kilitlendi! (1-DOF Roll)"),
                    *StoredVisualSections[i].MeshName, **SteerBoneName);
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("🔗 [STANDART SÜRÜŞ TEKERLEĞİ] '%s' tekerleği doğrudan şasiye bağlandı. (1-DOF Roll)"),
                    *StoredVisualSections[i].MeshName);
            }

            BoneConstraintMap.Add(FName(*StoredVisualSections[i].MeshName), Constraint);
            JointConstraints.Add(Constraint);
            SectionConstraintMap.Add(i, Constraint);
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

    // Ekran Bildirimleri (Sadece bShowDebugVisualsAndMessages aktifse)
    if (bShowDebugVisualsAndMessages && GEngine)
    {
        GEngine->AddOnScreenDebugMessage(801, 10.0f, FColor::Cyan,
            FString::Printf(TEXT("🚀 >>> [CHAOS FİZİK AKTİF!] %d Parça Canlı Simüle Ediliyor <<<"), VisualMeshComponents.Num()));
        
        GEngine->AddOnScreenDebugMessage(802, 10.0f, FColor::Emerald,
            FString::Printf(TEXT("🛡️ >>> [GÖVDE: %.1f KG | YERÇEKİMİ: AÇIK] Zemin Çarpışması Aktif <<<"), TotalMassKg));

        GEngine->AddOnScreenDebugMessage(803, 10.0f, FColor::Yellow,
            FString::Printf(TEXT("⚙️ >>> [%d ADET EKLEM KISITLAMASI BAĞLANDI] Tekerlekler Serbest Dönüyor! <<<"), JointConstraints.Num()));
    }
}

// =========================================================================================
// 🔒 [DO NOT MODIFY - WHEEL CONSTRAINT 1-DOF DEGREE OF FREEDOM LOCK]
// 🛑 KESİNLİKLE DEĞİŞTİRİLEMEZ: TEKERLEK SERBESTLİK DERECESİ (DOF) AYARLARI!
// Tekerlekler 3-DOF Linear Locked + Angular Twist (Roll) Free + Swing1/2 Locked standardındadır.
// Bu ayarlar aracın zeminde düzgün yuvarlanması, patlamaması ve devrilmemesi için sabittir!
// (PiSimModelImporter lines 3766-3786 Birebir Entegrasyon)
// =========================================================================================
void APiSimVehicle::ConfigureConstraintDof(UPhysicsConstraintComponent* Constraint, int32 SectionIndex)
{
    if (!Constraint) return;

    // 1) Doğrusal Hareket (Linear Motion) - Tekerlek şasiden kopamaz
    Constraint->SetLinearXLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);
    Constraint->SetLinearYLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);
    Constraint->SetLinearZLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);
    Constraint->SetDisableCollision(true); // Şasi ile tekerlek birbirini itmesin

    // 2) Açısal Hareket (Angular Motion) - GENEL GEÇER TEKERLEK STANDARDI:
    // TÜM TEKERLEKLER (Ön veya Arka ayrımı olmaksızın) TEK BİR EKSENDE (Roll / Twist) DÖNER!
    // Twist (Roll / Yuvarlanma): ACM_Free (Tekerlek zeminde ileri/geri serbestçe döner)
    // Swing1 (Yaw / Direksiyon): ACM_Locked (Direksiyon ekseni kilitli - Steer mafsalının referans açısıyla yönlendirilir)
    // Swing2 (Pitch / Kamber): ACM_Locked (Kamber açısı kilitli - devrilmez)
    Constraint->SetAngularTwistLimit(EAngularConstraintMotion::ACM_Free, 0.0f);
    Constraint->SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Locked, 0.0f);
    Constraint->SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Locked, 0.0f);
    Constraint->SetOrientationDriveTwistAndSwing(false, false);
}
// 🔒 [END OF WHEEL CONSTRAINT DOF LOCK]
// =========================================================================================

void APiSimVehicle::UpdateAllConstraintDofs()
{
    for (int32 i = 1; i < StoredVisualSections.Num(); ++i)
    {
        UPhysicsConstraintComponent* Constraint = FindConstraintForSection(i);
        if (Constraint)
        {
            ConfigureConstraintDof(Constraint, i);
        }
    }
}

UPhysicsConstraintComponent* APiSimVehicle::FindConstraintForSection(int32 SectionIndex)
{
    if (SectionIndex <= 0 || !StoredVisualSections.IsValidIndex(SectionIndex)) return nullptr;

    if (UPhysicsConstraintComponent** Found = SectionConstraintMap.Find(SectionIndex))
    {
        if (Found && *Found) return *Found;
    }

    FString TargetPrefix = FString::Printf(TEXT("PhysicsJoint_%d_"), SectionIndex);
    for (UPhysicsConstraintComponent* Constraint : JointConstraints)
    {
        if (Constraint && Constraint->GetName().StartsWith(TargetPrefix))
        {
            return Constraint;
        }
    }

    return nullptr;
}

UPhysicsConstraintComponent* APiSimVehicle::FindConstraintForBone(FName BoneName)
{
    if (BoneName.IsNone()) return nullptr;

    // 1) Doğrudan haritadan ara
    if (UPhysicsConstraintComponent** Found = BoneConstraintMap.Find(BoneName))
    {
        if (Found && *Found && !(*Found)->IsBeingDestroyed()) return *Found;
    }

    FString BoneStr = BoneName.ToString();

    // 2) Büyük/küçük harf duyarsız haritadan ara
    for (const auto& Pair : BoneConstraintMap)
    {
        if (Pair.Value && !Pair.Value->IsBeingDestroyed())
        {
            if (Pair.Key.ToString().Equals(BoneStr, ESearchCase::IgnoreCase))
            {
                return Pair.Value;
            }
        }
    }

    // 3) Steer <-> Wheel eşleşmesi (Örn: M_Steer_FR -> M_Wheel_FR kısıtlaması)
    FString Clean = BoneStr;
    Clean.RemoveFromStart(TEXT("M_Steer_"), ESearchCase::IgnoreCase);
    Clean.RemoveFromStart(TEXT("Steer_"), ESearchCase::IgnoreCase);
    Clean.RemoveFromStart(TEXT("M_"), ESearchCase::IgnoreCase);
    Clean.RemoveFromStart(TEXT("B_"), ESearchCase::IgnoreCase);

    for (const auto& Pair : BoneConstraintMap)
    {
        if (Pair.Value && !Pair.Value->IsBeingDestroyed())
        {
            FString KeyStr = Pair.Key.ToString();
            if (KeyStr.Contains(Clean, ESearchCase::IgnoreCase))
            {
                return Pair.Value;
            }
        }
    }

    // 4) JointConstraints isim araması
    for (UPhysicsConstraintComponent* Constraint : JointConstraints)
    {
        if (Constraint && !Constraint->IsBeingDestroyed())
        {
            FString CName = Constraint->GetName();
            if (CName.Contains(BoneStr, ESearchCase::IgnoreCase) || (Clean.Len() > 1 && CName.Contains(Clean, ESearchCase::IgnoreCase)))
            {
                return Constraint;
            }
        }
    }

    // 5) Section üzerinden ara
    int32 SecIdx = FindVisualSectionForBone(BoneName);
    if (SecIdx > 0)
    {
        return FindConstraintForSection(SecIdx);
    }

    return nullptr;
}

int32 APiSimVehicle::FindVisualSectionForBone(FName BoneName) const
{
    if (BoneName.IsNone()) return INDEX_NONE;
    FString BoneStr = BoneName.ToString();

    // 1) Doğrudan isim eşleşmesi
    for (int32 i = 0; i < StoredVisualSections.Num(); ++i)
    {
        if (StoredVisualSections[i].MeshName.Equals(BoneStr, ESearchCase::IgnoreCase))
        {
            return i;
        }
    }

    // 2) WheelToSteerBoneMap üzerinden ara (M_Steer_FR -> WheelIdx)
    for (const auto& Pair : WheelToSteerBoneMap)
    {
        if (Pair.Value.Equals(BoneStr, ESearchCase::IgnoreCase) ||
            BoneStr.Contains(Pair.Value, ESearchCase::IgnoreCase) ||
            Pair.Value.Contains(BoneStr, ESearchCase::IgnoreCase))
        {
            return Pair.Key;
        }
    }

    // 3) Temiz token eşleştirmesi (FR, FL, RR, RL)
    FString Clean = BoneStr;
    Clean.RemoveFromStart(TEXT("M_Steer_"), ESearchCase::IgnoreCase);
    Clean.RemoveFromStart(TEXT("Steer_"), ESearchCase::IgnoreCase);
    Clean.RemoveFromStart(TEXT("M_"), ESearchCase::IgnoreCase);
    Clean.RemoveFromStart(TEXT("B_"), ESearchCase::IgnoreCase);

    for (int32 i = 1; i < StoredVisualSections.Num(); ++i)
    {
        FString VName = StoredVisualSections[i].MeshName;
        if (VName.Contains(Clean, ESearchCase::IgnoreCase) &&
            (VName.Contains(TEXT("Wheel"), ESearchCase::IgnoreCase) || VName.StartsWith(TEXT("W_"))))
        {
            return i;
        }
    }

    for (int32 i = 1; i < StoredVisualSections.Num(); ++i)
    {
        if (StoredVisualSections[i].MeshName.Contains(Clean, ESearchCase::IgnoreCase))
        {
            return i;
        }
    }

    return INDEX_NONE;
}

// @state: WIP - Fiziğin aktif olup olmadığını döner
bool APiSimVehicle::IsVehiclePhysicsActive() const
{
    return bIsPhysicsSimulating;
}

// @state: WIP - Belirtilen iskelet kemiğine mikro-modül (Wheel, Servo vb.) atar
bool APiSimVehicle::AssignModuleToBone(FName BoneName, UActorComponent* ModuleComponent)
{
    if (!ModuleComponent || BoneName.IsNone())
    {
        return false;
    }

    if (BoneSlotRegistry.Contains(BoneName))
    {
        UActorComponent* OldComp = BoneSlotRegistry[BoneName];
        if (OldComp && OldComp != ModuleComponent)
        {
            OldComp->DestroyComponent();
        }
    }

    BoneSlotRegistry.Add(BoneName, ModuleComponent);
    return true;
}

// @state: WIP - Kemiğe atanmış mikro modülü döndürür
UActorComponent* APiSimVehicle::GetModuleAtBone(FName BoneName) const
{
    return BoneSlotRegistry.FindRef(BoneName);
}

// @state: WIP - UI Otopark için iskeletteki tüm kemik isimlerini toplar
void APiSimVehicle::GetVehicleBoneNames(TArray<FName>& OutBoneNames) const
{
    OutBoneNames.Empty();

    if (VehicleBoneNames.Num() > 0)
    {
        OutBoneNames = VehicleBoneNames;
        return;
    }

    if (VehicleSkeletalMesh && VehicleSkeletalMesh->GetSkeletalMeshAsset())
    {
        int32 NumBones = VehicleSkeletalMesh->GetNumBones();
        for (int32 i = 0; i < NumBones; ++i)
        {
            OutBoneNames.Add(VehicleSkeletalMesh->GetBoneName(i));
        }
    }
}

// @state: WIP - Telemetri abonesi ekler
void APiSimVehicle::AddTelemetrySubscriber(const FString& InIP, int32 InPort)
{
    FPiSimClientEndpoint NewEp;
    NewEp.IPAddress = InIP;
    NewEp.Port = InPort;
    NewEp.LastActiveTime = (GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.0;

    Subscriptions.TelemetrySubscribers.RemoveAll([&](const FPiSimClientEndpoint& Ep) {
        return Ep.IPAddress == InIP && Ep.Port == InPort;
    });

    Subscriptions.TelemetrySubscribers.Add(NewEp);
    UE_LOG(LogTemp, Log, TEXT("📡 [APiSimVehicle %d] Telemetri Abonesi Eklendi: %s:%d (Toplam Abone: %d)"),
        VehicleId, *InIP, InPort, Subscriptions.TelemetrySubscribers.Num());
}

// @state: WIP - Telemetri abonesini çıkarır
void APiSimVehicle::RemoveTelemetrySubscriber(const FString& InIP, int32 InPort)
{
    Subscriptions.TelemetrySubscribers.RemoveAll([&](const FPiSimClientEndpoint& Ep) {
        return Ep.IPAddress == InIP && Ep.Port == InPort;
    });
    UE_LOG(LogTemp, Log, TEXT("🔌 [APiSimVehicle %d] Telemetri Abonesi Çıkarıldı: %s:%d"), VehicleId, *InIP, InPort);
}

// @state: WIP - Canlı video abonesi ekler (On-Demand)
void APiSimVehicle::AddVideoSubscriber(const FString& InIP, int32 InPort)
{
    FPiSimClientEndpoint NewEp;
    NewEp.IPAddress = InIP;
    NewEp.Port = InPort;
    NewEp.LastActiveTime = (GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.0;

    Subscriptions.VideoSubscribers.RemoveAll([&](const FPiSimClientEndpoint& Ep) {
        return Ep.IPAddress == InIP && Ep.Port == InPort;
    });

    Subscriptions.VideoSubscribers.Add(NewEp);
    UE_LOG(LogTemp, Log, TEXT("📹 [APiSimVehicle %d] Canlı Video Abonesi Eklendi: %s:%d (Toplam Video İzleyici: %d)"),
        VehicleId, *InIP, InPort, Subscriptions.VideoSubscribers.Num());
}

// @state: WIP - Canlı video abonesini çıkarır
void APiSimVehicle::RemoveVideoSubscriber(const FString& InIP, int32 InPort)
{
    Subscriptions.VideoSubscribers.RemoveAll([&](const FPiSimClientEndpoint& Ep) {
        return Ep.IPAddress == InIP && Ep.Port == InPort;
    });
    UE_LOG(LogTemp, Log, TEXT("⏹️ [APiSimVehicle %d] Canlı Video Abonesi Çıkarıldı: %s:%d"), VehicleId, *InIP, InPort);
}


// =========================================================================
// Profesyonel Sürü & Bağımsız Komut Alıcısı (UDP Port 7402, SO_REUSEADDR)
// =========================================================================

void APiSimVehicle::InitCommandSocket()
{
    CloseCommandSocket();

    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem)
    {
        return;
    }

    // 1. Ortak Sürü & İsim/UID Tabanlı Kanal (UDP Port 7402 - SO_REUSEADDR)
    CommandSocket = FUdpSocketBuilder(TEXT("PiSimVehicleCommandSocket"))
        .AsNonBlocking()
        .AsReusable()
        .BoundToPort(CommandPort)
        .WithBroadcast()
        .WithReceiveBufferSize(65535);

    // 2. Araca Özel Doğrudan Alt-Port Kanalı (Örn: Araç 1 -> 7403, Araç 2 -> 7404, Araç 3 -> 7405)
    DedicatedCommandPort = CommandPort + FMath::Max(1, VehicleId);
    DedicatedCommandSocket = FUdpSocketBuilder(TEXT("PiSimVehicleDedicatedCommandSocket"))
        .AsNonBlocking()
        .AsReusable()
        .BoundToPort(DedicatedCommandPort)
        .WithBroadcast()
        .WithReceiveBufferSize(65535);

    if (CommandSocket)
    {
        UE_LOG(LogTemp, Log, TEXT("🎮 [APiSimVehicle] Araç '%s' (#%d) Komut Dinleyicileri Aktif! (Sürü Portu: %d | Doğrudan Özel Port: %d)"),
            *VehicleCode, VehicleId, CommandPort, DedicatedCommandPort);
    }
}

void APiSimVehicle::CloseCommandSocket()
{
    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (CommandSocket)
    {
        if (SocketSubsystem) SocketSubsystem->DestroySocket(CommandSocket);
        CommandSocket = nullptr;
    }
    if (DedicatedCommandSocket)
    {
        if (SocketSubsystem) SocketSubsystem->DestroySocket(DedicatedCommandSocket);
        DedicatedCommandSocket = nullptr;
    }
}

void APiSimVehicle::PollCommandSocket()
{
    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem)
    {
        return;
    }

    // 1. Ortak Port 7402: Gelen paketi aktif filodaki tüm araçlara dağıt (Windows SO_REUSEADDR unicast paketini hangi sokete verirse versin hedef araç yakalar)
    if (CommandSocket)
    {
        uint32 PendingSize = 0;
        while (CommandSocket->HasPendingData(PendingSize))
        {
            TArray<uint8> RecvBuffer;
            RecvBuffer.SetNumUninitialized(FMath::Min(PendingSize, 4096u));
            TSharedRef<FInternetAddr> SenderAddr = SocketSubsystem->CreateInternetAddr();
            int32 BytesRead = 0;
            if (CommandSocket->RecvFrom(RecvBuffer.GetData(), RecvBuffer.Num(), BytesRead, *SenderAddr) && BytesRead > 0)
            {
                RecvBuffer.Add(0);
                FString JsonStr = UTF8_TO_TCHAR((const char*)RecvBuffer.GetData());
                FScopeLock Lock(&APiSimVehicle::FleetLock);
                for (APiSimVehicle* Veh : APiSimVehicle::ActiveFleet)
                {
                    if (Veh && IsValid(Veh))
                    {
                        Veh->ProcessCommandJson(JsonStr);
                    }
                }
            }
        }
    }

    // 2. Doğrudan Özel Alt-Port (7402 + VehicleId): Bu porta gelen paket doğrudan BU araca aittir
    if (DedicatedCommandSocket)
    {
        uint32 PendingSize = 0;
        while (DedicatedCommandSocket->HasPendingData(PendingSize))
        {
            TArray<uint8> RecvBuffer;
            RecvBuffer.SetNumUninitialized(FMath::Min(PendingSize, 4096u));
            TSharedRef<FInternetAddr> SenderAddr = SocketSubsystem->CreateInternetAddr();
            int32 BytesRead = 0;
            if (DedicatedCommandSocket->RecvFrom(RecvBuffer.GetData(), RecvBuffer.Num(), BytesRead, *SenderAddr) && BytesRead > 0)
            {
                RecvBuffer.Add(0);
                FString JsonStr = UTF8_TO_TCHAR((const char*)RecvBuffer.GetData());
                // Doğrudan bu aracın portuna (Örn: 7403) atıldıysa target belirtilmese bile bu araç çalıştırır
                TSharedPtr<FJsonObject> JsonObj;
                TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
                if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
                {
                    JsonObj->SetStringField(TEXT("target"), VehicleCode);
                    FString RewrittenJson;
                    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RewrittenJson);
                    FJsonSerializer::Serialize(JsonObj.ToSharedRef(), Writer);
                    ProcessCommandJson(RewrittenJson);
                }
            }
        }
    }
}

void APiSimVehicle::ProcessCommandJson(const FString& JsonStr)
{
    TSharedPtr<FJsonObject> JsonObj;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
    if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
    {
        return;
    }

    // 1. ROS 2 rosbridge_suite / Topic Name Ayrıştırma: {"op":"publish", "topic":"/rover_01/cmd_vel", "msg":{...}}
    FString Target = JsonObj->HasField(TEXT("target")) ? JsonObj->GetStringField(TEXT("target")) : TEXT("all");
    if (JsonObj->HasField(TEXT("topic")))
    {
        FString TopicStr = JsonObj->GetStringField(TEXT("topic")); // Örn: "/rover_01/cmd_vel" veya "/RV01-8F3A9C/cmd_vel"
        TArray<FString> TopicParts;
        TopicStr.ParseIntoArray(TopicParts, TEXT("/"), true);
        if (TopicParts.Num() >= 2 && !TopicParts[0].Equals(TEXT("cmd_vel"), ESearchCase::IgnoreCase))
        {
            Target = TopicParts[0];
        }
    }

    // Hedef Araç Eşleştirme (Target Filtering):
    // "all" -> Tüm sürüye aynı anda
    // "rover_01" / "RV01-..." -> Yalnızca ilgili araç
    // "1" -> Araç ID'si 1 olan
    FString LegacyCode = FString::Printf(TEXT("rover_%02d"), VehicleId);
    bool bTargetMatch = Target.Equals(TEXT("all"), ESearchCase::IgnoreCase) ||
                        Target.Equals(VehicleCode, ESearchCase::IgnoreCase) ||
                        Target.Equals(VehicleName, ESearchCase::IgnoreCase) ||
                        Target.Equals(LegacyCode, ESearchCase::IgnoreCase) ||
                        VehicleCode.StartsWith(Target, ESearchCase::IgnoreCase) ||
                        Target.Equals(FString::FromInt(VehicleId));

    if (!bTargetMatch)
    {
        return; // Bu paket başka bir araca ait, sessizce geç
    }

    // Eğer rosbridge sarmalayıcısı ("msg": {...}) varsa iç mesaj nesnesini al
    TSharedPtr<FJsonObject> ActivePayload = JsonObj;
    const TSharedPtr<FJsonObject>* MsgObjPtr = nullptr;
    if (JsonObj->TryGetObjectField(TEXT("msg"), MsgObjPtr) && MsgObjPtr && MsgObjPtr->IsValid())
    {
        ActivePayload = *MsgObjPtr;
    }

    FString Cmd = JsonObj->HasField(TEXT("cmd")) ? JsonObj->GetStringField(TEXT("cmd")) : TEXT("cmd_vel");
    float TimeoutSec = JsonObj->HasField(TEXT("timeout_ms")) ? (float)JsonObj->GetNumberField(TEXT("timeout_ms")) / 1000.0f : 0.6f;

    if (Cmd.Equals(TEXT("cmd_vel"), ESearchCase::IgnoreCase) || Cmd.Equals(TEXT("drive"), ESearchCase::IgnoreCase) || ActivePayload->HasField(TEXT("linear")))
    {
        float LinearX = ActivePayload->HasField(TEXT("linear_x")) ? (float)ActivePayload->GetNumberField(TEXT("linear_x")) : 0.0f;
        float AngularZ = ActivePayload->HasField(TEXT("angular_z")) ? (float)ActivePayload->GetNumberField(TEXT("angular_z")) : 0.0f;

        // Standart ROS 2 geometry_msgs/msg/Twist iç içe JSON desteği: {"linear": {"x": 1.5, "y": 0, "z": 0}, "angular": {"x": 0, "y": 0, "z": 0.5}}
        const TSharedPtr<FJsonObject>* LinearObj = nullptr;
        if (ActivePayload->TryGetObjectField(TEXT("linear"), LinearObj) && LinearObj && LinearObj->IsValid())
        {
            if ((*LinearObj)->HasField(TEXT("x"))) LinearX = (float)(*LinearObj)->GetNumberField(TEXT("x"));
        }
        const TSharedPtr<FJsonObject>* AngularObj = nullptr;
        if (ActivePayload->TryGetObjectField(TEXT("angular"), AngularObj) && AngularObj && AngularObj->IsValid())
        {
            if ((*AngularObj)->HasField(TEXT("z"))) AngularZ = (float)(*AngularObj)->GetNumberField(TEXT("z"));
        }

        SetTwistVelocity(LinearX, AngularZ, TimeoutSec);
    }
    else if (Cmd.Equals(TEXT("stop"), ESearchCase::IgnoreCase) || Cmd.Equals(TEXT("estop"), ESearchCase::IgnoreCase))
    {
        StopVehicle();
    }
}

void APiSimVehicle::SetTwistVelocity(float InLinearX, float InAngularZ, float InTimeoutSec)
{
    TargetLinearVelocity = InLinearX;
    TargetAngularVelocity = InAngularZ;
    CommandWatchdogTimer = FMath::Max(0.1f, InTimeoutSec);
}

void APiSimVehicle::StopVehicle()
{
    TargetLinearVelocity = 0.0f;
    TargetAngularVelocity = 0.0f;
    CommandWatchdogTimer = 0.0f;
    ResetAllMotorTestValues();
}

void APiSimVehicle::UpdateKinematicsAndMotors(float DeltaTime)
{
    if (CommandWatchdogTimer > 0.0f)
    {
        CommandWatchdogTimer -= DeltaTime;
        if (CommandWatchdogTimer <= 0.0f)
        {
            TargetLinearVelocity = 0.0f;
            TargetAngularVelocity = 0.0f;
            ResetAllMotorTestValues();
            return;
        }

        // =====================================================================
        // Diferansiyel Sürüş Kinematiği (Standart AMR/Rover Modeli)
        // =====================================================================
        const float TrackWidth = 0.6f;  // Tekerlekler arası eksen genişliği (60 cm)
        const float MaxSpeed = 3.0f;    // Nominal azami hız 3.0 m/s
        float VLeft = TargetLinearVelocity - (TargetAngularVelocity * TrackWidth * 0.5f);
        float VRight = TargetLinearVelocity + (TargetAngularVelocity * TrackWidth * 0.5f);

        float NormLeft = FMath::Clamp(VLeft / MaxSpeed, -1.0f, 1.0f);
        float NormRight = FMath::Clamp(VRight / MaxSpeed, -1.0f, 1.0f);

        // Direksiyon açısı (Ackermann ön servo varsa: rad/s -> derece)
        float SteerAngleDeg = FMath::Clamp(FMath::RadiansToDegrees(TargetAngularVelocity * 0.6f), -45.0f, 45.0f);
        float NormSteer = SteerAngleDeg / 45.0f;

        // 1. UPiSimWheelComponent Modüllerini Güncelle
        TArray<UPiSimWheelComponent*> Wheels;
        GetComponents<UPiSimWheelComponent>(Wheels);
        for (UPiSimWheelComponent* Wheel : Wheels)
        {
            if (Wheel)
            {
                FVector WheelRelLoc = FVector::ZeroVector;
                if (USceneComponent* SceneComp = GetSceneComponentForBone(Wheel->TargetBoneName))
                {
                    WheelRelLoc = SceneComp->GetRelativeLocation();
                }

                FString BoneLower = Wheel->TargetBoneName.ToString().ToLower();
                bool bIsLeft = (WheelRelLoc.Y < -1.0f) || BoneLower.Contains(TEXT("_l")) || BoneLower.Contains(TEXT("left")) || BoneLower.Contains(TEXT("sol"));
                float WheelRPM = (bIsLeft ? NormLeft : NormRight) * 1200.0f;

                Wheel->bManualOverride = true;
                Wheel->SetTargetRPM(WheelRPM);
            }
        }

        // 2. Kemik Haritasını Güncelle (MotorTestValues -> Chaos Fizik & Animasyon)
        for (const auto& Pair : BoneSceneComponents)
        {
            FName BoneName = Pair.Key;
            USceneComponent* BoneComp = Pair.Value;
            if (!BoneComp) continue;

            FString LowerName = BoneName.ToString().ToLower();
            bool bIsSteer = LowerName.Contains(TEXT("steer")) || LowerName.Contains(TEXT("servo"));
            bool bIsWheel = LowerName.Contains(TEXT("wheel")) || LowerName.Contains(TEXT("teker"));

            if (bIsSteer)
            {
                MotorTestValues.Add(BoneName, NormSteer);
                BoneComp->SetRelativeRotation(FRotator(0.0f, SteerAngleDeg, 0.0f));
            }
            else if (bIsWheel)
            {
                FVector RelLoc = BoneComp->GetRelativeLocation();
                bool bIsLeft = (RelLoc.Y < -1.0f) || LowerName.Contains(TEXT("_l")) || LowerName.Contains(TEXT("left")) || LowerName.Contains(TEXT("sol"));
                float NormVal = bIsLeft ? NormLeft : NormRight;
                MotorTestValues.Add(BoneName, NormVal);
            }
        }

        // 3. Tepkisel ve Pürüzsüz İlerleme Desteği (Hem Fizik Simülasyonu hem Kinematik Mod)
        if (FMath::Abs(TargetLinearVelocity) > 0.01f || FMath::Abs(TargetAngularVelocity) > 0.01f)
        {
            FVector ForwardDir = GetActorForwardVector();
            FVector LinearOffset = ForwardDir * (TargetLinearVelocity * 100.0f * DeltaTime);
            FRotator AngularOffset(0.0f, FMath::RadiansToDegrees(TargetAngularVelocity) * DeltaTime, 0.0f);

            if (bIsPhysicsSimulating)
            {
                if (VisualMeshComponents.IsValidIndex(0) && VisualMeshComponents[0])
                {
                    VisualMeshComponents[0]->AddForce(ForwardDir * (TargetLinearVelocity * TotalMassKg * 250.0f));
                    VisualMeshComponents[0]->AddTorqueInRadians(FVector::UpVector * (TargetAngularVelocity * TotalMassKg * 8000.0f));
                }
            }
            else
            {
                AddActorWorldOffset(LinearOffset, true);
                AddActorWorldRotation(AngularOffset, true);
            }
        }
    }
}

void APiSimVehicle::UpdateLiveTelemetryState(float DeltaTime)
{
    if (!TelemetryGateway)
    {
        return;
    }

    // 1. Gerçek 3B Dünya Konumu (Unreal cm -> Metre)
    FVector WorldLocCm = GetActorLocation();
    FRotator WorldRot = GetActorRotation();
    if (bIsPhysicsSimulating && VisualMeshComponents.IsValidIndex(0) && VisualMeshComponents[0])
    {
        WorldLocCm = VisualMeshComponents[0]->GetComponentLocation();
        WorldRot = VisualMeshComponents[0]->GetComponentRotation();
    }

    if (!bHasPreviousLocation)
    {
        PreviousWorldLocation = WorldLocCm;
        bHasPreviousLocation = true;
    }

    // 2. Gerçek Hız Vektörü (m/s) - Hem fiziksel hem kinematik yer değiştirmeden hesaplanır
    FVector DeltaCm = WorldLocCm - PreviousWorldLocation;
    PreviousWorldLocation = WorldLocCm;

    FVector VelMs = (DeltaTime > 0.0001f) ? (DeltaCm / (100.0f * DeltaTime)) : FVector::ZeroVector;
    if (bIsPhysicsSimulating && VisualMeshComponents.IsValidIndex(0) && VisualMeshComponents[0])
    {
        FVector PhysVelMs = VisualMeshComponents[0]->GetPhysicsLinearVelocity() / 100.0f;
        if (PhysVelMs.SizeSquared() > VelMs.SizeSquared())
        {
            VelMs = PhysVelMs;
        }
    }

    float SpeedMs = VelMs.Size();
    if (SpeedMs < 0.05f && FMath::Abs(TargetLinearVelocity) > 0.01f && CommandWatchdogTimer > 0.0f)
    {
        SpeedMs = FMath::Abs(TargetLinearVelocity);
    }
    float SpeedKmh = SpeedMs * 3.6f;

    // 3. Dünya Metre Koordinatları (X: İleri/Kuzey, Y: Sağ/Doğu, Z: İrtifa)
    double PosX_m = (double)WorldLocCm.X / 100.0;
    double PosY_m = (double)WorldLocCm.Y / 100.0;
    double PosZ_m = (double)WorldLocCm.Z / 100.0;

    // 4. WGS84 GPS Koordinatları (Ankara Merkez: 39.9334° N, 32.8597° E)
    const double OriginLat = 39.9334;
    const double OriginLon = 32.8597;
    double Lat = OriginLat + (PosX_m / 111320.0);
    double Lon = OriginLon + (PosY_m / (111320.0 * FMath::Cos(FMath::DegreesToRadians(OriginLat))));

    // 5. IMU Verisini Besle
    FPiSimIMUData ImuData;
    ImuData.Orientation = WorldRot.Quaternion();
    ImuData.AngularVelocityRad = FVector(0.0f, 0.0f, TargetAngularVelocity);
    ImuData.LinearAcceleration = FVector(0.0f, 0.0f, -9.80665f);
    TelemetryGateway->FeedIMUData(ImuData);

    // 6. Barometre Verisini Besle
    FPiSimBaroData BaroData;
    BaroData.PressureAltitudeM = (float)PosZ_m;
    BaroData.DiffPressureHPa = (0.5f * 1.225f * SpeedMs * SpeedMs) / 100.0f;
    TelemetryGateway->FeedBaroData(BaroData);

    // 7. GPS ve Konum Verisini Besle
    FPiSimGPSData GpsData;
    GpsData.Latitude = Lat;
    GpsData.Longitude = Lon;
    GpsData.AltitudeMeters = (float)PosZ_m;
    GpsData.SpeedKmh = SpeedKmh;
    GpsData.VelocityNED = FVector(VelMs.X, VelMs.Y, -VelMs.Z);
    TelemetryGateway->FeedGPSData(GpsData);
}
