// GarageWidget.cpp
// Otopark / Slot Tabanlı Garaj Arayüzü Uygulaması

#include "GarageWidget.h"
#include "Vehicle.h"
#include "PiSimSpectator.h"
#include "MotorConfigurator.h"
#include "MotorDebug.h"
#include "ModelFbxImporter.h"
#include "SaveManager.h"
#include "Baker.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "SWebBrowser.h"
#include "TelemetryLogger.h"
#include "TelemetryGateway.h" 
#include "Pi5Bridge.h"
#include "AutopilotBridge.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Async/Async.h"

// @state: WIP - NativeConstruct: Sahnedeki aracı bulur ve bağlar
void UPiSimModularGarageWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Pi 5 bağlantı/keşif olaylarını dinlemek üzere Pi5Bridge'e abone ol
    UPiSimPi5Bridge::OnPi5BeaconDiscovered.AddUObject(this, &UPiSimModularGarageWidget::HandlePi5BeaconReceived);

    if (!TargetVehicle && GetWorld())
    {
        AActor* Found = UGameplayStatics::GetActorOfClass(GetWorld(), APiSimVehicle::StaticClass());
        if (Found)
        {
            SetTargetVehicle(Cast<APiSimVehicle>(Found));
        }
    }

    // Başlangıçta Pi 5'e Unreal Engine varlığını duyurmak için prob fırlat
    TriggerBeaconProbe();
}

void UPiSimModularGarageWidget::NativeDestruct()
{
    UPiSimPi5Bridge::OnPi5BeaconDiscovered.RemoveAll(this);

    Super::NativeDestruct();
}

// @state: WIP - NativeTick: Durum metinlerini ve buton etiketlerini günceller
void UPiSimModularGarageWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    static float LocalIpPushTimer = 0.0f;
    LocalIpPushTimer += InDeltaTime;
    if (LocalIpPushTimer > 2.0f && WebBrowserWidget.IsValid())
    {
        LocalIpPushTimer = 0.0f;
        ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
        if (SocketSubsystem)
        {
            bool bCanBindAll = false;
            TSharedPtr<FInternetAddr> LocalAddr = SocketSubsystem->GetLocalHostAddr(*GLog, bCanBindAll);
            if (LocalAddr.IsValid())
            {
                FString LocalIpStr = LocalAddr->ToString(false);
                FString Script = FString::Printf(TEXT("if (typeof setUeLocalIp === 'function') { setUeLocalIp('%s'); }"), *LocalIpStr);
                WebBrowserWidget->ExecuteJavascript(Script);
            }
        }
    }

    // Aktif eşleşmiş Pi 5 cihazını ve canlı filoyu düzenli olarak Web arayüzüne senkronize et
    static float BeaconSyncTimer = 0.0f;
    BeaconSyncTimer += InDeltaTime;
    if (BeaconSyncTimer > 1.0f && WebBrowserWidget.IsValid())
    {
        BeaconSyncTimer = 0.0f;
        FString FleetJson = GetActiveFleetJson();
        FString CleanFleetJson = FleetJson.Replace(TEXT("\n"), TEXT(" ")).Replace(TEXT("\r"), TEXT(""));
        FString FleetScript = FString::Printf(TEXT("if (typeof onFleetSyncedFromCpp === 'function') { onFleetSyncedFromCpp(%s); }"), *CleanFleetJson);
        WebBrowserWidget->ExecuteJavascript(FleetScript);

        APiSimVehicle* VehicleToCheck = TargetVehicle ? TargetVehicle : (FleetVehicles.Num() > 0 ? FleetVehicles[0] : nullptr);
        if (VehicleToCheck && VehicleToCheck->Pi5Bridge)
        {
            FString Target = VehicleToCheck->Pi5Bridge->TargetIP;
            if (!Target.IsEmpty() && Target != TEXT("127.0.0.1") && Target != TEXT("0.0.0.0"))
            {
                FString Script = FString::Printf(TEXT("if (typeof onBeaconDeviceDiscovered === 'function') { onBeaconDeviceDiscovered({name:'Raspberry Pi 5 (Companion)', ip:'%s', telemetry_port:%d, port:%d, msg:'HI FROM PI5', status:'ONLINE'}); }"),
                    *Target, VehicleToCheck->Pi5Bridge->TelemetryPort, VehicleToCheck->Pi5Bridge->TelemetryPort);
                WebBrowserWidget->ExecuteJavascript(Script);
            }
        }
    }

    if (!TargetVehicle && GetWorld())
    {
        AActor* Found = UGameplayStatics::GetActorOfClass(GetWorld(), APiSimVehicle::StaticClass());
        if (Found)
        {
            SetTargetVehicle(Cast<APiSimVehicle>(Found));
        }
    }

    if (TargetVehicle)
    {
        if (PhysicsButtonText.IsValid())
        {
            PhysicsButtonText->SetText(FText::FromString(TargetVehicle->IsVehiclePhysicsActive() ? TEXT("⚡ FİZİK AKTİF") : TEXT("⚡ FİZİK SİMÜLE ET")));
        }

        if (CollisionButtonText.IsValid())
        {
            CollisionButtonText->SetText(FText::FromString(TargetVehicle->IsCollisionMeshViewEnabled() ? TEXT("🛡️ UCX ZIRH MODU") : TEXT("🎨 GÖRSEL MOD")));
        }
    }
}

