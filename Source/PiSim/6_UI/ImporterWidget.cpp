// ImporterWidget.cpp
// Model İthalat Arayüzü Uygulaması

#include "ImporterWidget.h"
#include "Vehicle.h"
#include "ModelFbxImporter.h"
#include "GarageWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"

// @state: WIP - NativeConstruct: Sahnedeki hedef aracı otomatik bulur
void UPiSimModularImporterWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (!TargetVehicle && GetWorld())
    {
        AActor* Found = UGameplayStatics::GetActorOfClass(GetWorld(), APiSimVehicle::StaticClass());
        if (Found)
        {
            SetTargetVehicle(Cast<APiSimVehicle>(Found));
        }
    }
}

// @state: WIP - Hedef aracı atar ve dosya listesini doldurur
void UPiSimModularImporterWidget::SetTargetVehicle(APiSimVehicle* InVehicle)
{
    TargetVehicle = InVehicle;
    RefreshFileList();
}

// @state: WIP - Slate Widget ağacını oluşturur
TSharedRef<SWidget> UPiSimModularImporterWidget::RebuildWidget()
{
    return SNew(SBox)
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)
        [
            SNew(SBorder)
            .BorderBackgroundColor(FLinearColor(0.015f, 0.035f, 0.08f, 0.98f))
            .Padding(FMargin(24.0f))
            [
                SNew(SBox)
                .WidthOverride(620.0f)
                [
                    SNew(SVerticalBox)

                    // 1. Başlık
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(FMargin(0.0f, 0.0f, 0.0f, 16.0f))
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight()
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(TEXT("📦 PISIM MODEL İTHALATÇISI (CAD / FBX IMPORTER)")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
                            .ColorAndOpacity(FLinearColor(0.0f, 0.88f, 1.0f, 1.0f))
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f, 0.0f, 0.0f))
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(TEXT("Diskteki Saved/Robots/Cache/ klasöründen bir FBX model seçip araca yükleyin.")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                            .ColorAndOpacity(FLinearColor(0.7f, 0.75f, 0.85f, 1.0f))
                        ]
                    ]

                    // 2. Ana Gövde (Sol: Dosya Listesi, Sağ: Ayarlar)
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(FMargin(0.0f, 0.0f, 0.0f, 16.0f))
                    [
                        SNew(SHorizontalBox)

                        // Sol: Disk Dosya Listesi
                        + SHorizontalBox::Slot()
                        .FillWidth(0.55f)
                        .Padding(FMargin(0.0f, 0.0f, 12.0f, 0.0f))
                        [
                            SNew(SBorder)
                            .BorderBackgroundColor(FLinearColor(0.03f, 0.06f, 0.12f, 0.95f))
                            .Padding(FMargin(10.0f))
                            [
                                SNew(SVerticalBox)
                                + SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 6.0f))
                                [
                                    SNew(STextBlock)
                                    .Text(FText::FromString(TEXT("📂 MEVCUT FBX MODELLER")))
                                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                                    .ColorAndOpacity(FLinearColor::White)
                                ]
                                + SVerticalBox::Slot().AutoHeight()
                                [
                                    SNew(SBox)
                                    .HeightOverride(180.0f)
                                    [
                                        SAssignNew(FileListScrollBox, SScrollBox)
                                    ]
                                ]
                            ]
                        ]

                        // Sağ: Ayarlar ve Seçili Model Bilgisi
                        + SHorizontalBox::Slot()
                        .FillWidth(0.45f)
                        [
                            SNew(SBorder)
                            .BorderBackgroundColor(FLinearColor(0.03f, 0.06f, 0.12f, 0.95f))
                            .Padding(FMargin(10.0f))
                            [
                                SNew(SVerticalBox)

                                // Seçili Dosya
                                + SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 8.0f))
                                [
                                    SNew(SVerticalBox)
                                    + SVerticalBox::Slot().AutoHeight()
                                    [
                                        SNew(STextBlock).Text(FText::FromString(TEXT("Seçili Dosya:"))).ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f, 1.0f))
                                    ]
                                    + SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 2.0f, 0.0f, 0.0f))
                                    [
                                        SAssignNew(SelectedFileText, STextBlock)
                                        .Text(FText::FromString(SelectedFileName))
                                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                                        .ColorAndOpacity(FLinearColor(0.2f, 0.9f, 0.4f, 1.0f))
                                    ]
                                ]

                                // Ölçek Seçimi
                                + SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 6.0f, 0.0f, 8.0f))
                                [
                                    SNew(SVerticalBox)
                                    + SVerticalBox::Slot().AutoHeight()
                                    [
                                        SNew(SHorizontalBox)
                                        + SHorizontalBox::Slot().AutoWidth()
                                        [
                                            SNew(STextBlock).Text(FText::FromString(TEXT("İthalat Ölçeği: "))).ColorAndOpacity(FLinearColor::White)
                                        ]
                                        + SHorizontalBox::Slot().AutoWidth()
                                        [
                                            SAssignNew(ScaleStatusText, STextBlock)
                                            .Text(FText::FromString(FString::Printf(TEXT("%.1fx"), SelectedScale)))
                                            .ColorAndOpacity(FLinearColor(0.0f, 0.88f, 1.0f, 1.0f))
                                        ]
                                    ]
                                    + SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f, 0.0f, 0.0f))
                                    [
                                        SNew(SHorizontalBox)
                                        + SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
                                        [
                                            SNew(SButton)
                                            .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModularImporterWidget::OnScaleSelected, 0.1f))
                                            [ SNew(STextBlock).Text(FText::FromString(TEXT("0.1x"))) ]
                                        ]
                                        + SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
                                        [
                                            SNew(SButton)
                                            .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModularImporterWidget::OnScaleSelected, 1.0f))
                                            [ SNew(STextBlock).Text(FText::FromString(TEXT("1.0x"))) ]
                                        ]
                                        + SHorizontalBox::Slot().AutoWidth()
                                        [
                                            SNew(SButton)
                                            .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModularImporterWidget::OnScaleSelected, 10.0f))
                                            [ SNew(STextBlock).Text(FText::FromString(TEXT("10.0x"))) ]
                                        ]
                                    ]
                                ]

                                // Smooth Shading Checkbox
                                + SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 6.0f, 0.0f, 0.0f))
                                [
                                    SNew(SHorizontalBox)
                                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.0f, 0.0f, 6.0f, 0.0f))
                                    [
                                        SNew(SCheckBox)
                                        .IsChecked(bEnableSmoothNormals ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
                                        .OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
                                        {
                                            bEnableSmoothNormals = (NewState == ECheckBoxState::Checked);
                                        })
                                    ]
                                    + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
                                    [
                                        SNew(STextBlock).Text(FText::FromString(TEXT("Yumuşatılmış Normaller (Smooth)"))).ColorAndOpacity(FLinearColor::White)
                                    ]
                                ]
                            ]
                        ]
                    ]

                    // 3. Durum Bildirimi
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(FMargin(0.0f, 0.0f, 0.0f, 12.0f))
                    [
                        SAssignNew(StatusMessageText, STextBlock)
                        .Text(FText::FromString(TEXT("Hazır. Bir model seçip İthal Et butonuna basınız.")))
                        .ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f, 1.0f))
                    ]

                    // 4. Aksiyon Butonları
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(SHorizontalBox)

                        // [İTHAL ET]
                        + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        .Padding(FMargin(0.0f, 0.0f, 8.0f, 0.0f))
                        [
                            SNew(SButton)
                            .ButtonColorAndOpacity(FLinearColor(0.0f, 0.55f, 0.9f, 1.0f))
                            .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModularImporterWidget::OnImportButtonClicked))
                            [
                                SNew(SBox)
                                .HAlign(HAlign_Center)
                                .Padding(FMargin(0.0f, 8.0f))
                                [
                                    SNew(STextBlock)
                                    .Text(FText::FromString(TEXT("🚀 SEÇİLİ MODELİ İTHAL ET")))
                                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
                                    .ColorAndOpacity(FLinearColor::White)
                                ]
                            ]
                        ]

                        // [İPTAL / GARAJA GEÇ]
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        [
                            SNew(SButton)
                            .ButtonColorAndOpacity(FLinearColor(0.2f, 0.25f, 0.35f, 1.0f))
                            .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModularImporterWidget::OnCancelButtonClicked))
                            [
                                SNew(SBox)
                                .HAlign(HAlign_Center)
                                .Padding(FMargin(12.0f, 8.0f))
                                [
                                    SNew(STextBlock)
                                    .Text(FText::FromString(TEXT("GARAJA GEÇ")))
                                    .ColorAndOpacity(FLinearColor::White)
                                ]
                            ]
                        ]
                    ]
                ]
            ]
        ];
}

