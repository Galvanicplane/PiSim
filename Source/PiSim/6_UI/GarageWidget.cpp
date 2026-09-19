// GarageWidget.cpp

#include "GarageWidget.h"
#include "Vehicle.h"
#include "MotorConfigurator.h"

// @state: WIP - Garaj Widget başlatıcı
void UPiSimModularGarageWidget::NativeConstruct()
{
    Super::NativeConstruct();
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
                // Sürekli güncelleme gereken parametreleri yenile
            }
        }
    }
}

// @state: WIP - Hedef araç atamasını yapar
void UPiSimModularGarageWidget::SetTargetVehicle(APiSimVehicle* InVehicle)
{
    TargetVehicle = InVehicle;
    CurrentSelectedBone = NAME_None;
    CurrentActiveModule = nullptr;
    CurrentProperties.Empty();
}

// @state: WIP - Kemik seçildiğinde eski modülden ayrılır ve yeni modülün slotlarını çizer
void UPiSimModularGarageWidget::SelectBone(FName BoneName)
{
    CurrentSelectedBone = BoneName;
    CurrentActiveModule = nullptr;
    CurrentProperties.Empty();

    if (!TargetVehicle || BoneName.IsNone())
    {
        return;
    }

    UActorComponent* AssignedComp = TargetVehicle->GetModuleAtBone(BoneName);
    if (AssignedComp)
    {
        CurrentActiveModule = Cast<IPiSimInspectableModule>(AssignedComp);
        RefreshPropertyInspector();
    }
}

// @state: WIP - Aktif modülün parametre tanımlarını arayüz slotlarına doldurur
void UPiSimModularGarageWidget::RefreshPropertyInspector()
{
    CurrentProperties.Empty();
    if (CurrentActiveModule)
    {
        CurrentActiveModule->GetInspectableProperties(CurrentProperties);
    }
}

// @state: WIP - UI sliderından gelen değeri Enter'a gerek kalmadan anında modüle aktarır
void UPiSimModularGarageWidget::OnSliderValueChanged(FName PropertyId, float NewValue)
{
    if (CurrentActiveModule)
    {
        CurrentActiveModule->SetInspectablePropertyFloat(PropertyId, NewValue);
    }
}

// @state: WIP - UI metin kutusundan gelen değeri modüle aktarır
void UPiSimModularGarageWidget::OnTextBoxValueChanged(FName PropertyId, const FString& NewValue)
{
    if (CurrentActiveModule)
    {
        CurrentActiveModule->SetInspectablePropertyString(PropertyId, NewValue);
    }
}

// @state: WIP - UI aksiyon butonuna basıldığında ilgili aksiyonu modülde tetikler
void UPiSimModularGarageWidget::OnTriggerAction(FName ActionId)
{
    if (CurrentActiveModule)
    {
        CurrentActiveModule->TriggerInspectableAction(ActionId);
    }
}

// @state: WIP - Kemik için seçilen rolü MotorConfigurator üzerinden değiştirir
void UPiSimModularGarageWidget::ChangeBoneRole(FName BoneName, const FString& NewRole)
{
    if (!TargetVehicle || BoneName.IsNone())
    {
        return;
    }

    UPiSimMotorConfigurator::AssignMotorRoleToBone(TargetVehicle, BoneName, NewRole);
    SelectBone(BoneName);
}