// @state: WIP - Slate / WebUI Widget ağacını oluşturur (Başlangıçta DashboardUnreal açılır)
TSharedRef<SWidget> UPiSimModularGarageWidget::RebuildWidget()
{
    FString HtmlFilePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Source/PiSim/6_UI/WebUI/dashboardUnreal.html"));
    if (!FPaths::FileExists(HtmlFilePath))
    {
        HtmlFilePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Source/PiSim/6_UI/WebUI/garage.html"));
    }
    FString FileUrl = FString::Printf(TEXT("file:///%s"), *HtmlFilePath);

    if (FPaths::FileExists(HtmlFilePath))
    {
        UE_LOG(LogTemp, Warning, TEXT("[UPiSimModularGarageWidget] 🚀 WebUI DashboardUnreal HTML yukleniyor: %s"), *FileUrl);

        SAssignNew(WebBrowserWidget, SWebBrowser)
            .InitialURL(FileUrl)
            .ShowControls(false)
            .ShowAddressBar(false)
            .ShowErrorMessage(true)
            .SupportsTransparency(true)
            .BackgroundColor(FColor(0, 0, 0, 0))
            .BrowserFrameRate(60);

        if (WebBrowserWidget.IsValid())
        {
            WebBrowserWidget->BindUObject(TEXT("garage"), this, true);
        }

        return SNew(SBox)
            .HAlign(HAlign_Fill)
            .VAlign(VAlign_Fill)
            [
                WebBrowserWidget.ToSharedRef()
            ];
    }

    UE_LOG(LogTemp, Warning, TEXT("[UPiSimModularGarageWidget] WebUI garage.html bulunamadi, klasik Slate arayuzu baslatiliyor."));

    return SNew(SBox)
        .HAlign(HAlign_Fill)
        .VAlign(VAlign_Fill)
        .Visibility(EVisibility::SelfHitTestInvisible)
        [
            SNew(SVerticalBox)

            // 1. Üst Kontrol Çubuğu (Toolbar)
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(FMargin(10.0f, 10.0f, 10.0f, 5.0f))
            [
                SNew(SBorder)
                .BorderBackgroundColor(FLinearColor(0.04f, 0.08f, 0.15f, 0.95f))
                .Padding(FMargin(14.0f, 8.0f))
                [
                    SNew(SHorizontalBox)

                    // [MODEL DEĞİŞTİR] Butonu
                    + SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.0f, 0.0f, 6.0f, 0.0f))
                    [
                        SNew(SButton)
                        .ButtonColorAndOpacity(FLinearColor(0.42f, 0.15f, 0.65f, 1.0f))
                        .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModularGarageWidget::OnToggleModelClicked))
                        [
                            SAssignNew(ModelButtonText, STextBlock)
                            .Text(FText::FromString(FString::Printf(TEXT("📦 %s"), *ActiveModelFileName)))
                            .ColorAndOpacity(FLinearColor::White)
                        ]
                    ]

                    // [× 0.1] Ölçek
                    + SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.0f, 0.0f, 3.0f, 0.0f))
                    [
                        SNew(SButton)
                        .ButtonColorAndOpacity(FLinearColor(0.0f, 0.45f, 0.75f, 1.0f))
                        .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModularGarageWidget::OnMultiplyScale01Clicked))
                        [
                            SNew(STextBlock).Text(FText::FromString(TEXT(" ×0.1 "))).ColorAndOpacity(FLinearColor::White)
                        ]
                    ]

                    // [× 10] Ölçek
                    + SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.0f, 0.0f, 6.0f, 0.0f))
                    [
                        SNew(SButton)
                        .ButtonColorAndOpacity(FLinearColor(0.0f, 0.45f, 0.75f, 1.0f))
                        .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModularGarageWidget::OnMultiplyScale10Clicked))
                        [
                            SNew(STextBlock).Text(FText::FromString(TEXT(" ×10 "))).ColorAndOpacity(FLinearColor::White)
                        ]
                    ]

                    // [🔄 REIMPORT]
                    + SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.0f, 0.0f, 8.0f, 0.0f))
                    [
                        SNew(SButton)
                        .ButtonColorAndOpacity(FLinearColor(0.08f, 0.45f, 0.25f, 1.0f))
                        .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModularGarageWidget::OnReimportClicked))
                        [
                            SNew(STextBlock).Text(FText::FromString(TEXT("🔄 REIMPORT"))).ColorAndOpacity(FLinearColor::White)
                        ]
                    ]

                    // [UCX ZIRH / GÖRSEL MOD] Butonu
                    + SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.0f, 0.0f, 8.0f, 0.0f))
                    [
                        SNew(SButton)
                        .ButtonColorAndOpacity(FLinearColor(0.2f, 0.4f, 0.6f, 1.0f))
                        .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModularGarageWidget::OnToggleCollisionViewClicked))
                        [
                            SAssignNew(CollisionButtonText, STextBlock)
                            .Text(FText::FromString(TEXT("🎨 GÖRSEL MOD")))
                            .ColorAndOpacity(FLinearColor::White)
                        ]
                    ]

                    // [FİZİK SİMÜLE ET] Butonu
                    + SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.0f, 0.0f, 8.0f, 0.0f))
                    [
                        SNew(SButton)
                        .ButtonColorAndOpacity(FLinearColor(0.85f, 0.4f, 0.0f, 1.0f))
                        .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModularGarageWidget::OnTogglePhysicsClicked))
                        [
                            SAssignNew(PhysicsButtonText, STextBlock)
                            .Text(FText::FromString(TEXT("⚡ FİZİK SİMÜLE ET")))
                            .ColorAndOpacity(FLinearColor::White)
                        ]
                    ]

                    // [OTOMATİK ROL ATA]
                    + SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.0f, 0.0f, 8.0f, 0.0f))
                    [
                        SNew(SButton)
                        .ButtonColorAndOpacity(FLinearColor(0.2f, 0.65f, 0.4f, 1.0f))
                        .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModularGarageWidget::OnAutoAssignMotorsClicked))
                        [
                            SNew(STextBlock).Text(FText::FromString(TEXT("🤖 OTO ROL"))).ColorAndOpacity(FLinearColor::White)
                        ]
                    ]

                    // Profil Kaydet/Yükle Girişi
                    + SHorizontalBox::Slot().AutoWidth().Padding(FMargin(8.0f, 0.0f, 4.0f, 0.0f))
                    [
                        SAssignNew(ProfileNameInput, SEditableTextBox)
                        .Text(FText::FromString(TEXT("default_vehicle")))
                        .MinDesiredWidth(110.0f)
                    ]

                    + SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
                    [
                        SNew(SButton)
                        .ButtonColorAndOpacity(FLinearColor(0.3f, 0.4f, 0.5f, 1.0f))
                        .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModularGarageWidget::OnSaveProfileClicked))
                        [
                            SNew(STextBlock).Text(FText::FromString(TEXT("💾 KAYDET")))
                        ]
                    ]

                    + SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.0f, 0.0f, 6.0f, 0.0f))
                    [
                        SNew(SButton)
                        .ButtonColorAndOpacity(FLinearColor(0.3f, 0.4f, 0.5f, 1.0f))
                        .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModularGarageWidget::OnLoadProfileClicked))
                        [
                            SNew(STextBlock).Text(FText::FromString(TEXT("📂 YÜKLE")))
                        ]
                    ]

                    + SHorizontalBox::Slot().AutoWidth()
                    [
                        SNew(SButton)
                        .ButtonColorAndOpacity(FLinearColor(0.85f, 0.45f, 0.1f, 1.0f))
                        .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModularGarageWidget::OnBakeClicked))
                        [
                            SNew(STextBlock).Text(FText::FromString(TEXT("🔥 BAKE")))
                        ]
                    ]

                    + SHorizontalBox::Slot().FillWidth(1.0f).HAlign(HAlign_Right).VAlign(VAlign_Center)
                    [
                        SAssignNew(StatusMessageText, STextBlock)
                        .Text(FText::FromString(TEXT("Hazır")))
                        .ColorAndOpacity(FLinearColor(0.0f, 0.88f, 1.0f, 1.0f))
                    ]
                ]
            ]

            // 2. Ana Çalışma Alanı: Sol Panel (Sola Yaslı) + Orta Boşluk (Açık ve Tıklama Geçirgen) + Sağ Panel (Sağa Yaslı)
            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            .Padding(FMargin(10.0f, 5.0f, 10.0f, 10.0f))
            [
                SNew(SHorizontalBox)

                // Sol Panel: Kemik Ağacı (Sola Yaslı, Sabit Genişlik: 330px)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(FMargin(0.0f, 0.0f, 6.0f, 0.0f))
                [
                    SNew(SBox)
                    .WidthOverride(330.0f)
                    [
                        SNew(SBorder)
                        .BorderBackgroundColor(FLinearColor(0.02f, 0.05f, 0.1f, 0.95f))
                        .Padding(FMargin(12.0f))
                        [
                            SNew(SVerticalBox)
                            + SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 8.0f))
                            [
                                SNew(STextBlock)
                                .Text(FText::FromString(TEXT("🦴 KEMİK HİYERARŞİSİ")))
                                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
                                .ColorAndOpacity(FLinearColor(0.0f, 0.88f, 1.0f, 1.0f))
                            ]
                            + SVerticalBox::Slot().FillHeight(1.0f)
                            [
                                SAssignNew(BoneListScrollBox, SScrollBox)
                            ]
                        ]
                    ]
                ]

                // Orta Alan: Ekranın Üçte Biri (%40) Boş ve Tıklama Geçirgen (Viewport'a Ulaşım Sağlar)
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                [
                    SNew(SSpacer)
                    .Visibility(EVisibility::SelfHitTestInvisible)
                ]

                // Sağ Panel: Dinamik Otopark Parametre Paneli (Sağa Yaslı, Sabit Genişlik: 390px)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(FMargin(6.0f, 0.0f, 0.0f, 0.0f))
                [
                    SNew(SBox)
                    .WidthOverride(390.0f)
                    [
                        SNew(SBorder)
                        .BorderBackgroundColor(FLinearColor(0.02f, 0.05f, 0.1f, 0.95f))
                        .Padding(FMargin(14.0f))
                        [
                            SNew(SVerticalBox)

                            // Seçili Kemik Başlığı
                            + SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 8.0f))
                            [
                                SAssignNew(SelectedBoneTitleText, STextBlock)
                                .Text(FText::FromString(TEXT("Bir kemik seçiniz")))
                                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
                                .ColorAndOpacity(FLinearColor::White)
                            ]

                        // Rol Değiştirme Buton Çubuğu
                        + SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 12.0f))
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
                            [
                                SNew(SButton)
                                .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModularGarageWidget::OnAssignRoleClicked, FString(TEXT("Wheel"))))
                                [ SNew(STextBlock).Text(FText::FromString(TEXT("🚗 Tekerlek"))) ]
                            ]
                            + SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
                            [
                                SNew(SButton)
                                .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModularGarageWidget::OnAssignRoleClicked, FString(TEXT("Servo"))))
                                [ SNew(STextBlock).Text(FText::FromString(TEXT("🦾 Servo"))) ]
                            ]
                            + SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
                            [
                                SNew(SButton)
                                .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModularGarageWidget::OnAssignRoleClicked, FString(TEXT("Propeller"))))
                                [ SNew(STextBlock).Text(FText::FromString(TEXT("🛸 Pervane"))) ]
                            ]
                            + SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
                            [
                                SNew(SButton)
                                .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModularGarageWidget::OnAssignRoleClicked, FString(TEXT("LiftBody"))))
                                [ SNew(STextBlock).Text(FText::FromString(TEXT("🪽 Kanat"))) ]
                            ]
                            + SHorizontalBox::Slot().AutoWidth()
                            [
                                SNew(SButton)
                                .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModularGarageWidget::OnAssignRoleClicked, FString(TEXT("None"))))
                                [ SNew(STextBlock).Text(FText::FromString(TEXT("❌ Boş"))) ]
                            ]
                        ]

                        // Dinamik Modül Parametre Slotları
                        + SVerticalBox::Slot().FillHeight(1.0f)
                        [
                            SAssignNew(InspectorSlotScrollBox, SScrollBox)
                        ]
                    ]
                ]
            ]
        ]
    ];
}

// @state: WIP - Hedef araç atamasını yapar ve kemik listesini doldurur
void UPiSimModularGarageWidget::SetTargetVehicle(APiSimVehicle* InVehicle)
{
    TargetVehicle = InVehicle;
    CurrentSelectedBone = NAME_None;
    CurrentActiveModule = nullptr;
    CurrentProperties.Empty();

    RebuildBoneListView();
    RefreshPropertyInspector();
}

