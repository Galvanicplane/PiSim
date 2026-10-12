// ImporterWidget.h
// Model İthalat Arayüzü: Diskteki Saved/Robots/Cache/ klasöründeki FBX dosyalarını tarar,
// ölçek ve normal ayarlarını alır ve APiSimVehicle aktörüne bağlar.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "ImporterWidget.generated.h"

class APiSimVehicle;
class SScrollBox;
class STextBlock;

UCLASS()
class PISIM_API UPiSimModularImporterWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    virtual TSharedRef<SWidget> RebuildWidget() override;

    // Hedef aracı atar
    UFUNCTION(BlueprintCallable, Category = "PiSim|Importer")
    void SetTargetVehicle(APiSimVehicle* InVehicle);

    // Seçili modeli belirtilen parametrelerle araca yükler
    UFUNCTION(BlueprintCallable, Category = "PiSim|Importer")
    bool ExecuteImport(APiSimVehicle* InVehicle, const FString& FileName, float InScale = 1.0f, bool bInSmooth = true);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Importer")
    APiSimVehicle* TargetVehicle = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Importer")
    FString SelectedFileName = TEXT("robot_import_test.fbx");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Importer")
    float SelectedScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Importer")
    bool bEnableSmoothNormals = true;

    // Slate Aksiyonları
    FReply OnSelectFileClicked(FString FileName);
    FReply OnScaleSelected(float NewScale);
    FReply OnImportButtonClicked();
    FReply OnCancelButtonClicked();

protected:
    void RefreshFileList();

    TSharedPtr<SScrollBox> FileListScrollBox;
    TSharedPtr<STextBlock> SelectedFileText;
    TSharedPtr<STextBlock> ScaleStatusText;
    TSharedPtr<STextBlock> StatusMessageText;
};
