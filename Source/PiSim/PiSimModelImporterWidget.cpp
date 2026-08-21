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

    FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 14);
    FSlateFontInfo NormalFont = FCoreStyle::GetDefaultFontStyle("Regular", 10);
    FSlateFontInfo ButtonFont = FCoreStyle::GetDefaultFontStyle("Bold", 10);

    return SNew(SOverlay)
        // ---------------------------------------------------------------------
        // TOP HEADER BAR & CONTROLS
        // ---------------------------------------------------------------------
        + SOverlay::Slot()
        .HAlign(HAlign_Fill)
        .VAlign(VAlign_Top)
        [
            SNew(SBorder)
            .BorderBackgroundColor(FLinearColor(0.02f, 0.04f, 0.08f, 0.95f))
            .Padding(FMargin(20.0f, 12.0f))
            [
                SNew(SHorizontalBox)

                // Title Badge
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("🚀 PiSim MODEL IMPORTER | DEDICATED TEST SYSTEM")))
                    .Font(TitleFont)
                    .ColorAndOpacity(FLinearColor(0.0f, 0.94f, 1.0f, 1.0f))
                ]

                + SHorizontalBox::Slot().FillWidth(1.0f) // Spacer

                // Scale 0.1X Button
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(FMargin(4.0f, 0.0f))
                [
                    SNew(SButton)
                    .ButtonColorAndOpacity(FLinearColor(0.12f, 0.22f, 0.35f, 1.0f))
                    .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModelImporterWidget::OnScale01Clicked))
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT(" 🔍 0.1X ")))
                        .Font(ButtonFont)
                        .ColorAndOpacity(FLinearColor::White)
                    ]
                ]

                // Scale 1.0X Button
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(FMargin(4.0f, 0.0f))
                [
                    SNew(SButton)
                    .ButtonColorAndOpacity(FLinearColor(0.0f, 0.5f, 0.8f, 1.0f))
                    .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModelImporterWidget::OnScale10Clicked))
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT(" 📐 1.0X (Default) ")))
                        .Font(ButtonFont)
                        .ColorAndOpacity(FLinearColor::White)
                    ]
                ]

                // Scale 10.0X Button
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(FMargin(4.0f, 0.0f))
                [
                    SNew(SButton)
                    .ButtonColorAndOpacity(FLinearColor(0.12f, 0.22f, 0.35f, 1.0f))
                    .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModelImporterWidget::OnScale100Clicked))
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT(" 🔬 10.0X ")))
                        .Font(ButtonFont)
                        .ColorAndOpacity(FLinearColor::White)
                    ]
                ]

                // Reimport Button
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(FMargin(10.0f, 0.0f, 4.0f, 0.0f))
                [
                    SNew(SButton)
                    .ButtonColorAndOpacity(FLinearColor(0.1f, 0.55f, 0.3f, 1.0f))
                    .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModelImporterWidget::OnReimportClicked))
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT(" 🔄 REIMPORT FBX ")))
                        .Font(ButtonFont)
                        .ColorAndOpacity(FLinearColor::White)
                    ]
                ]

                // Toggle Physics Button
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(FMargin(6.0f, 0.0f, 0.0f, 0.0f))
                [
                    SNew(SButton)
                    .ButtonColorAndOpacity(FLinearColor(0.85f, 0.45f, 0.0f, 1.0f))
                    .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModelImporterWidget::OnTogglePhysicsClicked))
                    [
                        SAssignNew(PhysicsButtonText, STextBlock)
                        .Text(FText::FromString(TEXT(" ⚡ FİZİĞİ SİMÜLE ET ")))
                        .Font(ButtonFont)
                        .ColorAndOpacity(FLinearColor::White)
                    ]
                ]
            ]
        ]

        // ---------------------------------------------------------------------
        // LEFT TELEMETRY & INFO CARD
        // ---------------------------------------------------------------------
        + SOverlay::Slot()
        .HAlign(HAlign_Left)
        .VAlign(VAlign_Top)
        .Padding(FMargin(20.0f, 75.0f, 0.0f, 0.0f))
        [
            SNew(SBox)
            .WidthOverride(380.0f)
            [
                SNew(SBorder)
                .BorderBackgroundColor(FLinearColor(0.03f, 0.06f, 0.12f, 0.92f))
                .Padding(FMargin(16.0f))
                [
                    SNew(SVerticalBox)

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 0.0f, 0.0f, 10.0f)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("📊 MODEL & TEST BİLGİSİ")))
                        .Font(TitleFont)
                        .ColorAndOpacity(FLinearColor(0.0f, 0.94f, 1.0f, 1.0f))
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SAssignNew(StatusTextBlock, STextBlock)
                        .Text(FText::FromString(TEXT("Yükleniyor...")))
                        .Font(NormalFont)
                        .ColorAndOpacity(FLinearColor::White)
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 14.0f, 0.0f, 0.0f)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("🎮 KONTROLLER:\n  • G Tuşu: Tekerlek Hızına +1 RPM Ekle\n  • F Tuşu: Tekerlek Hızından -1 RPM Çıkar\n  • Sol Tık + Sürükle: 360° Orbit Dön\n  • Sağ Tık + Sürükle: Kamerayı Kaydır\n  • Fare Tekerleği: Yaklaş / Uzaklaş")))
                        .Font(NormalFont)
                        .ColorAndOpacity(FLinearColor(0.7f, 0.85f, 1.0f, 0.9f))
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

    if (TargetImporter && StatusTextBlock.IsValid())
    {
        FString Info = FString::Printf(
            TEXT("📁 Dosya: Saved/Robots/Cache/robot_import_test.fbx\n"
                 "🎨 Görsel Parçalar: %d Adet (Render Açık)\n"
                 "🛡️ UCX Çarpışma: %d Adet (Chaos Zırhı Giydirildi)\n"
                 "📐 Geçerli Ölçek: %.2fX\n"
                 "⚡ Fizik Simülasyonu: %s\n"
                 "🏎️ Tekerlek Hızı (RPM): %+.1f RPM (G: +1 | F: -1)"),
            TargetImporter->VisualMeshComponents.Num(),
            TargetImporter->UCXSections.Num(),
            TargetImporter->ImportScaleMultiplier,
            TargetImporter->bIsPhysicsSimulating ? TEXT("AKTİF (Canlı)") : TEXT("KAPALI (Statik)"),
            TargetImporter->AppliedWheelRpm
        );
        StatusTextBlock->SetText(FText::FromString(Info));
    }

    if (TargetImporter && PhysicsButtonText.IsValid())
    {
        FString PhysText = TargetImporter->bIsPhysicsSimulating ? TEXT(" ⚡ FİZİK: AKTİF (AÇIK) ") : TEXT(" ⚡ FİZİĞİ SİMÜLE ET ");
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