// @state: WIP - Sol paneldeki kemik listesini hem Slate hem WebBrowser arayüzüne basar
void UPiSimModularGarageWidget::RebuildBoneListView()
{
    if (!TargetVehicle && GetWorld())
    {
        AActor* Found = UGameplayStatics::GetActorOfClass(GetWorld(), APiSimVehicle::StaticClass());
        if (Found) SetTargetVehicle(Cast<APiSimVehicle>(Found));
    }

    // 1. WebBrowser Entegrasyonu (Modüler Otopark Sözleşmesi)
    if (WebBrowserWidget.IsValid() && TargetVehicle)
    {
        const TArray<FPiSimBoneHierarchyNode>& Hierarchy = TargetVehicle->GetVehicleBoneHierarchy();
        FString JsonList = TEXT("[");
        if (Hierarchy.Num() > 0)
        {
            for (int32 i = 0; i < Hierarchy.Num(); ++i)
            {
                const FPiSimBoneHierarchyNode& Node = Hierarchy[i];
                UActorComponent* Mod = TargetVehicle->GetModuleAtBone(Node.BoneName);
                FString RoleTag = TEXT("None");
                if (Mod)
                {
                    FString ClassName = Mod->GetClass()->GetName();
                    if (ClassName.Contains(TEXT("Wheel"))) RoleTag = TEXT("Wheel");
                    else if (ClassName.Contains(TEXT("Servo"))) RoleTag = TEXT("Servo");
                    else if (ClassName.Contains(TEXT("Propeller"))) RoleTag = TEXT("Propeller");
                    else if (ClassName.Contains(TEXT("LiftBody"))) RoleTag = TEXT("LiftBody");
                }

                if (i > 0) JsonList += TEXT(",");
                JsonList += FString::Printf(TEXT("{\"name\":\"%s\",\"parent\":\"%s\",\"indent\":%d,\"role\":\"%s\"}"),
                    *Node.BoneName.ToString(), *Node.ParentBoneName.ToString(), Node.IndentLevel, *RoleTag);
            }
        }
        else
        {
            TArray<FName> BoneNames;
            TargetVehicle->GetVehicleBoneNames(BoneNames);
            for (int32 i = 0; i < BoneNames.Num(); ++i)
            {
                FName Bone = BoneNames[i];
                UActorComponent* Mod = TargetVehicle->GetModuleAtBone(Bone);
                FString RoleTag = TEXT("None");
                if (Mod)
                {
                    FString ClassName = Mod->GetClass()->GetName();
                    if (ClassName.Contains(TEXT("Wheel"))) RoleTag = TEXT("Wheel");
                    else if (ClassName.Contains(TEXT("Servo"))) RoleTag = TEXT("Servo");
                    else if (ClassName.Contains(TEXT("Propeller"))) RoleTag = TEXT("Propeller");
                    else if (ClassName.Contains(TEXT("LiftBody"))) RoleTag = TEXT("LiftBody");
                }

                if (i > 0) JsonList += TEXT(",");
                JsonList += FString::Printf(TEXT("{\"name\":\"%s\",\"parent\":\"Root\",\"indent\":1,\"role\":\"%s\"}"),
                    *Bone.ToString(), *RoleTag);
            }
        }
        JsonList += TEXT("]");

        FString Script = FString::Printf(TEXT("if (typeof window.onBoneTreeUpdated === 'function') { window.onBoneTreeUpdated(%s); }"), *JsonList);
        WebBrowserWidget->ExecuteJavascript(Script);
    }

    if (!BoneListScrollBox.IsValid())
    {
        return;
    }

    BoneListScrollBox->ClearChildren();

    if (!TargetVehicle)
    {
        BoneListScrollBox->AddSlot()
        [
            SNew(STextBlock).Text(FText::FromString(TEXT("Hedef araç atanmadı."))).ColorAndOpacity(FLinearColor::Red)
        ];
        return;
    }

    TArray<FName> BoneNames;
    TargetVehicle->GetVehicleBoneNames(BoneNames);

    if (BoneNames.Num() == 0)
    {
        BoneListScrollBox->AddSlot()
        [
            SNew(STextBlock).Text(FText::FromString(TEXT("Henüz kemik bulunamadı.\nLütfen üst bardan bir model seçiniz."))).ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f, 1.0f))
        ];
        return;
    }

    const TArray<FPiSimBoneHierarchyNode>& Hierarchy = TargetVehicle->GetVehicleBoneHierarchy();
    if (Hierarchy.Num() > 0)
    {
        for (const FPiSimBoneHierarchyNode& Node : Hierarchy)
        {
            UActorComponent* Mod = TargetVehicle->GetModuleAtBone(Node.BoneName);
            FString ModuleTag = TEXT("🔘 Boş");
            FLinearColor TagColor = FLinearColor(0.5f, 0.5f, 0.5f, 0.8f);

            if (Mod)
            {
                FString ClassName = Mod->GetClass()->GetName();
                if (ClassName.Contains(TEXT("Wheel"))) { ModuleTag = TEXT("🚗 Teker"); TagColor = FLinearColor(0.0f, 0.88f, 1.0f); }
                else if (ClassName.Contains(TEXT("Servo"))) { ModuleTag = TEXT("🦾 Servo"); TagColor = FLinearColor(1.0f, 0.75f, 0.1f); }
                else if (ClassName.Contains(TEXT("Propeller"))) { ModuleTag = TEXT("🛸 İtki"); TagColor = FLinearColor(1.0f, 0.3f, 0.3f); }
                else if (ClassName.Contains(TEXT("LiftBody"))) { ModuleTag = TEXT("🪽 Kanat"); TagColor = FLinearColor(0.2f, 1.0f, 0.45f); }
            }

            float IndentPadding = FMath::Clamp(Node.IndentLevel, 0, 6) * 16.0f;
            FString Prefix = (Node.IndentLevel > 0) ? TEXT("↳ ") : TEXT("📦 ");
            FString DisplayText = Prefix + Node.BoneName.ToString();

            BoneListScrollBox->AddSlot()
                .Padding(FMargin(IndentPadding, 1.5f, 0.0f, 1.5f))
                [
                    SNew(SButton)
                    .ButtonColorAndOpacity(Node.BoneName == CurrentSelectedBone ? FLinearColor(0.0f, 0.45f, 0.8f, 1.0f) : FLinearColor(0.06f, 0.12f, 0.22f, 0.8f))
                    .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModularGarageWidget::OnSelectBoneClicked, Node.BoneName))
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
                        [
                            SNew(STextBlock).Text(FText::FromString(DisplayText)).ColorAndOpacity(FLinearColor::White)
                        ]
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                        [
                            SNew(STextBlock).Text(FText::FromString(ModuleTag)).ColorAndOpacity(TagColor)
                        ]
                    ]
                ];
        }
        return;
    }

    for (const FName& Bone : BoneNames)
    {
        UActorComponent* Mod = TargetVehicle->GetModuleAtBone(Bone);
        FString ModuleTag = TEXT("🔘 Boş");
        FLinearColor TagColor = FLinearColor(0.5f, 0.5f, 0.5f, 0.8f);

        if (Mod)
        {
            FString ClassName = Mod->GetClass()->GetName();
            if (ClassName.Contains(TEXT("Wheel"))) { ModuleTag = TEXT("🚗 Teker"); TagColor = FLinearColor(0.0f, 0.88f, 1.0f); }
            else if (ClassName.Contains(TEXT("Servo"))) { ModuleTag = TEXT("🦾 Servo"); TagColor = FLinearColor(1.0f, 0.75f, 0.1f); }
            else if (ClassName.Contains(TEXT("Propeller"))) { ModuleTag = TEXT("🛸 İtki"); TagColor = FLinearColor(1.0f, 0.3f, 0.3f); }
            else if (ClassName.Contains(TEXT("LiftBody"))) { ModuleTag = TEXT("🪽 Kanat"); TagColor = FLinearColor(0.2f, 1.0f, 0.45f); }
        }

        BoneListScrollBox->AddSlot()
            .Padding(FMargin(0.0f, 1.5f))
            [
                SNew(SButton)
                .ButtonColorAndOpacity(Bone == CurrentSelectedBone ? FLinearColor(0.0f, 0.45f, 0.8f, 1.0f) : FLinearColor(0.06f, 0.12f, 0.22f, 0.8f))
                .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModularGarageWidget::OnSelectBoneClicked, Bone))
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
                    [
                        SNew(STextBlock).Text(FText::FromName(Bone)).ColorAndOpacity(FLinearColor::White)
                    ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [
                        SNew(STextBlock).Text(FText::FromString(ModuleTag)).ColorAndOpacity(TagColor)
                    ]
                ]
            ];
    }
}

// @state: WIP - Kemik seçildiğinde arayüzü günceller
void UPiSimModularGarageWidget::SelectBone(FName BoneName)
{
    CurrentSelectedBone = BoneName;

    if (SelectedBoneTitleText.IsValid())
    {
        SelectedBoneTitleText->SetText(FText::FromString(FString::Printf(TEXT("🦴 Seçili Kemik: %s"), *BoneName.ToString())));
    }

    CurrentActiveModule = nullptr;
    if (TargetVehicle && !BoneName.IsNone())
    {
        UActorComponent* Comp = TargetVehicle->GetModuleAtBone(BoneName);
        if (Comp)
        {
            CurrentActiveModule = Cast<IPiSimInspectableModule>(Comp);
        }
    }

    RebuildBoneListView();
    RefreshPropertyInspector();
}

