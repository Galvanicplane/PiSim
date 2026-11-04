// IPiSimActuatorInterface.h
// PiSim Eyleyici Arayüzü: Tekerlek, Servo, Pervane ve Pistonların ortak sözleşmesi.
// Araç ve kontrolcüler modülleri Cast<T> yapmadan doğrudan bu arayüz üzerinden sürer.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "IPiSimActuatorInterface.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class UPiSimActuatorInterface : public UInterface
{
    GENERATED_BODY()
};

class PISIM_API IPiSimActuatorInterface
{
    GENERATED_BODY()

public:
    /** Eyleyiciye normalize edilmiş kontrol sinyali gönderir (-1.0 .. +1.0) */
    virtual void SetActuatorNormalized(float Value) = 0;

    /** Eyleyiciye doğrudan fiziksel hedef değeri verir (Tekerlek için RPM, Servo için Derece, Piston için cm) */
    virtual void SetActuatorDirect(float PhysicalValue) = 0;

    /** Standart RC / ArduPilot PWM sinyali ile sürer (1000 .. 2000 us) */
    virtual void SetActuatorPWM(int32 InPWM) = 0;

    /** Mevcut normalize edilmiş durum/çıkış değerini döndürür (-1.0 .. +1.0) */
    virtual float GetActuatorNormalized() const = 0;

    /** Bağlı olduğu hedef kemik veya parça adını döndürür */
    virtual FName GetTargetBoneName() const = 0;
    virtual void SetTargetBoneName(FName InBoneName) = 0;

    /** Eyleyici türünü döndürür ("Wheel", "Servo", "Propeller", "Actuator") */
    virtual FString GetActuatorType() const = 0;

    /** ArduPilot Kanal Numarasını döndürür (1 .. 16) */
    virtual int32 GetArduPilotChannel() const = 0;
    virtual void SetArduPilotChannel(int32 InChannel) = 0;

    /** Eyleyicinin yerel fiziksel eksenini döndürür */
    virtual FVector GetActuatorAxis() const = 0;
    virtual void SetActuatorAxis(const FVector& InAxis) = 0;
};
