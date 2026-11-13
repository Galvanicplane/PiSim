#include "Vehicle.h"
#include "IPiSimActuatorInterface.h"
#include "PiSimTopicBridge.h"
#include "CameraSensor.h"
#include "ModelFbxImporter.h"
#include "GarageWidget.h"
#include "ImporterWidget.h"
#include "Wheel.h"
#include "Servo.h"
#include "Propeller.h"
#include "Actuator.h"
#include "Track.h"
#include "LiftBody.h"
#include "MotorDebug.h"
#include "IMUSensor.h"
#include "GPSSensor.h"
#include "BarometerSensor.h"
#include "LidarSensor.h"
#include "DrawDebugHelpers.h"
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
int32 APiSimVehicle::ActiveWheelSpinAxisMode = 1; // Default: Mode 1: Y Ekseni (0,1,0) [Swing2 ACM_Free] // 0=X (1,0,0) [Twist], 1=Y (0,1,0) [Swing2], 2=Z (0,0,1) [Swing1]

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





    TopicBridge = CreateDefaultSubobject<UPiSimTopicBridge>(TEXT("TopicBridge"));

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
    // ÖNEMLİ: Bileşenlerin (Pi5Bridge, TopicBridge) BeginPlay'i çalışmadan ÖNCE
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

    // 'Y' tuşuna basıldığında tekerlek dönme eksenini ve ACM mafsal serbestisini döngüsel değiştir (Canlı Debug)
    // NOT: Araç WASD ile doğrudan klavyeden sürülmez; tüm sürüş komutları yalnızca Pi 5 / Zenoh Topic'leri üzerinden gelir.
    if (UWorld* World = GetWorld())
    {
        if (APlayerController* PC = World->GetFirstPlayerController())
        {
            if (PC->WasInputKeyJustPressed(EKeys::Y) && !bSensorPlacementMode)
            {
                CycleWheelSpinAxis();
            }

            // @state: WIP - Blender tarzı X / Y / Z + Fare + Enter Etkileşimli Sensör Yerleştirme Modu
            if (bSensorPlacementMode && !ActivePlacementSensorBone.IsNone())
            {
                if (PC->WasInputKeyJustPressed(EKeys::X)) ActivePlacementAxis = 1;
                else if (PC->WasInputKeyJustPressed(EKeys::Y)) ActivePlacementAxis = 2;
                else if (PC->WasInputKeyJustPressed(EKeys::Z)) ActivePlacementAxis = 3;
                else if (PC->WasInputKeyJustPressed(EKeys::Enter) || PC->WasInputKeyJustPressed(EKeys::LeftMouseButton))
                {
                    ConfirmInteractiveSensorPlacement();
                }
                else if (PC->WasInputKeyJustPressed(EKeys::Escape) || PC->WasInputKeyJustPressed(EKeys::RightMouseButton))
                {
                    CancelInteractiveSensorPlacement();
                }

                if (bSensorPlacementMode)
                {
                    float MouseDX = 0.0f, MouseDY = 0.0f;
                    PC->GetInputMouseDelta(MouseDX, MouseDY);
                    FVector DeltaMove = FVector::ZeroVector;
                    const float MoveScale = 1.5f;
                    if (ActivePlacementAxis == 1) DeltaMove.X = (MouseDX + MouseDY) * MoveScale;
                    else if (ActivePlacementAxis == 2) DeltaMove.Y = MouseDX * MoveScale;
                    else if (ActivePlacementAxis == 3) DeltaMove.Z = MouseDY * MoveScale;
                    else DeltaMove = FVector(MouseDY * MoveScale, MouseDX * MoveScale, 0.0f);

                    FTransform* SocketT = SensorSocketTransforms.Find(ActivePlacementSensorBone.ToString());
                    if (SocketT && !DeltaMove.IsNearlyZero())
                    {
                        SocketT->AddToTranslation(DeltaMove);
                    }
                    if (USceneComponent* SensorBoneComp = GetSceneComponentForBone(ActivePlacementSensorBone))
                    {
                        if (!DeltaMove.IsNearlyZero())
                        {
                            SensorBoneComp->AddLocalOffset(DeltaMove);
                        }
                        FVector Origin = SensorBoneComp->GetComponentLocation();
                        FColor AxisColor = (ActivePlacementAxis == 1) ? FColor::Red : (ActivePlacementAxis == 2) ? FColor::Green : (ActivePlacementAxis == 3) ? FColor::Blue : FColor::Yellow;
                        FVector AxisDir = (ActivePlacementAxis == 1) ? FVector(1,0,0) : (ActivePlacementAxis == 2) ? FVector(0,1,0) : FVector(0,0,1);
                        if (ActivePlacementAxis != 0)
                        {
                            DrawDebugLine(World, Origin - AxisDir * 250.0f, Origin + AxisDir * 250.0f, AxisColor, false, -1.0f, 0, 2.5f);
                        }
                        DrawDebugSphere(World, Origin, 8.0f, 12, AxisColor, false, -1.0f, 0, 2.0f);
                    }
                }
            }
        }
    }

    // NOT: Tüm motor / servo / aktüatör modülleri kendi içlerinden TopicBridge kontrol topic'lerini dinleyerek kendi hareketlerini uygular; APiSimVehicle dışarıdan BoneSceneComponents taraması veya müdahale yapmaz.
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
    BoneSlotRegistry.Empty();
    MultiBoneSlotRegistry.Empty();
    BoneJointConfigs.Empty();
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

