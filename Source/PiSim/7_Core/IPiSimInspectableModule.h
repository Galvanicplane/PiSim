// IPiSimInspectableModule.h
// Otopark / Slot arayüzü sözleşmesi. Modüller UI'a bu arayüz üzerinden bağlanır; Cast<T> gerektirmez.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "IPiSimInspectableModule.generated.h"

UENUM(BlueprintType)
enum class EPiSimPropertyType : uint8
{
    Slider          UMETA(DisplayName = "Float Slider"),
    InputText       UMETA(DisplayName = "Metin Kutusu"),
    Toggle          UMETA(DisplayName = "Aç/Kapa Anahtarı"),
    Dropdown        UMETA(DisplayName = "Açılır Liste"),
    ActionButton    UMETA(DisplayName = "Tetikleyici Buton")
};

USTRUCT(BlueprintType)
struct PISIM_API FPiSimPropertyDescriptor
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Property")
    FName PropertyId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Property")
    FString DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Property")
    EPiSimPropertyType PropertyType = EPiSimPropertyType::Slider;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Property")
    float MinValue = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Property")
    float MaxValue = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Property")
    float CurrentFloatValue = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Property")
    FString CurrentStringValue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Property")
    TArray<FString> DropdownOptions;

    /** Değer değişimi Tick ile sürekli mi yoksa tek seferlik tetikleme mi */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Property")
    bool bContinuousTickUpdate = false;
};

UINTERFACE(MinimalAPI, BlueprintType)
class UPiSimInspectableModule : public UInterface
{
    GENERATED_BODY()
};

class PISIM_API IPiSimInspectableModule
{
    GENERATED_BODY()

public:
    /** Modülün düzenlenebilir parametre listesini UI'a sunar */
    virtual void GetInspectableProperties(TArray<FPiSimPropertyDescriptor>& OutProperties) = 0;

    /** UI slider veya butonundan gelen float değer değişikliğini uygular */
    virtual void SetInspectablePropertyFloat(FName PropertyId, float NewValue) = 0;

    /** UI metin kutusundan gelen string değer değişikliğini uygular */
    virtual void SetInspectablePropertyString(FName PropertyId, const FString& NewValue) = 0;

    /** UI butonuna tıklandığında aksiyonu tetikler */
    virtual void TriggerInspectableAction(FName ActionId) = 0;
};
