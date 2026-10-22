// GarageWidget.h
// Otopark / Slot Tabanlı Garaj Arayüzü:
// Solda kemik ve sensör listesi, sağda seçilen modülün dinamik parametre slotları (Slider, Metin kutusu, Buton).
// Cast<T> yapmaz; IPiSimInspectableModule arayüzü ile dinamik abone olur ve ayrılır.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "IPiSimInspectableModule.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "GarageWidget.generated.h"

class APiSimVehicle;
class SScrollBox;
class STextBlock;
class SEditableTextBox;

UCLASS()
class PISIM_API UPiSimModularGarageWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual TSharedRef<SWidget> RebuildWidget() override;

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

    // WebUI JS Köprüsü UFUNCTION'ları
    UFUNCTION(BlueprintCallable, Category = "PiSim|UI")
    void TogglePhysics() { OnTogglePhysicsClicked(); }

    UFUNCTION(BlueprintCallable, Category = "PiSim|UI")
    void ToggleCollisionView() { OnToggleCollisionViewClicked(); }

    UFUNCTION(BlueprintCallable, Category = "PiSim|UI")
    void ToggleModel() { OnToggleModelClicked(); }

    UFUNCTION(BlueprintCallable, Category = "PiSim|UI")
    void ScaleDown() { OnMultiplyScale01Clicked(); }

    UFUNCTION(BlueprintCallable, Category = "PiSim|UI")
    void ScaleUp() { OnMultiplyScale10Clicked(); }

    UFUNCTION(BlueprintCallable, Category = "PiSim|UI")
    void ReimportModel() { OnReimportClicked(); }

    UFUNCTION(BlueprintCallable, Category = "PiSim|UI")
    void AutoAssignMotors() { OnAutoAssignMotorsClicked(); }

    UFUNCTION(BlueprintCallable, Category = "PiSim|UI")
    void SaveProfile() { OnSaveProfileClicked(); }

    UFUNCTION(BlueprintCallable, Category = "PiSim|UI")
    void BakeModel() { OnBakeClicked(); }

    // WebUI Fare ile 3D Kamera Kontrolü (Orbit / Pan / Zoom)
    UFUNCTION(BlueprintCallable, Category = "PiSim|UI")
    void MouseDelta(float DeltaX, float DeltaY, int32 Mode);

    UFUNCTION(BlueprintCallable, Category = "PiSim|UI")
    void MouseZoom(float Direction);

    // WebUI: Canlı motor test değeri uygular (-1.0f .. +1.0f)
    UFUNCTION(BlueprintCallable, Category = "PiSim|UI")
    void SetMotorTestValue(FName BoneName, float Value);

    // WebUI: Sayfa açıldığında veya model değiştiğinde araç iskeletini ve modül verilerini talep eder
    UFUNCTION(BlueprintCallable, Category = "PiSim|UI")
    void RequestVehicleState();

    // WebUI: 3D Kemik teşhis çizgilerini açıp kapatır ('U' tuşunun arayüz karşılığı)
    UFUNCTION(BlueprintCallable, Category = "PiSim|UI")
    void ToggleDebugDraw();

    // UI Aksiyon Tetikleyicileri
    FReply OnTogglePhysicsClicked();
    FReply OnToggleCollisionViewClicked();
    FReply OnToggleModelClicked();
    FReply OnMultiplyScale01Clicked();
    FReply OnMultiplyScale10Clicked();
    FReply OnReimportClicked();
    FReply OnAutoAssignMotorsClicked();
    FReply OnSaveProfileClicked();
    FReply OnLoadProfileClicked();
    FReply OnBakeClicked();
    FReply OnSelectBoneClicked(FName BoneName);
    FReply OnAssignRoleClicked(FString RoleName);

protected:
    IPiSimInspectableModule* CurrentActiveModule = nullptr;

    void RebuildBoneListView();
    void RefreshPropertyInspector();

    TSharedPtr<class SWebBrowser> WebBrowserWidget;
    TSharedPtr<SScrollBox> BoneListScrollBox;
    TSharedPtr<SScrollBox> InspectorSlotScrollBox;
    TSharedPtr<STextBlock> SelectedBoneTitleText;
    TSharedPtr<SEditableTextBox> ProfileNameInput;
    TSharedPtr<STextBlock> StatusMessageText;

    TSharedPtr<STextBlock> ModelButtonText;
    TSharedPtr<STextBlock> CollisionButtonText;
    TSharedPtr<STextBlock> PhysicsButtonText;
    TSharedPtr<STextBlock> ScaleText;

    float CurrentScaleMultiplier = 1.0f;
    FString ActiveModelFileName = TEXT("robot_import_test.fbx");
    bool bSmoothNormals = true;
};