// @state: WIP - Kemiğe bağlı veya çocuğundaki UCX fiziksel çarpışma bileşenini döner (Asla Visual NoCollision mesh döndürmez)
UPrimitiveComponent* APiSimVehicle::GetPrimitiveComponentForBone(FName BoneName) const
{
    if (BoneName.IsNone()) return nullptr;

    FString BoneStr = BoneName.ToString();
    FString Clean = BoneStr;
    Clean.RemoveFromStart(TEXT("M_"), ESearchCase::IgnoreCase);
    Clean.RemoveFromStart(TEXT("B_"), ESearchCase::IgnoreCase);

    // 1) Öncelik SADECE UCX çarpışma zırhı parçalarındadır
    for (UProceduralMeshComponent* CollComp : CollisionMeshComponents)
    {
        if (CollComp)
        {
            FString CName = CollComp->GetName();
            if (CName.Contains(BoneStr, ESearchCase::IgnoreCase) || (!Clean.IsEmpty() && CName.Contains(Clean, ESearchCase::IgnoreCase)))
            {
                return CollComp;
            }
        }
    }

    // 2) Kemik sahne bileşeni veya altındaki UCX/fizik bileşenini ara
    USceneComponent* SceneComp = GetSceneComponentForBone(BoneName);
    if (SceneComp)
    {
        if (UPrimitiveComponent* DirectPrim = Cast<UPrimitiveComponent>(SceneComp))
        {
            if (CollisionMeshComponents.Contains(DirectPrim) || DirectPrim->IsSimulatingPhysics())
            {
                return DirectPrim;
            }
        }
        for (int32 c = 0; c < SceneComp->GetNumChildrenComponents(); ++c)
        {
            if (UPrimitiveComponent* ChildPrim = Cast<UPrimitiveComponent>(SceneComp->GetChildComponent(c)))
            {
                if (CollisionMeshComponents.Contains(ChildPrim) || ChildPrim->IsSimulatingPhysics())
                {
                    return ChildPrim;
                }
            }
        }
    }

    return nullptr;
}