// @state: WIP - Seçilen modülün IPiSimInspectableModule özelliklerini hem Slate hem WebBrowser slotlarına doldurur
void UPiSimModularGarageWidget::RefreshPropertyInspector()
{
    if (!TargetVehicle && GetWorld())
    {
        AActor* Found = UGameplayStatics::GetActorOfClass(GetWorld(), APiSimVehicle::StaticClass());
        if (Found) SetTargetVehicle(Cast<APiSimVehicle>(Found));
    }

    // 1. WebBrowser Entegrasyonu (Modüler Otopark Sözleşmesi)
    if (WebBrowserWidget.IsValid())
    {
        FString RoleStr = TEXT("None");
        if (CurrentActiveModule)
        {
            UActorComponent* Comp = Cast<UActorComponent>(CurrentActiveModule);
            if (Comp)
            {
                FString ClassName = Comp->GetClass()->GetName();
                if (ClassName.Contains(TEXT("Wheel"))) RoleStr = TEXT("Wheel");
                else if (ClassName.Contains(TEXT("Servo"))) RoleStr = TEXT("Servo");
                else if (ClassName.Contains(TEXT("Propeller"))) RoleStr = TEXT("Propeller");
                else if (ClassName.Contains(TEXT("LiftBody"))) RoleStr = TEXT("LiftBody");
                else RoleStr = ClassName;
            }
        }

        float TestVal = (TargetVehicle && !CurrentSelectedBone.IsNone()) ? TargetVehicle->GetMotorTestValue(CurrentSelectedBone) : 0.0f;

        TArray<FPiSimPropertyDescriptor> Props;
        if (CurrentActiveModule)
        {
            CurrentActiveModule->GetInspectableProperties(Props);
        }

        FString JsonProps = TEXT("[");
        for (int32 i = 0; i < Props.Num(); ++i)
        {
            const FPiSimPropertyDescriptor& P = Props[i];
            if (i > 0) JsonProps += TEXT(",");
            JsonProps += FString::Printf(TEXT("{\"id\":\"%s\",\"name\":\"%s\",\"type\":%d,\"min\":%f,\"max\":%f,\"floatVal\":%f,\"strVal\":\"%s\"}"),
                *P.PropertyId.ToString(), *P.DisplayName, (int32)P.PropertyType, P.MinValue, P.MaxValue, P.CurrentFloatValue, *P.CurrentStringValue);
        }
        JsonProps += TEXT("]");

        FString JsonObj = FString::Printf(TEXT("{\"bone\":\"%s\",\"role\":\"%s\",\"testValue\":%f,\"properties\":%s}"),
            *CurrentSelectedBone.ToString(), *RoleStr, TestVal, *JsonProps);

        FString Script = FString::Printf(TEXT("if (typeof window.onModulePropertiesUpdated === 'function') { window.onModulePropertiesUpdated(%s); }"), *JsonObj);
        WebBrowserWidget->ExecuteJavascript(Script);
    }

    if (!InspectorSlotScrollBox.IsValid())
    {
        return;
    }

    InspectorSlotScrollBox->ClearChildren();
    CurrentProperties.Empty();

    if (CurrentSelectedBone.IsNone())
    {
        InspectorSlotScrollBox->AddSlot()
        [
            SNew(STextBlock).Text(FText::FromString(TEXT("Lütfen soldan bir kemik seçiniz."))).ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f, 1.0f))
        ];
        return;
    }

    // 1. CANLI MOTOR TEST SLIDER'I (22e300b pattern - Canlı test ve doğrudan dönüş)
    float CurrentTestVal = TargetVehicle ? TargetVehicle->GetMotorTestValue(CurrentSelectedBone) : 0.0f;
    TSharedPtr<STextBlock> TestValTextBlock;

    InspectorSlotScrollBox->AddSlot()
        .Padding(FMargin(0.0f, 0.0f, 0.0f, 12.0f))
        [
            SNew(SBorder)
            .BorderBackgroundColor(FLinearColor(0.04f, 0.10f, 0.18f, 0.95f))
            .Padding(FMargin(10.0f))
            [
                SNew(SVerticalBox)
                // Başlık ve Yüzde Değeri
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("⚡ CANLI MOTOR TESTİ")))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                        .ColorAndOpacity(FLinearColor(1.0f, 0.75f, 0.1f, 1.0f))
                    ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [
                        SAssignNew(TestValTextBlock, STextBlock)
                        .Text(FText::FromString(FMath::Abs(CurrentTestVal) < 0.01f ? TEXT("0.0% (DURUYOR)") : FString::Printf(TEXT("%+.1f%%"), CurrentTestVal * 100.0f)))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                        .ColorAndOpacity(FLinearColor(0.2f, 1.0f, 0.5f, 1.0f))
                    ]
                ]
                // Test Slider'ı (-1.0f .. +1.0f)
                + SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 8.0f, 0.0f, 8.0f))
                [
                    SNew(SSlider)
                    .Value((CurrentTestVal + 1.0f) * 0.5f)
                    .OnValueChanged_Lambda([this, TestValTextBlock](float NewAlpha)
                    {
                        float RealVal = (NewAlpha * 2.0f) - 1.0f;
                        if (TestValTextBlock.IsValid())
                        {
                            TestValTextBlock->SetText(FText::FromString(FMath::Abs(RealVal) < 0.01f ? TEXT("0.0% (DURUYOR)") : FString::Printf(TEXT("%+.1f%%"), RealVal * 100.0f)));
                        }

                        if (!TargetVehicle && GetWorld())
                        {
                            AActor* Found = UGameplayStatics::GetActorOfClass(GetWorld(), APiSimVehicle::StaticClass());
                            if (Found) SetTargetVehicle(Cast<APiSimVehicle>(Found));
                        }

                        if (TargetVehicle && !CurrentSelectedBone.IsNone())
                        {
                            TargetVehicle->SetMotorTestValue(CurrentSelectedBone, RealVal);
                        }

                        if (GEngine)
                        {
                            GEngine->AddOnScreenDebugMessage(920, 1.5f, FColor::Yellow,
                                FString::Printf(TEXT("🎛️ [CANLI TEST SLIDER] Kemik: '%s' | Değer: %+.2f (%+.1f%%)"),
                                    *CurrentSelectedBone.ToString(), RealVal, RealVal * 100.0f));
                        }
                        UE_LOG(LogTemp, Warning, TEXT("🎛️ [CANLI TEST SLIDER] Kemik: '%s' | Değer: %+.2f (%+.1f%%)"),
                            *CurrentSelectedBone.ToString(), RealVal, RealVal * 100.0f);
                    })
                ]
                // Sıfırla (Reset) Butonu
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SButton)
                    .ButtonColorAndOpacity(FLinearColor(0.08f, 0.16f, 0.12f, 1.0f))
                    .OnClicked_Lambda([this, TestValTextBlock]() -> FReply
                    {
                        if (!TargetVehicle && GetWorld())
                        {
                            AActor* Found = UGameplayStatics::GetActorOfClass(GetWorld(), APiSimVehicle::StaticClass());
                            if (Found) SetTargetVehicle(Cast<APiSimVehicle>(Found));
                        }

                        if (TargetVehicle && !CurrentSelectedBone.IsNone())
                        {
                            TargetVehicle->SetMotorTestValue(CurrentSelectedBone, 0.0f);
                        }
                        if (TestValTextBlock.IsValid())
                        {
                            TestValTextBlock->SetText(FText::FromString(TEXT("0.0% (DURUYOR)")));
                        }

                        if (GEngine)
                        {
                            GEngine->AddOnScreenDebugMessage(920, 2.0f, FColor::Cyan,
                                FString::Printf(TEXT("🔄 [SLIDER SIFIRLANDI] Kemik: '%s' -> 0.0%%"), *CurrentSelectedBone.ToString()));
                        }
                        RefreshPropertyInspector();
                        return FReply::Handled();
                    })
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("🔄 SIFIRLA / DURDUR (0%)")))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                        .ColorAndOpacity(FLinearColor(0.4f, 1.0f, 0.7f, 1.0f))
                        .Justification(ETextJustify::Center)
                    ]
                ]
            ]
        ];

    if (!CurrentActiveModule)
    {
        InspectorSlotScrollBox->AddSlot()
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("Bu kemiğe atanmış mikro modül bulunmuyor.\nYukarıdaki butonlardan bir rol seçerek modül ekleyebilirsiniz.")))
            .ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f, 1.0f))
        ];
        return;
    }

    CurrentActiveModule->GetInspectableProperties(CurrentProperties);

    for (const FPiSimPropertyDescriptor& Prop : CurrentProperties)
    {
        FName PropId = Prop.PropertyId;

        if (Prop.PropertyType == EPiSimPropertyType::Slider)
        {
            TSharedPtr<STextBlock> ValTextBlock;

            InspectorSlotScrollBox->AddSlot()
                .Padding(FMargin(0.0f, 6.0f))
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().FillWidth(1.0f)
                        [
                            SNew(STextBlock).Text(FText::FromString(Prop.DisplayName)).ColorAndOpacity(FLinearColor(0.8f, 0.9f, 1.0f, 1.0f))
                        ]
                        + SHorizontalBox::Slot().AutoWidth()
                        [
                            SAssignNew(ValTextBlock, STextBlock)
                            .Text(FText::FromString(FString::Printf(TEXT("%.2f"), Prop.CurrentFloatValue)))
                            .ColorAndOpacity(FLinearColor(0.0f, 0.88f, 1.0f, 1.0f))
                        ]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 3.0f, 0.0f, 0.0f))
                    [
                        SNew(SSlider)
                        .Value((Prop.CurrentFloatValue - Prop.MinValue) / FMath::Max(0.001f, Prop.MaxValue - Prop.MinValue))
                        .OnValueChanged_Lambda([this, PropId, Prop, ValTextBlock](float NewAlpha)
                        {
                            float RealVal = Prop.MinValue + (NewAlpha * (Prop.MaxValue - Prop.MinValue));
                            if (ValTextBlock.IsValid())
                            {
                                ValTextBlock->SetText(FText::FromString(FString::Printf(TEXT("%.2f"), RealVal)));
                            }
                            OnSliderValueChanged(PropId, RealVal);
                        })
                    ]
                ];
        }
        else if (Prop.PropertyType == EPiSimPropertyType::Toggle)
        {
            InspectorSlotScrollBox->AddSlot()
                .Padding(FMargin(0.0f, 6.0f))
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1.0f)
                    [
                        SNew(STextBlock).Text(FText::FromString(Prop.DisplayName)).ColorAndOpacity(FLinearColor::White)
                    ]
                    + SHorizontalBox::Slot().AutoWidth()
                    [
                        SNew(SCheckBox)
                        .IsChecked(Prop.CurrentFloatValue > 0.5f ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
                        .OnCheckStateChanged_Lambda([this, PropId](ECheckBoxState NewState)
                        {
                            OnSliderValueChanged(PropId, NewState == ECheckBoxState::Checked ? 1.0f : 0.0f);
                        })
                    ]
                ];
        }
    }
}

