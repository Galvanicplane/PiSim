// PiSimModelImporterWidget.cpp
// Clean, Dedicated Slate-Powered 3-Tab Studio Widget for PiSimModelImporter.

#include "PiSimModelImporterWidget.h"
#include "PiSimModelImporter.h"
#include "PiSimActuatorComponent.h"
#include "PiSimAeroWingComponent.h"
#include "PiSimAutopilotManager.h"
#include "PiSimVirtualSensorSuite.h"
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
    FSlateFontInfo ParamFont = FCoreStyle::GetDefaultFontStyle("Regular", 8);

    auto MakeParamRow = [this, ParamFont](const FString& LabelStr, TSharedPtr<SEditableTextBox>& OutBox, EPiSimParamId ParamId) -> TSharedRef<SWidget>
    {
        return SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .FillWidth(0.6f)
            .VAlign(VAlign_Center)
            .Padding(FMargin(0.0f, 1.0f))
            [
                SNew(STextBlock)
                .Text(FText::FromString(LabelStr))
                .Font(ParamFont)
                .ColorAndOpacity(FLinearColor(0.85f, 0.9f, 1.0f, 1.0f))
            ]
            + SHorizontalBox::Slot()
            .FillWidth(0.4f)
            .VAlign(VAlign_Center)
            .Padding(FMargin(4.0f, 1.0f, 0.0f, 1.0f))
            [
                SAssignNew(OutBox, SEditableTextBox)
                .Font(ParamFont)
                .SelectAllTextWhenFocused(true)
                .ClearKeyboardFocusOnCommit(true)
                .OnTextCommitted(FOnTextCommitted::CreateUObject(this, &UPiSimModelImporterWidget::OnParamTextCommitted, ParamId))
            ];
    };

    auto MakeAeroRow = [&](const FString& LabelStr, const FLinearColor& RowColor, FAeroHudRowCells& OutCells, bool bBold) -> TSharedRef<SWidget>
    {
        FSlateFontInfo CellFont = bBold ? FCoreStyle::GetDefaultFontStyle("Bold", 9) : FCoreStyle::GetDefaultFontStyle("Regular", 9);

        return SNew(SHorizontalBox)
            // Sütun 1: Renk Rozeti + Etiket (110px)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                SNew(SBox).WidthOverride(110.0f)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.0f, 0.0f, 6.0f, 0.0f))
                    [
                        SNew(SBox).WidthOverride(9.0f).HeightOverride(9.0f)
                        [
                            SNew(SBorder).BorderBackgroundColor(RowColor)
                        ]
                    ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [
                        SNew(STextBlock).Text(FText::FromString(LabelStr))
                        .Font(CellFont)
                        .ColorAndOpacity(RowColor)
                    ]
                ]
            ]
            // Sütun 2: Fx (75px)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                SNew(SBox).WidthOverride(75.0f).HAlign(HAlign_Right)
                [
                    SAssignNew(OutCells.Fx, STextBlock).Text(FText::FromString(TEXT("+0.0 N")))
                    .Font(CellFont).ColorAndOpacity(RowColor)
                ]
            ]
            // Sütun 3: Fy (75px)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                SNew(SBox).WidthOverride(75.0f).HAlign(HAlign_Right)
                [
                    SAssignNew(OutCells.Fy, STextBlock).Text(FText::FromString(TEXT("+0.0 N")))
                    .Font(CellFont).ColorAndOpacity(RowColor)
                ]
            ]
            // Sütun 4: Fz (75px)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                SNew(SBox).WidthOverride(75.0f).HAlign(HAlign_Right)
                [
                    SAssignNew(OutCells.Fz, STextBlock).Text(FText::FromString(TEXT("+0.0 N")))
                    .Font(CellFont).ColorAndOpacity(RowColor)
                ]
            ]
            // Sütun 5: Ayırıcı │ (20px)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                SNew(SBox).WidthOverride(20.0f).HAlign(HAlign_Center)
                [
                    SNew(STextBlock).Text(FText::FromString(TEXT("│")))
                    .Font(CellFont).ColorAndOpacity(FLinearColor(0.35f, 0.45f, 0.6f, 0.8f))
                ]
            ]
            // Sütun 6: Mx (85px)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                SNew(SBox).WidthOverride(85.0f).HAlign(HAlign_Right)
                [
                    SAssignNew(OutCells.Mx, STextBlock).Text(FText::FromString(TEXT("+0.00 N·m")))
                    .Font(CellFont).ColorAndOpacity(RowColor)
                ]
            ]
            // Sütun 7: My (85px)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                SNew(SBox).WidthOverride(85.0f).HAlign(HAlign_Right)
                [
                    SAssignNew(OutCells.My, STextBlock).Text(FText::FromString(TEXT("+0.00 N·m")))
                    .Font(CellFont).ColorAndOpacity(RowColor)
                ]
            ]
            // Sütun 8: Mz (85px)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                SNew(SBox).WidthOverride(85.0f).HAlign(HAlign_Right)
                [
                    SAssignNew(OutCells.Mz, STextBlock).Text(FText::FromString(TEXT("+0.00 N·m")))
                    .Font(CellFont).ColorAndOpacity(RowColor)
                ]
            ];
    };

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

                // Live Active Importer Domain Badge (Land, Air, Sea)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(FMargin(8.0f, 0.0f, 0.0f, 0.0f))
                [
                    SAssignNew(ImporterDomainBadgeBorder, SBorder)
                    .BorderBackgroundColor(FLinearColor(0.02f, 0.2f, 0.35f, 0.95f))
                    .Padding(FMargin(10.0f, 3.0f))
                    [
                        SAssignNew(ImporterDomainBadgeText, STextBlock)
                        .Text(FText::FromString(TEXT("✈️ AIR IMPORTER")))
                        .Font(BadgeFont)
                        .ColorAndOpacity(FLinearColor(0.0f, 0.9f, 1.0f, 1.0f))
                    ]
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(FMargin(8.0f, 0.0f, 0.0f, 0.0f))
                [
                    SAssignNew(ControlModeBadgeBorder, SBorder)
                    .BorderBackgroundColor(FLinearColor(0.14f, 0.1f, 0.32f, 0.95f))
                    .Padding(FMargin(10.0f, 3.0f))
                    [
                        SAssignNew(ControlModeBadgeText, STextBlock)
                        .Text(FText::FromString(TEXT("🎮 MOD: Direkt ROS 2")))
                        .Font(BadgeFont)
                        .ColorAndOpacity(FLinearColor(0.8f, 0.75f, 1.0f, 1.0f))
                    ]
                ]

                + SHorizontalBox::Slot().FillWidth(1.0f) // Spacer

                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(FMargin(2.0f, 0.0f))
                [
                    SNew(SButton)
                    .ButtonColorAndOpacity(FLinearColor(0.18f, 0.08f, 0.34f, 1.0f))
                    .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModelImporterWidget::OnCycleControlModeClicked))
                    .ToolTipText(FText::FromString(TEXT("Kontrol modunu sırayla değiştir: Direkt ROS 2 / ArduPilot SITL / PX4 SITL / PX4 HITL")))
                    [
                        SAssignNew(ControlModeButtonText, STextBlock)
                        .Text(FText::FromString(TEXT(" 🔁 MOD DEĞİŞTİR ")))
                        .Font(ButtonFont)
                        .ColorAndOpacity(FLinearColor::White)
                    ]
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(FMargin(2.0f, 0.0f))
                [
                    SNew(SButton)
                    .ButtonColorAndOpacity(FLinearColor(0.12f, 0.24f, 0.42f, 1.0f))
                    .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModelImporterWidget::OnCycleSerialPortClicked))
                    .ToolTipText(FText::FromString(TEXT("PX4 HITL için seri portu değiştir. SITL modlarında aktif uç nokta bilgisini gösterir.")))
                    [
                        SAssignNew(SerialPortButtonText, STextBlock)
                        .Text(FText::FromString(TEXT(" COM3 ")))
                        .Font(ButtonFont)
                        .ColorAndOpacity(FLinearColor::White)
                    ]
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(FMargin(2.0f, 0.0f))
                [
                    SNew(SButton)
                    .ButtonColorAndOpacity(FLinearColor(0.06f, 0.4f, 0.22f, 1.0f))
                    .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModelImporterWidget::OnToggleAutopilotLinkClicked))
                    .ToolTipText(FText::FromString(TEXT("Seçili kontrol backend'ine bağlan veya bağlantıyı kes")))
                    [
                        SAssignNew(AutopilotLinkButtonText, STextBlock)
                        .Text(FText::FromString(TEXT(" 🔌 BAĞLAN ")))
                        .Font(ButtonFont)
                        .ColorAndOpacity(FLinearColor::White)
                    ]
                ]

                // Scale Multiply * 0.1
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(FMargin(2.0f, 0.0f))
                [
                    SNew(SButton)
                    .ButtonColorAndOpacity(FLinearColor(0.08f, 0.16f, 0.28f, 1.0f))
                    .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModelImporterWidget::OnMultiplyScale01Clicked))
                    .ToolTipText(FText::FromString(TEXT("Modelin import ölçeğini 0.1 ile çarp (* 0.1)")))
                    [
                        SNew(STextBlock).Text(FText::FromString(TEXT(" × 0.1 "))).Font(ButtonFont).ColorAndOpacity(FLinearColor::White)
                    ]
                ]

                // Scale Multiply * 10
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(FMargin(2.0f, 0.0f))
                [
                    SNew(SButton)
                    .ButtonColorAndOpacity(FLinearColor(0.0f, 0.45f, 0.75f, 1.0f))
                    .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModelImporterWidget::OnMultiplyScale10Clicked))
                    .ToolTipText(FText::FromString(TEXT("Modelin import ölçeğini 10 ile çarp (* 10)")))
                    [
                        SNew(STextBlock).Text(FText::FromString(TEXT(" × 10 "))).Font(ButtonFont).ColorAndOpacity(FLinearColor::White)
                    ]
                ]

                // Model Switch Button (robot_import_test.fbx <-> robot_import_test1.fbx)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(FMargin(6.0f, 0.0f, 2.0f, 0.0f))
                [
                    SNew(SButton)
                    .ButtonColorAndOpacity(FLinearColor(0.42f, 0.15f, 0.65f, 1.0f))
                    .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModelImporterWidget::OnToggleModelClicked))
                    [
                        SAssignNew(ModelButtonText, STextBlock)
                        .Text(FText::FromString(TEXT(" 📦 MODEL: test.fbx ")))
                        .Font(ButtonFont)
                        .ColorAndOpacity(FLinearColor::White)
                    ]
                ]

                // Reimport
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(FMargin(2.0f, 0.0f, 2.0f, 0.0f))
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
        // 2) TOP-CENTER FLIGHT TEST TELEMETRY (KUVVETLER & CoG MOMENTLERİ)
        // ---------------------------------------------------------------------
        + SOverlay::Slot()
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Top)
        .Padding(FMargin(0.0f, 48.0f, 0.0f, 0.0f))
        [
            SNew(SBorder)
            .BorderBackgroundColor(FLinearColor(0.012f, 0.02f, 0.04f, 0.94f))
            .Padding(FMargin(14.0f, 8.0f))
            [
                SNew(SVerticalBox)

                // Panel Başlığı
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.0f, 0.0f, 0.0f, 4.0f)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("⚡ PiSim AERODİNAMİK UÇUŞ TESTİ TELEMETRİSİ (KUVVETLER & CoG MOMENTLERİ)")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                    .ColorAndOpacity(FLinearColor(0.0f, 0.92f, 1.0f, 1.0f))
                ]

                // Tablo Başlık Satırı (Sabit Sütun Genişlikleri)
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.0f, 0.0f, 0.0f, 3.0f)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [
                        SNew(SBox).WidthOverride(110.0f)
                        [
                            SNew(STextBlock).Text(FText::FromString(TEXT("KAYNAK")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                            .ColorAndOpacity(FLinearColor(0.6f, 0.7f, 0.85f, 1.0f))
                        ]
                    ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [
                        SNew(SBox).WidthOverride(75.0f).HAlign(HAlign_Right)
                        [
                            SNew(STextBlock).Text(FText::FromString(TEXT("Fx (İleri)")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                            .ColorAndOpacity(FLinearColor(0.6f, 0.7f, 0.85f, 1.0f))
                        ]
                    ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [
                        SNew(SBox).WidthOverride(75.0f).HAlign(HAlign_Right)
                        [
                            SNew(STextBlock).Text(FText::FromString(TEXT("Fy (Yan)")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                            .ColorAndOpacity(FLinearColor(0.6f, 0.7f, 0.85f, 1.0f))
                        ]
                    ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [
                        SNew(SBox).WidthOverride(75.0f).HAlign(HAlign_Right)
                        [
                            SNew(STextBlock).Text(FText::FromString(TEXT("Fz (Dikey)")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                            .ColorAndOpacity(FLinearColor(0.6f, 0.7f, 0.85f, 1.0f))
                        ]
                    ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [
                        SNew(SBox).WidthOverride(20.0f).HAlign(HAlign_Center)
                        [
                            SNew(STextBlock).Text(FText::FromString(TEXT("│")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                            .ColorAndOpacity(FLinearColor(0.35f, 0.45f, 0.6f, 0.8f))
                        ]
                    ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [
                        SNew(SBox).WidthOverride(85.0f).HAlign(HAlign_Right)
                        [
                            SNew(STextBlock).Text(FText::FromString(TEXT("Mx (Roll)")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                            .ColorAndOpacity(FLinearColor(0.6f, 0.7f, 0.85f, 1.0f))
                        ]
                    ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [
                        SNew(SBox).WidthOverride(85.0f).HAlign(HAlign_Right)
                        [
                            SNew(STextBlock).Text(FText::FromString(TEXT("My (Pitch)")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                            .ColorAndOpacity(FLinearColor(0.6f, 0.7f, 0.85f, 1.0f))
                        ]
                    ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [
                        SNew(SBox).WidthOverride(85.0f).HAlign(HAlign_Right)
                        [
                            SNew(STextBlock).Text(FText::FromString(TEXT("Mz (Yaw)")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                            .ColorAndOpacity(FLinearColor(0.6f, 0.7f, 0.85f, 1.0f))
                        ]
                    ]
                ]

                // Sütunlu Satırlar
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f)
                [
                    MakeAeroRow(TEXT("LİFT"), FLinearColor(0.1f, 1.0f, 0.45f), AeroHudLiftCells, false)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f)
                [
                    MakeAeroRow(TEXT("DRAG"), FLinearColor(1.0f, 0.55f, 0.05f), AeroHudDragCells, false)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f)
                [
                    MakeAeroRow(TEXT("İTKİ"), FLinearColor(0.9f, 0.3f, 1.0f), AeroHudThrustCells, false)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f)
                [
                    MakeAeroRow(TEXT("YERÇEKİMİ"), FLinearColor(1.0f, 0.25f, 0.25f), AeroHudGravCells, false)
                ]

                // İnce Ayırıcı Çizgi
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
                [
                    SNew(SBorder)
                    .BorderBackgroundColor(FLinearColor(0.25f, 0.35f, 0.5f, 0.7f))
                    .Padding(FMargin(0.0f, 0.5f))
                ]

                // TOPLAM NET Satırı
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f)
                [
                    MakeAeroRow(TEXT("TOPLAM NET"), FLinearColor::White, AeroHudTotalCells, true)
                ]

                // Alt Durum Çubuğu (Hız, AoA, L/D, CoG, Lift Çarpanı)
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.0f, 3.0f, 0.0f, 0.0f)
                [
                    SAssignNew(AeroHudFooterText, STextBlock)
                    .Text(FText::FromString(TEXT("Hız: 0.0 km/h   │   AoA: +0.0°   │   L/D: 0.0   │   CoG: +0.0 cm [O/P]   │   Lift Çarpanı: 1.00x [L/K]")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                    .ColorAndOpacity(FLinearColor(0.95f, 0.85f, 0.2f, 1.0f))
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
                            .Padding(0.0f, 0.0f, 0.0f, 6.0f)
                            [
                                SNew(SBorder).BorderBackgroundColor(FLinearColor(0.1f, 0.25f, 0.45f, 0.6f)).Padding(FMargin(0.0f, 0.5f))
                            ]

                            // Live ROS 2 Actuator Feedback Banner
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, 0.0f, 0.0f, 8.0f)
                            [
                                SNew(SBorder)
                                .BorderBackgroundColor(FLinearColor(0.02f, 0.12f, 0.22f, 0.95f))
                                .Padding(FMargin(8.0f, 5.0f))
                                [
                                    SAssignNew(RosControlLiveBannerText, STextBlock)
                                    .Text(FText::FromString(TEXT("📡 CANLI ROS 2: Bekleniyor (/cmd_vel)...")))
                                    .Font(ParamFont)
                                    .ColorAndOpacity(FLinearColor(0.2f, 1.0f, 0.5f, 1.0f))
                                ]
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

                            // Chassis & Aerodynamics Container (Shown if BoneIndex == 0)
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            [
                                SAssignNew(ChassisBox, SBox)
                                .Visibility(EVisibility::Collapsed)
                                [
                                    SNew(SVerticalBox)

                                    // Header
                                    + SVerticalBox::Slot()
                                    .AutoHeight()
                                    .Padding(0.0f, 0.0f, 0.0f, 6.0f)
                                    [
                                        SNew(STextBlock)
                                        .Text(FText::FromString(TEXT("✈️ AERODİNAMİK VE ŞASİ AYARLARI")))
                                        .Font(CardHeaderFont)
                                        .ColorAndOpacity(FLinearColor(0.0f, 0.88f, 1.0f, 1.0f))
                                    ]

                                    // Aero rows
                                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f)
                                    [ MakeParamRow(TEXT("Kanat Alanı (S) [m²]"), AeroWingAreaInput, EPiSimParamId::AeroWingArea) ]
                                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f)
                                    [ MakeParamRow(TEXT("Kanat Açıklığı (b) [m]"), AeroWingspanInput, EPiSimParamId::AeroWingspan) ]
                                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f)
                                    [ MakeParamRow(TEXT("Taşıma Sabiti (CL0)"), AeroCL0Input, EPiSimParamId::AeroCL0) ]
                                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f)
                                    [ MakeParamRow(TEXT("Taşıma Eğimi (CL_α) [/rad]"), AeroCLAlphaInput, EPiSimParamId::AeroCLAlpha) ]
                                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f)
                                    [ MakeParamRow(TEXT("Sürükleme Sabiti (CD0)"), AeroCD0Input, EPiSimParamId::AeroCD0) ]
                                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f)
                                    [ MakeParamRow(TEXT("Stall Açısı [°]"), AeroStallAngleInput, EPiSimParamId::AeroStallAngle) ]
                                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f)
                                    [ MakeParamRow(TEXT("Elevon Etkinliği"), AeroElevonEffectInput, EPiSimParamId::AeroElevonEffect) ]
                                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f)
                                    [ MakeParamRow(TEXT("CoL İleri (Lift Noktası) [cm]"), AeroCoLForwardInput, EPiSimParamId::AeroCoLForward) ]
                                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f)
                                    [ MakeParamRow(TEXT("CoG İleri (Ağırlık Merk.) [cm]"), AeroCoGForwardInput, EPiSimParamId::AeroCoGForward) ]
                                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f)
                                    [ MakeParamRow(TEXT("Eylemsizlik (Inertia Scale)"), AeroInertiaTensorScaleInput, EPiSimParamId::AeroInertiaTensorScale) ]
                                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f)
                                    [ MakeParamRow(TEXT("Şasi Kütlesi (Mass) [kg]"), ChassisMassInput, EPiSimParamId::ChassisMass) ]

                                    // Quick CoG / Gizmo buttons
                                    + SVerticalBox::Slot()
                                    .AutoHeight()
                                    .Padding(0.0f, 6.0f, 0.0f, 2.0f)
                                    [
                                        SNew(SHorizontalBox)
                                        + SHorizontalBox::Slot()
                                        .FillWidth(0.5f)
                                        .Padding(FMargin(0.0f, 0.0f, 2.0f, 0.0f))
                                        [
                                            SNew(SButton)
                                            .ButtonColorAndOpacity(FLinearColor(0.2f, 0.1f, 0.35f, 1.0f))
                                            .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModelImporterWidget::OnSetCoGToBoneEndClicked))
                                            .ContentPadding(FMargin(4.0f, 4.0f))
                                            [
                                                SNew(STextBlock)
                                                .Text(FText::FromString(TEXT("🎯 CoG -> KEMİK UCUNA AL")))
                                                .Font(ParamFont)
                                                .ColorAndOpacity(FLinearColor::White)
                                            ]
                                        ]
                                        + SHorizontalBox::Slot()
                                        .FillWidth(0.5f)
                                        .Padding(FMargin(2.0f, 0.0f, 0.0f, 0.0f))
                                        [
                                            SNew(SButton)
                                            .ButtonColorAndOpacity(FLinearColor(0.08f, 0.25f, 0.35f, 1.0f))
                                            .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModelImporterWidget::OnSetCoGToCenterOfMassClicked))
                                            .ContentPadding(FMargin(4.0f, 4.0f))
                                            [
                                                SNew(STextBlock)
                                                .Text(FText::FromString(TEXT("⚖️ CoG -> GEOMETRİK MERKEZE")))
                                                .Font(ParamFont)
                                                .ColorAndOpacity(FLinearColor::White)
                                            ]
                                        ]
                                    ]

                                    + SVerticalBox::Slot()
                                    .AutoHeight()
                                    .Padding(0.0f, 2.0f, 0.0f, 6.0f)
                                    [
                                        SNew(SButton)
                                        .ButtonColorAndOpacity(FLinearColor(0.05f, 0.3f, 0.2f, 1.0f))
                                        .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModelImporterWidget::OnToggleAeroGizmosClicked))
                                        .ContentPadding(FMargin(6.0f, 4.0f))
                                        [
                                            SAssignNew(AeroGizmoToggleText, STextBlock)
                                            .Text(FText::FromString(TEXT("📐 3D VEKTÖR GİZMO: AÇIK")))
                                            .Font(BadgeFont)
                                            .ColorAndOpacity(FLinearColor(0.2f, 1.0f, 0.6f, 1.0f))
                                        ]
                                    ]

                                    // Aero Kuvvet Ölçek Butonları (Debug: 0.1x / 1x / 10x)
                                    + SVerticalBox::Slot()
                                    .AutoHeight()
                                    .Padding(0.0f, 2.0f, 0.0f, 4.0f)
                                    [
                                        SNew(SHorizontalBox)
                                        + SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 2.0f, 0.0f)
                                        [
                                            SNew(SButton)
                                            .ButtonColorAndOpacity(FLinearColor(0.4f, 0.08f, 0.08f, 1.0f))
                                            .OnClicked_Lambda([this]() -> FReply {
                                                if (TargetImporter) TargetImporter->AeroConfig.AeroForceScale = 0.1f;
                                                return FReply::Handled();
                                            })
                                            .ContentPadding(FMargin(4.0f, 3.0f))
                                            [ SNew(STextBlock).Text(FText::FromString(TEXT("⬇ 0.1x"))).Font(BadgeFont).ColorAndOpacity(FLinearColor(1.0f, 0.5f, 0.5f, 1.0f)).Justification(ETextJustify::Center) ]
                                        ]
                                        + SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f, 0.0f)
                                        [
                                            SNew(SButton)
                                            .ButtonColorAndOpacity(FLinearColor(0.05f, 0.25f, 0.05f, 1.0f))
                                            .OnClicked_Lambda([this]() -> FReply {
                                                if (TargetImporter) TargetImporter->AeroConfig.AeroForceScale = 1.0f;
                                                return FReply::Handled();
                                            })
                                            .ContentPadding(FMargin(4.0f, 3.0f))
                                            [ SNew(STextBlock).Text(FText::FromString(TEXT("↔ 1x"))).Font(BadgeFont).ColorAndOpacity(FLinearColor(0.4f, 1.0f, 0.4f, 1.0f)).Justification(ETextJustify::Center) ]
                                        ]
                                        + SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f, 0.0f, 0.0f, 0.0f)
                                        [
                                            SNew(SButton)
                                            .ButtonColorAndOpacity(FLinearColor(0.08f, 0.08f, 0.45f, 1.0f))
                                            .OnClicked_Lambda([this]() -> FReply {
                                                if (TargetImporter) TargetImporter->AeroConfig.AeroForceScale = 10.0f;
                                                return FReply::Handled();
                                            })
                                            .ContentPadding(FMargin(4.0f, 3.0f))
                                            [ SNew(STextBlock).Text(FText::FromString(TEXT("⬆ 10x"))).Font(BadgeFont).ColorAndOpacity(FLinearColor(0.5f, 0.7f, 1.0f, 1.0f)).Justification(ETextJustify::Center) ]
                                        ]
                                    ]

                                    // Dinamik Lift Çarpanı Adım Butonları (-0.2x / +0.2x)
                                    + SVerticalBox::Slot()
                                    .AutoHeight()
                                    .Padding(0.0f, 2.0f, 0.0f, 4.0f)
                                    [
                                        SNew(SHorizontalBox)
                                        + SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 2.0f, 0.0f)
                                        [
                                            SNew(SButton)
                                            .ButtonColorAndOpacity(FLinearColor(0.35f, 0.15f, 0.05f, 1.0f))
                                            .OnClicked_Lambda([this]() -> FReply {
                                                if (TargetImporter) TargetImporter->DecreaseLiftScale();
                                                return FReply::Handled();
                                            })
                                            .ContentPadding(FMargin(4.0f, 3.0f))
                                            [ SNew(STextBlock).Text(FText::FromString(TEXT("➖ Lift -0.2x (K)"))).Font(BadgeFont).ColorAndOpacity(FLinearColor::White).Justification(ETextJustify::Center) ]
                                        ]
                                        + SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f, 0.0f, 0.0f, 0.0f)
                                        [
                                            SNew(SButton)
                                            .ButtonColorAndOpacity(FLinearColor(0.05f, 0.35f, 0.3f, 1.0f))
                                            .OnClicked_Lambda([this]() -> FReply {
                                                if (TargetImporter) TargetImporter->IncreaseLiftScale();
                                                return FReply::Handled();
                                            })
                                            .ContentPadding(FMargin(4.0f, 3.0f))
                                            [ SNew(STextBlock).Text(FText::FromString(TEXT("➕ Lift +0.2x (L)"))).Font(BadgeFont).ColorAndOpacity(FLinearColor::White).Justification(ETextJustify::Center) ]
                                        ]
                                    ]

                                    // Live Flight Telemetry Section
                                    + SVerticalBox::Slot()
                                    .AutoHeight()
                                    .Padding(0.0f, 4.0f, 0.0f, 2.0f)
                                    [
                                        SNew(STextBlock)
                                        .Text(FText::FromString(TEXT("📊 CANLI UÇUŞ VE AERO TELEMETRİSİ:")))
                                        .Font(BadgeFont)
                                        .ColorAndOpacity(FLinearColor(1.0f, 0.75f, 0.1f, 1.0f))
                                    ]
                                    + SVerticalBox::Slot()
                                    .AutoHeight()
                                    .Padding(0.0f, 0.0f, 0.0f, 6.0f)
                                    [
                                        SAssignNew(FlightTelemetryLiveText, STextBlock)
                                        .Text(FText::FromString(TEXT("Uçuş verileri bekleniyor...")))
                                        .Font(DataFont)
                                        .ColorAndOpacity(FLinearColor(0.9f, 0.95f, 1.0f, 1.0f))
                                    ]
                                ]
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

                                    // Motor Editable Parameters
                                    + SVerticalBox::Slot()
                                    .AutoHeight()
                                    .Padding(0.0f, 2.0f, 0.0f, 6.0f)
                                    [
                                        SNew(SVerticalBox)
                                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f)
                                        [ MakeParamRow(TEXT("Maksimum Hız [RPM]"), MotorMaxRpmInput, EPiSimParamId::MotorMaxRpm) ]
                                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f)
                                        [ MakeParamRow(TEXT("Maksimum Tork [Nm]"), MotorMaxTorqueInput, EPiSimParamId::MotorMaxTorque) ]
                                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f)
                                        [ MakeParamRow(TEXT("Min Sınır [°]"), MotorMinLimitInput, EPiSimParamId::MotorMinLimit) ]
                                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f)
                                        [ MakeParamRow(TEXT("Maks Sınır [°]"), MotorMaxLimitInput, EPiSimParamId::MotorMaxLimit) ]
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

                                    // Thruster-only: İtki Yönü Toggle Butonu
                                    + SVerticalBox::Slot()
                                    .AutoHeight()
                                    .Padding(0.0f, 4.0f, 0.0f, 0.0f)
                                    [
                                        SAssignNew(ThrusterControlsBox, SBox)
                                        .Visibility(EVisibility::Collapsed)
                                        [
                                            SNew(SButton)
                                            .ButtonColorAndOpacity(FLinearColor(0.08f, 0.08f, 0.35f, 1.0f))
                                            .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModelImporterWidget::OnToggleReverseThrustClicked))
                                            .ContentPadding(FMargin(10.0f, 6.0f))
                                            [
                                                SAssignNew(ReverseThrustButtonText, STextBlock)
                                                .Text(FText::FromString(TEXT("➡️ İTKİ YÖNÜ: NORMAL (Tıkla → Ters Çevir)")))
                                                .Font(ButtonFont)
                                                .ColorAndOpacity(FLinearColor::White)
                                            ]
                                        ]
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
                            .Padding(0.0f, 0.0f, 0.0f, 6.0f)
                            [
                                SAssignNew(ModelCadStatusText, STextBlock).Text(FText::FromString(TEXT("Yükleniyor..."))).Font(DataFont).ColorAndOpacity(FLinearColor(0.85f, 0.92f, 1.0f, 1.0f))
                            ]

                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)[ SNew(SBorder).BorderBackgroundColor(FLinearColor(0.1f, 0.25f, 0.45f, 0.6f)).Padding(FMargin(0.0f, 0.5f)) ]

                            // Card 3: Modular Sockets & Components Live Debug
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, 3.0f, 0.0f, 3.0f)
                            [
                                SNew(STextBlock).Text(FText::FromString(TEXT("🔌 MODÜLER SOKETLER & BİLEŞENLER (CANLI DEBUG)"))).Font(CardHeaderFont).ColorAndOpacity(FLinearColor(0.0f, 0.88f, 1.0f, 1.0f))
                            ]
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            [
                                SAssignNew(ModularComponentsDebugText, STextBlock).Text(FText::FromString(TEXT("Bileşenler taranıyor..."))).Font(DataFont).ColorAndOpacity(FLinearColor(0.85f, 0.95f, 1.0f, 1.0f))
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
    const UPiSimAutopilotManager* Autopilot = TargetImporter->AutopilotManager;

    // 1) Top Connection Badge & Colors
    if (ConnectionBadgeText.IsValid() && ConnectionBadgeBorder.IsValid())
    {
        const bool bDirectRosMode = !Autopilot || Autopilot->ControlMode == EPiSimControlMode::DirectROS2;
        if (!bDirectRosMode && Autopilot)
        {
            const FPiSimAutopilotTelemetry& Tel = Autopilot->Telemetry;
            if (Tel.bReceivingActuators)
            {
                ConnectionBadgeBorder->SetBorderBackgroundColor(FLinearColor(0.02f, 0.32f, 0.12f, 0.95f));
                ConnectionBadgeText->SetText(FText::FromString(FString::Printf(TEXT("🟢 OTOPİLOT AKTİF: %s"), *Tel.StatusLine)));
                ConnectionBadgeText->SetColorAndOpacity(FLinearColor(0.2f, 1.0f, 0.4f, 1.0f));
            }
            else if (Tel.bLinkUp)
            {
                ConnectionBadgeBorder->SetBorderBackgroundColor(FLinearColor(0.18f, 0.22f, 0.02f, 0.95f));
                ConnectionBadgeText->SetText(FText::FromString(FString::Printf(TEXT("🟡 OTOPİLOT BAĞLI: %s"), *Tel.StatusLine)));
                ConnectionBadgeText->SetColorAndOpacity(FLinearColor(1.0f, 0.9f, 0.3f, 1.0f));
            }
            else
            {
                ConnectionBadgeBorder->SetBorderBackgroundColor(FLinearColor(0.28f, 0.08f, 0.08f, 0.95f));
                ConnectionBadgeText->SetText(FText::FromString(FString::Printf(TEXT("🔴 OTOPİLOT BEKLENİYOR: %s"), *Tel.StatusLine)));
                ConnectionBadgeText->SetColorAndOpacity(FLinearColor(1.0f, 0.45f, 0.45f, 1.0f));
            }
        }
        else if (TargetImporter->bIsPiConnected)
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

    if (ControlModeBadgeText.IsValid() && ControlModeBadgeBorder.IsValid())
    {
        FString ModeText = TEXT("🎮 MOD: Direkt ROS 2");
        FLinearColor ModeBg(0.08f, 0.16f, 0.28f, 0.95f);
        FLinearColor ModeFg(0.6f, 0.9f, 1.0f, 1.0f);

        if (Autopilot)
        {
            switch (Autopilot->ControlMode)
            {
            case EPiSimControlMode::DirectROS2:
                ModeText = TEXT("🎮 MOD: Direkt ROS 2");
                ModeBg = FLinearColor(0.08f, 0.16f, 0.28f, 0.95f);
                ModeFg = FLinearColor(0.6f, 0.9f, 1.0f, 1.0f);
                break;
            case EPiSimControlMode::ArduPilotSITL:
                ModeText = TEXT("🎮 MOD: ArduPilot SITL");
                ModeBg = FLinearColor(0.24f, 0.16f, 0.02f, 0.95f);
                ModeFg = FLinearColor(1.0f, 0.85f, 0.35f, 1.0f);
                break;
            case EPiSimControlMode::PX4SITL:
                ModeText = TEXT("🎮 MOD: PX4 SITL");
                ModeBg = FLinearColor(0.16f, 0.08f, 0.30f, 0.95f);
                ModeFg = FLinearColor(0.85f, 0.75f, 1.0f, 1.0f);
                break;
            case EPiSimControlMode::PX4HITL:
                ModeText = TEXT("🎮 MOD: PX4 HITL");
                ModeBg = FLinearColor(0.30f, 0.08f, 0.22f, 0.95f);
                ModeFg = FLinearColor(1.0f, 0.70f, 0.92f, 1.0f);
                break;
            }
        }

        ControlModeBadgeBorder->SetBorderBackgroundColor(ModeBg);
        ControlModeBadgeText->SetText(FText::FromString(ModeText));
        ControlModeBadgeText->SetColorAndOpacity(ModeFg);
    }

    if (ControlModeButtonText.IsValid() && Autopilot)
    {
        ControlModeButtonText->SetText(FText::FromString(TEXT(" 🔁 MOD DEĞİŞTİR ")));
    }

    if (SerialPortButtonText.IsValid() && Autopilot)
    {
        FString PortLabel;
        switch (Autopilot->ControlMode)
        {
        case EPiSimControlMode::DirectROS2:
            PortLabel = FString::Printf(TEXT(" Pi5: %s "), *TargetImporter->BridgeTargetIP);
            break;
        case EPiSimControlMode::ArduPilotSITL:
            PortLabel = FString::Printf(TEXT(" UDP:%d "), Autopilot->ArduPilotListenPort);
            break;
        case EPiSimControlMode::PX4SITL:
            PortLabel = FString::Printf(TEXT(" TCP:%d / UDP:%d "), Autopilot->Px4SitlTcpPort, Autopilot->Px4SitlUdpPort);
            break;
        case EPiSimControlMode::PX4HITL:
            PortLabel = FString::Printf(TEXT(" %s "), *Autopilot->SerialPortName);
            break;
        }
        SerialPortButtonText->SetText(FText::FromString(PortLabel));
    }

    if (AutopilotLinkButtonText.IsValid() && Autopilot)
    {
        const bool bConnected = (Autopilot->ControlMode == EPiSimControlMode::DirectROS2)
            ? TargetImporter->bIsPiConnected
            : Autopilot->Telemetry.bLinkUp;
        AutopilotLinkButtonText->SetText(FText::FromString(bConnected ? TEXT(" 🔌 BAĞLANTIYI KES ") : TEXT(" 🔌 BAĞLAN ")));
    }

    // Active Importer Domain Badge Update
    if (ImporterDomainBadgeText.IsValid() && ImporterDomainBadgeBorder.IsValid())
    {
        switch (TargetImporter->VehicleDomain)
        {
            case EVehicleDomain::Air:
                ImporterDomainBadgeBorder->SetBorderBackgroundColor(FLinearColor(0.02f, 0.25f, 0.45f, 0.95f));
                ImporterDomainBadgeText->SetText(FText::FromString(TEXT("✈️ AIR IMPORTER (Airframe)")));
                ImporterDomainBadgeText->SetColorAndOpacity(FLinearColor(0.0f, 0.9f, 1.0f, 1.0f));
                break;
            case EVehicleDomain::Land:
                ImporterDomainBadgeBorder->SetBorderBackgroundColor(FLinearColor(0.35f, 0.25f, 0.02f, 0.95f));
                ImporterDomainBadgeText->SetText(FText::FromString(TEXT("🚗 LAND IMPORTER (Chassis)")));
                ImporterDomainBadgeText->SetColorAndOpacity(FLinearColor(1.0f, 0.85f, 0.2f, 1.0f));
                break;
            case EVehicleDomain::Sea:
                ImporterDomainBadgeBorder->SetBorderBackgroundColor(FLinearColor(0.02f, 0.3f, 0.2f, 0.95f));
                ImporterDomainBadgeText->SetText(FText::FromString(TEXT("⚓ SEA IMPORTER (Hull)")));
                ImporterDomainBadgeText->SetColorAndOpacity(FLinearColor(0.2f, 1.0f, 0.6f, 1.0f));
                break;
            default:
                ImporterDomainBadgeBorder->SetBorderBackgroundColor(FLinearColor(0.15f, 0.15f, 0.15f, 0.95f));
                ImporterDomainBadgeText->SetText(FText::FromString(TEXT("⚙️ BASE IMPORTER")));
                ImporterDomainBadgeText->SetColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f, 1.0f));
                break;
        }
    }

    // Live ROS Control Banner Update (Tab 1)
    if (RosControlLiveBannerText.IsValid())
    {
        const bool bDirectRosMode = !Autopilot || Autopilot->ControlMode == EPiSimControlMode::DirectROS2;
        if (!bDirectRosMode && Autopilot)
        {
            const FPiSimAutopilotTelemetry& Tel = Autopilot->Telemetry;
            const FString PhysStatus = TargetImporter->bIsPhysicsSimulating ? TEXT("⚡ FİZİK AKTİF") : TEXT("⚠️ FİZİK KAPALI");
            FString BannerText = FString::Printf(
                TEXT("🧭 OTOPİLOT KONTROLÜ │ %s\n  • Sensör TX: %3.1f Hz | Aktüatör RX: %3.1f Hz | Armed: %s\n  • Komutlar : Roll=%+.2f Pitch=%+.2f Yaw=%+.2f Thr=%+.2f | %s"),
                *Tel.StatusLine,
                Tel.SensorTxHz,
                Tel.ActuatorRxHz,
                Tel.bArmed ? TEXT("EVET") : TEXT("HAYIR"),
                Tel.RollCmd,
                Tel.PitchCmd,
                Tel.YawCmd,
                Tel.ThrottleCmd,
                *PhysStatus);
            RosControlLiveBannerText->SetText(FText::FromString(BannerText));
            RosControlLiveBannerText->SetColorAndOpacity(Tel.bReceivingActuators ? FLinearColor(0.3f, 1.0f, 0.45f, 1.0f) : FLinearColor(1.0f, 0.85f, 0.25f, 1.0f));
        }
        else if (TargetImporter->bIsPiConnected && TargetImporter->TotalPacketsReceived > 0)
        {
            FString RosStr = TEXT("");
            FString PhysStatus = TargetImporter->bIsPhysicsSimulating ? TEXT("⚡ FİZİK AKTİF") : TEXT("⚠️ FİZİK KAPALI (Fizik Simüle Et'e basın)");
            if (TargetImporter->VehicleDomain == EVehicleDomain::Air)
            {
                float ThrPct = FMath::Clamp(TargetImporter->TargetLinearX * 100.0f, 0.0f, 100.0f);
                float ElevonAngleL = (TargetImporter->LastRxAngularVel.Y + TargetImporter->LastRxAngularVel.Z) * 15.0f;
                float ElevonAngleR = (TargetImporter->LastRxAngularVel.Y - TargetImporter->LastRxAngularVel.Z) * 15.0f;
                RosStr = FString::Printf(TEXT("📡 CANLI ROS 2 [%3.1f Hz | Paket #%d] │ %s\n  • Girdi : LinX(Gaz)=%+.2f, AngZ(Dönüş)=%+.2f | Hız: %4.1f km/h\n  • Çıktı : İtki=%%%0.0f | Elevon Sol=%+.1f°, Sağ=%+.1f°"),
                    TargetImporter->RxPacketRateHz, TargetImporter->TotalPacketsReceived, *PhysStatus,
                    TargetImporter->TargetLinearX, TargetImporter->TargetAngularZ, TargetImporter->CurrentForwardSpeedKmh,
                    ThrPct, ElevonAngleL, ElevonAngleR);
            }
            else
            {
                RosStr = FString::Printf(TEXT("📡 CANLI ROS 2 [%3.1f Hz | Paket #%d] │ %s\n  • Girdi : LinX=%+.2f m/s, AngZ=%+.2f rad/s | Hız: %4.1f km/h\n  • Çıktı : Sol=%+.0f RPM, Sağ=%+.0f RPM"),
                    TargetImporter->RxPacketRateHz, TargetImporter->TotalPacketsReceived, *PhysStatus,
                    TargetImporter->TargetLinearX, TargetImporter->TargetAngularZ, TargetImporter->CurrentForwardSpeedKmh,
                    TargetImporter->LeftWheelsRpm, TargetImporter->RightWheelsRpm);
            }
            RosControlLiveBannerText->SetText(FText::FromString(RosStr));
            RosControlLiveBannerText->SetColorAndOpacity(TargetImporter->bIsPhysicsSimulating ? FLinearColor(0.2f, 1.0f, 0.45f, 1.0f) : FLinearColor(1.0f, 0.85f, 0.2f, 1.0f));
        }
        else
        {
            FString PhysNotice = TargetImporter->bIsPhysicsSimulating ? TEXT("⚡ Fizik Aktif") : TEXT("⚠️ Fizik Kapalı");
            RosControlLiveBannerText->SetText(FText::FromString(FString::Printf(
                TEXT("📡 CANLI ROS 2: Bekleniyor (Pi 5 -> Port 7400: /cmd_vel) │ %s\n  • Pi 5 / ROS 2'den Twist gönderildiğinde motorlar ve telemetri anlık akacaktır."), *PhysNotice)));
            RosControlLiveBannerText->SetColorAndOpacity(FLinearColor(0.65f, 0.8f, 0.95f, 1.0f));
        }
    }

    // Model Button Text Update
    if (ModelButtonText.IsValid())
    {
        FString ShortName = TargetImporter->ActiveModelFileName.Contains(TEXT("test1")) ? TEXT(" 📦 MODEL: test1.fbx ") : TEXT(" 📦 MODEL: test.fbx ");
        ModelButtonText->SetText(FText::FromString(ShortName));
    }

    // 1.5) Update Top-Center Aero Telemetry Table (Always Visible)
    if (TargetImporter)
    {
        const FPiSimFlightTelemetry& Tel = TargetImporter->FlightTelemetry;

        auto UpdateCells = [](FAeroHudRowCells& Cells, const FVector& F, const FVector& M)
        {
            if (Cells.Fx.IsValid()) Cells.Fx->SetText(FText::FromString(FString::Printf(TEXT("%+.1f N"), F.X)));
            if (Cells.Fy.IsValid()) Cells.Fy->SetText(FText::FromString(FString::Printf(TEXT("%+.1f N"), F.Y)));
            if (Cells.Fz.IsValid()) Cells.Fz->SetText(FText::FromString(FString::Printf(TEXT("%+.1f N"), F.Z)));
            if (Cells.Mx.IsValid()) Cells.Mx->SetText(FText::FromString(FString::Printf(TEXT("%+.2f N·m"), M.X)));
            if (Cells.My.IsValid()) Cells.My->SetText(FText::FromString(FString::Printf(TEXT("%+.2f N·m"), M.Y)));
            if (Cells.Mz.IsValid()) Cells.Mz->SetText(FText::FromString(FString::Printf(TEXT("%+.2f N·m"), M.Z)));
        };

        UpdateCells(AeroHudLiftCells, Tel.LiftForceBodyN, Tel.LiftMomentBodyNm);
        UpdateCells(AeroHudDragCells, Tel.DragForceBodyN, Tel.DragMomentBodyNm);
        UpdateCells(AeroHudThrustCells, Tel.ThrustForceBodyN, Tel.ThrustMomentBodyNm);
        UpdateCells(AeroHudGravCells, Tel.GravityForceBodyN, FVector::ZeroVector);
        UpdateCells(AeroHudTotalCells, Tel.NetForceBodyN, Tel.NetMomentBodyNm);

        if (AeroHudFooterText.IsValid())
        {
            FString FootStr = FString::Printf(
                TEXT("Hız: %.1f km/h   │   AoA: %+.1f°   │   L/D: %.1f   │   CoG: %+.1f cm [O/P]   │   Lift Çarpanı: %.2fx [L/K]"),
                Tel.AirspeedKmh, Tel.AlphaDeg, Tel.LiftDragRatio, Tel.CoGForwardCm, Tel.LiftScale);
            AeroHudFooterText->SetText(FText::FromString(FootStr));
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

        // Populate editable text boxes whenever the selected bone changes
        if (TargetImporter->SelectedBoneIndex == 0)
        {
            if (AeroWingAreaInput.IsValid()) AeroWingAreaInput->SetText(FText::FromString(FString::Printf(TEXT("%.3f"), TargetImporter->AeroConfig.WingArea)));
            if (AeroWingspanInput.IsValid()) AeroWingspanInput->SetText(FText::FromString(FString::Printf(TEXT("%.2f"), TargetImporter->AeroConfig.Wingspan)));
            if (AeroCL0Input.IsValid()) AeroCL0Input->SetText(FText::FromString(FString::Printf(TEXT("%.2f"), TargetImporter->AeroConfig.CL0)));
            if (AeroCLAlphaInput.IsValid()) AeroCLAlphaInput->SetText(FText::FromString(FString::Printf(TEXT("%.2f"), TargetImporter->AeroConfig.CLAlpha)));
            if (AeroCD0Input.IsValid()) AeroCD0Input->SetText(FText::FromString(FString::Printf(TEXT("%.3f"), TargetImporter->AeroConfig.CD0)));
            if (AeroStallAngleInput.IsValid()) AeroStallAngleInput->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), TargetImporter->AeroConfig.StallAngleDeg)));
            if (AeroElevonEffectInput.IsValid()) AeroElevonEffectInput->SetText(FText::FromString(FString::Printf(TEXT("%.2f"), TargetImporter->AeroConfig.ElevonEffectiveness)));
            if (AeroCoLForwardInput.IsValid()) AeroCoLForwardInput->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), TargetImporter->AeroConfig.CoLForwardCm)));
            if (AeroCoGForwardInput.IsValid()) AeroCoGForwardInput->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), TargetImporter->AeroConfig.CoGForwardCm)));
            if (AeroInertiaTensorScaleInput.IsValid()) AeroInertiaTensorScaleInput->SetText(FText::FromString(FString::Printf(TEXT("%.2f"), TargetImporter->AeroConfig.InertiaTensorScale)));
            if (ChassisMassInput.IsValid())
                ChassisMassInput->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), TargetImporter->ChassisMassKg)));
        }
        else if (TargetImporter->SelectedBoneIndex > 0 && TargetImporter->ConfiguredMotors.IsValidIndex(TargetImporter->SelectedBoneIndex))
        {
            const FPiSimMotorItem& MotorSel = TargetImporter->ConfiguredMotors[TargetImporter->SelectedBoneIndex];
            if (MotorMaxRpmInput.IsValid()) MotorMaxRpmInput->SetText(FText::FromString(FString::Printf(TEXT("%.0f"), MotorSel.MaxVelocityRPM)));
            if (MotorMaxTorqueInput.IsValid()) MotorMaxTorqueInput->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), MotorSel.MaxTorqueNm)));
            if (MotorMinLimitInput.IsValid()) MotorMinLimitInput->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), MotorSel.MinLimitDeg)));
            if (MotorMaxLimitInput.IsValid()) MotorMaxLimitInput->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), MotorSel.MaxLimitDeg)));
        }

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

            FString BtnLabel = FString::Printf(TEXT("%s  [%d] %s"), *RoleIcon, i, *Motor.BoneName);

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
                if (ChassisBox.IsValid()) ChassisBox->SetVisibility(EVisibility::Visible);
                if (ExistingMotorBox.IsValid()) ExistingMotorBox->SetVisibility(EVisibility::Collapsed);
                if (AssignMotorBox.IsValid()) AssignMotorBox->SetVisibility(EVisibility::Collapsed);
                if (MotorTestSlider.IsValid()) MotorTestSlider->SetVisibility(EVisibility::Collapsed);
                if (MotorTestSliderValueText.IsValid()) MotorTestSliderValueText->SetVisibility(EVisibility::Collapsed);

                if (SelectedBoneRoleBadgeText.IsValid())
                    SelectedBoneRoleBadgeText->SetText(FText::FromString(TEXT("🛡️ Durum: Ana Kök Gövde (Şasi & Aerodinamik)")));

                if (MotorDetailsText.IsValid())
                {
                    float ChassisM = TargetImporter->VisualSections.Num() > 0 ? TargetImporter->VisualSections[0].MassKg : 30.0f;
                    FString ChassisInfo = FString::Printf(
                        TEXT("  • Parça Türü   : Robotun Ana Şasisi (Kök Gövde)\n"
                             "  • Toplam Parça : %d Adet Alt Kemik / Mesh\n"
                             "  • Kütle (Fizik): %.1f kg (Zeminle Çarpışır)\n"
                             "  • Kemik Boyu   : %.1f cm (İleri Boyut)\n"
                             "  • Aerodinamik  : Canlı İnce Kanat + Girdap Modeli"),
                        TargetImporter->ConfiguredMotors.Num(),
                        ChassisM,
                        TargetImporter->AeroConfig.ChassisBoneLengthCm
                    );
                    MotorDetailsText->SetText(FText::FromString(ChassisInfo));
                }

                if (FlightTelemetryLiveText.IsValid())
                {
                    float SpeedKmh = TargetImporter->AeroConfig.CurrentAirspeedKmh;
                    float SpeedMs = SpeedKmh / 3.6f;
                    float DynQ = 0.5f * TargetImporter->AeroConfig.AirDensity * SpeedMs * SpeedMs;
                    float LiftN = TargetImporter->AeroConfig.CurrentLiftNewtons;
                    float DragN = TargetImporter->AeroConfig.CurrentDragNewtons;
                    float AlphaDeg = TargetImporter->AeroConfig.CurrentAlphaDeg;

                    FString FltInfo = FString::Printf(
                        TEXT("  • Hava Hızı (Airspeed) : %5.1f km/h  (%.1f m/s)\n"
                             "  • Dinamik Basınç (q)   : %5.1f Pa\n"
                             "  • Hücum Açısı (AoA)    : %+5.1f°\n"
                             "  • Taşıma Kuvveti (Lift): %6.1f N\n"
                             "  • Sürükleme (Drag)     : %6.1f N\n"
                             "  • Kuvvet Ölçeği        : %.1fx\n"
                             "  • 3D Vektör Gizmosu    : %s"),
                        SpeedKmh, SpeedMs,
                        DynQ,
                        AlphaDeg,
                        LiftN,
                        DragN,
                        TargetImporter->AeroConfig.AeroForceScale,
                        TargetImporter->AeroConfig.bShowAeroGizmos ? TEXT("🟢 AÇIK") : TEXT("⚪ KAPALI")
                    );
                    FlightTelemetryLiveText->SetText(FText::FromString(FltInfo));
                }
            }
            else if (SelMotor.Role != EPiSimMotorRole::None)
            {
                if (ChassisBox.IsValid()) ChassisBox->SetVisibility(EVisibility::Collapsed);
                if (ExistingMotorBox.IsValid()) ExistingMotorBox->SetVisibility(EVisibility::Visible);
                if (AssignMotorBox.IsValid()) AssignMotorBox->SetVisibility(EVisibility::Collapsed);
                if (MotorTestSlider.IsValid()) MotorTestSlider->SetVisibility(EVisibility::Visible);
                if (MotorTestSliderValueText.IsValid()) MotorTestSliderValueText->SetVisibility(EVisibility::Visible);

                // Thruster kontrol panelini yalnızca Thruster rolü seçiliyken göster
                bool bIsThrusterRole = (SelMotor.Role == EPiSimMotorRole::Thruster);
                if (ThrusterControlsBox.IsValid())
                    ThrusterControlsBox->SetVisibility(bIsThrusterRole ? EVisibility::Visible : EVisibility::Collapsed);
                if (ReverseThrustButtonText.IsValid() && bIsThrusterRole)
                {
                    ReverseThrustButtonText->SetText(FText::FromString(
                        SelMotor.bReverseThrust
                            ? TEXT("⬅️ İTKİ YÖNÜ: TERS / PUSHER (Tıkla → Normal Yap)")
                            : TEXT("➡️ İTKİ YÖNÜ: NORMAL / TRACTOR (Tıkla → Ters Çevir)")
                    ));
                }

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
                    FString Details;
                    if (TargetImporter->bIsAdvancedMode)
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

                    // Thruster ise itki yönü bilgisini ekle
                    if (SelMotor.Role == EPiSimMotorRole::Thruster)
                    {
                        Details += SelMotor.bReverseThrust
                            ? TEXT("\n  • İtki Yönü    : ⬅️ TERS (Pusher / Reverse)")
                            : TEXT("\n  • İtki Yönü    : ➡️ NORMAL (Tractor / Forward)");
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

                if (MotorTestSlider.IsValid() && TargetImporter->bIsPiConnected && TargetImporter->TotalPacketsReceived > 0)
                {
                    MotorTestSlider->SetValue(SelMotor.CurrentTestValue);
                }
            }
            else
            {
                if (ChassisBox.IsValid()) ChassisBox->SetVisibility(EVisibility::Collapsed);
                if (ExistingMotorBox.IsValid()) ExistingMotorBox->SetVisibility(EVisibility::Collapsed);
                if (AssignMotorBox.IsValid()) AssignMotorBox->SetVisibility(EVisibility::Visible);

                if (SelectedBoneRoleBadgeText.IsValid())
                    SelectedBoneRoleBadgeText->SetText(FText::FromString(TEXT("📦 Durum: Pasif Gövde (Motor Yok)")));
            }
        }
        else
        {
            BoneInspectorBorder->SetVisibility(EVisibility::Collapsed);
            if (ChassisBox.IsValid()) ChassisBox->SetVisibility(EVisibility::Collapsed);
        }
    }

    // 5) TAB 2: TELEMETRY & CONNECTION TEXTS
    if (ConnectionStagesText.IsValid())
    {
        FString StagesStr;
        if (Autopilot && Autopilot->ControlMode != EPiSimControlMode::DirectROS2)
        {
            const FPiSimAutopilotTelemetry& Tel = Autopilot->Telemetry;
            FString BackendStr = TEXT("Bilinmiyor");
            FString EndpointStr = TEXT("-");
            switch (Autopilot->ControlMode)
            {
            case EPiSimControlMode::ArduPilotSITL:
                BackendStr = TEXT("ArduPilot SITL");
                EndpointStr = FString::Printf(TEXT("UDP %d"), Autopilot->ArduPilotListenPort);
                break;
            case EPiSimControlMode::PX4SITL:
                BackendStr = TEXT("PX4 SITL");
                EndpointStr = FString::Printf(TEXT("TCP %d / UDP %d"), Autopilot->Px4SitlTcpPort, Autopilot->Px4SitlUdpPort);
                break;
            case EPiSimControlMode::PX4HITL:
                BackendStr = TEXT("PX4 HITL");
                EndpointStr = FString::Printf(TEXT("%s @ %d"), *Autopilot->SerialPortName, Autopilot->SerialBaudRate);
                break;
            default:
                break;
            }

            StagesStr = FString::Printf(
                TEXT("  • Seçili Backend   : %s\n"
                     "  • Uç Nokta         : %s\n"
                     "  • Bağlantı Durumu  : %s\n"
                     "  • Sensör Yayını    : %5.1f Hz\n"
                     "  • Aktüatör Geri D. : %5.1f Hz"),
                *BackendStr,
                *EndpointStr,
                *Tel.StatusLine,
                Tel.SensorTxHz,
                Tel.ActuatorRxHz);
        }
        else
        {
            FString SocketStatus = TargetImporter->bIsSocketBound ? TEXT("🟢 AÇIK (Dinliyor)") : TEXT("🔴 KAPALI / HATA");
            FString Pi5Status = TargetImporter->bIsPiConnected ?
                FString::Printf(TEXT("🟢 BAĞLANDI (IP: %s)"), *TargetImporter->ConnectedPiIP) :
                (TargetImporter->ConnectionStage == 5 ? TEXT("🔴 KOPTU (Zaman Aşımı)") : TEXT("🟡 BEKLENİYOR..."));

            StagesStr = FString::Printf(
                TEXT("  • Aşama 1: UE5 Dinleme Soketi : 0.0.0.0:7400 [%s]\n"
                     "  • Aşama 2: Telemetri Hedefleri: %s:7401 [🟢 HAZIR]\n"
                     "  • Aşama 3: Pi 5 Handshake     : %s\n"
                     "  • Aşama 4: Canlı Veri Akışı   : %5.1f Hz  (Toplam: %d Paket)"),
                *SocketStatus, *TargetImporter->BridgeTargetIP, *Pi5Status, TargetImporter->RxPacketRateHz, TargetImporter->TotalPacketsReceived
            );
        }
        ConnectionStagesText->SetText(FText::FromString(StagesStr));
    }

    if (IncomingDataText.IsValid())
    {
        FString InDataStr = TEXT("");
        if (Autopilot && Autopilot->ControlMode != EPiSimControlMode::DirectROS2)
        {
            const FPiSimAutopilotTelemetry& Tel = Autopilot->Telemetry;
            FString PwmLine = TEXT("  • PWM Çıkışları    : veri yok");
            if (Tel.MotorPwmUs.Num() > 0)
            {
                FString PwmJoined;
                const int32 ShowCount = FMath::Min(4, Tel.MotorPwmUs.Num());
                for (int32 i = 0; i < ShowCount; ++i)
                {
                    if (!PwmJoined.IsEmpty())
                    {
                        PwmJoined += TEXT(" | ");
                    }
                    PwmJoined += FString::Printf(TEXT("M%d=%dµs"), i + 1, Tel.MotorPwmUs[i]);
                }
                PwmLine = FString::Printf(TEXT("  • PWM Çıkışları    : %s"), *PwmJoined);
            }

            InDataStr = FString::Printf(
                TEXT("  • Roll / Pitch     : %+.2f / %+.2f\n"
                     "  • Yaw / Throttle   : %+.2f / %+.2f\n"
                     "  • Armed            : %s\n"
                     "%s"),
                Tel.RollCmd,
                Tel.PitchCmd,
                Tel.YawCmd,
                Tel.ThrottleCmd,
                Tel.bArmed ? TEXT("EVET") : TEXT("HAYIR"),
                *PwmLine);
        }
        else if (TargetImporter->VehicleDomain == EVehicleDomain::Air)
        {
            float ThrottlePct = FMath::Clamp(TargetImporter->TargetLinearX * 100.0f, 0.0f, 100.0f);
            float PitchDeg = FMath::Clamp(TargetImporter->TargetAngularZ * 10.0f, -20.0f, 20.0f);
            float RollDeg = FMath::Clamp(TargetImporter->LastRxAngularVel.X * 10.0f, -20.0f, 20.0f);

            InDataStr = FString::Printf(
                TEXT("  • ROS Topic       : /cmd_vel (Hava Aracı Uçuş Kontrolü)\n"
                     "  • İleri Gaz (Thr) : %%%.1f (Linear.X: %+.2f)\n"
                     "  • Yunuslama(Pitch): %+.1f° (Angular.Y: %+.2f rad/s)\n"
                     "  • Yatış (Roll)    : %+.1f° (Angular.X: %+.2f rad/s)\n"
                     "  • Sapma (Yaw)     : %+.1f° (Angular.Z: %+.2f rad/s)\n"
                     "  • Elevon Çıkışı   : Sol=%+.1f°, Sağ=%+.1f°"),
                ThrottlePct, TargetImporter->TargetLinearX,
                PitchDeg, TargetImporter->LastRxAngularVel.Y,
                RollDeg, TargetImporter->LastRxAngularVel.X,
                TargetImporter->TargetAngularZ * 10.0f, TargetImporter->TargetAngularZ,
                PitchDeg + RollDeg, PitchDeg - RollDeg
            );
        }
        else
        {
            InDataStr = FString::Printf(
                TEXT("  • ROS Topic        : /cmd_vel (geometry_msgs/Twist)\n"
                     "  • Doğrusal Hız (X) : %+.2f m/s (İleri/Geri)\n"
                     "  • Açısal Hız (Yaw) : %+.2f rad/s (Dönüş)\n"
                     "  • Sol / Sağ RPM    : %+6.1f / %+6.1f RPM\n"
                     "  • Manuel Ofset     : %+.1f RPM (G / F Tuşları)"),
                TargetImporter->TargetLinearX, TargetImporter->TargetAngularZ,
                TargetImporter->LeftWheelsRpm, TargetImporter->RightWheelsRpm, TargetImporter->AppliedWheelRpm
            );
        }
        IncomingDataText->SetText(FText::FromString(InDataStr));
    }

    if (ConnectionDebugLogText.IsValid())
    {
        if (Autopilot && Autopilot->ControlMode != EPiSimControlMode::DirectROS2)
        {
            FString DebugText = FString::Printf(TEXT("  • Backend Durumu   : %s"), *Autopilot->Telemetry.StatusLine);
            if (TargetImporter->ConnectionDebugLogs.Num() > 0)
            {
                DebugText += TEXT("\n");
                DebugText += FString::Join(TargetImporter->ConnectionDebugLogs, TEXT("\n"));
            }
            ConnectionDebugLogText->SetText(FText::FromString(DebugText));
        }
        else if (TargetImporter->ConnectionDebugLogs.Num() > 0)
        {
            ConnectionDebugLogText->SetText(FText::FromString(FString::Join(TargetImporter->ConnectionDebugLogs, TEXT("\n"))));
        }
        else
        {
            ConnectionDebugLogText->SetText(FText::FromString(TEXT("  Henüz bir bağlantı olayı kaydedilmedi.")));
        }
    }

    if (OutgoingTelemetryText.IsValid())
    {
        FString OutStr;
        if (Autopilot && Autopilot->ControlMode != EPiSimControlMode::DirectROS2 && TargetImporter->VirtualSensors)
        {
            const FPiSimImuSensorData& Imu = TargetImporter->VirtualSensors->ImuData;
            const FPiSimGpsSensorData& Gps = TargetImporter->VirtualSensors->GpsData;
            const FPiSimBaroSensorData& Baro = TargetImporter->VirtualSensors->BaroData;
            OutStr = FString::Printf(
                TEXT("  • Sanal IMU        : A=(%+5.2f, %+5.2f, %+5.2f) m/s²\n"
                     "  • Sanal Gyro       : G=(%+5.2f, %+5.2f, %+5.2f) rad/s\n"
                     "  • GPS              : %.6f, %.6f | %.1f m\n"
                     "  • Baro / Pitot     : %.1f hPa | %.2f hPa\n"
                     "  • UE5 -> Pi 5 TX   : %5.1f Hz (%s:7401)"),
                Imu.AccelNED.X, Imu.AccelNED.Y, Imu.AccelNED.Z,
                Imu.GyroNED.X, Imu.GyroNED.Y, Imu.GyroNED.Z,
                Gps.LatitudeDeg, Gps.LongitudeDeg, Gps.AltitudeAMSL,
                Baro.AbsPressureHPa, Baro.DiffPressureHPa,
                TargetImporter->TxPacketRateHz, *TargetImporter->BridgeTargetIP);
        }
        else
        {
            OutStr = FString::Printf(
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
        }
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

    if (ModularComponentsDebugText.IsValid())
    {
        FString CompStr = TEXT("");

        // 1) Araç Sınıfı (Domain)
        FString DomainStr = TEXT("Bilinmeyen");
        switch (TargetImporter->VehicleDomain)
        {
            case EVehicleDomain::Air: DomainStr = TEXT("✈️ HAVA ARACI (Airframe / Kanat & İtki)"); break;
            case EVehicleDomain::Land: DomainStr = TEXT("🚗 KARA ARACI (Chassis / Tekerlekli)"); break;
            case EVehicleDomain::Sea: DomainStr = TEXT("⚓ DENİZ ARACI (Hull / Tekne & USV)"); break;
            default: DomainStr = TEXT("⚙️ GENEL MODEL"); break;
        }
        CompStr += FString::Printf(TEXT("  • Araç Sınıfı    : %s\n"), *DomainStr);

        // 2) Aktüatörler
        CompStr += FString::Printf(TEXT("  • Aktüatör Sayısı: %d Adet\n"), TargetImporter->AttachedActuators.Num());
        if (TargetImporter->AttachedActuators.Num() == 0)
        {
            CompStr += TEXT("    (Henüz bağlı aktüatör yok)\n");
        }
        else
        {
            for (int32 a = 0; a < TargetImporter->AttachedActuators.Num(); ++a)
            {
                UPiSimActuatorComponent* Act = TargetImporter->AttachedActuators[a];
                if (!Act) continue;

                FString TypeStr = TEXT("DC");
                switch (Act->MotorType)
                {
                    case EPiSimMotorType::BLDC_ESC: TypeStr = TEXT("BLDC [ESC]"); break;
                    case EPiSimMotorType::Servo_Position: TypeStr = TEXT("Servo [Açı]"); break;
                    case EPiSimMotorType::Stepper: TypeStr = TEXT("Step"); break;
                    default: TypeStr = TEXT("DC"); break;
                }

                if (Act->Role == EPiSimMotorRole::Thruster)
                {
                    CompStr += FString::Printf(TEXT("    [%d] %s (%s): %%%.0f Gaz | %.1f N İtki (%s)\n"),
                        a, *Act->ActuatorName, *TypeStr, Act->TargetNormalizedValue * 100.0f, Act->CurrentOutputForceOrTorque, *Act->Ros2Topic);
                }
                else if (Act->Role == EPiSimMotorRole::ServoJoint)
                {
                    CompStr += FString::Printf(TEXT("    [%d] %s (%s): %+.1f° Açı (Hedef: %+.1f°) (%s)\n"),
                        a, *Act->ActuatorName, *TypeStr, Act->CurrentAngleDeg, Act->TargetAngleDeg, *Act->Ros2Topic);
                }
                else
                {
                    CompStr += FString::Printf(TEXT("    [%d] %s (%s): %.0f RPM | %.1f Nm\n"),
                        a, *Act->ActuatorName, *TypeStr, Act->TargetRPM, Act->CurrentOutputForceOrTorque);
                }
            }
        }

        // 3) Kanat / Kaldırma Kuvveti Gövdeleri
        CompStr += FString::Printf(TEXT("  • Kuvvet Gövdesi : %d Kanat\n"), TargetImporter->AttachedWingBodies.Num());
        if (TargetImporter->AttachedWingBodies.Num() == 0)
        {
            CompStr += TEXT("    (Kanat gövdesi yok)\n");
        }
        else
        {
            for (int32 w = 0; w < TargetImporter->AttachedWingBodies.Num(); ++w)
            {
                UPiSimAeroWingComponent* Wing = TargetImporter->AttachedWingBodies[w];
                if (!Wing) continue;

                CompStr += FString::Printf(TEXT("    [%d] %s: V=%.1f km/h | AoA=%+.1f° | Elevon=%+.1f° | Lift=%+.1f N | Drag=%.1f N\n"),
                    w, *Wing->WingName, Wing->AirspeedKmh, Wing->AngleOfAttackDeg, Wing->ControlSurfaceDeflectionDeg, Wing->CurrentLiftN, Wing->CurrentDragN);
            }
        }

        ModularComponentsDebugText->SetText(FText::FromString(CompStr));
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

FReply UPiSimModelImporterWidget::OnMultiplyScale01Clicked()
{
    if (TargetImporter) TargetImporter->MultiplyScale_0_1X();
    return FReply::Handled();
}

FReply UPiSimModelImporterWidget::OnMultiplyScale10Clicked()
{
    if (TargetImporter) TargetImporter->MultiplyScale_10_0X();
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

FReply UPiSimModelImporterWidget::OnToggleModelClicked()
{
    if (TargetImporter) TargetImporter->ToggleModelFile();
    return FReply::Handled();
}

void UPiSimModelImporterWidget::OnParamTextCommitted(const FText& NewText, ETextCommit::Type CommitType, EPiSimParamId ParamId)
{
    if (!TargetImporter) return;

    float Val = FCString::Atof(*NewText.ToString());

    switch (ParamId)
    {
        case EPiSimParamId::AeroWingArea:
            TargetImporter->AeroConfig.WingArea = FMath::Max(0.01f, Val);
            if (TargetImporter->VisualSections.Num() > 0) TargetImporter->VisualSections[0].WingArea = TargetImporter->AeroConfig.WingArea;
            break;
        case EPiSimParamId::AeroWingspan:
            TargetImporter->AeroConfig.Wingspan = FMath::Max(0.1f, Val);
            if (TargetImporter->VisualSections.Num() > 0) TargetImporter->VisualSections[0].Wingspan = TargetImporter->AeroConfig.Wingspan;
            break;
        case EPiSimParamId::AeroCL0:
            TargetImporter->AeroConfig.CL0 = Val;
            if (TargetImporter->VisualSections.Num() > 0) TargetImporter->VisualSections[0].CL0 = Val;
            break;
        case EPiSimParamId::AeroCLAlpha:
            TargetImporter->AeroConfig.CLAlpha = Val;
            if (TargetImporter->VisualSections.Num() > 0) TargetImporter->VisualSections[0].CLAlpha = Val;
            break;
        case EPiSimParamId::AeroCD0:
            TargetImporter->AeroConfig.CD0 = FMath::Max(0.001f, Val);
            if (TargetImporter->VisualSections.Num() > 0) TargetImporter->VisualSections[0].CD0 = TargetImporter->AeroConfig.CD0;
            break;
        case EPiSimParamId::AeroStallAngle:
            TargetImporter->AeroConfig.StallAngleDeg = FMath::Clamp(Val, 5.0f, 45.0f);
            if (TargetImporter->VisualSections.Num() > 0) TargetImporter->VisualSections[0].StallAngleDeg = TargetImporter->AeroConfig.StallAngleDeg;
            break;
        case EPiSimParamId::AeroElevonEffect:
            TargetImporter->AeroConfig.ElevonEffectiveness = FMath::Max(0.0f, Val);
            if (TargetImporter->VisualSections.Num() > 0) TargetImporter->VisualSections[0].ElevonEffectiveness = TargetImporter->AeroConfig.ElevonEffectiveness;
            break;
        case EPiSimParamId::AeroCoLForward:
            TargetImporter->AeroConfig.CoLForwardCm = Val;
            if (TargetImporter->VisualSections.Num() > 0) TargetImporter->VisualSections[0].CoLForwardCm = Val;
            break;
        case EPiSimParamId::AeroCoGForward:
            TargetImporter->AeroConfig.CoGForwardCm = Val;
            if (TargetImporter->VisualSections.Num() > 0) TargetImporter->VisualSections[0].CoGForwardCm = Val;
            TargetImporter->ApplyCenterOfMass();
            break;
        case EPiSimParamId::AeroInertiaTensorScale:
            TargetImporter->AeroConfig.InertiaTensorScale = FMath::Clamp(Val, 0.1f, 100.0f);
            if (TargetImporter->VisualMeshComponents.IsValidIndex(0) && TargetImporter->VisualMeshComponents[0])
            {
                FBodyInstance* BI = TargetImporter->VisualMeshComponents[0]->GetBodyInstance();
                if (BI)
                {
                    BI->InertiaTensorScale = FVector(TargetImporter->AeroConfig.InertiaTensorScale);
                    BI->UpdateMassProperties();
                }
            }
            break;
        case EPiSimParamId::ChassisMass:
            TargetImporter->ChassisMassKg = FMath::Max(0.1f, Val);
            if (TargetImporter->VisualSections.Num() > 0)
            {
                TargetImporter->VisualSections[0].MassKg = TargetImporter->ChassisMassKg;
            }
            if (TargetImporter->VisualMeshComponents.IsValidIndex(0) && TargetImporter->VisualMeshComponents[0])
            {
                TargetImporter->VisualMeshComponents[0]->SetMassOverrideInKg(NAME_None, TargetImporter->ChassisMassKg, true);
            }
            break;
        case EPiSimParamId::MotorMaxRpm:
            if (TargetImporter->ConfiguredMotors.IsValidIndex(TargetImporter->SelectedBoneIndex))
            {
                TargetImporter->ConfiguredMotors[TargetImporter->SelectedBoneIndex].MaxVelocityRPM = FMath::Max(0.0f, Val);
            }
            break;
        case EPiSimParamId::MotorMaxTorque:
            if (TargetImporter->ConfiguredMotors.IsValidIndex(TargetImporter->SelectedBoneIndex))
            {
                TargetImporter->ConfiguredMotors[TargetImporter->SelectedBoneIndex].MaxTorqueNm = FMath::Max(0.0f, Val);
            }
            break;
        case EPiSimParamId::MotorMinLimit:
            if (TargetImporter->ConfiguredMotors.IsValidIndex(TargetImporter->SelectedBoneIndex))
            {
                TargetImporter->ConfiguredMotors[TargetImporter->SelectedBoneIndex].MinLimitDeg = Val;
            }
            break;
        case EPiSimParamId::MotorMaxLimit:
            if (TargetImporter->ConfiguredMotors.IsValidIndex(TargetImporter->SelectedBoneIndex))
            {
                TargetImporter->ConfiguredMotors[TargetImporter->SelectedBoneIndex].MaxLimitDeg = Val;
            }
            break;
    }
}

FReply UPiSimModelImporterWidget::OnToggleAeroGizmosClicked()
{
    if (TargetImporter)
    {
        TargetImporter->ToggleAeroGizmos();
        if (AeroGizmoToggleText.IsValid())
        {
            AeroGizmoToggleText->SetText(FText::FromString(
                TargetImporter->AeroConfig.bShowAeroGizmos ? TEXT("📐 3D VEKTÖR GİZMO: AÇIK") : TEXT("📐 3D VEKTÖR GİZMO: KAPALI")));
        }
    }
    return FReply::Handled();
}

FReply UPiSimModelImporterWidget::OnSetCoGToBoneEndClicked()
{
    if (TargetImporter)
    {
        TargetImporter->SetCoGToBoneEnd();
        if (AeroCoGForwardInput.IsValid())
        {
            AeroCoGForwardInput->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), TargetImporter->AeroConfig.CoGForwardCm)));
        }
    }
    return FReply::Handled();
}