// @state: WIP - UI Slider'ından gelen motor test değerini uygular (Bir kemikteki 0..N modüle iletir)
void APiSimVehicle::SetMotorTestValue(FName BoneName, float Value)
{
    MotorTestValues.Add(BoneName, Value);

    TArray<UActorComponent*> Mods = GetModulesAtBone(BoneName);
    UActorComponent* Mod = (Mods.Num() > 0) ? Mods[0] : nullptr;
    FString ModStatus = TEXT("🔘 Modül Atanmamış");
    for (UActorComponent* M : Mods)
    {
        if (IPiSimActuatorInterface* Actuator = Cast<IPiSimActuatorInterface>(M))
        {
            Actuator->SetActuatorNormalized(Value);
            ModStatus = FString::Printf(TEXT("⚙️ %s (%d): %+.2f"), *Actuator->GetActuatorType(), Mods.Num(), Value);
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
        // ⚠️ KESİN KURAL: Fizik aktifken SetRelativeRotation YASAKTIR (sadece statik garaj önizlemesinde izinli)
        if (TargetComp && !bIsPhysicsSimulating)
        {
            TargetComp->SetRelativeRotation(FRotator(0.0f, TargetAngleDeg, 0.0f));
            bRotated = true;
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
                FVector LocalAxle = GetCurrentWheelSpinAxis();
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
            if (IPiSimActuatorInterface* Actuator = Cast<IPiSimActuatorInterface>(Pair.Value))
            {
                Actuator->SetActuatorNormalized(0.0f);
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

        // ⚠️ UCX_ yoksa ASLA görsel köşe noktalarını collision olarak kullanma (Kullanıcı Kuralı: Sadece uyarı göster!)
        if (!bHasCollision)
        {
            UE_LOG(LogTemp, Warning, TEXT("⚠️ [FİZİK UYARI] '%s' parçası için UCX_ çarpışma zırhı yok! Görsel mesh'e collision eklenmedi."),
                *StoredVisualSections[i].MeshName);
            VisComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            VisComp->RecreatePhysicsState();
            VisComp->SetSimulatePhysics(false);
            continue;
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

    // 2) Açısal Hareket (Angular Motion):
    // ActiveWheelSpinAxisMode değişkenine göre mafsal serbestisi verilir:
    // Mode 0: X Ekseni (1,0,0) -> Twist (Roll) ACM_Free, Swing1 ve Swing2 Locked
    // Mode 1: Y Ekseni (0,1,0) -> Swing2 (Pitch) ACM_Free, Twist ve Swing1 Locked
    // Mode 2: Z Ekseni (0,0,1) -> Swing1 (Yaw) ACM_Free, Twist ve Swing2 Locked
    if (ActiveWheelSpinAxisMode == 0)
    {
        Constraint->SetAngularTwistLimit(EAngularConstraintMotion::ACM_Free, 0.0f);
        Constraint->SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Locked, 0.0f);
        Constraint->SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Locked, 0.0f);
    }
    else if (ActiveWheelSpinAxisMode == 1)
    {
        Constraint->SetAngularTwistLimit(EAngularConstraintMotion::ACM_Locked, 0.0f);
        Constraint->SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Locked, 0.0f);
        Constraint->SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Free, 0.0f);
    }
    else // Mode 2: Z
    {
        Constraint->SetAngularTwistLimit(EAngularConstraintMotion::ACM_Locked, 0.0f);
        Constraint->SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Free, 0.0f);
        Constraint->SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Locked, 0.0f);
    }
    Constraint->SetOrientationDriveTwistAndSwing(false, false);
    Constraint->UpdateConstraintFrames();
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