void UPiSimModularGarageWidget::OnSliderValueChanged(FName PropertyId, float NewValue)
{
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(921, 2.0f, FColor::Cyan,
            FString::Printf(TEXT("⚙️ [PARAMETRE SLIDER] Kemik: '%s' | Param: '%s' -> %.2f"),
                *CurrentSelectedBone.ToString(), *PropertyId.ToString(), NewValue));
    }
    UE_LOG(LogTemp, Warning, TEXT("⚙️ [PARAMETRE SLIDER] Kemik: '%s' | Param: '%s' -> %.2f"),
        *CurrentSelectedBone.ToString(), *PropertyId.ToString(), NewValue);

    if (CurrentActiveModule)
    {
        CurrentActiveModule->SetInspectablePropertyFloat(PropertyId, NewValue);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ [GARAGE UI SLIDER] '%s' kemiğinde atanmış modül bileşeni bulunamadı!"), *CurrentSelectedBone.ToString());
    }
}

void UPiSimModularGarageWidget::OnTextBoxValueChanged(FName PropertyId, const FString& NewValue)
{
    if (CurrentActiveModule)
    {
        CurrentActiveModule->SetInspectablePropertyString(PropertyId, NewValue);
    }
}

void UPiSimModularGarageWidget::OnTriggerAction(FName ActionId)
{
    if (CurrentActiveModule)
    {
        CurrentActiveModule->TriggerInspectableAction(ActionId);
    }
}

// @state: WIP - Kemiğin motor/modül rolünü değiştirir ve arayüzü günceller
void UPiSimModularGarageWidget::ChangeBoneRole(FName BoneName, const FString& NewRole)
{
    if (!TargetVehicle || BoneName.IsNone())
    {
        return;
    }

    if (NewRole == TEXT("None"))
    {
        TargetVehicle->AssignModuleToBone(BoneName, nullptr);
    }
    else
    {
        UPiSimMotorConfigurator::AssignMotorRoleToBone(TargetVehicle, BoneName, NewRole);
    }

    RebuildBoneListView();
    SelectBone(BoneName);
}

FReply UPiSimModularGarageWidget::OnSelectBoneClicked(FName BoneName)
{
    SelectBone(BoneName);
    return FReply::Handled();
}

FReply UPiSimModularGarageWidget::OnAssignRoleClicked(FString RoleName)
{
    if (!CurrentSelectedBone.IsNone())
    {
        ChangeBoneRole(CurrentSelectedBone, RoleName);
    }
    return FReply::Handled();
}

// @state: WIP - Modeli Değiştir butonuna basıldığında mevcut FBX modelleri arasında döner
FReply UPiSimModularGarageWidget::OnToggleModelClicked()
{
    TArray<FString> Avail = UPiSimModelFbxImporter::GetAvailableCacheModels();
    if (Avail.Num() > 0)
    {
        int32 CurIdx = Avail.IndexOfByKey(ActiveModelFileName);
        int32 NextIdx = (CurIdx + 1) % Avail.Num();
        ActiveModelFileName = Avail[NextIdx];
    }
    else
    {
        if (ActiveModelFileName.Equals(TEXT("robot_import_test.fbx"), ESearchCase::IgnoreCase))
            ActiveModelFileName = TEXT("robot_import_test1.fbx");
        else
            ActiveModelFileName = TEXT("robot_import_test.fbx");
    }

    if (ModelButtonText.IsValid())
    {
        ModelButtonText->SetText(FText::FromString(FString::Printf(TEXT("📦 %s"), *ActiveModelFileName)));
    }

    OnReimportClicked();
    return FReply::Handled();
}

// @state: WIP - İthalat ölçeğini 0.1 ile çarpar
FReply UPiSimModularGarageWidget::OnMultiplyScale01Clicked()
{
    CurrentScaleMultiplier = FMath::Clamp(CurrentScaleMultiplier * 0.1f, 0.001f, 1000.0f);
    OnReimportClicked();
    return FReply::Handled();
}

// @state: WIP - İthalat ölçeğini 10 ile çarpar
FReply UPiSimModularGarageWidget::OnMultiplyScale10Clicked()
{
    CurrentScaleMultiplier = FMath::Clamp(CurrentScaleMultiplier * 10.0f, 0.001f, 1000.0f);
    OnReimportClicked();
    return FReply::Handled();
}

// @state: WIP - Aktif modeli diskten tekrar okur ve kemik listesini tazeler
FReply UPiSimModularGarageWidget::OnReimportClicked()
{
    if (TargetVehicle)
    {
        FString FullPath = FPaths::ProjectSavedDir() / TEXT("Robots/Cache") / ActiveModelFileName;
        bool bOk = UPiSimModelFbxImporter::ImportFbxFileToVehicle(TargetVehicle, FullPath, CurrentScaleMultiplier, bSmoothNormals);
        if (bOk)
        {
            RebuildBoneListView();
            if (StatusMessageText.IsValid())
            {
                StatusMessageText->SetText(FText::FromString(FString::Printf(TEXT("Model: %s (Ölçek: %.2fx)"), *ActiveModelFileName, CurrentScaleMultiplier)));
            }
        }
        else
        {
            if (StatusMessageText.IsValid())
            {
                StatusMessageText->SetText(FText::FromString(FString::Printf(TEXT("HATA: %s bulunamadı!"), *ActiveModelFileName)));
            }
        }
    }
    return FReply::Handled();
}

// @state: WIP - Görsel model ile UCX çarpışma zırhı arasındaki görünürlüğü değiştirir
FReply UPiSimModularGarageWidget::OnToggleCollisionViewClicked()
{
    if (TargetVehicle)
    {
        bool bNewShow = !TargetVehicle->IsCollisionMeshViewEnabled();
        TargetVehicle->SetCollisionMeshViewEnabled(bNewShow);
        if (CollisionButtonText.IsValid())
        {
            CollisionButtonText->SetText(FText::FromString(bNewShow ? TEXT("🛡️ UCX ZIRH MODU") : TEXT("🎨 GÖRSEL MOD")));
        }
    }
    return FReply::Handled();
}

// @state: WIP - Fizik simülasyonunu açıp kapatır
FReply UPiSimModularGarageWidget::OnTogglePhysicsClicked()
{
    if (!TargetVehicle && GetWorld())
    {
        AActor* Found = UGameplayStatics::GetActorOfClass(GetWorld(), APiSimVehicle::StaticClass());
        if (Found) SetTargetVehicle(Cast<APiSimVehicle>(Found));
    }

    if (TargetVehicle)
    {
        bool bActive = TargetVehicle->IsVehiclePhysicsActive();
        TargetVehicle->SetVehiclePhysicsEnabled(!bActive);
        if (PhysicsButtonText.IsValid())
        {
            PhysicsButtonText->SetText(FText::FromString(!bActive ? TEXT("⚡ FİZİK AKTİF") : TEXT("⚡ FİZİK SİMÜLE ET")));
        }
        if (StatusMessageText.IsValid())
        {
            StatusMessageText->SetText(FText::FromString(!bActive ? TEXT("🟢 Fizik Aktif") : TEXT("🔴 Fizik Kapalı")));
        }
    }
    else
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("❌ [HATA] Sahnedeki APiSimVehicle aktörü bulunamadı!"));
        }
    }
    return FReply::Handled();
}

// @state: WIP - Otomatik motor rollerini atar
FReply UPiSimModularGarageWidget::OnAutoAssignMotorsClicked()
{
    if (TargetVehicle)
    {
        UPiSimMotorConfigurator::AutoAssignMotorsByBoneNames(TargetVehicle);
        SelectBone(CurrentSelectedBone);
        if (StatusMessageText.IsValid())
        {
            StatusMessageText->SetText(FText::FromString(TEXT("🤖 Otomatik motor ataması yapıldı.")));
        }
    }
    return FReply::Handled();
}

