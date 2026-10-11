// Wheel.h
// Tekerlek Mikro Modülü: Drive Wheel (Tahrikli), Steered Wheel (Direksiyonlu) ve Free Wheel (Serbest Dönen Caster) fiziğini yönetir.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "IPiSimInspectableModule.h"
#include "Wheel.generated.h"

UENUM(BlueprintType)
enum class EPiSimWheelMode : uint8
{
    DriveWheel   UMETA(DisplayName = "Tahrikli Tekerlek (Drive Wheel)"),
    SteeredWheel UMETA(DisplayName = "Direksiyonlu Tekerlek (Steered Wheel)"),
    FreeWheel    UMETA(DisplayName = "Serbest Sarhoş Tekerlek (Free Caster)")
};

UCLASS(ClassGroup=(PiSim), meta=(BlueprintSpawnableComponent))
class PISIM_API UPiSimWheelComponent : public UActorComponent, public IPiSimInspectableModule
{
    GENERATED_BODY()

public:
    UPiSimWheelComponent();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // IPiSimInspectableModule Arayüz Fonksiyonları (Otopark UI Entegrasyonu)
    virtual void GetInspectableProperties(TArray<FPiSimPropertyDescriptor>& OutProperties) override;
    virtual void SetInspectablePropertyFloat(FName PropertyId, float NewValue) override;
    virtual void SetInspectablePropertyString(FName PropertyId, const FString& NewValue) override;
    virtual void TriggerInspectableAction(FName ActionId) override;

    UFUNCTION(BlueprintCallable, Category = "PiSim|Wheel")
    void SetTargetRPM(float InRPM) { TargetRPM = InRPM; }

    UFUNCTION(BlueprintCallable, Category = "PiSim|Wheel")
    void SetSteerAngle(float InAngleDeg) { TargetSteerAngleDeg = FMath::Clamp(InAngleDeg, -MaxSteerAngleDeg, MaxSteerAngleDeg); }

    UFUNCTION(BlueprintCallable, Category = "PiSim|Wheel")
    void SetWheelMode(EPiSimWheelMode InMode) { WheelMode = InMode; }

    UFUNCTION(BlueprintCallable, Category = "PiSim|Wheel")
    void SetFromPWM(int32 InPWM, bool bIsSteering = false);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Wheel")
    FName TargetBoneName = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Wheel")
    EPiSimWheelMode WheelMode = EPiSimWheelMode::DriveWheel;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Wheel")
    float TargetRPM = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Wheel")
    float CurrentRPM = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Wheel")
    float TargetSteerAngleDeg = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Wheel")
    float CurrentSteerAngleDeg = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Wheel")
    float MaxSteerAngleDeg = 35.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Wheel")
    float RadiusCm = 15.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Wheel")
    float FrictionCoefficient = 0.85f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Wheel")
    int32 ArduPilotChannel = 3; // Gaz veya Direksiyon kanalı

    /** Manuel UI testi açıksa ağ / otopilot komutları değeri ezmez */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Wheel")
    bool bManualOverride = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Wheel")
    FVector LocalSpinAxis = FVector(0.0f, 1.0f, 0.0f); // Varsayılan Y Pitch ekseni

protected:
    UPROPERTY()
    class USkeletalMeshComponent* CachedSkeletalMesh = nullptr;

    void ApplyWheelPhysics(float DeltaTime);
};