// @state: WIP - 'Y' tuşu ile tekerlek dönme eksenini ve ACM mafsal serbestisini döngüsel değiştirir
void APiSimVehicle::SetWheelSpinAxisMode(int32 InMode)
{
    ActiveWheelSpinAxisMode = ((InMode % 3) + 3) % 3;
    FVector NewAxis = GetCurrentWheelSpinAxis();

    // 1. Bu aracın tüm tekerlek modüllerini yeni eksene güncelle
    TArray<UPiSimWheelComponent*> WheelMods;
    GetComponents<UPiSimWheelComponent>(WheelMods);
    for (UPiSimWheelComponent* Wheel : WheelMods)
    {
        if (Wheel)
        {
            Wheel->LocalSpinAxis = NewAxis;
        }
    }

    // 2. Canlı tüm eklem kısıtlamalarını (JointConstraints) yeni eksen moduna göre güncelle
    for (int32 i = 0; i < JointConstraints.Num(); ++i)
    {
        if (JointConstraints[i])
        {
            ConfigureConstraintDof(JointConstraints[i], i);
        }
    }

    FString ModeName  = (ActiveWheelSpinAxisMode == 0) ? TEXT("MOD 0: X Ekseni (1, 0, 0)") :
                        (ActiveWheelSpinAxisMode == 1) ? TEXT("MOD 1: Y Ekseni (0, 1, 0)") :
                                                        TEXT("MOD 2: Z Ekseni (0, 0, 1)");
    FString FreeAcm   = (ActiveWheelSpinAxisMode == 0) ? TEXT("Twist / Roll (ACM_Free)") :
                        (ActiveWheelSpinAxisMode == 1) ? TEXT("Swing2 / Pitch (ACM_Free)") :
                                                        TEXT("Swing1 / Yaw (ACM_Free)");
    FString LockedAcm = (ActiveWheelSpinAxisMode == 0) ? TEXT("Swing1 & Swing2 (ACM_Locked)") :
                        (ActiveWheelSpinAxisMode == 1) ? TEXT("Twist & Swing1 (ACM_Locked)") :
                                                        TEXT("Twist & Swing2 (ACM_Locked)");

    FString OnScreenMsg = FString::Printf(TEXT("[PISIM EKSEN (Y)]: %s | Donme Ekseni: (%s) | Serbest ACM: %s | Kilitli: %s"), *ModeName, *NewAxis.ToString(), *FreeAcm, *LockedAcm);
    UE_LOG(LogTemp, Warning, TEXT("%s"), *OnScreenMsg);
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(99234, 5.0f, FColor::Cyan, OnScreenMsg);
    }
}

void APiSimVehicle::CycleWheelSpinAxis()
{
    int32 NextMode = (ActiveWheelSpinAxisMode + 1) % 3;
    FScopeLock Lock(&FleetLock);
    for (APiSimVehicle* V : ActiveFleet)
    {
        if (V && IsValid(V))
        {
            V->SetWheelSpinAxisMode(NextMode);
        }
    }
}

