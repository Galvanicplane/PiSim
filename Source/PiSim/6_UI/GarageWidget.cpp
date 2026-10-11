// GarageWidget.cpp

#include "GarageWidget.h"
#include "Vehicle.h"
#include "MotorConfigurator.h"
#include "SaveManager.h"
#include "Baker.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Kismet/GameplayStatics.h"

// @state: WIP - Garaj Widget başlatıcı
void UPiSimModularGarageWidget::NativeConstruct()
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

// @state: WIP - Tick anında sürekli güncellenen parametreleri modülden çeker
void UPiSimModularGarageWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (CurrentActiveModule)
    {
        for (FPiSimPropertyDescriptor& Prop : CurrentProperties)
        {
            if (Prop.bContinuousTickUpdate)
            {
                // Dinamik parametreleri periyodik olarak canlı tutar
            }
        }
    }
}

// @state: WIP - Dinamik Slate görsel arayüz hiyerarşisini kurar
TSharedRef<SWidget> UPiSimModularGarageWidget::RebuildWidget()
{
    return SNew(SBox)
        .HAlign(HAlign_Fill)
        .VAlign(VAlign_Fill)
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

                    // Fizik Aç/Kapa Butonu
                    + SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.0f, 0.0f, 8.0f, 0.0f))
                    [
                        SNew(SButton)
                        .ButtonColorAndOpacity(FLinearColor(0.1f, 0.5f, 0.9f, 1.0f))
                        .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModularGarageWidget::OnTogglePhysicsClicked))
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(TEXT("⚡ FİZİĞİ AÇ / KAPAT")))
                            .ColorAndOpacity(FLinearColor::White)
                        ]
                    ]

                    // Otomatik Kemik Atama Butonu
                    + SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.0f, 0.0f, 8.0f, 0.0f))
                    [
                        SNew(SButton)
                        .ButtonColorAndOpacity(FLinearColor(0.2f, 0.65f, 0.4f, 1.0f))
                        .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModularGarageWidget::OnAutoAssignMotorsClicked))
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(TEXT("🤖 OTOMATİK ROL ATA")))
                            .ColorAndOpacity(FLinearColor::White)
                        ]
                    ]

                    // Profil Kaydet/Yükle Girişi
                    + SHorizontalBox::Slot().AutoWidth().Padding(FMargin(10.0f, 0.0f, 6.0f, 0.0f))
                    [
                        SAssignNew(ProfileNameInput, SEditableTextBox)
                        .Text(FText::FromString(TEXT("default_vehicle")))
                        .MinDesiredWidth(130.0f)
                    ]

                    + SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.0f, 0.0f, 6.0f, 0.0f))
                    [
                        SNew(SButton)
                        .ButtonColorAndOpacity(FLinearColor(0.3f, 0.4f, 0.5f, 1.0f))
                        .OnClicked(FOnClicked::CreateUObject(this, &UPiSimModularGarageWidget::OnSaveProfileClicked))
                        [
                            SNew(STextBlock).Text(FText::FromString(TEXT("💾 KAYDET")))
                        ]
                    ]

                    + SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.0f, 0.0f, 8.0f, 0.0f))
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
                            SNew(STextBlock).Text(FText::FromString(TEXT("🔥 BAKE (KİLİTLE)")))
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

            // 2. Ana Çalışma Alanı: Sol Kemik Ağacı + Sağ Dinamik Otopark Paneli
            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            .Padding(FMargin(10.0f, 5.0f, 10.0f, 10.0f))
            [
                SNew(SHorizontalBox)

                // Sol Panel: Kemik Listesi
                + SHorizontalBox::Slot()
                .FillWidth(0.35f)
                .Padding(FMargin(0.0f, 0.0f, 6.0f, 0.0f))
                [
                    SNew(SBorder)
                    .BorderBackgroundColor(FLinearColor(0.02f, 0.05f, 0.1f, 0.95f))
                    .Padding(FMargin(12.0f))
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 8.0f))
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(TEXT("🦴 İSKELET VE KEMİK LİSTESİ")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
                            .ColorAndOpacity(FLinearColor(0.0f, 0.88f, 1.0f, 1.0f))
                        ]
                        + SVerticalBox::Slot().FillHeight(1.0f)
                        [
                            SAssignNew(BoneListScrollBox, SScrollBox)
                        ]
                    ]
                ]

                // Sağ Panel: Dinamik Otopark Parametre Paneli (IPiSimInspectableModule)
                + SHorizontalBox::Slot()
                .FillWidth(0.65f)
                .Padding(FMargin(6.0f, 0.0f, 0.0f, 0.0f))
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

// @state: WIP - İskeletteki kemikleri tarayıp sol listeyi doldurur
void UPiSimModularGarageWidget::RebuildBoneListView()
{
    if (!BoneListScrollBox.IsValid())
    {
        return;
    }

    BoneListScrollBox->ClearChildren();

    if (!TargetVehicle)
    {
        return;
    }

    TArray<FName> BoneNames;
    TargetVehicle->GetVehicleBoneNames(BoneNames);

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

// @state: WIP - Kemik seçildiğinde eski modülden ayrılır ve yeni modülün slotlarını çizer
void UPiSimModularGarageWidget::SelectBone(FName BoneName)
{
    CurrentSelectedBone = BoneName;
    CurrentActiveModule = nullptr;
    CurrentProperties.Empty();

    if (SelectedBoneTitleText.IsValid())
    {
        SelectedBoneTitleText->SetText(FText::FromString(FString::Printf(TEXT("📍 Kemik: %s"), *BoneName.ToString())));
    }

    if (TargetVehicle && !BoneName.IsNone())
    {
        UActorComponent* AssignedComp = TargetVehicle->GetModuleAtBone(BoneName);
        if (AssignedComp)
        {
            CurrentActiveModule = Cast<IPiSimInspectableModule>(AssignedComp);
        }
    }

    RebuildBoneListView();
    RefreshPropertyInspector();
}

// @state: WIP - IPiSimInspectableModule parametrelerini arayüz slotlarına dinamik olarak çizer
void UPiSimModularGarageWidget::RefreshPropertyInspector()
{
    if (!InspectorSlotScrollBox.IsValid())
    {
        return;
    }

    InspectorSlotScrollBox->ClearChildren();
    CurrentProperties.Empty();

    if (!CurrentActiveModule)
    {
        InspectorSlotScrollBox->AddSlot().Padding(FMargin(0.0f, 20.0f))
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
    if (CurrentActiveModule)
    {
        CurrentActiveModule->SetInspectablePropertyFloat(PropertyId, NewValue);
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

FReply UPiSimModularGarageWidget::OnTogglePhysicsClicked()
{
    if (TargetVehicle)
    {
        bool bActive = TargetVehicle->IsVehiclePhysicsActive();
        TargetVehicle->SetVehiclePhysicsEnabled(!bActive);
        if (StatusMessageText.IsValid())
        {
            StatusMessageText->SetText(FText::FromString(!bActive ? TEXT("🟢 Fizik Aktif") : TEXT("🔴 Fizik Kapalı")));
        }
    }
    return FReply::Handled();
}

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

FReply UPiSimModularGarageWidget::OnSaveProfileClicked()
{
    if (TargetVehicle && ProfileNameInput.IsValid())
    {
        FString OutMsg;
        UPiSimSaveManager::SaveProfile(TargetVehicle, ProfileNameInput->GetText().ToString(), OutMsg);
        if (StatusMessageText.IsValid())
        {
            StatusMessageText->SetText(FText::FromString(OutMsg));
        }
    }
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