FReply UPiSimModelImporterWidget::OnSetCoGToCenterOfMassClicked()
{
    if (TargetImporter)
    {
        TargetImporter->SetCoGToCenterOfMass();
        if (AeroCoGForwardInput.IsValid())
        {
            AeroCoGForwardInput->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), TargetImporter->AeroConfig.CoGForwardCm)));
        }
    }
    return FReply::Handled();
}



FReply UPiSimModelImporterWidget::OnToggleReverseThrustClicked()
{
    if (!TargetImporter) return FReply::Handled();

    int32 SelIdx = TargetImporter->SelectedBoneIndex;
    if (!TargetImporter->ConfiguredMotors.IsValidIndex(SelIdx)) return FReply::Handled();

    FPiSimMotorItem& Motor = TargetImporter->ConfiguredMotors[SelIdx];
    if (Motor.Role != EPiSimMotorRole::Thruster) return FReply::Handled();

    Motor.bReverseThrust = !Motor.bReverseThrust;

    UE_LOG(LogTemp, Warning, TEXT("Thruster '%s' itki yonu: %s"),
        *Motor.BoneName,
        Motor.bReverseThrust ? TEXT("TERS (Pusher)") : TEXT("NORMAL (Tractor)"));

    return FReply::Handled();
}

FReply UPiSimModelImporterWidget::OnCycleControlModeClicked()
{
    if (TargetImporter && TargetImporter->AutopilotManager)
    {
        uint8 Next = ((uint8)TargetImporter->AutopilotManager->ControlMode + 1) % 4;
        TargetImporter->AutopilotManager->SetControlMode((EPiSimControlMode)Next);
    }
    return FReply::Handled();
}

FReply UPiSimModelImporterWidget::OnCycleSerialPortClicked()
{
    if (TargetImporter && TargetImporter->AutopilotManager)
    {
        TargetImporter->AutopilotManager->CycleSerialPort();
    }
    return FReply::Handled();
}

FReply UPiSimModelImporterWidget::OnToggleAutopilotLinkClicked()
{
    if (TargetImporter && TargetImporter->AutopilotManager)
    {
        if (TargetImporter->AutopilotManager->ControlMode == EPiSimControlMode::DirectROS2)
        {
            TargetImporter->AddConnectionDebugLog(TEXT("Direct ROS 2"));
        }
        else
        {
            if (TargetImporter->AutopilotManager->Telemetry.bLinkUp)
            {
                TargetImporter->AutopilotManager->Disconnect();
            }
            else
            {
                TargetImporter->AutopilotManager->Connect();
            }
        }
    }
    return FReply::Handled();
}