FVector APiSimVehicle::GetCurrentWheelSpinAxis() const
{
    if (ActiveWheelSpinAxisMode == 0) return FVector(1.0f, 0.0f, 0.0f);
    if (ActiveWheelSpinAxisMode == 1) return FVector(0.0f, 1.0f, 0.0f);
    return FVector(0.0f, 0.0f, 1.0f);
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

// @state: WIP - Belirtilen iskelet kemiğine mikro-modül (Wheel, Servo vb.) atar (0, 1 veya N modül destekler)
bool APiSimVehicle::AssignModuleToBone(FName BoneName, UActorComponent* ModuleComponent)
{
    if (!ModuleComponent || BoneName.IsNone())
    {
        return false;
    }

    FPiSimBoneModuleList& SlotList = MultiBoneSlotRegistry.FindOrAdd(BoneName);
    if (!SlotList.Modules.Contains(ModuleComponent))
    {
        SlotList.Modules.Add(ModuleComponent);
    }

    // Geriye dönük uyumluluk için ilk/ana modülü BoneSlotRegistry'de tut
    if (!BoneSlotRegistry.Contains(BoneName) || !BoneSlotRegistry[BoneName])
    {
        BoneSlotRegistry.Add(BoneName, ModuleComponent);
    }

    // Modülün kendi içindeki ParentVehicle / VehicleName / VehicleIndex kimlik değişkenlerini doldur
    if (IPiSimActuatorInterface* Actuator = Cast<IPiSimActuatorInterface>(ModuleComponent))
    {
        Actuator->InitializeModuleIdentity(this);
    }
    return true;
}

// @state: WIP - Kemiğe atanmış birincil mikro modülü döndürür
UActorComponent* APiSimVehicle::GetModuleAtBone(FName BoneName) const
{
    if (const FPiSimBoneModuleList* SlotList = MultiBoneSlotRegistry.Find(BoneName))
    {
        for (UActorComponent* Comp : SlotList->Modules)
        {
            if (Comp && IsValid(Comp)) return Comp;
        }
    }
    return BoneSlotRegistry.FindRef(BoneName);
}

// @state: WIP - Bir kemiğe atanmış tüm modülleri (0, 1 veya N adet) döndürür
TArray<UActorComponent*> APiSimVehicle::GetModulesAtBone(FName BoneName) const
{
    TArray<UActorComponent*> Result;
    if (const FPiSimBoneModuleList* SlotList = MultiBoneSlotRegistry.Find(BoneName))
    {
        for (UActorComponent* Comp : SlotList->Modules)
        {
            if (Comp && IsValid(Comp)) Result.Add(Comp);
        }
    }
    else if (UActorComponent* Single = BoneSlotRegistry.FindRef(BoneName))
    {
        if (IsValid(Single)) Result.Add(Single);
    }
    return Result;
}

// @state: WIP - Kemiğin açısal/doğrusal serbestlik eksenlerini ve statik/kinetik direnç katsayılarını ayarlar
void APiSimVehicle::SetBoneJointConfig(FName BoneName, const FPiSimBoneJointConfig& Config)
{
    if (BoneName.IsNone()) return;
    FPiSimBoneJointConfig CopyCfg = Config;
    CopyCfg.BoneName = BoneName;
    BoneJointConfigs.Add(BoneName, CopyCfg);

    if (UPhysicsConstraintComponent* Constraint = FindConstraintForBone(BoneName))
    {
        Constraint->SetLinearXLimit(CopyCfg.bFreePosX ? ELinearConstraintMotion::LCM_Free : ELinearConstraintMotion::LCM_Locked, 0.0f);
        Constraint->SetLinearYLimit(CopyCfg.bFreePosY ? ELinearConstraintMotion::LCM_Free : ELinearConstraintMotion::LCM_Locked, 0.0f);
        Constraint->SetLinearZLimit(CopyCfg.bFreePosZ ? ELinearConstraintMotion::LCM_Free : ELinearConstraintMotion::LCM_Locked, 0.0f);
        Constraint->SetAngularTwistLimit(CopyCfg.bFreeRotX ? EAngularConstraintMotion::ACM_Free : EAngularConstraintMotion::ACM_Locked, 0.0f);
        Constraint->SetAngularSwing2Limit(CopyCfg.bFreeRotY ? EAngularConstraintMotion::ACM_Free : EAngularConstraintMotion::ACM_Locked, 0.0f);
        Constraint->SetAngularSwing1Limit(CopyCfg.bFreeRotZ ? EAngularConstraintMotion::ACM_Free : EAngularConstraintMotion::ACM_Locked, 0.0f);
        Constraint->UpdateConstraintFrames();
    }
}

FPiSimBoneJointConfig APiSimVehicle::GetBoneJointConfig(FName BoneName) const
{
    if (const FPiSimBoneJointConfig* Found = BoneJointConfigs.Find(BoneName))
    {
        return *Found;
    }
    FPiSimBoneJointConfig DefaultCfg;
    DefaultCfg.BoneName = BoneName;
    return DefaultCfg;
}

// @state: WIP - Blender tarzı X/Y/Z + Fare + Enter etkileşimli sensör konumlandırmasını başlatır
void APiSimVehicle::StartInteractiveSensorPlacement(FName SensorBoneOrName)
{
    if (SensorBoneOrName.IsNone()) return;
    bSensorPlacementMode = true;
    ActivePlacementSensorBone = SensorBoneOrName;
    ActivePlacementAxis = 0;

    if (const FTransform* ExistingT = SensorSocketTransforms.Find(SensorBoneOrName.ToString()))
    {
        PlacementInitialOffset = ExistingT->GetLocation();
    }
    else if (USceneComponent* Comp = GetSceneComponentForBone(SensorBoneOrName))
    {
        PlacementInitialOffset = Comp->GetRelativeLocation();
        FTransform NewT;
        NewT.SetLocation(PlacementInitialOffset);
        SensorSocketTransforms.Add(SensorBoneOrName.ToString(), NewT);
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(991, 8.0f, FColor::Cyan,
            FString::Printf(TEXT("🎯 [BLENDER SENSÖR YERLEŞTİRME] '%s' seçili | Eksen Kilidi: X / Y / Z | Onay: ENTER | İptal: ESC"), *SensorBoneOrName.ToString()));
    }
}

void APiSimVehicle::ConfirmInteractiveSensorPlacement()
{
    bSensorPlacementMode = false;
    ActivePlacementAxis = 0;
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(991, 3.0f, FColor::Green,
            FString::Printf(TEXT("✅ [SENSÖR KONUMU ONAYLANDI] '%s'"), *ActivePlacementSensorBone.ToString()));
    }
    ActivePlacementSensorBone = NAME_None;
}