// @state: WIP - Diskteki modelleri tarar ve buton listesini oluşturur
void UPiSimModularImporterWidget::RefreshFileList()
{
    if (!FileListScrollBox.IsValid())
    {
        return;
    }

    FileListScrollBox->ClearChildren();

    TArray<FString> Models = UPiSimModelFbxImporter::GetAvailableCacheModels();
    if (Models.Num() == 0)
    {
        FileListScrollBox->AddSlot()
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("Saved/Robots/Cache/ klasöründe FBX dosyası bulunamadı.")))
            .ColorAndOpacity(FLinearColor::Red)
        ];
        return;
    }

    for (const FString& ModelName : Models)
    {
        bool bIsSelected = (ModelName == SelectedFileName);

        FileListScrollBox->AddSlot()
            .Padding(FMargin(0.0f, 2.0f))
            [
                SNew(SButton)
                .ButtonColorAndOpacity(bIsSelected ? FLinearColor(0.0f, 0.45f, 0.85f, 1.0f) : FLinearColor(0.08f, 0.14f, 0.24f, 0.8f))
                .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModularImporterWidget::OnSelectFileClicked, ModelName))
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(FString::Printf(TEXT("📦 %s"), *ModelName)))
                    .ColorAndOpacity(FLinearColor::White)
                ]
            ];
    }
}

