// PiSimModelImporterWidget.cpp
// Clean, Dedicated Slate-Powered HUD Widget for PiSimModelImporter.

#include "PiSimModelImporterWidget.h"
#include "PiSimModelImporter.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"

TSharedRef<SWidget> UPiSimModelImporterWidget::RebuildWidget()
{
    if (!TargetImporter && GetWorld())
    {
        AActor* Found = UGameplayStatics::GetActorOfClass(GetWorld(), APiSimModelImporter::StaticClass());
        TargetImporter = Cast<APiSimModelImporter>(Found);
    }

    FSlateFontInfo HeaderTitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 13);
    FSlateFontInfo CardHeaderFont = FCoreStyle::GetDefaultFontStyle("Bold", 10);
    FSlateFontInfo DataFont = FCoreStyle::GetDefaultFontStyle("Regular", 9);
    FSlateFontInfo BadgeFont = FCoreStyle::GetDefaultFontStyle("Bold", 10);
    FSlateFontInfo ButtonFont = FCoreStyle::GetDefaultFontStyle("Bold", 9);

    return SNew(SOverlay)
        // ---------------------------------------------------------------------
        // 1) TOP HEADER BAR & CONNECTION BADGE & QUICK CONTROLS
        // ---------------------------------------------------------------------
        + SOverlay::Slot()
        .HAlign(HAlign_Fill)
        .VAlign(VAlign_Top)
        [
            SNew(SBorder)
            .BorderBackgroundColor(FLinearColor(0.015f, 0.03f, 0.07f, 0.96f))
            .Padding(FMargin(18.0f, 10.0f))
            [
                SNew(SHorizontalBox)

                // Title
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("⚡ PiSim // HARDWARE-IN-THE-LOOP TELEMETRY")))
                    .Font(HeaderTitleFont)
                    .ColorAndOpacity(FLinearColor(0.0f, 0.88f, 1.0f, 1.0f))
                ]

                // Live Dynamic Connection Badge
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(FMargin(18.0f, 0.0f, 0.0f, 0.0f))
                [
                    SAssignNew(ConnectionBadgeBorder, SBorder)
                    .BorderBackgroundColor(FLinearColor(0.25f, 0.18f, 0.02f, 0.95f)) // Amber initial
                    .Padding(FMargin(10.0f, 4.0f))
                    [
                        SAssignNew(ConnectionBadgeText, STextBlock)
                        .Text(FText::FromString(TEXT("🟡 PI 5 BEKLENİYOR (Port 7400)")))
                        .Font(BadgeFont)
                        .ColorAndOpacity(FLinearColor(1.0f, 0.85f, 0.2f, 1.0f))
                    ]
                ]

                + SHorizontalBox::Slot().FillWidth(1.0f) // Spacer

                // Scale 0.1X Button
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(FMargin(3.0f, 0.0f))
                [
                    SNew(SButton)
                    .ButtonColorAndOpacity(FLinearColor(0.08f, 0.16f, 0.28f, 1.0f))
                    .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModelImporterWidget::OnScale01Clicked))
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT(" 0.1X ")))
                        .Font(ButtonFont)
                        .ColorAndOpacity(FLinearColor::White)
                    ]
                ]

                // Scale 1.0X Button
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(FMargin(3.0f, 0.0f))
                [
                    SNew(SButton)
                    .ButtonColorAndOpacity(FLinearColor(0.0f, 0.45f, 0.75f, 1.0f))
                    .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModelImporterWidget::OnScale10Clicked))
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT(" 1.0X (1:1) ")))
                        .Font(ButtonFont)
                        .ColorAndOpacity(FLinearColor::White)
                    ]
                ]

                // Scale 10.0X Button
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(FMargin(3.0f, 0.0f))
                [
                    SNew(SButton)
                    .ButtonColorAndOpacity(FLinearColor(0.08f, 0.16f, 0.28f, 1.0f))
                    .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModelImporterWidget::OnScale100Clicked))
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT(" 10.0X ")))
                        .Font(ButtonFont)
                        .ColorAndOpacity(FLinearColor::White)
                    ]
                ]

                // Reimport Button
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(FMargin(8.0f, 0.0f, 3.0f, 0.0f))
                [
                    SNew(SButton)
                    .ButtonColorAndOpacity(FLinearColor(0.08f, 0.45f, 0.25f, 1.0f))
                    .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModelImporterWidget::OnReimportClicked))
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT(" 🔄 REIMPORT ")))
                        .Font(ButtonFont)
                        .ColorAndOpacity(FLinearColor::White)
                    ]
                ]

                // Toggle Physics Button
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(FMargin(5.0f, 0.0f, 0.0f, 0.0f))
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
        // 2) LEFT TELEMETRY DASHBOARD (TRANSLUCENT GLASSMORPHISM)
        // ---------------------------------------------------------------------
        + SOverlay::Slot()
        .HAlign(HAlign_Left)
        .VAlign(VAlign_Top)
        .Padding(FMargin(18.0f, 68.0f, 0.0f, 0.0f))
        [
            SNew(SBox)
            .WidthOverride(420.0f)
            [
                SNew(SBorder)
                .BorderBackgroundColor(FLinearColor(0.015f, 0.035f, 0.08f, 0.94f))
                .Padding(FMargin(16.0f, 14.0f))
                [
                    SNew(SVerticalBox)

                    // Card 1: Network & Ethernet Link
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 0.0f, 0.0f, 4.0f)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("📡 ETHERNET HABERLEŞME & VERİ AKIŞI")))
                        .Font(CardHeaderFont)
                        .ColorAndOpacity(FLinearColor(0.0f, 0.88f, 1.0f, 1.0f))
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 0.0f, 0.0f, 12.0f)
                    [
                        SAssignNew(TelemetryStatsText, STextBlock)
                        .Text(FText::FromString(TEXT("Ağ istatistikleri bekleniyor...")))
                        .Font(DataFont)
                        .ColorAndOpacity(FLinearColor(0.9f, 0.95f, 1.0f, 1.0f))
                    ]

                    // Separator
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 0.0f, 0.0f, 6.0f)
                    [
                        SNew(SBorder)
                        .BorderBackgroundColor(FLinearColor(0.1f, 0.2f, 0.35f, 0.6f))
                        .Padding(FMargin(0.0f, 0.5f))
                    ]

                    // Card 2: Control Inputs (RX from Pi 5)
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 6.0f, 0.0f, 4.0f)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("🎮 AKTÜATÖR & KONTROL KOMUTLARI (Pi 5 ➔ UE5)")))
                        .Font(CardHeaderFont)
                        .ColorAndOpacity(FLinearColor(0.2f, 1.0f, 0.5f, 1.0f))
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 0.0f, 0.0f, 12.0f)
                    [
                        SAssignNew(ControlInputsText, STextBlock)
                        .Text(FText::FromString(TEXT("Kontrol komutu yok...")))
                        .Font(DataFont)
                        .ColorAndOpacity(FLinearColor(0.9f, 0.95f, 1.0f, 1.0f))
                    ]

                    // Separator
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 0.0f, 0.0f, 6.0f)
                    [
                        SNew(SBorder)
                        .BorderBackgroundColor(FLinearColor(0.1f, 0.2f, 0.35f, 0.6f))
                        .Padding(FMargin(0.0f, 0.5f))
                    ]

                    // Card 3: Kinematics & IMU (TX to Pi 5)
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 6.0f, 0.0f, 4.0f)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("🏎️ ROBOT KİNEMATİK & IMU ÇIKTILARI (UE5 ➔ Pi 5)")))
                        .Font(CardHeaderFont)
                        .ColorAndOpacity(FLinearColor(1.0f, 0.65f, 0.1f, 1.0f))
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 0.0f, 0.0f, 12.0f)
                    [
                        SAssignNew(KinematicsText, STextBlock)
                        .Text(FText::FromString(TEXT("Kinematik verisi hesaplanıyor...")))
                        .Font(DataFont)
                        .ColorAndOpacity(FLinearColor(0.9f, 0.95f, 1.0f, 1.0f))
                    ]

                    // Separator
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 0.0f, 0.0f, 6.0f)
                    [
                        SNew(SBorder)
                        .BorderBackgroundColor(FLinearColor(0.1f, 0.2f, 0.35f, 0.6f))
                        .Padding(FMargin(0.0f, 0.5f))
                    ]

                    // Card 4: Model & Collision Information
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 6.0f, 0.0f, 4.0f)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("📦 CAD GEOMETRİ & CHAOS FİZİK ZIRHI")))
                        .Font(CardHeaderFont)
                        .ColorAndOpacity(FLinearColor(0.7f, 0.85f, 1.0f, 0.9f))
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SAssignNew(StatusTextBlock, STextBlock)
                        .Text(FText::FromString(TEXT("Yükleniyor...")))
                        .Font(DataFont)
                        .ColorAndOpacity(FLinearColor(0.75f, 0.85f, 0.95f, 1.0f))
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

    // 1) Update Connection Badge & Colors
    if (ConnectionBadgeText.IsValid() && ConnectionBadgeBorder.IsValid())
    {
        if (TargetImporter->bIsPiConnected)
        {
            ConnectionBadgeBorder->SetBorderBackgroundColor(FLinearColor(0.02f, 0.30f, 0.12f, 0.95f));
            ConnectionBadgeText->SetText(FText::FromString(FString::Printf(TEXT("🟢 BAĞLI: %s"), *TargetImporter->ConnectedPiIP)));
            ConnectionBadgeText->SetColorAndOpacity(FLinearColor(0.2f, 1.0f, 0.4f, 1.0f));
        }
        else
        {
            ConnectionBadgeBorder->SetBorderBackgroundColor(FLinearColor(0.28f, 0.18f, 0.02f, 0.95f));
            ConnectionBadgeText->SetText(FText::FromString(TEXT("🟡 PI 5 BEKLENİYOR (Port 7400)")));
            ConnectionBadgeText->SetColorAndOpacity(FLinearColor(1.0f, 0.85f, 0.2f, 1.0f));
        }
    }

    // 2) Update Network Stats Text
    if (TelemetryStatsText.IsValid())
    {
        FString NetStr = FString::Printf(
            TEXT("  • Pi 5 IP Adresi   : %s\n"
                 "  • Gelen Veri Hızı  : %5.1f Hz  (Toplam: %d Paket)\n"
                 "  • Giden Telemetri  : %5.1f Hz  (Toplam: %d Paket)\n"
                 "  • Soket Portları   : Dinleme: 7400 | Telemetri: 7401"),
            *TargetImporter->ConnectedPiIP,
            TargetImporter->RxPacketRateHz,
            TargetImporter->TotalPacketsReceived,
            TargetImporter->TxPacketRateHz,
            TargetImporter->TotalPacketsSent
        );
        TelemetryStatsText->SetText(FText::FromString(NetStr));
    }

    // 3) Update Control Inputs Text
    if (ControlInputsText.IsValid())
    {
        FString CtrlStr = FString::Printf(
            TEXT("  • Hedef Hız (m/s)  : %+.2f m/s\n"
                 "  • Hedef Açısal Yaw : %+.2f rad/s\n"
                 "  • Sol Tekerlekler  : %+6.1f RPM\n"
                 "  • Sağ Tekerlekler  : %+6.1f RPM\n"
                 "  • Manuel Ofset     : %+.1f RPM (G / F Tuşları)"),
            TargetImporter->TargetLinearX,
            TargetImporter->TargetAngularZ,
            TargetImporter->LeftWheelsRpm,
            TargetImporter->RightWheelsRpm,
            TargetImporter->AppliedWheelRpm
        );
        ControlInputsText->SetText(FText::FromString(CtrlStr));
    }

    // 4) Update Kinematics Text
    if (KinematicsText.IsValid())
    {
        FString KinStr = FString::Printf(
            TEXT("  • Gövde Hızı (km/h): %5.1f km/h\n"
                 "  • Anlık İvme (X)   : %+6.2f m/s²\n"
                 "  • Anlık İvme (Y)   : %+6.2f m/s²\n"
                 "  • Anlık İvme (Z)   : %+6.2f m/s²"),
            TargetImporter->CurrentForwardSpeedKmh,
            TargetImporter->CurrentLinearAccel.X,
            TargetImporter->CurrentLinearAccel.Y,
            TargetImporter->CurrentLinearAccel.Z
        );
        KinematicsText->SetText(FText::FromString(KinStr));
    }

    // 5) Update Status / CAD Geometry Text
    if (StatusTextBlock.IsValid())
    {
        FString StatusStr = FString::Printf(
            TEXT("  • Görsel Parçalar  : %d Adet (Render)\n"
                 "  • UCX Çarpışma     : %d Adet (Chaos Convex)\n"
                 "  • Simülasyon Durumu: %s"),
            TargetImporter->VisualMeshComponents.Num(),
            TargetImporter->UCXSections.Num(),
            TargetImporter->bIsPhysicsSimulating ? TEXT("⚡ AKTİF (Chaos Simülasyonu)") : TEXT("⏸️ DURAKLATILDI (Statik)")
        );
        StatusTextBlock->SetText(FText::FromString(StatusStr));
    }

    // 6) Update Physics Button Text
    if (PhysicsButtonText.IsValid())
    {
        FString PhysText = TargetImporter->bIsPhysicsSimulating ? TEXT(" ⚡ FİZİK: AÇIK ") : TEXT(" ⚡ FİZİK SİMÜLE ET ");
        PhysicsButtonText->SetText(FText::FromString(PhysText));
    }
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