void APiSimVehicle::CancelInteractiveSensorPlacement()
{
    if (!ActivePlacementSensorBone.IsNone())
    {
        if (FTransform* SocketT = SensorSocketTransforms.Find(ActivePlacementSensorBone.ToString()))
        {
            SocketT->SetLocation(PlacementInitialOffset);
        }
        if (USceneComponent* Comp = GetSceneComponentForBone(ActivePlacementSensorBone))
        {
            Comp->SetRelativeLocation(PlacementInitialOffset);
        }
    }
    bSensorPlacementMode = false;
    ActivePlacementAxis = 0;
    ActivePlacementSensorBone = NAME_None;
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
// Tekil Kontrol Topic Alıcısı (UDP Port 7402 -> TopicBridge)
// =========================================================================

void APiSimVehicle::InitCommandSocket()
{
    CloseCommandSocket();

    // Ana dinleyici TopicBridge bileşenidir; çift soket bind çakışmasını önlemek için TopicBridge varsa ikinci soket açılmaz
    if (TopicBridge)
    {
        return;
    }

    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem)
    {
        return;
    }

    CommandSocket = FUdpSocketBuilder(TEXT("PiSimVehicleCommandSocket"))
        .AsNonBlocking()
        .AsReusable()
        .BoundToPort(CommandPort)
        .WithBroadcast()
        .WithReceiveBufferSize(65535);
}

void APiSimVehicle::CloseCommandSocket()
{
    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (CommandSocket)
    {
        if (SocketSubsystem) SocketSubsystem->DestroySocket(CommandSocket);
        CommandSocket = nullptr;
    }
}

void APiSimVehicle::PollCommandSocket()
{
    if (!CommandSocket)
    {
        return;
    }

    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem)
    {
        return;
    }

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

// @state: WIP - Araç kontrol komutları dışarıdan kemik taramasıyla değil, yalnızca TopicBridge üzerinden modüllerin kendi içine yayınlanır
void APiSimVehicle::ProcessCommandJson(const FString& JsonStr)
{
    if (TopicBridge)
    {
        TopicBridge->ProcessIncomingPacketJson(JsonStr);
    }
}

void APiSimVehicle::SetTwistVelocity(float InLinearX, float InAngularZ, float InTimeoutSec)
{
    bDirectActuatorControl = false;
    TargetLinearVelocity = InLinearX;
    TargetAngularVelocity = InAngularZ;
    CommandWatchdogTimer = FMath::Max(0.1f, InTimeoutSec);
}