// @state: WIP - Aracı verilen veya mevcut isimle JSON profili olarak kaydeder
bool UPiSimModularGarageWidget::SaveProfile(const FString& InProfileName)
{
    if (!TargetVehicle && GetWorld())
    {
        TArray<AActor*> FoundVehicles;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), APiSimVehicle::StaticClass(), FoundVehicles);
        if (FoundVehicles.Num() > 0)
        {
            TargetVehicle = Cast<APiSimVehicle>(FoundVehicles[0]);
        }
    }

    if (!TargetVehicle)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ [UPiSimModularGarageWidget::SaveProfile] Hedef araç bulunamadı!"));
        return false;
    }

    FString ActualProfileName = InProfileName.TrimStartAndEnd();
    if (ActualProfileName.IsEmpty())
    {
        if (ProfileNameInput.IsValid() && !ProfileNameInput->GetText().IsEmpty())
        {
            ActualProfileName = ProfileNameInput->GetText().ToString().TrimStartAndEnd();
        }
        else if (!ActiveModelFileName.IsEmpty())
        {
            ActualProfileName = FPaths::GetBaseFilename(ActiveModelFileName);
        }
        else
        {
            ActualProfileName = FString::Printf(TEXT("Ozel_Model_%d"), FMath::RandRange(10, 99));
        }
    }

    ActualProfileName = ActualProfileName.Replace(TEXT(" "), TEXT("_"));

    FString OutMsg;
    bool bSuccess = UPiSimSaveManager::SaveProfile(TargetVehicle, ActualProfileName, OutMsg);

    UE_LOG(LogTemp, Warning, TEXT("💾 [GarageWidget::SaveProfile] Profil: '%s' -> Sonuç: %d (%s)"),
        *ActualProfileName, bSuccess, *OutMsg);

    if (StatusMessageText.IsValid())
    {
        StatusMessageText->SetText(FText::FromString(OutMsg));
    }

    if (WebBrowserWidget)
    {
        FString Script = FString::Printf(TEXT("if (typeof window.onProfileSaved === 'function') { window.onProfileSaved('%s', %s, '%s'); }"),
            *ActualProfileName, bSuccess ? TEXT("true") : TEXT("false"), *OutMsg);
        WebBrowserWidget->ExecuteJavascript(Script);
    }

    return bSuccess;
}

FReply UPiSimModularGarageWidget::OnSaveProfileClicked()
{
    FString NameToUse = ProfileNameInput.IsValid() ? ProfileNameInput->GetText().ToString() : TEXT("");
    SaveProfile(NameToUse);
    return FReply::Handled();
}

FReply UPiSimModularGarageWidget::OnLoadProfileClicked()
{
    if (TargetVehicle && ProfileNameInput.IsValid())
    {
        FString OutMsg;
        UPiSimSaveManager::LoadProfile(TargetVehicle, ProfileNameInput->GetText().ToString(), OutMsg);
        SelectBone(CurrentSelectedBone);
        if (StatusMessageText.IsValid())
        {
            StatusMessageText->SetText(FText::FromString(OutMsg));
        }
    }
    return FReply::Handled();
}

FReply UPiSimModularGarageWidget::OnBakeClicked()
{
    if (TargetVehicle)
    {
        FString Summary;
        UPiSimBaker::BakeVehicle(TargetVehicle, Summary);
        if (StatusMessageText.IsValid())
        {
            StatusMessageText->SetText(FText::FromString(Summary));
        }
    }
    return FReply::Handled();
}

APiSimSpectatorPawn* UPiSimModularGarageWidget::GetOrCreateSpectatorPawn()
{
    if (!GetWorld()) return nullptr;

    APlayerController* PC = GetOwningPlayer();
    if (!PC) PC = GetWorld()->GetFirstPlayerController();

    APiSimSpectatorPawn* Spectator = PC ? Cast<APiSimSpectatorPawn>(PC->GetPawn()) : nullptr;
    if (!Spectator)
    {
        Spectator = Cast<APiSimSpectatorPawn>(UGameplayStatics::GetActorOfClass(GetWorld(), APiSimSpectatorPawn::StaticClass()));
    }
    if (!Spectator && PC)
    {
        FVector SpawnLoc = FVector(-450.0f, 0.0f, 280.0f);
        FRotator SpawnRot = FRotator(-20.0f, 0.0f, 0.0f);
        if (PC->PlayerCameraManager)
        {
            SpawnLoc = PC->PlayerCameraManager->GetCameraLocation();
            SpawnRot = PC->PlayerCameraManager->GetCameraRotation();
        }
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Spectator = GetWorld()->SpawnActor<APiSimSpectatorPawn>(APiSimSpectatorPawn::StaticClass(), SpawnLoc, SpawnRot, SpawnParams);
        if (Spectator)
        {
            Spectator->DashboardWidget = this;
            PC->SetViewTarget(Spectator);
        }
    }
    return Spectator;
}

void UPiSimModularGarageWidget::MouseDelta(float DeltaX, float DeltaY, int32 Mode)
{
    if (APiSimSpectatorPawn* Spectator = GetOrCreateSpectatorPawn())
    {
        APlayerController* PC = GetOwningPlayer() ? GetOwningPlayer() : GetWorld()->GetFirstPlayerController();
        if (PC && PC->GetViewTarget() != Spectator)
        {
            PC->SetViewTarget(Spectator);
        }
        Spectator->AddOrbitDelta(DeltaX, DeltaY);
    }

    if (TargetVehicle)
    {
        if (Mode == 0)
        {
            TargetVehicle->AddOrbitDelta(DeltaX, DeltaY);
        }
        else if (Mode == 1)
        {
            TargetVehicle->AddPanDelta(DeltaX, DeltaY);
        }
    }
}

void UPiSimModularGarageWidget::MouseZoom(float Direction)
{
    if (APiSimSpectatorPawn* Spectator = GetOrCreateSpectatorPawn())
    {
        Spectator->AddZoomDelta(Direction);
    }

    if (TargetVehicle)
    {
        TargetVehicle->AddZoomDelta(Direction);
    }
}

void UPiSimModularGarageWidget::FlyCamera(float Forward, float Right, float Up, float Boost)
{
    APiSimSpectatorPawn* Spectator = GetOrCreateSpectatorPawn();
    if (!Spectator) return;

    APlayerController* PC = GetOwningPlayer() ? GetOwningPlayer() : (GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr);
    if (PC && PC->GetViewTarget() != Spectator)
    {
        PC->SetViewTarget(Spectator);
    }

    // Eğer Orbit veya FPV modundayken uçuş tuşlarına basıldıysa serbest editör uçuşuna geç
    if (Spectator->bIsOrbitingVehicle || Spectator->IsFpvCameraMode())
    {
        Spectator->ReturnToFreeFlight();
    }

    APlayerController* PC2 = GetOwningPlayer() ? GetOwningPlayer() : (GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr);
    FRotator CamRot = (PC2 && PC2->PlayerCameraManager) ? PC2->PlayerCameraManager->GetCameraRotation() : Spectator->GetActorRotation();
    const FRotationMatrix RotMat(CamRot);
    const FVector FwdDir   = RotMat.GetUnitAxis(EAxis::X);
    const FVector RightDir = RotMat.GetUnitAxis(EAxis::Y);
    const FVector UpDir    = FVector::UpVector;

    FVector MoveVec = (FwdDir * Forward) + (RightDir * Right) + (UpDir * Up);
    if (!MoveVec.IsNearlyZero())
    {
        float StepDist = Spectator->FlySpeedMultiplier * FMath::Max(0.2f, Boost) * 0.016f;
        Spectator->AddActorWorldOffset(MoveVec.GetSafeNormal() * StepDist, false);
    }
}

// @state: WIP - WebUI üzerinden gelen motor canlı test değerini (-1.0 .. +1.0) araca iletir
void UPiSimModularGarageWidget::SetMotorTestValue(FName BoneName, float Value)
{
    if (!TargetVehicle && GetWorld())
    {
        AActor* Found = UGameplayStatics::GetActorOfClass(GetWorld(), APiSimVehicle::StaticClass());
        if (Found) SetTargetVehicle(Cast<APiSimVehicle>(Found));
    }

    if (TargetVehicle && !BoneName.IsNone())
    {
        TargetVehicle->SetMotorTestValue(BoneName, Value);
    }
}

// @state: WIP - WebUI sayfa açıldığında veya model yenilendiğinde araç kemik ve modül durumunu tazeler
void UPiSimModularGarageWidget::RequestVehicleState()
{
    if (!TargetVehicle && GetWorld())
    {
        AActor* Found = UGameplayStatics::GetActorOfClass(GetWorld(), APiSimVehicle::StaticClass());
        if (Found) SetTargetVehicle(Cast<APiSimVehicle>(Found));
    }

    RebuildBoneListView();

    if (CurrentSelectedBone.IsNone() && TargetVehicle)
    {
        TArray<FName> BoneNames;
        TargetVehicle->GetVehicleBoneNames(BoneNames);
        if (BoneNames.Num() > 0)
        {
            SelectBone(BoneNames[0]);
            return;
        }
    }

    RefreshPropertyInspector();
}

// @state: WIP - WebUI arayüzünden 3D kemik teşhis çizgilerini açıp kapatır ('U' tuşu karşılığı)
void UPiSimModularGarageWidget::ToggleDebugDraw()
{
    if (!TargetVehicle && GetWorld())
    {
        AActor* Found = UGameplayStatics::GetActorOfClass(GetWorld(), APiSimVehicle::StaticClass());
        if (Found) SetTargetVehicle(Cast<APiSimVehicle>(Found));
    }

    if (TargetVehicle && TargetVehicle->MotorDebugComponent)
    {
        TargetVehicle->MotorDebugComponent->ToggleDebugDraw();
    }
}

// @state: WIP - Saved/Vehicles/ altındaki kayıtlı tüm araç profillerini JSON listesi olarak döner
FString UPiSimModularGarageWidget::GetSavedProfilesJson()
{
    TArray<FString> Profiles;
    UPiSimSaveManager::GetSavedProfileNames(Profiles);

    TArray<FString> Defaults = { TEXT("Quadcopter_X4"), TEXT("Sabit_Kanat_Delta"), TEXT("Tarla_Rover_4x4") };
    for (const FString& Def : Defaults)
    {
        if (!Profiles.Contains(Def))
        {
            Profiles.Add(Def);
        }
    }

    FString JsonStr = TEXT("[");
    for (int32 i = 0; i < Profiles.Num(); ++i)
    {
        JsonStr += FString::Printf(TEXT("\"%s\""), *Profiles[i]);
        if (i < Profiles.Num() - 1)
        {
            JsonStr += TEXT(",");
        }
    }
    JsonStr += TEXT("]");

    if (WebBrowserWidget.IsValid())
    {
        FString Script = FString::Printf(TEXT("if (typeof window.onSavedProfilesLoaded === 'function') { window.onSavedProfilesLoaded('%s'); }"), *JsonStr);
        WebBrowserWidget->ExecuteJavascript(Script);
    }

    return JsonStr;
}

