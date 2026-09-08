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
        // 2) LEFT PANEL: GELEN VERİLER & BAĞLANTI AŞAMALARI (Pi 5 ➔ UE5)
        // ---------------------------------------------------------------------
        + SOverlay::Slot()
        .HAlign(HAlign_Left)
        .VAlign(VAlign_Top)
        .Padding(FMargin(18.0f, 64.0f, 0.0f, 0.0f))
        [
            SNew(SBox)
            .WidthOverride(440.0f)
            [
                SNew(SBorder)
                .BorderBackgroundColor(FLinearColor(0.012f, 0.025f, 0.06f, 0.95f))
                .Padding(FMargin(14.0f, 12.0f))
                [
                    SNew(SVerticalBox)

                    // Card 1: Connection Stages (Bağlantı Aşamaları)
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 0.0f, 0.0f, 3.0f)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("🔗 Pİ 5 BAĞLANTI AŞAMALARI (1 - 4)")))
                        .Font(CardHeaderFont)
                        .ColorAndOpacity(FLinearColor(0.0f, 0.88f, 1.0f, 1.0f))
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 0.0f, 0.0f, 10.0f)
                    [
                        SAssignNew(ConnectionStagesText, STextBlock)
                        .Text(FText::FromString(TEXT("Aşama bilgileri hazırlanıyor...")))
                        .Font(DataFont)
                        .ColorAndOpacity(FLinearColor(0.9f, 0.95f, 1.0f, 1.0f))
                    ]

                    // Separator
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 0.0f, 0.0f, 6.0f)
                    [
                        SNew(SBorder)
                        .BorderBackgroundColor(FLinearColor(0.1f, 0.25f, 0.45f, 0.6f))
                        .Padding(FMargin(0.0f, 0.5f))
                    ]

                    // Card 2: Incoming Data (GELEN VERİLER - Pi 5 -> UE5)
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 3.0f, 0.0f, 3.0f)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("🎮 GELEN VERİLER (Pi 5 ➔ UE5 - Twist Komutları)")))
                        .Font(CardHeaderFont)
                        .ColorAndOpacity(FLinearColor(0.2f, 1.0f, 0.5f, 1.0f))
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 0.0f, 0.0f, 10.0f)
                    [
                        SAssignNew(IncomingDataText, STextBlock)
                        .Text(FText::FromString(TEXT("Gelen kontrol komutu bekleniyor...")))
                        .Font(DataFont)
                        .ColorAndOpacity(FLinearColor(0.9f, 0.95f, 1.0f, 1.0f))
                    ]

                    // Separator
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 0.0f, 0.0f, 6.0f)
                    [
                        SNew(SBorder)
                        .BorderBackgroundColor(FLinearColor(0.1f, 0.25f, 0.45f, 0.6f))
                        .Padding(FMargin(0.0f, 0.5f))
                    ]

                    // Card 3: Live Connection & Event Log (Canlı Bağlantı Günlüğü)
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 3.0f, 0.0f, 3.0f)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("📋 CANLI BAĞLANTI & HATA GÜNLÜĞÜ (DEBUG LOG)")))
                        .Font(CardHeaderFont)
                        .ColorAndOpacity(FLinearColor(1.0f, 0.55f, 0.25f, 1.0f))
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SAssignNew(ConnectionDebugLogText, STextBlock)
                        .Text(FText::FromString(TEXT("Log bekleniyor...")))
                        .Font(DataFont)
                        .ColorAndOpacity(FLinearColor(1.0f, 0.92f, 0.75f, 1.0f))
                    ]
                ]
            ]
        ]

        // ---------------------------------------------------------------------
        // 3) RIGHT PANEL: GİDEN VERİLER & TELEMETRİ (UE5 ➔ Pi 5) & CAD
        // ---------------------------------------------------------------------
        + SOverlay::Slot()
        .HAlign(HAlign_Right)
        .VAlign(VAlign_Top)
        .Padding(FMargin(0.0f, 64.0f, 18.0f, 0.0f))
        [
            SNew(SBox)
            .WidthOverride(420.0f)
            [
                SNew(SBorder)
                .BorderBackgroundColor(FLinearColor(0.012f, 0.025f, 0.06f, 0.95f))
                .Padding(FMargin(14.0f, 12.0f))
                [
                    SNew(SVerticalBox)

                    // Card 1: Outgoing Telemetry (GİDEN VERİLER - UE5 -> Pi 5)
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 0.0f, 0.0f, 3.0f)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("📡 GİDEN VERİLER (UE5 ➔ Pi 5 - IMU TELEMETRİSİ)")))
                        .Font(CardHeaderFont)
                        .ColorAndOpacity(FLinearColor(1.0f, 0.75f, 0.1f, 1.0f))
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 0.0f, 0.0f, 10.0f)
                    [
                        SAssignNew(OutgoingTelemetryText, STextBlock)
                        .Text(FText::FromString(TEXT("Telemetri verisi hazırlanıyor...")))
                        .Font(DataFont)
                        .ColorAndOpacity(FLinearColor(0.9f, 0.95f, 1.0f, 1.0f))
                    ]

                    // Separator
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 0.0f, 0.0f, 6.0f)
                    [
                        SNew(SBorder)
                        .BorderBackgroundColor(FLinearColor(0.1f, 0.25f, 0.45f, 0.6f))
                        .Padding(FMargin(0.0f, 0.5f))
                    ]

                    // Card 2: CAD Geometry & Chaos Physics
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 3.0f, 0.0f, 3.0f)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("📦 CAD GEOMETRİ & CHAOS FİZİK ZIRHI")))
                        .Font(CardHeaderFont)
                        .ColorAndOpacity(FLinearColor(0.7f, 0.85f, 1.0f, 0.95f))
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SAssignNew(ModelCadStatusText, STextBlock)
                        .Text(FText::FromString(TEXT("Yükleniyor...")))
                        .Font(DataFont)
                        .ColorAndOpacity(FLinearColor(0.85f, 0.92f, 1.0f, 1.0f))
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

    // 2) SOL PANEL - Card 1: Connection Stages (Bağlantı Aşamaları)
    if (ConnectionStagesText.IsValid())
    {
        FString SocketStatus = TargetImporter->bIsSocketBound ? TEXT("🟢 AÇIK (Dinliyor)") : TEXT("🔴 KAPALI / HATA");
        FString Pi5Status = TargetImporter->bIsPiConnected ?
            FString::Printf(TEXT("🟢 BAĞLANDI (IP: %s)"), *TargetImporter->ConnectedPiIP) :
            (TargetImporter->ConnectionStage == 5 ? TEXT("🔴 KOPTU (Zaman Aşımı)") : TEXT("🟡 BEKLENİYOR..."));

        FString LastPacketTimeStr;
        if (TargetImporter->LastRxTimestampSec > 0.0f && GetWorld())
        {
            float TimeSinceLastSec = GetWorld()->GetTimeSeconds() - TargetImporter->LastRxTimestampSec;
            LastPacketTimeStr = FString::Printf(TEXT("%.2f sn önce"), TimeSinceLastSec);
        }
        else
        {
            LastPacketTimeStr = TEXT("Henüz paket alınmadı");
        }

        FString StagesStr = FString::Printf(
            TEXT("  • Aşama 1: UE5 Dinleme Soketi : 0.0.0.0:7400 [%s]\n"
                 "  • Aşama 2: Telemetri Hedefleri: %s:7401 [🟢 HAZIR]\n"
                 "  • Aşama 3: Pi 5 Handshake     : %s\n"
                 "  • Aşama 4: Canlı Veri Akışı   : %5.1f Hz  (Toplam: %d Paket)\n"
                 "  • Son Paket Geliş Zamanı      : %s"),
            *SocketStatus,
            *TargetImporter->BridgeTargetIP,
            *Pi5Status,
            TargetImporter->RxPacketRateHz,
            TargetImporter->TotalPacketsReceived,
            *LastPacketTimeStr
        );
        ConnectionStagesText->SetText(FText::FromString(StagesStr));
    }

    // 3) SOL PANEL - Card 2: Incoming Data (GELEN VERİLER - Pi 5 ➔ UE5)
    if (IncomingDataText.IsValid())
    {
        FString InDataStr = FString::Printf(
            TEXT("  • Son Paket Boyutu : %d Bayt (geometry_msgs/Twist)\n"
                 "  • Doğrusal Hız (X) : %+.2f m/s  (Y: %+.2f, Z: %+.2f)\n"
                 "  • Açısal Hız (Yaw) : %+.2f rad/s\n"
                 "  • Sol Motor Hızı   : %+6.1f RPM\n"
                 "  • Sağ Motor Hızı   : %+6.1f RPM\n"
                 "  • Manuel RPM Ofset : %+.1f RPM (G / F Tuşları)"),
            TargetImporter->LastRxPacketBytes,
            TargetImporter->TargetLinearX,
            TargetImporter->LastRxLinearVel.Y,
            TargetImporter->LastRxLinearVel.Z,
            TargetImporter->TargetAngularZ,
            TargetImporter->LeftWheelsRpm,
            TargetImporter->RightWheelsRpm,
            TargetImporter->AppliedWheelRpm
        );
        IncomingDataText->SetText(FText::FromString(InDataStr));
    }

    // 4) SOL PANEL - Card 3: Live Connection & Event Log (Canlı Bağlantı Günlüğü)
    if (ConnectionDebugLogText.IsValid())
    {
        if (TargetImporter->ConnectionDebugLogs.Num() > 0)
        {
            FString Combined = FString::Join(TargetImporter->ConnectionDebugLogs, TEXT("\n"));
            ConnectionDebugLogText->SetText(FText::FromString(Combined));
        }
        else
        {
            ConnectionDebugLogText->SetText(FText::FromString(TEXT("  Henüz bir bağlantı olayı kaydedilmedi.")));
        }
    }

    // 5) SAĞ PANEL - Card 1: Outgoing Telemetry (GİDEN VERİLER - UE5 ➔ Pi 5)
    if (OutgoingTelemetryText.IsValid())
    {
        FString OutStr = FString::Printf(
            TEXT("  • Telemetri Paketi : #%d (%d Bayt, %5.1f Hz)\n"
                 "  • Gönderim Hedefi  : %s:7401 (Pi 5 Ethernet)\n"
                 "  • Gövde Hızı       : %5.1f km/h\n"
                 "  • İvmeölçer (Accel): X=%+5.2f, Y=%+5.2f, Z=%+5.2f m/s²\n"
                 "  • Jiroskop (Gyro)  : X=%+5.2f, Y=%+5.2f, Z=%+5.2f deg/s\n"
                 "  • Oryantasyon Quat : (X=%.3f, Y=%.3f, Z=%.3f, W=%.3f)"),
            TargetImporter->TotalPacketsSent,
            TargetImporter->LastTxPacketBytes > 0 ? TargetImporter->LastTxPacketBytes : 80,
            TargetImporter->TxPacketRateHz,
            *TargetImporter->BridgeTargetIP,
            TargetImporter->CurrentForwardSpeedKmh,
            TargetImporter->CurrentLinearAccel.X,
            TargetImporter->CurrentLinearAccel.Y,
            TargetImporter->CurrentLinearAccel.Z,
            TargetImporter->LastTxGyro.X,
            TargetImporter->LastTxGyro.Y,
            TargetImporter->LastTxGyro.Z,
            TargetImporter->LastTxQuat.X,
            TargetImporter->LastTxQuat.Y,
            TargetImporter->LastTxQuat.Z,
            TargetImporter->LastTxQuat.W
        );
        OutgoingTelemetryText->SetText(FText::FromString(OutStr));
    }

    // 6) SAĞ PANEL - Card 2: CAD Geometry & Chaos Physics
    if (ModelCadStatusText.IsValid())
    {
        FString CadStr = FString::Printf(
            TEXT("  • Görsel Parçalar  : %d Adet (Procedural Mesh Render)\n"
                 "  • UCX Çarpışma     : %d Adet (Chaos Convex Zırh)\n"
                 "  • Simülasyon Durumu: %s\n"
                 "  • Kamera Durumu    : Gövdeye Kilitli (World Location Snap)"),
            TargetImporter->VisualMeshComponents.Num(),
            TargetImporter->UCXSections.Num(),
            TargetImporter->bIsPhysicsSimulating ? TEXT("⚡ AKTİF (Chaos Simülasyonu)") : TEXT("⏸️ DURAKLATILDI (Statik)")
        );
        ModelCadStatusText->SetText(FText::FromString(CadStr));
    }

    // 7) Physics Button Text
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