void APiSimVehicle::StopVehicle()
{
    bDirectActuatorControl = false;
    TargetLinearVelocity = 0.0f;
    TargetAngularVelocity = 0.0f;
    CommandWatchdogTimer = 0.0f;
    if (TopicBridge)
    {
        TopicBridge->ProcessIncomingPacketJson(FString::Printf(TEXT("{\"topic\":\"/%s/channels\",\"cmd\":\"stop\",\"target\":\"%s\",\"channels\":{\"1\":1500,\"2\":1500,\"3\":1500,\"4\":1500}}"), *VehicleCode, *VehicleCode));
    }
}

// @state: WIP - APiSimVehicle içinden dış motor/kemik taraması ve kinematik müdahale tamamen kaldırıldı; her modül kendi topic'ini dinler
void APiSimVehicle::UpdateKinematicsAndMotors(float DeltaTime)
{
    if (CommandWatchdogTimer > 0.0f)
    {
        CommandWatchdogTimer -= DeltaTime;
        if (CommandWatchdogTimer <= 0.0f)
        {
            TargetLinearVelocity = 0.0f;
            TargetAngularVelocity = 0.0f;
        }
    }
}

void APiSimVehicle::UpdateLiveTelemetryState(float DeltaTime)
{
    // Sensör güncellemesi

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
    FeedIMUData(ImuData);

    // 6. Barometre Verisini Besle
    FPiSimBaroData BaroData;
    BaroData.PressureAltitudeM = (float)PosZ_m;
    BaroData.DiffPressureHPa = (0.5f * 1.225f * SpeedMs * SpeedMs) / 100.0f;
    FeedBaroData(BaroData);

    // 7. GPS ve Konum Verisini Besle
    FPiSimGPSData GpsData;
    GpsData.Latitude = Lat;
    GpsData.Longitude = Lon;
    GpsData.AltitudeMeters = (float)PosZ_m;
    GpsData.SpeedKmh = SpeedKmh;
    GpsData.VelocityNED = FVector(VelMs.X, VelMs.Y, -VelMs.Z);
    FeedGPSData(GpsData);
}

void APiSimVehicle::FeedIMUData(const FPiSimIMUData& InData)
{
    FScopeLock Lock(&SensorDataLock);
    LatestIMUData = InData;
}

void APiSimVehicle::FeedBaroData(const FPiSimBaroData& InData)
{
    FScopeLock Lock(&SensorDataLock);
    LatestBaroData = InData;
}

void APiSimVehicle::FeedGPSData(const FPiSimGPSData& InData)
{
    FScopeLock Lock(&SensorDataLock);
    LatestGPSData = InData;
}

void APiSimVehicle::FeedCameraFrame(const TArray<uint8>& InJpegBytes)
{
    FScopeLock Lock(&SensorDataLock);
    LatestCameraJpeg = InJpegBytes;
}

bool APiSimVehicle::GetLatestIMUData(FPiSimIMUData& OutData)
{
    FScopeLock Lock(&SensorDataLock);
    OutData = LatestIMUData;
    return true;
}

bool APiSimVehicle::GetLatestBaroData(FPiSimBaroData& OutData)
{
    FScopeLock Lock(&SensorDataLock);
    OutData = LatestBaroData;
    return true;
}

bool APiSimVehicle::GetLatestGPSData(FPiSimGPSData& OutData)
{
    FScopeLock Lock(&SensorDataLock);
    OutData = LatestGPSData;
    return true;
}

bool APiSimVehicle::GetLatestCameraFrame(TArray<uint8>& OutJpegBytes)
{
    FScopeLock Lock(&SensorDataLock);
    OutJpegBytes = LatestCameraJpeg;
    return OutJpegBytes.Num() > 0;
}