// @state: WIP - WebUI: Haritada fareyle tıklanan noktaya seçilen aracı ışınlar veya spawn eder
bool UPiSimModularGarageWidget::SpawnVehicleAtCursor(const FString& ProfileName)
{
    APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!PC)
    {
        return false;
    }

    FHitResult HitResult;
    bool bHit = PC->GetHitResultUnderCursor(ECC_Visibility, false, HitResult);
    FVector TargetLocation = bHit ? (HitResult.ImpactPoint + FVector(0.0f, 0.0f, 60.0f)) : FVector(0.0f, 0.0f, 100.0f);

    UE_LOG(LogTemp, Warning, TEXT("🚀 [UPiSimModularGarageWidget] Araç Konuşlanıyor: '%s' -> (X=%.1f, Y=%.1f, Z=%.1f)"),
        *ProfileName, TargetLocation.X, TargetLocation.Y, TargetLocation.Z);

    // İlk araç henüz boş ise mevcudu oraya ışınla, yoksa filoya yeni bağımsız araç ekle
    APiSimVehicle* VehicleToInit = nullptr;
    if (FleetVehicles.Num() == 0 || (FleetVehicles.Num() == 1 && FleetVehicles[0]->StoredVisualSections.Num() == 0))
    {
        if (FleetVehicles.Num() > 0 && FleetVehicles[0])
        {
            VehicleToInit = FleetVehicles[0];
            VehicleToInit->TeleportTo(TargetLocation, FRotator::ZeroRotator, false, true);
        }
    }

    if (!VehicleToInit && GetWorld())
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        VehicleToInit = GetWorld()->SpawnActor<APiSimVehicle>(APiSimVehicle::StaticClass(), TargetLocation, FRotator::ZeroRotator, SpawnParams);
        if (VehicleToInit)
        {
            VehicleToInit->InitVehicleIdentity(0);
            FleetVehicles.Add(VehicleToInit);
        }
    }

    if (VehicleToInit)
    {
        // Varsayılan olarak tüm kemik ve debug görsel/metinlerini kapat
        VehicleToInit->SetDebugVisualsEnabled(false);

        FString OutMsg;
        UPiSimSaveManager::LoadProfile(VehicleToInit, ProfileName, OutMsg);

        // Profil yüklendikten sonra da debug görsellerinin kapalı kaldığını garanti et
        VehicleToInit->SetDebugVisualsEnabled(false);

        // KURAL: Yeni araç spawn olduğunda kamerayı otomatik olarak o araca KİLİTLEME!
        // Editör kamerası serbest uçuşta (Free-Flight) kalır; sadece sol panelden seçildiğinde Orbit'e geçer.
        if (!TargetVehicle)
        {
            TargetVehicle = VehicleToInit;
        }

        if (WebBrowserWidget.IsValid())
        {
            FString FleetJson = GetActiveFleetJson();
            FString CleanFleetJson = FleetJson.Replace(TEXT("\n"), TEXT(" ")).Replace(TEXT("\r"), TEXT(""));
            FString FleetScript = FString::Printf(TEXT("if (typeof onFleetSyncedFromCpp === 'function') { onFleetSyncedFromCpp(%s); }"), *CleanFleetJson);
            WebBrowserWidget->ExecuteJavascript(FleetScript);
        }

        if (VehicleToInit->bShowDebugVisualsAndMessages && GEngine)
        {
            GEngine->AddOnScreenDebugMessage(810, 4.0f, FColor::Green,
                FString::Printf(TEXT("✅ [%s] Filoya Eklendi! (Kod: %s, ID: %d)"),
                    *ProfileName, *VehicleToInit->VehicleCode, VehicleToInit->VehicleId));
        }
        return true;
    }

    return false;
}

// @state: WIP - Aktif filodaki tüm araçları canlı telemetri bilgileriyle birlikte JSON dizisi olarak döner
FString UPiSimModularGarageWidget::GetActiveFleetJson()
{
    TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject);
    TArray<TSharedPtr<FJsonValue>> FleetArray;

    FleetVehicles.RemoveAll([](APiSimVehicle* V) { return !IsValid(V); });

    if (GetWorld())
    {
        TArray<AActor*> AllWorldVehicles;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), APiSimVehicle::StaticClass(), AllWorldVehicles);
        for (AActor* Act : AllWorldVehicles)
        {
            APiSimVehicle* V = Cast<APiSimVehicle>(Act);
            if (V && !FleetVehicles.Contains(V))
            {
                FleetVehicles.Add(V);
            }
        }
    }

    APiSimSpectatorPawn* Spectator = GetOrCreateSpectatorPawn();

    for (APiSimVehicle* Vehicle : FleetVehicles)
    {
        if (!Vehicle) continue;

        if (Vehicle->VehicleCode.IsEmpty() || Vehicle->VehicleId <= 0)
        {
            Vehicle->InitVehicleIdentity(0);
        }

        TSharedPtr<FJsonObject> VObj = MakeShareable(new FJsonObject);
        VObj->SetNumberField(TEXT("id"), Vehicle->VehicleId);
        VObj->SetStringField(TEXT("code"), Vehicle->VehicleCode);
        VObj->SetStringField(TEXT("name"), Vehicle->VehicleName);
        VObj->SetBoolField(TEXT("is_focused"), Vehicle->bIsFocusedVehicle);

        int32 SitlListen = Vehicle->AutopilotBridge ? Vehicle->AutopilotBridge->ListenPort : (9002 + Vehicle->VehicleId * 10);
        int32 SitlSend = Vehicle->AutopilotBridge ? Vehicle->AutopilotBridge->SendPort : (9003 + Vehicle->VehicleId * 10);
        int32 TelemetryPort = 7400;
        int32 VideoPort = 7401;
        int32 DedicatedCmdPort = 7402 + Vehicle->VehicleId;
        bool bVideoSub = Vehicle->Pi5Bridge ? Vehicle->Pi5Bridge->IsVideoSubscribed() : false;

        VObj->SetNumberField(TEXT("sitl_listen_port"), SitlListen);
        VObj->SetNumberField(TEXT("sitl_send_port"), SitlSend);
        VObj->SetNumberField(TEXT("telemetry_port"), TelemetryPort);
        VObj->SetNumberField(TEXT("video_port"), VideoPort);
        VObj->SetNumberField(TEXT("dedicated_port"), DedicatedCmdPort);
        VObj->SetBoolField(TEXT("video_subscribed"), bVideoSub);

        FVector Loc = Vehicle->GetActorLocation();
        FRotator Rot = Vehicle->GetActorRotation();

        VObj->SetNumberField(TEXT("loc_x"), Loc.X);
        VObj->SetNumberField(TEXT("loc_y"), Loc.Y);
        VObj->SetNumberField(TEXT("loc_z"), Loc.Z);
        VObj->SetNumberField(TEXT("roll"), Rot.Roll);
        VObj->SetNumberField(TEXT("pitch"), Rot.Pitch);
        VObj->SetNumberField(TEXT("yaw"), Rot.Yaw);

        float SpeedKmh = 0.0f;
        double Lat = 39.9334 + (Loc.X / 100.0) / 111320.0;
        double Lon = 32.8597 + (Loc.Y / 100.0) / (111320.0 * FMath::Cos(FMath::DegreesToRadians(39.9334)));
        float AltM = 850.0f + (Loc.Z / 100.0f);

        if (Vehicle->TelemetryGateway)
        {
            FPiSimGPSData Gps;
            if (Vehicle->TelemetryGateway->GetLatestGPSData(Gps))
            {
                SpeedKmh = Gps.SpeedKmh;
                if (FMath::Abs(Gps.Latitude) > 0.001) Lat = Gps.Latitude;
                if (FMath::Abs(Gps.Longitude) > 0.001) Lon = Gps.Longitude;
                AltM = Gps.AltitudeMeters;
            }
        }

        VObj->SetNumberField(TEXT("speed"), FMath::RoundToFloat(SpeedKmh * 10.0f) / 10.0f);
        VObj->SetNumberField(TEXT("lat"), Lat);
        VObj->SetNumberField(TEXT("lon"), Lon);
        VObj->SetNumberField(TEXT("altitude"), FMath::RoundToFloat(AltM * 10.0f) / 10.0f);
        VObj->SetNumberField(TEXT("battery"), 98);
        bool bIsFpv = (Spectator && Spectator->IsFpvCameraMode() && Spectator->FocusedVehicle == Vehicle);
        VObj->SetBoolField(TEXT("is_fpv"), bIsFpv);

        FleetArray.Add(MakeShareable(new FJsonValueObject(VObj)));
    }

    FString CamModeStr = TEXT("FREE");
    if (Spectator)
    {
        if (Spectator->IsFpvCameraMode()) CamModeStr = TEXT("FPV");
        else if (Spectator->bIsOrbitingVehicle) CamModeStr = TEXT("ORBIT");
    }
    RootObject->SetStringField(TEXT("camera_mode"), CamModeStr);
    RootObject->SetArrayField(TEXT("fleet"), FleetArray);

    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);
    return OutputString;
}

