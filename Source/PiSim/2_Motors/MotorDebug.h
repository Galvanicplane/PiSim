// MotorDebug.h
// Motor & Kemik Hata Ayıklama Modülü: 
// Kemik hiyerarşisini 3D debug çizgileriyle çizer, koordinat eksenlerini ve line trace ışınlarını gösterir,
// ve her kemiğe bağlı visual mesh ile UCX collision parçalarının vertex sayılarını görselleştirir.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "IPiSimInspectableModule.h"
#include "MotorDebug.generated.h"

UCLASS(ClassGroup=(PiSim), meta=(BlueprintSpawnableComponent))
class PISIM_API UPiSimMotorDebugComponent : public UActorComponent, public IPiSimInspectableModule
{
    GENERATED_BODY()

public:
    UPiSimMotorDebugComponent();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // IPiSimInspectableModule Arayüzü
    virtual void GetInspectableProperties(TArray<FPiSimPropertyDescriptor>& OutProperties) override;
    virtual void SetInspectablePropertyFloat(FName PropertyId, float NewValue) override;
    virtual void SetInspectablePropertyString(FName PropertyId, const FString& NewValue) override;
    virtual void TriggerInspectableAction(FName ActionId) override;

    /** 3D Kemik Çizgileri ve Teşhis Metinlerini Genel Aç/Kapa Anahtarı ('U' tuşu ile tetiklenir) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Debug")
    bool bEnableDebugDraw = false;

    // @state: WIP - 'U' tuşu ile kemik teşhis çizgilerini ve ekrandaki debug metinlerini açıp kapatır
    UFUNCTION(BlueprintCallable, Category = "PiSim|Debug")
    void ToggleDebugDraw();

    /** 3D Kemik Eksenlerini Göster (X: Kırmızı İleri, Y: Yeşil Sağ, Z: Mavi Yukarı) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Debug")
    bool bShowBoneAxes = false;

    /** Kemik Ebeveyn-Çocuk Bağlantı Çizgilerini Göster (Sarı Çizgiler) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Debug")
    bool bShowBoneHierarchyLines = false;

    /** Her Kemikten İleriye Doğru Kanal Bazlı Line Trace Yap ve Işın Çiz (Kanal: Visibility) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Debug")
    bool bShowLineTraces = false;

    /** Dünyada Kemik Üzerine 3D Bilgi Metni Yaz (Açı, Visual Verts, UCX Verts) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Debug")
    bool bShow3DText = false;

    /** Eksen Çizgi Uzunluğu (cm) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Debug")
    float AxisLength = 25.0f;

    /** Line Trace Mesafesi (cm) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Debug")
    float TraceDistance = 100.0f;

    /** Konsol log sıklığı kontrolü (saniye) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Debug")
    float LogIntervalSec = 1.0f;

    UFUNCTION(BlueprintCallable, Category = "PiSim|Debug")
    void DumpDiagnosticsToLog();

private:
    float LastLogTime = 0.0f;

    void DrawBoneDiagnostics();
};