// @state: WIP - Dosya seçildiğinde seçimi günceller
FReply UPiSimModularImporterWidget::OnSelectFileClicked(FString FileName)
{
    SelectedFileName = FileName;
    if (SelectedFileText.IsValid())
    {
        SelectedFileText->SetText(FText::FromString(SelectedFileName));
    }
    RefreshFileList();
    return FReply::Handled();
}

// @state: WIP - Ölçek seçildiğinde ölçeği günceller
FReply UPiSimModularImporterWidget::OnScaleSelected(float NewScale)
{
    SelectedScale = NewScale;
    if (ScaleStatusText.IsValid())
    {
        ScaleStatusText->SetText(FText::FromString(FString::Printf(TEXT("%.1fx"), SelectedScale)));
    }
    return FReply::Handled();
}

// @state: WIP - Modeli ithal eder ve Garaj arayüzünü açar
FReply UPiSimModularImporterWidget::OnImportButtonClicked()
{
    if (!TargetVehicle)
    {
        if (StatusMessageText.IsValid())
        {
            StatusMessageText->SetText(FText::FromString(TEXT("HATA: Hedef araç bulunamadı!")));
        }
        return FReply::Handled();
    }

    FString FullPath = FPaths::ProjectSavedDir() / TEXT("Robots/Cache") / SelectedFileName;
    bool bOk = UPiSimModelFbxImporter::ImportFbxFileToVehicle(TargetVehicle, FullPath, SelectedScale, bEnableSmoothNormals);

    if (bOk)
    {
        // Kendini kapat ve Garaj arayüzünü aç
        RemoveFromParent();

        APlayerController* PC = GetOwningPlayer();
        if (PC)
        {
            UPiSimModularGarageWidget* GarageUI = CreateWidget<UPiSimModularGarageWidget>(PC, UPiSimModularGarageWidget::StaticClass());
            if (GarageUI)
            {
                GarageUI->SetTargetVehicle(TargetVehicle);
                GarageUI->AddToViewport(10);
            }
        }
    }
    else
    {
        if (StatusMessageText.IsValid())
        {
            StatusMessageText->SetText(FText::FromString(FString::Printf(TEXT("HATA: %s ithal edilemedi!"), *SelectedFileName)));
        }
    }

    return FReply::Handled();
}

// @state: WIP - İptal edip doğrudan Garaj arayüzüne geçer
FReply UPiSimModularImporterWidget::OnCancelButtonClicked()
{
    RemoveFromParent();

    APlayerController* PC = GetOwningPlayer();
    if (PC && TargetVehicle)
    {
        UPiSimModularGarageWidget* GarageUI = CreateWidget<UPiSimModularGarageWidget>(PC, UPiSimModularGarageWidget::StaticClass());
        if (GarageUI)
        {
            GarageUI->SetTargetVehicle(TargetVehicle);
            GarageUI->AddToViewport(10);
        }
    }

    return FReply::Handled();
}

// @state: WIP - C++ Blueprint ExecuteImport köprüsü
bool UPiSimModularImporterWidget::ExecuteImport(APiSimVehicle* InVehicle, const FString& FileName, float InScale, bool bInSmooth)
{
    if (!InVehicle) return false;
    FString FullPath = FPaths::ProjectSavedDir() / TEXT("Robots/Cache") / FileName;
    return UPiSimModelFbxImporter::ImportFbxFileToVehicle(InVehicle, FullPath, InScale, bInSmooth);
}