// @state: WIP - Sol panelden seçilen araca 360° Orbit modunda odaklanır
bool UPiSimModularGarageWidget::FocusVehicle(int32 InVehicleId)
{
    APlayerController* PC = GetOwningPlayer();
    if (!PC && GetWorld())
    {
        PC = GetWorld()->GetFirstPlayerController();
    }

    APiSimVehicle* FoundVehicle = nullptr;
    for (APiSimVehicle* Vehicle : FleetVehicles)
    {
        if (Vehicle)
        {
            if (Vehicle->VehicleId == InVehicleId)
            {
                Vehicle->bIsFocusedVehicle = true;
                FoundVehicle = Vehicle;
            }
            else
            {
                Vehicle->bIsFocusedVehicle = false;
            }
        }
    }

    if (FoundVehicle)
    {
        SetTargetVehicle(FoundVehicle);
        APiSimSpectatorPawn* Spectator = GetOrCreateSpectatorPawn();
        if (Spectator)
        {
            Spectator->SetFocusedVehicle(FoundVehicle);
            if (PC && PC->GetViewTarget() != Spectator)
            {
                PC->SetViewTargetWithBlend(Spectator, 0.25f);
            }
        }
        else if (PC)
        {
            PC->SetViewTargetWithBlend(FoundVehicle, 0.4f);
        }
        UE_LOG(LogTemp, Log, TEXT("🎯 [UPiSimModularGarageWidget] Araç %d Orbit Moduna Odaklandı!"), InVehicleId);
        return true;
    }

    return false;
}

// @state: WIP - Seçili aracın kendi üst kamerasına (FPV) kilitlenir (Her tick aynı transform)
bool UPiSimModularGarageWidget::EnterVehicleCameraView(int32 InVehicleId)
{
    APlayerController* PC = GetOwningPlayer();
    if (!PC && GetWorld())
    {
        PC = GetWorld()->GetFirstPlayerController();
    }

    APiSimVehicle* FoundVehicle = nullptr;
    for (APiSimVehicle* Vehicle : FleetVehicles)
    {
        if (Vehicle && Vehicle->VehicleId == InVehicleId)
        {
            FoundVehicle = Vehicle;
            break;
        }
    }
    if (!FoundVehicle)
    {
        FoundVehicle = TargetVehicle;
    }
    if (!FoundVehicle)
    {
        return false;
    }

    SetTargetVehicle(FoundVehicle);
    APiSimSpectatorPawn* Spectator = GetOrCreateSpectatorPawn();
    if (Spectator)
    {
        Spectator->EnterVehicleCameraMode(FoundVehicle);
        if (PC && PC->GetViewTarget() != Spectator)
        {
            PC->SetViewTarget(Spectator);
        }
        return true;
    }
    return false;
}

// @state: WIP - Araç kamerasından (FPV) veya Orbit'ten çıkıp ana serbest editör kamerasına döner (ESC)
void UPiSimModularGarageWidget::ExitVehicleCameraView()
{
    APlayerController* PC = GetOwningPlayer();
    if (!PC && GetWorld())
    {
        PC = GetWorld()->GetFirstPlayerController();
    }

    for (APiSimVehicle* Vehicle : FleetVehicles)
    {
        if (Vehicle)
        {
            Vehicle->bIsFocusedVehicle = false;
        }
    }

    APiSimSpectatorPawn* Spectator = GetOrCreateSpectatorPawn();
    if (Spectator)
    {
        Spectator->ReturnToFreeFlight();
        if (PC && PC->GetViewTarget() != Spectator)
        {
            PC->SetViewTarget(Spectator);
        }
    }
}

// @state: WIP - Belirtilen aracın video yayını aboneliğini açıp kapatır (On-Demand Video)
void UPiSimModularGarageWidget::SetVehicleVideoSubscribed(int32 InVehicleId, bool bSubscribe)
{
    for (APiSimVehicle* Vehicle : FleetVehicles)
    {
        if (Vehicle && Vehicle->VehicleId == InVehicleId)
        {
            FString TargetIp = Vehicle->Pi5Bridge ? Vehicle->Pi5Bridge->TargetIP : TEXT("127.0.0.1");
            int32 VideoPort = Vehicle->Pi5Bridge ? Vehicle->Pi5Bridge->VideoPort : (7401 + Vehicle->VehicleId * 10);
            if (bSubscribe)
            {
                Vehicle->AddVideoSubscriber(TargetIp, VideoPort);
            }
            else
            {
                Vehicle->RemoveVideoSubscriber(TargetIp, VideoPort);
            }

            if (Vehicle->Pi5Bridge)
            {
                Vehicle->Pi5Bridge->SetVideoSubscribed(bSubscribe);
            }
            return;
        }
    }
}

// @state: WIP - Belirtilen aracın 1 dakikalık periyotlarla kaydedilen telemetri tablosunu JSON olarak çeker
FString UPiSimModularGarageWidget::GetTelemetryTableJson(int32 InVehicleId, int32 MaxRows)
{
    for (APiSimVehicle* Vehicle : FleetVehicles)
    {
        if (Vehicle && Vehicle->VehicleId == InVehicleId)
        {
            if (Vehicle->TelemetryLogger)
            {
                return Vehicle->TelemetryLogger->GetRecentTelemetryTableJson(MaxRows);
            }
        }
    }

    if (TargetVehicle && TargetVehicle->TelemetryLogger)
    {
        return TargetVehicle->TelemetryLogger->GetRecentTelemetryTableJson(MaxRows);
    }

    return TEXT("{\"total_logged\":0,\"rows\":[]}");
}

// @state: WIP - Tüm filodaki birikmiş telemetri satırlarını anında SQL dosyasına kaydeder
void UPiSimModularGarageWidget::FlushTelemetryToSql()
{
    for (APiSimVehicle* Vehicle : FleetVehicles)
    {
        if (Vehicle && Vehicle->TelemetryLogger)
        {
            Vehicle->TelemetryLogger->FlushBatchToSql();
        }
    }

    if (TargetVehicle && TargetVehicle->bShowDebugVisualsAndMessages && GEngine)
    {
        GEngine->AddOnScreenDebugMessage(811, 3.5f, FColor::Cyan,
            TEXT("💾 [SQL Telemetri] 1 Dakikalık Batch Veritabanına Başarıyla Yazıldı (Saved/Telemetry/flight_telemetry.sql)"));
    }
}

// @state: WIP - WebUI sayfaları arasında geçiş yapar (garage.html <-> dashboardUnreal.html)
void UPiSimModularGarageWidget::NavigateToPage(const FString& PageFileName)
{
    FString HtmlFilePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Source/PiSim/6_UI/WebUI") / PageFileName);
    FString FileUrl = FString::Printf(TEXT("file:///%s"), *HtmlFilePath);

    if (WebBrowserWidget.IsValid() && FPaths::FileExists(HtmlFilePath))
    {
        UE_LOG(LogTemp, Warning, TEXT("[UPiSimModularGarageWidget] Sayfa Geçişi: %s"), *FileUrl);
        WebBrowserWidget->LoadURL(FileUrl);
    }
}

// @state: WIP - WebUI'dan probe talebi geldiğinde TelemetryGateway üzerinden fırlatır
void UPiSimModularGarageWidget::TriggerBeaconProbe()
{
    UPiSimTelemetryGateway::BroadcastProbe(TEXT("HI FROM UNREAL"));
}

// @state: WIP - WebUI'dan manuel IP ayarlandığında Pi5Bridge köprüsüne iletir ve görsel arayüzü günceller
bool UPiSimModularGarageWidget::SetTargetIP(const FString& InIP, int32 InPort)
{
    int32 SafePort = (InPort >= 9000 || InPort <= 0) ? 7400 : InPort;
    APiSimVehicle* VehicleToConfig = TargetVehicle ? TargetVehicle : (FleetVehicles.Num() > 0 ? FleetVehicles[0] : nullptr);
    if (VehicleToConfig)
    {
        if (VehicleToConfig->Pi5Bridge)
        {
            bool bSuccess = VehicleToConfig->Pi5Bridge->SetTargetIP(InIP, SafePort);

            if (WebBrowserWidget.IsValid())
            {
                FString Script = FString::Printf(TEXT("if (typeof setUeBridgeNodesActive === 'function') { setUeBridgeNodesActive(true, '%s', %d); }"), *InIP, SafePort);
                WebBrowserWidget->ExecuteJavascript(Script);
            }

            return bSuccess;
        }
    }

    return false;
}

// @state: WIP - Pi5Bridge tarafından yakalanan beacon paketini WebUI arayüzüne iletir (Görsel Katman)
void UPiSimModularGarageWidget::HandlePi5BeaconReceived(const FString& BeaconJson)
{
    UE_LOG(LogTemp, Warning, TEXT("📡 [GarageWidget] Pi 5 Beacon Alındı ve Web Arayüzüne İletiliyor: %s"), *BeaconJson);
    AsyncTask(ENamedThreads::GameThread, [this, BeaconJson]()
    {
        if (WebBrowserWidget.IsValid())
        {
            FString CleanJson = BeaconJson.Replace(TEXT("\n"), TEXT(" ")).Replace(TEXT("\r"), TEXT(""));
            FString Script = FString::Printf(TEXT("if (typeof onBeaconDeviceDiscovered === 'function') { onBeaconDeviceDiscovered(%s); }"), *CleanJson);
            WebBrowserWidget->ExecuteJavascript(Script);
        }
    });
}

