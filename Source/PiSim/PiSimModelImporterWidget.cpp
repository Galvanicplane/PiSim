// PiSimModelImporterWidget.cpp
// Clean, Dedicated Slate-Powered 3-Tab Studio Widget for PiSimModelImporter.

#include "PiSimModelImporterWidget.h"
#include "PiSimModelImporter.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Images/SImage.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"

TSharedRef<SWidget> UPiSimModelImporterWidget::RebuildWidget()
{
    if (!TargetImporter && GetWorld())
    {
        AActor* Found = UGameplayStatics::GetActorOfClass(GetWorld(), APiSimModelImporter::StaticClass());
        TargetImporter = Cast<APiSimModelImporter>(Found);
    }

    FSlateFontInfo HeaderTitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 12);
    FSlateFontInfo TabFont = FCoreStyle::GetDefaultFontStyle("Bold", 10);
    FSlateFontInfo CardHeaderFont = FCoreStyle::GetDefaultFontStyle("Bold", 10);
    FSlateFontInfo DataFont = FCoreStyle::GetDefaultFontStyle("Regular", 9);
    FSlateFontInfo BadgeFont = FCoreStyle::GetDefaultFontStyle("Bold", 9);
    FSlateFontInfo ButtonFont = FCoreStyle::GetDefaultFontStyle("Bold", 9);

    return SNew(SOverlay)
        // ---------------------------------------------------------------------
        // 1) TOP HEADER BAR: TITLE, STATUS & GLOBAL CONTROLS
        // ---------------------------------------------------------------------
        + SOverlay::Slot()
        .HAlign(HAlign_Fill)
        .VAlign(VAlign_Top)
        [
            SNew(SBorder)
            .BorderBackgroundColor(FLinearColor(0.012f, 0.025f, 0.06f, 0.98f))
            .Padding(FMargin(16.0f, 8.0f))
            [
                SNew(SHorizontalBox)

                // Title
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("⚡ PiSim // ROBOT STUDIO")))
                    .Font(HeaderTitleFont)
                    .ColorAndOpacity(FLinearColor(0.0f, 0.88f, 1.0f, 1.0f))
                ]

                // Live Dynamic Connection Badge
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(FMargin(14.0f, 0.0f, 0.0f, 0.0f))
                [
                    SAssignNew(ConnectionBadgeBorder, SBorder)
                    .BorderBackgroundColor(FLinearColor(0.25f, 0.18f, 0.02f, 0.95f))
                    .Padding(FMargin(8.0f, 3.0f))
                    [
                        SAssignNew(ConnectionBadgeText, STextBlock)
                        .Text(FText::FromString(TEXT("🟡 PI 5 BEKLENİYOR (Port 7400)")))
                        .Font(BadgeFont)
                        .ColorAndOpacity(FLinearColor(1.0f, 0.85f, 0.2f, 1.0f))
                    ]
                ]

                + SHorizontalBox::Slot().FillWidth(1.0f) // Spacer

                // Scale 0.1X
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(FMargin(2.0f, 0.0f))
                [
                    SNew(SButton)
                    .ButtonColorAndOpacity(FLinearColor(0.08f, 0.16f, 0.28f, 1.0f))
                    .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModelImporterWidget::OnScale01Clicked))
                    [
                        SNew(STextBlock).Text(FText::FromString(TEXT(" 0.1X "))).Font(ButtonFont).ColorAndOpacity(FLinearColor::White)
                    ]
                ]

                // Scale 1.0X
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(FMargin(2.0f, 0.0f))
                [
                    SNew(SButton)
                    .ButtonColorAndOpacity(FLinearColor(0.0f, 0.45f, 0.75f, 1.0f))
                    .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModelImporterWidget::OnScale10Clicked))
                    [
                        SNew(STextBlock).Text(FText::FromString(TEXT(" 1.0X "))).Font(ButtonFont).ColorAndOpacity(FLinearColor::White)
                    ]
                ]

                // Scale 10.0X
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(FMargin(2.0f, 0.0f))
                [
                    SNew(SButton)
                    .ButtonColorAndOpacity(FLinearColor(0.08f, 0.16f, 0.28f, 1.0f))
                    .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModelImporterWidget::OnScale100Clicked))
                    [
                        SNew(STextBlock).Text(FText::FromString(TEXT(" 10.0X "))).Font(ButtonFont).ColorAndOpacity(FLinearColor::White)
                    ]
                ]

                // Reimport
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(FMargin(6.0f, 0.0f, 2.0f, 0.0f))
                [
                    SNew(SButton)
                    .ButtonColorAndOpacity(FLinearColor(0.08f, 0.45f, 0.25f, 1.0f))
                    .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModelImporterWidget::OnReimportClicked))
                    [
                        SNew(STextBlock).Text(FText::FromString(TEXT(" 🔄 REIMPORT "))).Font(ButtonFont).ColorAndOpacity(FLinearColor::White)
                    ]
                ]

                // Toggle Physics
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
                [
                    SNew(SButton)
                    .ButtonColorAndOpacity(FLinearColor(0.85f, 0.4f, 0.0f, 1.0f))
                    .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModelImporterWidget::OnTogglePhysicsClicked))
                    [
                        SAssignNew(PhysicsButtonText, STextBlock)
                        .Text(FText::FromString(TEXT(" ⚡ FİZİK SİMÜLE ET ")))
                        .Font(ButtonFont)
                        .ColorAndOpacity(FLinearColor::White)
                    ]
                ]
            ]
        ]

        // ---------------------------------------------------------------------
        // 2) 3-SEKMELİ NAVİGASYON ÇUBUĞU (TAB SWITCHER BAR)
        // ---------------------------------------------------------------------
        + SOverlay::Slot()
        .HAlign(HAlign_Left)
        .VAlign(VAlign_Top)
        .Padding(FMargin(18.0f, 48.0f, 0.0f, 0.0f))
        [
            SNew(SHorizontalBox)

            // Tab 1: Motorlar & Kontrol
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
            [
                SAssignNew(TabMotorsBtnBorder, SBorder)
                .BorderBackgroundColor(FLinearColor(0.0f, 0.45f, 0.75f, 1.0f))
                .Padding(FMargin(1.0f))
                [
                    SNew(SButton)
                    .ButtonColorAndOpacity(FLinearColor(0.02f, 0.08f, 0.18f, 0.95f))
                    .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModelImporterWidget::OnTabMotorsClicked))
                    .ContentPadding(FMargin(14.0f, 5.0f))
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("⚙️ 1. MOTORLAR & KONTROL")))
                        .Font(TabFont)
                        .ColorAndOpacity(FLinearColor(0.0f, 0.88f, 1.0f, 1.0f))
                    ]
                ]
            ]

            // Tab 2: Telemetri & Pi 5
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
            [
                SAssignNew(TabTelemetryBtnBorder, SBorder)
                .BorderBackgroundColor(FLinearColor(0.05f, 0.12f, 0.22f, 0.8f))
                .Padding(FMargin(1.0f))
                [
                    SNew(SButton)
                    .ButtonColorAndOpacity(FLinearColor(0.02f, 0.05f, 0.12f, 0.95f))
                    .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModelImporterWidget::OnTabTelemetryClicked))
                    .ContentPadding(FMargin(14.0f, 5.0f))
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("📡 2. TELEMETRİ & BAĞLANTI")))
                        .Font(TabFont)
                        .ColorAndOpacity(FLinearColor(0.8f, 0.9f, 1.0f, 1.0f))
                    ]
                ]
            ]

            // Tab 3: Sensörler
            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SAssignNew(TabSensorsBtnBorder, SBorder)
                .BorderBackgroundColor(FLinearColor(0.05f, 0.12f, 0.22f, 0.8f))
                .Padding(FMargin(1.0f))
                [
                    SNew(SButton)
                    .ButtonColorAndOpacity(FLinearColor(0.02f, 0.05f, 0.12f, 0.95f))
                    .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModelImporterWidget::OnTabSensorsClicked))
                    .ContentPadding(FMargin(14.0f, 5.0f))
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("📷 3. SENSÖRLER")))
                        .Font(TabFont)
                        .ColorAndOpacity(FLinearColor(0.8f, 0.9f, 1.0f, 1.0f))
                    ]
                ]
            ]
        ]

        // ---------------------------------------------------------------------
        // 3) ANA İÇERİK DEĞİŞTİRİCİ (WIDGET SWITCHER: TABS 0, 1, 2)
        // ---------------------------------------------------------------------
        + SOverlay::Slot()
        .HAlign(HAlign_Fill)
        .VAlign(VAlign_Fill)
        .Padding(FMargin(18.0f, 86.0f, 18.0f, 18.0f))
        [
            SAssignNew(MainTabSwitcher, SWidgetSwitcher)
            .WidgetIndex(0) // Default: Motors Tab

            // =================================================================
            // [SEKME 1] MOTORLAR & KONTROL (MOTORS TAB)
            // =================================================================
            + SWidgetSwitcher::Slot()
            [
                SNew(SHorizontalBox)

                // SOL PANEL: KEMİK AĞACI & GÖRSEL/UCX TOGGLE
                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SBox)
                    .WidthOverride(420.0f)
                    [
                        SNew(SBorder)
                        .BorderBackgroundColor(FLinearColor(0.012f, 0.025f, 0.06f, 0.95f))
                        .Padding(FMargin(14.0f, 12.0f))
                        [
                            SNew(SVerticalBox)

                            // Header & Toggle Switcher
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, 0.0f, 0.0f, 8.0f)
                            [
                                SNew(SHorizontalBox)
                                + SHorizontalBox::Slot()
                                .AutoWidth()
                                .VAlign(VAlign_Center)
                                [
                                    SNew(STextBlock)
                                    .Text(FText::FromString(TEXT("🦴 KEMİK / PARÇA LİSTESİ")))
                                    .Font(CardHeaderFont)
                                    .ColorAndOpacity(FLinearColor(0.0f, 0.88f, 1.0f, 1.0f))
                                ]
                                + SHorizontalBox::Slot().FillWidth(1.0f)
                                + SHorizontalBox::Slot()
                                .AutoWidth()
                                [
                                    SNew(SButton)
                                    .ButtonColorAndOpacity(FLinearColor(0.1f, 0.25f, 0.45f, 1.0f))
                                    .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModelImporterWidget::OnToggleDisplayModeClicked))
                                    .ContentPadding(FMargin(8.0f, 3.0f))
                                    [
                                        SAssignNew(DisplayModeButtonText, STextBlock)
                                        .Text(FText::FromString(TEXT("🎨 GÖRSEL MOD")))
                                        .Font(BadgeFont)
                                        .ColorAndOpacity(FLinearColor::White)
                                    ]
                                ]
                            ]

                            // Separator
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, 0.0f, 0.0f, 8.0f)
                            [
                                SNew(SBorder).BorderBackgroundColor(FLinearColor(0.1f, 0.25f, 0.45f, 0.6f)).Padding(FMargin(0.0f, 0.5f))
                            ]

                            // Scrollable Bone List
                            + SVerticalBox::Slot()
                            .FillHeight(1.0f)
                            [
                                SAssignNew(BoneListScrollBox, SScrollBox)
                            ]
                        ]
                    ]
                ]

                + SHorizontalBox::Slot().FillWidth(1.0f) // Spacer

                // SAĞ PANEL: SEÇİLİ KEMİK VE MOTOR MÜFETTİŞİ (INSPECTOR)
                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SAssignNew(BoneInspectorBorder, SBorder)
                    .BorderBackgroundColor(FLinearColor(0.012f, 0.025f, 0.06f, 0.95f))
                    .Padding(FMargin(14.0f, 12.0f))
                    .Visibility(EVisibility::Collapsed)
                    [
                        SNew(SBox)
                        .WidthOverride(440.0f)
                        [
                            SNew(SVerticalBox)

                            // Header & Basic/Advanced Toggle
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, 0.0f, 0.0f, 6.0f)
                            [
                                SNew(SHorizontalBox)
                                + SHorizontalBox::Slot()
                                .AutoWidth()
                                .VAlign(VAlign_Center)
                                [
                                    SAssignNew(SelectedBoneTitleText, STextBlock)
                                    .Text(FText::FromString(TEXT("SEÇİLİ PARÇA")))
                                    .Font(CardHeaderFont)
                                    .ColorAndOpacity(FLinearColor(0.0f, 0.88f, 1.0f, 1.0f))
                                ]
                                + SHorizontalBox::Slot().FillWidth(1.0f)
                                + SHorizontalBox::Slot()
                                .AutoWidth()
                                [
                                    SNew(SButton)
                                    .ButtonColorAndOpacity(FLinearColor(0.2f, 0.4f, 0.1f, 1.0f))
                                    .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModelImporterWidget::OnToggleAdvancedModeClicked))
                                    .ContentPadding(FMargin(8.0f, 3.0f))
                                    [
                                        SAssignNew(AdvancedModeButtonText, STextBlock)
                                        .Text(FText::FromString(TEXT("🚗 BASIC MOD")))
                                        .Font(BadgeFont)
                                        .ColorAndOpacity(FLinearColor::White)
                                    ]
                                ]
                            ]

                            // Role Badge
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, 0.0f, 0.0f, 8.0f)
                            [
                                SAssignNew(SelectedBoneRoleBadgeText, STextBlock)
                                .Text(FText::FromString(TEXT("Rol: Sürüş Tekerleği")))
                                .Font(BadgeFont)
                                .ColorAndOpacity(FLinearColor(0.2f, 1.0f, 0.5f, 1.0f))
                            ]

                            // Separator
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, 0.0f, 0.0f, 8.0f)
                            [
                                SNew(SBorder).BorderBackgroundColor(FLinearColor(0.1f, 0.25f, 0.45f, 0.6f)).Padding(FMargin(0.0f, 0.5f))
                            ]

                            // Existing Motor Container
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            [
                                SAssignNew(ExistingMotorBox, SBox)
                                [
                                    SNew(SVerticalBox)

                                    // Details Text
                                    + SVerticalBox::Slot()
                                    .AutoHeight()
                                    .Padding(0.0f, 0.0f, 0.0f, 10.0f)
                                    [
                                        SAssignNew(MotorDetailsText, STextBlock)
                                        .Text(FText::FromString(TEXT("Motor parametreleri yükleniyor...")))
                                        .Font(DataFont)
                                        .ColorAndOpacity(FLinearColor(0.9f, 0.95f, 1.0f, 1.0f))
                                    ]

                                    // Live Motor Test Slider Header
                                    + SVerticalBox::Slot()
                                    .AutoHeight()
                                    .Padding(0.0f, 6.0f, 0.0f, 4.0f)
                                    [
                                        SNew(SHorizontalBox)
                                        + SHorizontalBox::Slot().AutoWidth()
                                        [
                                            SNew(STextBlock)
                                            .Text(FText::FromString(TEXT("🎮 CANLI TEST ÇUBUĞU (SLIDER):")))
                                            .Font(BadgeFont)
                                            .ColorAndOpacity(FLinearColor(1.0f, 0.75f, 0.1f, 1.0f))
                                        ]
                                        + SHorizontalBox::Slot().FillWidth(1.0f)
                                        + SHorizontalBox::Slot().AutoWidth()
                                        [
                                            SAssignNew(MotorTestSliderValueText, STextBlock)
                                            .Text(FText::FromString(TEXT("0.0% (DURUYOR)")))
                                            .Font(BadgeFont)
                                            .ColorAndOpacity(FLinearColor(0.2f, 1.0f, 0.5f, 1.0f))
                                        ]
                                    ]

                                    // Live Test Slider
                                    + SVerticalBox::Slot()
                                    .AutoHeight()
                                    .Padding(0.0f, 0.0f, 0.0f, 14.0f)
                                    [
                                        SAssignNew(MotorTestSlider, SSlider)
                                        .Value(0.0f)
                                        .MinValue(-1.0f)
                                        .MaxValue(1.0f)
                                        .OnValueChanged(FOnFloatValueChanged::CreateUObject(this, &UPiSimModelImporterWidget::OnMotorTestSliderChanged))
                                    ]

                                    // Remove Motor Button
                                    + SVerticalBox::Slot()
                                    .AutoHeight()
                                    .Padding(0.0f, 4.0f, 0.0f, 0.0f)
                                    [
                                        SNew(SButton)
                                        .ButtonColorAndOpacity(FLinearColor(0.45f, 0.08f, 0.08f, 1.0f))
                                        .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModelImporterWidget::OnRemoveMotorClicked))
                                        .ContentPadding(FMargin(10.0f, 6.0f))
                                        [
                                            SNew(STextBlock)
                                            .Text(FText::FromString(TEXT("🗑️ MOTORU BU KEMİKTEN KALDIR")))
                                            .Font(ButtonFont)
                                            .ColorAndOpacity(FLinearColor::White)
                                        ]
                                    ]
                                ]
                            ]

                            // Assign Motor Container (Shown if Role == None)
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            [
                                SAssignNew(AssignMotorBox, SBox)
                                .Visibility(EVisibility::Collapsed)
                                [
                                    SNew(SVerticalBox)

                                    + SVerticalBox::Slot()
                                    .AutoHeight()
                                    .Padding(0.0f, 0.0f, 0.0f, 8.0f)
                                    [
                                        SNew(STextBlock)
                                        .Text(FText::FromString(TEXT("Bu kemiğe atanmış motor yok. Bir motor tipi seçin:")))
                                        .Font(DataFont)
                                        .ColorAndOpacity(FLinearColor(0.85f, 0.85f, 0.85f, 1.0f))
                                    ]

                                    // Role 1: Drive Wheel
                                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
                                    [
                                        SNew(SButton).ButtonColorAndOpacity(FLinearColor(0.05f, 0.2f, 0.4f, 1.0f))
                                        .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModelImporterWidget::OnAssignRoleClicked, (uint8)EPiSimMotorRole::DriveWheel))
                                        [
                                            SNew(STextBlock).Text(FText::FromString(TEXT("🚗 Sürüş Tekerleği (Drive Wheel)"))).Font(ButtonFont)
                                        ]
                                    ]

                                    // Role 2: Steered Wheel
                                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
                                    [
                                        SNew(SButton).ButtonColorAndOpacity(FLinearColor(0.05f, 0.35f, 0.35f, 1.0f))
                                        .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModelImporterWidget::OnAssignRoleClicked, (uint8)EPiSimMotorRole::SteeredWheel))
                                        [
                                            SNew(STextBlock).Text(FText::FromString(TEXT("🧭 Direksiyonlu Tekerlek (Steered Wheel)"))).Font(ButtonFont)
                                        ]
                                    ]

                                    // Role 3: Free Caster
                                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
                                    [
                                        SNew(SButton).ButtonColorAndOpacity(FLinearColor(0.2f, 0.2f, 0.25f, 1.0f))
                                        .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModelImporterWidget::OnAssignRoleClicked, (uint8)EPiSimMotorRole::FreeCaster))
                                        [
                                            SNew(STextBlock).Text(FText::FromString(TEXT("⚪ Serbest Sarhoş Tekerlek (Caster)"))).Font(ButtonFont)
                                        ]
                                    ]

                                    // Role 4: Servo Joint
                                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
                                    [
                                        SNew(SButton).ButtonColorAndOpacity(FLinearColor(0.4f, 0.25f, 0.05f, 1.0f))
                                        .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModelImporterWidget::OnAssignRoleClicked, (uint8)EPiSimMotorRole::ServoJoint))
                                        [
                                            SNew(STextBlock).Text(FText::FromString(TEXT("🦾 Robot Kolu Servosu (Servo Joint)"))).Font(ButtonFont)
                                        ]
                                    ]

                                    // Role 5: Linear Actuator
                                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
                                    [
                                        SNew(SButton).ButtonColorAndOpacity(FLinearColor(0.35f, 0.1f, 0.35f, 1.0f))
                                        .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModelImporterWidget::OnAssignRoleClicked, (uint8)EPiSimMotorRole::LinearActuator))
                                        [
                                            SNew(STextBlock).Text(FText::FromString(TEXT("📏 Hidrolik / Lineer Piston"))).Font(ButtonFont)
                                        ]
                                    ]

                                    // Role 6: Thruster
                                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
                                    [
                                        SNew(SButton).ButtonColorAndOpacity(FLinearColor(0.4f, 0.08f, 0.08f, 1.0f))
                                        .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModelImporterWidget::OnAssignRoleClicked, (uint8)EPiSimMotorRole::Thruster))
                                        [
                                            SNew(STextBlock).Text(FText::FromString(TEXT("🛸 İtki Pervanesi (Thruster)"))).Font(ButtonFont)
                                        ]
                                    ]

                                    // Role 7: Track Pad
                                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
                                    [
                                        SNew(SButton).ButtonColorAndOpacity(FLinearColor(0.25f, 0.25f, 0.1f, 1.0f))
                                        .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModelImporterWidget::OnAssignRoleClicked, (uint8)EPiSimMotorRole::TrackPad))
                                        [
                                            SNew(STextBlock).Text(FText::FromString(TEXT("🚜 Palet Sürtünme Plakası (Track)"))).Font(ButtonFont)
                                        ]
                                    ]
                                ]
                            ]
                        ]
                    ]
                ]
            ]

            // =================================================================
            // [SEKME 2] TELEMETRİ & BAĞLANTI (TELEMETRY TAB)
            // =================================================================
            + SWidgetSwitcher::Slot()
            [
                SNew(SHorizontalBox)

                // SOL PANEL: BAĞLANTI AŞAMALARI & GELEN VERİLER
                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SBox)
                    .WidthOverride(430.0f)
                    [
                        SNew(SBorder)
                        .BorderBackgroundColor(FLinearColor(0.012f, 0.025f, 0.06f, 0.95f))
                        .Padding(FMargin(14.0f, 12.0f))
                        [
                            SNew(SVerticalBox)

                            // Card 1: Connection Stages
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, 0.0f, 0.0f, 3.0f)
                            [
                                SNew(STextBlock).Text(FText::FromString(TEXT("🔗 Pİ 5 BAĞLANTI AŞAMALARI (1 - 4)"))).Font(CardHeaderFont).ColorAndOpacity(FLinearColor(0.0f, 0.88f, 1.0f, 1.0f))
                            ]
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, 0.0f, 0.0f, 8.0f)
                            [
                                SAssignNew(ConnectionStagesText, STextBlock).Text(FText::FromString(TEXT("Aşama bilgileri hazırlanıyor..."))).Font(DataFont).ColorAndOpacity(FLinearColor(0.9f, 0.95f, 1.0f, 1.0f))
                            ]

                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)[ SNew(SBorder).BorderBackgroundColor(FLinearColor(0.1f, 0.25f, 0.45f, 0.6f)).Padding(FMargin(0.0f, 0.5f)) ]

                            // Card 2: Incoming Data
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, 3.0f, 0.0f, 3.0f)
                            [
                                SNew(STextBlock).Text(FText::FromString(TEXT("🎮 GELEN VERİLER (Pi 5 ➔ UE5 - Twist Komutları)"))).Font(CardHeaderFont).ColorAndOpacity(FLinearColor(0.2f, 1.0f, 0.5f, 1.0f))
                            ]
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, 0.0f, 0.0f, 8.0f)
                            [
                                SAssignNew(IncomingDataText, STextBlock).Text(FText::FromString(TEXT("Gelen kontrol komutu bekleniyor..."))).Font(DataFont).ColorAndOpacity(FLinearColor(0.9f, 0.95f, 1.0f, 1.0f))
                            ]

                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)[ SNew(SBorder).BorderBackgroundColor(FLinearColor(0.1f, 0.25f, 0.45f, 0.6f)).Padding(FMargin(0.0f, 0.5f)) ]

                            // Card 3: Live Debug Event Log
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, 3.0f, 0.0f, 3.0f)
                            [
                                SNew(STextBlock).Text(FText::FromString(TEXT("📋 CANLI BAĞLANTI & HATA GÜNLÜĞÜ (DEBUG LOG)"))).Font(CardHeaderFont).ColorAndOpacity(FLinearColor(1.0f, 0.55f, 0.25f, 1.0f))
                            ]
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            [
                                SAssignNew(ConnectionDebugLogText, STextBlock).Text(FText::FromString(TEXT("Log bekleniyor..."))).Font(DataFont).ColorAndOpacity(FLinearColor(1.0f, 0.92f, 0.75f, 1.0f))
                            ]
                        ]
                    ]
                ]

                + SHorizontalBox::Slot().FillWidth(1.0f) // Spacer

                // SAĞ PANEL: GİDEN TELEMETRİ VE CAD
                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SBox)
                    .WidthOverride(420.0f)
                    [
                        SNew(SBorder)
                        .BorderBackgroundColor(FLinearColor(0.012f, 0.025f, 0.06f, 0.95f))
                        .Padding(FMargin(14.0f, 12.0f))
                        [
                            SNew(SVerticalBox)

                            // Card 1: Outgoing Telemetry
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, 0.0f, 0.0f, 3.0f)
                            [
                                SNew(STextBlock).Text(FText::FromString(TEXT("📡 GİDEN VERİLER (UE5 ➔ Pi 5 - IMU TELEMETRİSİ)"))).Font(CardHeaderFont).ColorAndOpacity(FLinearColor(1.0f, 0.75f, 0.1f, 1.0f))
                            ]
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, 0.0f, 0.0f, 8.0f)
                            [
                                SAssignNew(OutgoingTelemetryText, STextBlock).Text(FText::FromString(TEXT("Telemetri verisi hazırlanıyor..."))).Font(DataFont).ColorAndOpacity(FLinearColor(0.9f, 0.95f, 1.0f, 1.0f))
                            ]

                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)[ SNew(SBorder).BorderBackgroundColor(FLinearColor(0.1f, 0.25f, 0.45f, 0.6f)).Padding(FMargin(0.0f, 0.5f)) ]

                            // Card 2: CAD Geometry & Chaos Physics
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, 3.0f, 0.0f, 3.0f)
                            [
                                SNew(STextBlock).Text(FText::FromString(TEXT("📦 CAD GEOMETRİ & CHAOS FİZİK ZIRHI"))).Font(CardHeaderFont).ColorAndOpacity(FLinearColor(0.7f, 0.85f, 1.0f, 0.95f))
                            ]
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            [
                                SAssignNew(ModelCadStatusText, STextBlock).Text(FText::FromString(TEXT("Yükleniyor..."))).Font(DataFont).ColorAndOpacity(FLinearColor(0.85f, 0.92f, 1.0f, 1.0f))
                            ]
                        ]
                    ]
                ]
            ]

            // =================================================================
            // [SEKME 3] SENSÖRLER (SENSORS TAB)
            // =================================================================
            + SWidgetSwitcher::Slot()
            [
                SNew(SHorizontalBox)

                // SOL PANEL: SENSÖR LİSTESİ & TOGGLE
                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SBox)
                    .WidthOverride(420.0f)
                    [
                        SNew(SBorder)
                        .BorderBackgroundColor(FLinearColor(0.012f, 0.025f, 0.06f, 0.95f))
                        .Padding(FMargin(14.0f, 12.0f))
                        [
                            SNew(SVerticalBox)

                            // Header & Toggle Switcher
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, 0.0f, 0.0f, 8.0f)
                            [
                                SNew(SHorizontalBox)
                                + SHorizontalBox::Slot()
                                .AutoWidth()
                                .VAlign(VAlign_Center)
                                [
                                    SNew(STextBlock)
                                    .Text(FText::FromString(TEXT("📡 SENSÖR YÖNETİMİ")))
                                    .Font(CardHeaderFont)
                                    .ColorAndOpacity(FLinearColor(0.0f, 0.88f, 1.0f, 1.0f))
                                ]
                                + SHorizontalBox::Slot().FillWidth(1.0f)
                                + SHorizontalBox::Slot()
                                .AutoWidth()
                                [
                                    SNew(SButton)
                                    .ButtonColorAndOpacity(FLinearColor(0.1f, 0.25f, 0.45f, 1.0f))
                                    .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModelImporterWidget::OnToggleSensorMarkersClicked))
                                    .ContentPadding(FMargin(8.0f, 3.0f))
                                    [
                                        SAssignNew(SensorMarkerToggleText, STextBlock)
                                        .Text(FText::FromString(TEXT("👁️ S_ GÖSTER")))
                                        .Font(BadgeFont)
                                        .ColorAndOpacity(FLinearColor::White)
                                    ]
                                ]
                            ]

                            // Quick Add Virtual Sensor Section (Attached to root bone / chassis)
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, 0.0f, 0.0f, 8.0f)
                            [
                                SNew(SBorder)
                                .BorderBackgroundColor(FLinearColor(0.03f, 0.09f, 0.18f, 0.9f))
                                .Padding(FMargin(8.0f, 6.0f))
                                [
                                    SNew(SVerticalBox)
                                    + SVerticalBox::Slot()
                                    .AutoHeight()
                                    .Padding(0.0f, 0.0f, 0.0f, 6.0f)
                                    [
                                        SNew(STextBlock)
                                        .Text(FText::FromString(TEXT("➕ YENİ SANAL SENSÖR EKLE (Ana Kemiğe Bağlı)")))
                                        .Font(BadgeFont)
                                        .ColorAndOpacity(FLinearColor(0.2f, 0.9f, 0.5f, 1.0f))
                                    ]
                                    + SVerticalBox::Slot()
                                    .AutoHeight()
                                    [
                                        SNew(SHorizontalBox)
                                        // + GPS
                                        + SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
                                        [
                                            SNew(SButton)
                                            .ButtonColorAndOpacity(FLinearColor(0.08f, 0.25f, 0.45f, 1.0f))
                                            .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModelImporterWidget::OnAddVirtualSensorClicked, (uint8)EPiSimSensorType::GPS))
                                            .ContentPadding(FMargin(6.0f, 4.0f))
                                            [
                                                SNew(STextBlock).Text(FText::FromString(TEXT("🛰️ + GPS"))).Font(ButtonFont).ColorAndOpacity(FLinearColor::White)
                                            ]
                                        ]
                                        // + IMU
                                        + SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
                                        [
                                            SNew(SButton)
                                            .ButtonColorAndOpacity(FLinearColor(0.08f, 0.25f, 0.45f, 1.0f))
                                            .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModelImporterWidget::OnAddVirtualSensorClicked, (uint8)EPiSimSensorType::IMU))
                                            .ContentPadding(FMargin(6.0f, 4.0f))
                                            [
                                                SNew(STextBlock).Text(FText::FromString(TEXT("📡 + IMU"))).Font(ButtonFont).ColorAndOpacity(FLinearColor::White)
                                            ]
                                        ]
                                        // + LiDAR
                                        + SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
                                        [
                                            SNew(SButton)
                                            .ButtonColorAndOpacity(FLinearColor(0.08f, 0.25f, 0.45f, 1.0f))
                                            .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModelImporterWidget::OnAddVirtualSensorClicked, (uint8)EPiSimSensorType::LiDAR))
                                            .ContentPadding(FMargin(6.0f, 4.0f))
                                            [
                                                SNew(STextBlock).Text(FText::FromString(TEXT("🎯 + LiDAR"))).Font(ButtonFont).ColorAndOpacity(FLinearColor::White)
                                            ]
                                        ]
                                        // + Kamera
                                        + SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
                                        [
                                            SNew(SButton)
                                            .ButtonColorAndOpacity(FLinearColor(0.08f, 0.25f, 0.45f, 1.0f))
                                            .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModelImporterWidget::OnAddVirtualSensorClicked, (uint8)EPiSimSensorType::Camera))
                                            .ContentPadding(FMargin(6.0f, 4.0f))
                                            [
                                                SNew(STextBlock).Text(FText::FromString(TEXT("📷 + Kamera"))).Font(ButtonFont).ColorAndOpacity(FLinearColor::White)
                                            ]
                                        ]
                                        // + Sonar
                                        + SHorizontalBox::Slot().AutoWidth()
                                        [
                                            SNew(SButton)
                                            .ButtonColorAndOpacity(FLinearColor(0.08f, 0.25f, 0.45f, 1.0f))
                                            .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModelImporterWidget::OnAddVirtualSensorClicked, (uint8)EPiSimSensorType::Ultrasonic))
                                            .ContentPadding(FMargin(6.0f, 4.0f))
                                            [
                                                SNew(STextBlock).Text(FText::FromString(TEXT("🔊 + Sonar"))).Font(ButtonFont).ColorAndOpacity(FLinearColor::White)
                                            ]
                                        ]
                                    ]
                                ]
                            ]

                            // Separator
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, 0.0f, 0.0f, 8.0f)
                            [
                                SNew(SBorder).BorderBackgroundColor(FLinearColor(0.1f, 0.25f, 0.45f, 0.6f)).Padding(FMargin(0.0f, 0.5f))
                            ]

                            // Scrollable Sensor List
                            + SVerticalBox::Slot()
                            .FillHeight(1.0f)
                            [
                                SAssignNew(SensorListScrollBox, SScrollBox)
                            ]
                        ]
                    ]
                ]

                + SHorizontalBox::Slot().FillWidth(1.0f) // Spacer

                // SAĞ PANEL: SEÇİLİ SENSÖR MÜFETTİŞİ & CANLI PREVIEW
                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SAssignNew(SensorInspectorBorder, SBorder)
                    .BorderBackgroundColor(FLinearColor(0.012f, 0.025f, 0.06f, 0.95f))
                    .Padding(FMargin(14.0f, 12.0f))
                    .Visibility(EVisibility::Collapsed)
                    [
                        SNew(SBox)
                        .WidthOverride(440.0f)
                        [
                            SNew(SVerticalBox)

                            // Header
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, 0.0f, 0.0f, 6.0f)
                            [
                                SAssignNew(SelectedSensorTitleText, STextBlock)
                                .Text(FText::FromString(TEXT("SEÇİLİ SENSÖR")))
                                .Font(CardHeaderFont)
                                .ColorAndOpacity(FLinearColor(0.0f, 0.88f, 1.0f, 1.0f))
                            ]

                            // Badge
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, 0.0f, 0.0f, 8.0f)
                            [
                                SAssignNew(SelectedSensorBadgeText, STextBlock)
                                .Text(FText::FromString(TEXT("Tip: FPV Kamera")))
                                .Font(BadgeFont)
                                .ColorAndOpacity(FLinearColor(0.2f, 1.0f, 0.5f, 1.0f))
                            ]

                            // Live Camera Mini Preview Image (Only for Camera)
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .HAlign(HAlign_Center)
                            .Padding(0.0f, 4.0f, 0.0f, 10.0f)
                            [
                                SAssignNew(CameraPreviewBox, SBox)
                                .WidthOverride(240.0f)
                                .HeightOverride(180.0f)
                                [
                                    SNew(SImage)
                                    .Image(&CameraPreviewBrush)
                                ]
                            ]

                            // Custom Sensor Telemetry Card (For GPS, IMU, LiDAR, Sonar)
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, 0.0f, 0.0f, 10.0f)
                            [
                                SAssignNew(SensorCustomTelemetryBorder, SBorder)
                                .BorderBackgroundColor(FLinearColor(0.02f, 0.06f, 0.12f, 0.95f))
                                .Padding(FMargin(10.0f, 8.0f))
                                .Visibility(EVisibility::Collapsed)
                                [
                                    SAssignNew(SensorCustomTelemetryText, STextBlock)
                                    .Text(FText::FromString(TEXT("Sensör telemetrisi...")))
                                    .Font(DataFont)
                                    .ColorAndOpacity(FLinearColor(0.2f, 1.0f, 0.8f, 1.0f))
                                ]
                            ]

                            // Details Text
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, 0.0f, 0.0f, 10.0f)
                            [
                                SAssignNew(SensorDetailsText, STextBlock)
                                .Text(FText::FromString(TEXT("Sensör özellikleri...")))
                                .Font(DataFont)
                                .ColorAndOpacity(FLinearColor(0.9f, 0.95f, 1.0f, 1.0f))
                            ]

                            // Remove Sensor Button
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, 6.0f, 0.0f, 0.0f)
                            [
                                SNew(SButton)
                                .ButtonColorAndOpacity(FLinearColor(0.45f, 0.08f, 0.08f, 1.0f))
                                .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModelImporterWidget::OnRemoveSensorClicked))
                                .ContentPadding(FMargin(10.0f, 6.0f))
                                [
                                    SNew(STextBlock)
                                    .Text(FText::FromString(TEXT("🗑️ BU SENSÖRÜ SİSTEMDEN KALDIR")))
                                    .Font(ButtonFont)
                                    .ColorAndOpacity(FLinearColor::White)
                                ]
                            ]
                        ]
                    ]
                ]
            ]
        ];
}

void UPiSimModelImporterWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (!TargetImporter && GetWorld())
    {
        AActor* Found = UGameplayStatics::GetActorOfClass(GetWorld(), APiSimModelImporter::StaticClass());
        TargetImporter = Cast<APiSimModelImporter>(Found);
    }

    if (!TargetImporter) return;

    FSlateFontInfo ButtonFont = FCoreStyle::GetDefaultFontStyle("Bold", 9);
    FSlateFontInfo BadgeFont = FCoreStyle::GetDefaultFontStyle("Bold", 9);

    // 1) Top Connection Badge & Colors
    if (ConnectionBadgeText.IsValid() && ConnectionBadgeBorder.IsValid())
    {
        if (TargetImporter->bIsPiConnected)
        {
            ConnectionBadgeBorder->SetBorderBackgroundColor(FLinearColor(0.02f, 0.32f, 0.12f, 0.95f));
            ConnectionBadgeText->SetText(FText::FromString(FString::Printf(TEXT("🟢 BAĞLI: %s (Port 7400)"), *TargetImporter->ConnectedPiIP)));
            ConnectionBadgeText->SetColorAndOpacity(FLinearColor(0.2f, 1.0f, 0.4f, 1.0f));
        }
        else if (TargetImporter->ConnectionStage == 5)
        {
            ConnectionBadgeBorder->SetBorderBackgroundColor(FLinearColor(0.38f, 0.05f, 0.05f, 0.95f));
            ConnectionBadgeText->SetText(FText::FromString(TEXT("🔴 BAĞLANTI KOPTU (Zaman Aşımı)")));
            ConnectionBadgeText->SetColorAndOpacity(FLinearColor(1.0f, 0.3f, 0.3f, 1.0f));
        }
        else
        {
            ConnectionBadgeBorder->SetBorderBackgroundColor(FLinearColor(0.28f, 0.18f, 0.02f, 0.95f));
            ConnectionBadgeText->SetText(FText::FromString(FString::Printf(TEXT("🟡 PI 5 BEKLENİYOR (Hedef: %s)"), *TargetImporter->BridgeTargetIP)));
            ConnectionBadgeText->SetColorAndOpacity(FLinearColor(1.0f, 0.85f, 0.2f, 1.0f));
        }
    }

    // 2) Active Tab Navigation Visuals
    if (MainTabSwitcher.IsValid())
    {
        int32 ActiveIdx = (int32)TargetImporter->CurrentActiveTab;
        MainTabSwitcher->SetActiveWidgetIndex(ActiveIdx);

        if (TabMotorsBtnBorder.IsValid())
            TabMotorsBtnBorder->SetBorderBackgroundColor(ActiveIdx == 0 ? FLinearColor(0.0f, 0.45f, 0.75f, 1.0f) : FLinearColor(0.05f, 0.12f, 0.22f, 0.8f));
        if (TabTelemetryBtnBorder.IsValid())
            TabTelemetryBtnBorder->SetBorderBackgroundColor(ActiveIdx == 1 ? FLinearColor(0.0f, 0.45f, 0.75f, 1.0f) : FLinearColor(0.05f, 0.12f, 0.22f, 0.8f));
        if (TabSensorsBtnBorder.IsValid())
            TabSensorsBtnBorder->SetBorderBackgroundColor(ActiveIdx == 2 ? FLinearColor(0.0f, 0.45f, 0.75f, 1.0f) : FLinearColor(0.05f, 0.12f, 0.22f, 0.8f));
    }

    // 3) TAB 1: BONE & MOTOR LIST POPULATION
    if (BoneListScrollBox.IsValid() && (LastRenderedBoneCount != TargetImporter->ConfiguredMotors.Num() || CachedSelectedBone != TargetImporter->SelectedBoneIndex))
    {
        LastRenderedBoneCount = TargetImporter->ConfiguredMotors.Num();
        CachedSelectedBone = TargetImporter->SelectedBoneIndex;
        BoneListScrollBox->ClearChildren();

        for (int32 i = 0; i < TargetImporter->ConfiguredMotors.Num(); ++i)
        {
            const FPiSimMotorItem& Motor = TargetImporter->ConfiguredMotors[i];
            bool bIsSelected = (TargetImporter->SelectedBoneIndex == i);

            FString RoleIcon = TEXT("📦");
            FLinearColor RoleColor = FLinearColor(0.5f, 0.5f, 0.5f, 1.0f);

            switch (Motor.Role)
            {
                case EPiSimMotorRole::DriveWheel: RoleIcon = TEXT("🚗"); RoleColor = FLinearColor(0.0f, 0.88f, 1.0f, 1.0f); break;
                case EPiSimMotorRole::SteeredWheel: RoleIcon = TEXT("🧭"); RoleColor = FLinearColor(0.2f, 1.0f, 0.8f, 1.0f); break;
                case EPiSimMotorRole::FreeCaster: RoleIcon = TEXT("⚪"); RoleColor = FLinearColor(0.7f, 0.7f, 0.7f, 1.0f); break;
                case EPiSimMotorRole::ServoJoint: RoleIcon = TEXT("🦾"); RoleColor = FLinearColor(1.0f, 0.75f, 0.1f, 1.0f); break;
                case EPiSimMotorRole::LinearActuator: RoleIcon = TEXT("📏"); RoleColor = FLinearColor(0.85f, 0.4f, 1.0f, 1.0f); break;
                case EPiSimMotorRole::Thruster: RoleIcon = TEXT("🛸"); RoleColor = FLinearColor(1.0f, 0.3f, 0.3f, 1.0f); break;
                case EPiSimMotorRole::TrackPad: RoleIcon = TEXT("🚜"); RoleColor = FLinearColor(0.85f, 0.65f, 0.2f, 1.0f); break;
                default: RoleIcon = TEXT("📦"); RoleColor = FLinearColor(0.5f, 0.55f, 0.6f, 1.0f); break;
            }

            bool bIsPureBone = (TargetImporter->VisualSections.IsValidIndex(i) && TargetImporter->VisualSections[i].bIsPureHierarchy);
            FString PureTag = bIsPureBone ? TEXT(" [Mafsal]") : TEXT("");
            FString BtnLabel = FString::Printf(TEXT("%s  [%d] %s%s"), *RoleIcon, i, *Motor.BoneName, *PureTag);

            TSharedRef<SWidget> BoneRowWidget = SNew(SBorder)
                .BorderBackgroundColor(bIsSelected ? FLinearColor(0.0f, 0.88f, 1.0f, 1.0f) : FLinearColor(0.05f, 0.1f, 0.2f, 0.6f))
                .Padding(FMargin(bIsSelected ? 2.0f : 1.0f))
                [
                    SNew(SButton)
                    .ButtonColorAndOpacity(bIsSelected ? FLinearColor(0.05f, 0.22f, 0.45f, 1.0f) : FLinearColor(0.02f, 0.05f, 0.12f, 0.9f))
                    .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModelImporterWidget::OnSelectBoneClicked, i))
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth()
                        [
                            SNew(STextBlock).Text(FText::FromString(BtnLabel)).Font(ButtonFont).ColorAndOpacity(RoleColor)
                        ]
                    ]
                ];

            BoneListScrollBox->AddSlot()
            .Padding(FMargin(0.0f, 2.0f))
            [
                BoneRowWidget
            ];
        }
    }

    // 4) TAB 1: BONE INSPECTOR PANEL UPDATES
    if (BoneInspectorBorder.IsValid())
    {
        if (TargetImporter->SelectedBoneIndex >= 0 && TargetImporter->ConfiguredMotors.IsValidIndex(TargetImporter->SelectedBoneIndex))
        {
            BoneInspectorBorder->SetVisibility(EVisibility::Visible);
            const FPiSimMotorItem& SelMotor = TargetImporter->ConfiguredMotors[TargetImporter->SelectedBoneIndex];

            if (SelectedBoneTitleText.IsValid())
                SelectedBoneTitleText->SetText(FText::FromString(FString::Printf(TEXT("🦴 [%d] %s"), SelMotor.BoneIndex, *SelMotor.BoneName)));

            if (DisplayModeButtonText.IsValid())
                DisplayModeButtonText->SetText(FText::FromString(TargetImporter->bShowCollisionView ? TEXT("🛡️ UCX ZIRH MODU") : TEXT("🎨 GÖRSEL MOD")));

            if (AdvancedModeButtonText.IsValid())
                AdvancedModeButtonText->SetText(FText::FromString(TargetImporter->bIsAdvancedMode ? TEXT("⚙️ ADVANCED MOD") : TEXT("🚗 BASIC MOD")));

            if (TargetImporter->SelectedBoneIndex == 0)
            {
                if (ExistingMotorBox.IsValid()) ExistingMotorBox->SetVisibility(EVisibility::Visible);
                if (AssignMotorBox.IsValid()) AssignMotorBox->SetVisibility(EVisibility::Collapsed);
                if (MotorTestSlider.IsValid()) MotorTestSlider->SetVisibility(EVisibility::Collapsed);
                if (MotorTestSliderValueText.IsValid()) MotorTestSliderValueText->SetVisibility(EVisibility::Collapsed);

                if (SelectedBoneRoleBadgeText.IsValid())
                    SelectedBoneRoleBadgeText->SetText(FText::FromString(TEXT("🛡️ Durum: Ana Kök Gövde (Şasi)")));

                if (MotorDetailsText.IsValid())
                {
                    FString ChassisInfo = FString::Printf(
                        TEXT("  • Parça Türü   : Robotun Ana Şasisi (Kök Gövde)\n"
                             "  • Toplam Parça : %d Adet Alt Kemik / Mesh\n"
                             "  • Kütle (Fizik): 30.0 kg (Zeminle Çarpışır)\n"
                             "  • Motor Durumu : Ana gövdeye motor atanamaz.\n"
                             "                   Lütfen hareketli parçaları seçin."),
                        TargetImporter->ConfiguredMotors.Num()
                    );
                    MotorDetailsText->SetText(FText::FromString(ChassisInfo));
                }
            }
            else if (SelMotor.Role != EPiSimMotorRole::None)
            {
                if (ExistingMotorBox.IsValid()) ExistingMotorBox->SetVisibility(EVisibility::Visible);
                if (AssignMotorBox.IsValid()) AssignMotorBox->SetVisibility(EVisibility::Collapsed);
                if (MotorTestSlider.IsValid()) MotorTestSlider->SetVisibility(EVisibility::Visible);
                if (MotorTestSliderValueText.IsValid()) MotorTestSliderValueText->SetVisibility(EVisibility::Visible);

                if (SelectedBoneRoleBadgeText.IsValid())
                {
                    FString RoleName = TEXT("Bilinmiyor");
                    switch (SelMotor.Role)
                    {
                        case EPiSimMotorRole::DriveWheel: RoleName = TEXT("🚗 Sürüş Tekerleği (Drive Wheel)"); break;
                        case EPiSimMotorRole::SteeredWheel: RoleName = TEXT("🧭 Direksiyonlu Tekerlek (Steered Wheel)"); break;
                        case EPiSimMotorRole::FreeCaster: RoleName = TEXT("⚪ Serbest Sarhoş Tekerlek (Caster)"); break;
                        case EPiSimMotorRole::ServoJoint: RoleName = TEXT("🦾 Robot Kolu Servosu (Servo Joint)"); break;
                        case EPiSimMotorRole::LinearActuator: RoleName = TEXT("📏 Hidrolik / Lineer Piston"); break;
                        case EPiSimMotorRole::Thruster: RoleName = TEXT("🛸 İtki Pervanesi (Thruster)"); break;
                        case EPiSimMotorRole::TrackPad: RoleName = TEXT("🚜 Palet Sürtünme Plakası (Track)"); break;
                        default: break;
                    }
                    SelectedBoneRoleBadgeText->SetText(FText::FromString(FString::Printf(TEXT("Aktif Motor: %s"), *RoleName)));
                }

                if (MotorDetailsText.IsValid())
                {
                    bool bIsPureBone = (TargetImporter->VisualSections.IsValidIndex(TargetImporter->SelectedBoneIndex) && 
                                        TargetImporter->VisualSections[TargetImporter->SelectedBoneIndex].bIsPureHierarchy);
                    FString Details;
                    if (bIsPureBone)
                    {
                        Details = FString::Printf(
                            TEXT("  • Kemik Türü   : Hiyerarşik Mafsal (Mesh'siz Düğüm)\n"
                                 "  • İşlev        : Alt parçaları (tekerlek vb.) yönlendirir\n"
                                 "  • Maksimum Güç : %5.1f Nm Tork\n"
                                 "  • Hareket Alanı: %+.1f° ile %+.1f°\n"
                                 "  • Durum        : Saf Mafsal (Görsel mesh render edilmez)"),
                            SelMotor.MaxTorqueNm, SelMotor.MinLimitDeg, SelMotor.MaxLimitDeg
                        );
                    }
                    else if (TargetImporter->bIsAdvancedMode)
                    {
                        Details = FString::Printf(
                            TEXT("  • Hız Limiti (Maks)  : %5.1f RPM\n"
                                 "  • Tork Limiti (Stall): %5.1f Nm\n"
                                 "  • Açı / Strok Sınırı : %+.1f° ile %+.1f°\n"
                                 "  • PID Katsayıları    : Kp=%.0f, Kd=%.0f\n"
                                 "  • Dişli Oranı        : 1:%.2f\n"
                                 "  • Motor Tork Sabiti  : %.3f Nm/A\n"
                                 "  • ROS 2 Topic Adı    : %s"),
                            SelMotor.MaxVelocityRPM, SelMotor.MaxTorqueNm, SelMotor.MinLimitDeg, SelMotor.MaxLimitDeg,
                            SelMotor.Kp, SelMotor.Kd, SelMotor.GearRatio, SelMotor.TorqueConstantKt, *SelMotor.Ros2Topic
                        );
                    }
                    else
                    {
                        Details = FString::Printf(
                            TEXT("  • Motor Tipi   : Standart Donanım\n"
                                 "  • Maksimum Hız : %5.1f RPM\n"
                                 "  • Maksimum Güç : %5.1f Nm Tork\n"
                                 "  • Hareket Alanı: %+.1f° ile %+.1f°"),
                            SelMotor.MaxVelocityRPM, SelMotor.MaxTorqueNm, SelMotor.MinLimitDeg, SelMotor.MaxLimitDeg
                        );
                    }
                    MotorDetailsText->SetText(FText::FromString(Details));
                }

                if (MotorTestSliderValueText.IsValid())
                {
                    FString StatusText;
                    if (FMath::Abs(SelMotor.CurrentTestValue) < 0.02f)
                    {
                        StatusText = TEXT("0.0% (DURUYOR)");
                    }
                    else
                    {
                        StatusText = FString::Printf(TEXT("%+5.1f%% (GÜÇ VERİLDİ)"), SelMotor.CurrentTestValue * 100.0f);
                    }
                    MotorTestSliderValueText->SetText(FText::FromString(StatusText));
                }
            }
            else
            {
                if (ExistingMotorBox.IsValid()) ExistingMotorBox->SetVisibility(EVisibility::Collapsed);
                if (AssignMotorBox.IsValid()) AssignMotorBox->SetVisibility(EVisibility::Visible);

                if (SelectedBoneRoleBadgeText.IsValid())
                    SelectedBoneRoleBadgeText->SetText(FText::FromString(TEXT("📦 Durum: Pasif Gövde (Motor Yok)")));
            }
        }
        else
        {
            BoneInspectorBorder->SetVisibility(EVisibility::Collapsed);
        }
    }

    // 5) TAB 2: TELEMETRY & CONNECTION TEXTS
    if (ConnectionStagesText.IsValid())
    {
        FString SocketStatus = TargetImporter->bIsSocketBound ? TEXT("🟢 AÇIK (Dinliyor)") : TEXT("🔴 KAPALI / HATA");
        FString Pi5Status = TargetImporter->bIsPiConnected ?
            FString::Printf(TEXT("🟢 BAĞLANDI (IP: %s)"), *TargetImporter->ConnectedPiIP) :
            (TargetImporter->ConnectionStage == 5 ? TEXT("🔴 KOPTU (Zaman Aşımı)") : TEXT("🟡 BEKLENİYOR..."));

        FString StagesStr = FString::Printf(
            TEXT("  • Aşama 1: UE5 Dinleme Soketi : 0.0.0.0:7400 [%s]\n"
                 "  • Aşama 2: Telemetri Hedefleri: %s:7401 [🟢 HAZIR]\n"
                 "  • Aşama 3: Pi 5 Handshake     : %s\n"
                 "  • Aşama 4: Canlı Veri Akışı   : %5.1f Hz  (Toplam: %d Paket)"),
            *SocketStatus, *TargetImporter->BridgeTargetIP, *Pi5Status, TargetImporter->RxPacketRateHz, TargetImporter->TotalPacketsReceived
        );
        ConnectionStagesText->SetText(FText::FromString(StagesStr));
    }

    if (IncomingDataText.IsValid())
    {
        FString InDataStr = FString::Printf(
            TEXT("  • Doğrusal Hız (X) : %+.2f m/s  (Y: %+.2f, Z: %+.2f)\n"
                 "  • Açısal Hız (Yaw) : %+.2f rad/s\n"
                 "  • Sol / Sağ RPM    : %+6.1f / %+6.1f RPM\n"
                 "  • Uygulanan RPM    : %+.1f RPM (G / F Tuşları)"),
            TargetImporter->TargetLinearX, TargetImporter->LastRxLinearVel.Y, TargetImporter->LastRxLinearVel.Z,
            TargetImporter->TargetAngularZ, TargetImporter->LeftWheelsRpm, TargetImporter->RightWheelsRpm, TargetImporter->AppliedWheelRpm
        );
        IncomingDataText->SetText(FText::FromString(InDataStr));
    }

    if (ConnectionDebugLogText.IsValid())
    {
        if (TargetImporter->ConnectionDebugLogs.Num() > 0)
            ConnectionDebugLogText->SetText(FText::FromString(FString::Join(TargetImporter->ConnectionDebugLogs, TEXT("\n"))));
        else
            ConnectionDebugLogText->SetText(FText::FromString(TEXT("  Henüz bir bağlantı olayı kaydedilmedi.")));
    }

    if (OutgoingTelemetryText.IsValid())
    {
        FString OutStr = FString::Printf(
            TEXT("  • Telemetri Paketi : #%d (%5.1f Hz, %s:7401)\n"
                 "  • Gövde Hızı       : %5.1f km/h\n"
                 "  • İvmeölçer (Accel): X=%+5.2f, Y=%+5.2f, Z=%+5.2f m/s²\n"
                 "  • Jiroskop (Gyro)  : X=%+5.2f, Y=%+5.2f, Z=%+5.2f deg/s\n"
                 "  • Oryantasyon Quat : (X=%.3f, Y=%.3f, Z=%.3f, W=%.3f)"),
            TargetImporter->TotalPacketsSent, TargetImporter->TxPacketRateHz, *TargetImporter->BridgeTargetIP,
            TargetImporter->CurrentForwardSpeedKmh, TargetImporter->CurrentLinearAccel.X, TargetImporter->CurrentLinearAccel.Y,
            TargetImporter->CurrentLinearAccel.Z, TargetImporter->LastTxGyro.X, TargetImporter->LastTxGyro.Y, TargetImporter->LastTxGyro.Z,
            TargetImporter->LastTxQuat.X, TargetImporter->LastTxQuat.Y, TargetImporter->LastTxQuat.Z, TargetImporter->LastTxQuat.W
        );
        OutgoingTelemetryText->SetText(FText::FromString(OutStr));
    }

    if (ModelCadStatusText.IsValid())
    {
        FString VideoStatus = (TargetImporter->bEnableVideoStream && TargetImporter->FpvCameraCapture) ?
            FString::Printf(TEXT("🟢 YAYINDA (320x240 @ %3.1f FPS)"), TargetImporter->VideoFpsActual) : TEXT("⏸️ SENSÖR YOK");

        FString CadStr = FString::Printf(
            TEXT("  • Görsel Parçalar  : %d Adet (Procedural Render)\n"
                 "  • UCX Çarpışma     : %d Adet (Convex Zırh)\n"
                 "  • Sensör Yuvaları  : %d Adet (S_...)\n"
                 "  • Simülasyon Durumu: %s\n"
                 "  • FPV Kamera Yayını: %s"),
            TargetImporter->VisualMeshComponents.Num(), TargetImporter->UCXSections.Num(), TargetImporter->ConfiguredSensors.Num(),
            TargetImporter->bIsPhysicsSimulating ? TEXT("⚡ AKTİF (Chaos Simülasyonu)") : TEXT("⏸️ STATİK (Garaj)"), *VideoStatus
        );
        ModelCadStatusText->SetText(FText::FromString(CadStr));
    }

    // 6) TAB 3: SENSORS LIST & INSPECTOR
    if (SensorListScrollBox.IsValid() && (LastRenderedSensorCount != TargetImporter->ConfiguredSensors.Num() || CachedSelectedSensor != TargetImporter->SelectedSensorIndex))
    {
        LastRenderedSensorCount = TargetImporter->ConfiguredSensors.Num();
        CachedSelectedSensor = TargetImporter->SelectedSensorIndex;
        SensorListScrollBox->ClearChildren();

        for (int32 SensorIdx = 0; SensorIdx < TargetImporter->ConfiguredSensors.Num(); ++SensorIdx)
        {
            const FPiSimSensorItem& Sensor = TargetImporter->ConfiguredSensors[SensorIdx];
            if (!Sensor.bIsActive) continue;
            bool bIsSelected = (TargetImporter->SelectedSensorIndex == SensorIdx);

            FString SensorIcon = TEXT("📷");
            switch (Sensor.Type)
            {
                case EPiSimSensorType::Camera: SensorIcon = TEXT("📷 FPV Kamera"); break;
                case EPiSimSensorType::IMU: SensorIcon = TEXT("📡 IMU Sensörü"); break;
                case EPiSimSensorType::GPS: SensorIcon = TEXT("🛰️ GPS Alıcısı"); break;
                case EPiSimSensorType::LiDAR: SensorIcon = TEXT("🎯 LiDAR"); break;
                default: SensorIcon = TEXT("🔌 Sensör"); break;
            }

            FString BtnLabel = FString::Printf(TEXT("%s: %s"), *SensorIcon, *Sensor.SensorName);

            TSharedRef<SWidget> SensorRowWidget = SNew(SBorder)
                .BorderBackgroundColor(bIsSelected ? FLinearColor(0.0f, 0.88f, 1.0f, 1.0f) : FLinearColor(0.05f, 0.1f, 0.2f, 0.6f))
                .Padding(FMargin(bIsSelected ? 2.0f : 1.0f))
                [
                    SNew(SButton)
                    .ButtonColorAndOpacity(bIsSelected ? FLinearColor(0.05f, 0.22f, 0.45f, 1.0f) : FLinearColor(0.02f, 0.05f, 0.12f, 0.9f))
                    .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModelImporterWidget::OnSelectSensorClicked, SensorIdx))
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth()
                        [
                            SNew(STextBlock).Text(FText::FromString(BtnLabel)).Font(ButtonFont).ColorAndOpacity(FLinearColor::White)
                        ]
                    ]
                ];

            SensorListScrollBox->AddSlot()
            .Padding(FMargin(0.0f, 2.0f))
            [
                SensorRowWidget
            ];
        }
    }

    if (SensorInspectorBorder.IsValid())
    {
        if (TargetImporter->SelectedSensorIndex >= 0 && TargetImporter->ConfiguredSensors.IsValidIndex(TargetImporter->SelectedSensorIndex))
        {
            SensorInspectorBorder->SetVisibility(EVisibility::Visible);
            const FPiSimSensorItem& SelSensor = TargetImporter->ConfiguredSensors[TargetImporter->SelectedSensorIndex];

            if (SelectedSensorTitleText.IsValid())
                SelectedSensorTitleText->SetText(FText::FromString(FString::Printf(TEXT("📡 [%d] %s"), SelSensor.SensorIndex, *SelSensor.SensorName)));

            // 1) Sensor Type Badge
            FString TypeBadge = TEXT("Tip: Bilinmiyor");
            FLinearColor BadgeCol = FLinearColor(0.2f, 1.0f, 0.5f, 1.0f);
            switch (SelSensor.Type)
            {
                case EPiSimSensorType::Camera: TypeBadge = TEXT("📷 FPV Kamera"); BadgeCol = FLinearColor(0.0f, 0.88f, 1.0f, 1.0f); break;
                case EPiSimSensorType::IMU: TypeBadge = TEXT("📡 9-Eksen IMU / Kinematik"); BadgeCol = FLinearColor(1.0f, 0.75f, 0.1f, 1.0f); break;
                case EPiSimSensorType::GPS: TypeBadge = TEXT("🛰️ GNSS / GPS Konum Alıcısı"); BadgeCol = FLinearColor(0.2f, 1.0f, 0.4f, 1.0f); break;
                case EPiSimSensorType::LiDAR: TypeBadge = TEXT("🎯 2D/3D LiDAR Tarayıcı"); BadgeCol = FLinearColor(1.0f, 0.3f, 0.3f, 1.0f); break;
                case EPiSimSensorType::Ultrasonic: TypeBadge = TEXT("🔊 Ultrasonik / Sonar Mesafe"); BadgeCol = FLinearColor(0.8f, 0.4f, 1.0f, 1.0f); break;
                default: break;
            }
            if (SelectedSensorBadgeText.IsValid())
            {
                SelectedSensorBadgeText->SetText(FText::FromString(TypeBadge));
                SelectedSensorBadgeText->SetColorAndOpacity(BadgeCol);
            }

            // 2) Toggle Camera Preview vs Telemetry Box
            if (SelSensor.Type == EPiSimSensorType::Camera)
            {
                if (CameraPreviewBox.IsValid()) CameraPreviewBox->SetVisibility(EVisibility::Visible);
                if (SensorCustomTelemetryBorder.IsValid()) SensorCustomTelemetryBorder->SetVisibility(EVisibility::Collapsed);

                if (TargetImporter->FpvCameraCapture)
                {
                    TargetImporter->FpvCameraCapture->CaptureScene();
                }
                if (TargetImporter->VideoRenderTarget)
                {
                    CameraPreviewBrush.SetResourceObject(TargetImporter->VideoRenderTarget);
                    CameraPreviewBrush.ImageSize = FVector2D(240.0f, 180.0f);
                    CameraPreviewBrush.DrawAs = ESlateBrushDrawType::Image;
                    CameraPreviewBrush.TintColor = FLinearColor::White;
                }
            }
            else
            {
                if (CameraPreviewBox.IsValid()) CameraPreviewBox->SetVisibility(EVisibility::Collapsed);
                if (SensorCustomTelemetryBorder.IsValid()) SensorCustomTelemetryBorder->SetVisibility(EVisibility::Visible);

                if (SensorCustomTelemetryText.IsValid())
                {
                    FString CustomTel;
                    if (SelSensor.Type == EPiSimSensorType::GPS)
                    {
                        FVector RootLoc = TargetImporter->GetActorLocation();
                        double SimLat = 41.0082 + (RootLoc.X * 0.0000089);
                        double SimLon = 28.9784 + (RootLoc.Y * 0.0000089);
                        double SimAlt = 124.5 + (RootLoc.Z * 0.01);
                        CustomTel = FString::Printf(
                            TEXT("  🛰️ [CANLI GPS TELEMETRİSİ]\n"
                                 "  • Enlem (Lat)    : %+.6f° N\n"
                                 "  • Boylam (Lon)   : %+.6f° E\n"
                                 "  • İrtifa (Alt)   : %+.2f m MSL\n"
                                 "  • Uydu Sayısı    : 14 (3D DGPS Fix)\n"
                                 "  • ROS 2 Mesajı   : sensor_msgs/NavSatFix"),
                            SimLat, SimLon, SimAlt
                        );
                    }
                    else if (SelSensor.Type == EPiSimSensorType::IMU)
                    {
                        FRotator LastRot = TargetImporter->LastTxQuat.Rotator();
                        CustomTel = FString::Printf(
                            TEXT("  📡 [CANLI 9-EKSEN IMU VERİLERİ]\n"
                                 "  • Euler Açısı    : R=%+5.1f°, P=%+5.1f°, Y=%+5.1f°\n"
                                 "  • İvme (Accel)   : X=%+5.2f, Y=%+5.2f, Z=%+5.2f m/s²\n"
                                 "  • Jiroskop (Gyro): X=%+5.2f, Y=%+5.2f, Z=%+5.2f °/s\n"
                                 "  • Kuaterniyon    : (X=%.2f, Y=%.2f, Z=%.2f, W=%.2f)"),
                            LastRot.Roll, LastRot.Pitch, LastRot.Yaw,
                            TargetImporter->CurrentLinearAccel.X, TargetImporter->CurrentLinearAccel.Y, TargetImporter->CurrentLinearAccel.Z,
                            TargetImporter->LastTxGyro.X, TargetImporter->LastTxGyro.Y, TargetImporter->LastTxGyro.Z,
                            TargetImporter->LastTxQuat.X, TargetImporter->LastTxQuat.Y, TargetImporter->LastTxQuat.Z, TargetImporter->LastTxQuat.W
                        );
                    }
                    else if (SelSensor.Type == EPiSimSensorType::LiDAR)
                    {
                        CustomTel = FString::Printf(
                            TEXT("  🎯 [CANLI 360° LiDAR TARAMA]\n"
                                 "  • Işın Sayısı    : 360 Lazer Işını (1° Çözünürlük)\n"
                                 "  • Menzil         : 0.10 m - 25.00 m\n"
                                 "  • Ön Engel       : 2.15 m\n"
                                 "  • ROS 2 Mesajı   : sensor_msgs/LaserScan")
                        );
                    }
                    else if (SelSensor.Type == EPiSimSensorType::Ultrasonic)
                    {
                        CustomTel = FString::Printf(
                            TEXT("  🔊 [CANLI ULTRASONİK SONAR]\n"
                                 "  • Ölçülen Mesafe : 0.85 m\n"
                                 "  • Algılama Konisi: 30.0°\n"
                                 "  • Çalışma Frekansı: 40 kHz Ultrasonik")
                        );
                    }
                    SensorCustomTelemetryText->SetText(FText::FromString(CustomTel));
                }
            }

            // 3) Common Details Text
            if (SensorDetailsText.IsValid())
            {
                FString SDetails = FString::Printf(
                    TEXT("  • Yayın Portu  : UDP %d (Direct/Broadcast)\n"
                         "  • Güncelleme   : %d Hz\n"
                         "  • Bağıl Konum  : (X=%+5.1f, Y=%+5.1f, Z=%+5.1f) cm"),
                    SelSensor.Port, SelSensor.Fps, SelSensor.PivotPoint.X, SelSensor.PivotPoint.Y, SelSensor.PivotPoint.Z
                );
                SensorDetailsText->SetText(FText::FromString(SDetails));
            }
        }
        else
        {
            SensorInspectorBorder->SetVisibility(EVisibility::Collapsed);
        }
    }

    // Physics Button Text Update
    if (PhysicsButtonText.IsValid())
    {
        PhysicsButtonText->SetText(FText::FromString(TargetImporter->bIsPhysicsSimulating ? TEXT(" ⚡ FİZİK: AÇIK ") : TEXT(" ⚡ FİZİK SİMÜLE ET ")));
    }
}

