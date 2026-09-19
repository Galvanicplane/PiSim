// GarageWidget.h
// Otopark / Slot Tabanlı Garaj Arayüzü:
// Solda kemik ve sensör listesi, sağda seçilen modülün dinamik parametre slotları (Slider, Metin kutusu, Buton).
// Cast<T> yapmaz; IPiSimInspectableModule arayüzü ile dinamik abone olur ve ayrılır.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "IPiSimInspectableModule.h"
#include "GarageWidget.generated.h"

class APiSimVehicle;

UCLASS()
class PISIM_API UPiSimModularGarageWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    // Hedef aracı bağlar
    UFUNCTION(BlueprintCallable, Category = "PiSim|UI")
    void SetTargetVehicle(APiSimVehicle* InVehicle);

    // Kemiğe tıklandığında sağ paneldeki parametre slotlarını dinamik olarak doldurur
    UFUNCTION(BlueprintCallable, Category = "PiSim|UI")
    void SelectBone(FName BoneName);

    // Sağ paneldeki bir float slider değeri değiştiğinde
    UFUNCTION(BlueprintCallable, Category = "PiSim|UI")
    void OnSliderValueChanged(FName PropertyId, float NewValue);

    // Sağ paneldeki bir metin kutusu değiştiğinde
    UFUNCTION(BlueprintCallable, Category = "PiSim|UI")
    void OnTextBoxValueChanged(FName PropertyId, const FString& NewValue);

    // Sağ paneldeki bir aksiyon butonu tıklandığında
    UFUNCTION(BlueprintCallable, Category = "PiSim|UI")
    void OnTriggerAction(FName ActionId);

    // Kemiğin motor rolünü değiştirme (Wheel, Servo, Propeller, LiftBody)
    UFUNCTION(BlueprintCallable, Category = "PiSim|UI")
    void ChangeBoneRole(FName BoneName, const FString& NewRole);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|UI")
    APiSimVehicle* TargetVehicle = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|UI")
    FName CurrentSelectedBone = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|UI")
    TArray<FPiSimPropertyDescriptor> CurrentProperties;

protected:
    IPiSimInspectableModule* CurrentActiveModule = nullptr;

    void RefreshPropertyInspector();
};