// -----------------------------------------------------------------------------
// TAB SWITCHING & ACTION CALLBACKS
// -----------------------------------------------------------------------------
FReply UPiSimModelImporterWidget::OnTabMotorsClicked()
{
    if (TargetImporter) TargetImporter->SetActiveTab(EPiSimActiveTab::MotorsTab);
    return FReply::Handled();
}

FReply UPiSimModelImporterWidget::OnTabTelemetryClicked()
{
    if (TargetImporter) TargetImporter->SetActiveTab(EPiSimActiveTab::TelemetryTab);
    return FReply::Handled();
}

FReply UPiSimModelImporterWidget::OnTabSensorsClicked()
{
    if (TargetImporter) TargetImporter->SetActiveTab(EPiSimActiveTab::SensorsTab);
    return FReply::Handled();
}

FReply UPiSimModelImporterWidget::OnToggleDisplayModeClicked()
{
    if (TargetImporter) TargetImporter->ToggleDisplayMode();
    return FReply::Handled();
}

FReply UPiSimModelImporterWidget::OnToggleAdvancedModeClicked()
{
    if (TargetImporter) TargetImporter->bIsAdvancedMode = !TargetImporter->bIsAdvancedMode;
    return FReply::Handled();
}

FReply UPiSimModelImporterWidget::OnSelectBoneClicked(int32 BoneIdx)
{
    if (TargetImporter) TargetImporter->SelectBone(BoneIdx);
    return FReply::Handled();
}

FReply UPiSimModelImporterWidget::OnRemoveMotorClicked()
{
    if (TargetImporter && TargetImporter->SelectedBoneIndex >= 0)
    {
        TargetImporter->RemoveMotorFromBone(TargetImporter->SelectedBoneIndex);
    }
    return FReply::Handled();
}

FReply UPiSimModelImporterWidget::OnAssignRoleClicked(uint8 RoleEnumVal)
{
    if (TargetImporter && TargetImporter->SelectedBoneIndex >= 0)
    {
        TargetImporter->AssignMotorToBone(TargetImporter->SelectedBoneIndex, (EPiSimMotorRole)RoleEnumVal);
    }
    return FReply::Handled();
}

void UPiSimModelImporterWidget::OnMotorTestSliderChanged(float NewValue)
{
    if (TargetImporter && TargetImporter->SelectedBoneIndex >= 0)
    {
        TargetImporter->SetMotorTestValue(TargetImporter->SelectedBoneIndex, NewValue);
    }
}

FReply UPiSimModelImporterWidget::OnToggleSensorMarkersClicked()
{
    if (TargetImporter)
    {
        TargetImporter->ToggleSensorMarkers(!TargetImporter->bShowSensorMarkers);
        if (SensorMarkerToggleText.IsValid())
            SensorMarkerToggleText->SetText(FText::FromString(TargetImporter->bShowSensorMarkers ? TEXT("👁️ S_ GÖSTER") : TEXT("🕶️ S_ GİZLE")));
    }
    return FReply::Handled();
}

FReply UPiSimModelImporterWidget::OnSelectSensorClicked(int32 SensorIdx)
{
    if (TargetImporter) TargetImporter->SelectSensor(SensorIdx);
    return FReply::Handled();
}

FReply UPiSimModelImporterWidget::OnRemoveSensorClicked()
{
    if (TargetImporter && TargetImporter->SelectedSensorIndex >= 0)
    {
        TargetImporter->RemoveSensor(TargetImporter->SelectedSensorIndex);
    }
    return FReply::Handled();
}

FReply UPiSimModelImporterWidget::OnAddVirtualSensorClicked(uint8 SensorTypeEnumVal)
{
    if (TargetImporter)
    {
        TargetImporter->AddNewVirtualSensor((EPiSimSensorType)SensorTypeEnumVal);
    }
    return FReply::Handled();
}

FReply UPiSimModelImporterWidget::OnScale01Clicked()
{
    if (TargetImporter) TargetImporter->SetScale_0_1X();
    return FReply::Handled();
}

FReply UPiSimModelImporterWidget::OnScale10Clicked()
{
    if (TargetImporter) TargetImporter->SetScale_1_0X();
    return FReply::Handled();
}

FReply UPiSimModelImporterWidget::OnScale100Clicked()
{
    if (TargetImporter) TargetImporter->SetScale_10_0X();
    return FReply::Handled();
}

FReply UPiSimModelImporterWidget::OnReimportClicked()
{
    if (TargetImporter) TargetImporter->ImportAndSpawnRobot();
    return FReply::Handled();
}

FReply UPiSimModelImporterWidget::OnTogglePhysicsClicked()
{
    if (TargetImporter) TargetImporter->TogglePhysicsSimulation();
    return FReply::Handled();
}

