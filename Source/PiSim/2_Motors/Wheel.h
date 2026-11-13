// Wheel.h
// Tekerlek Mikro Modülü: Drive Wheel (Tahrikli), Steered Wheel (Direksiyonlu) ve Free Wheel (Serbest Dönen Caster) fiziğini yönetir.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "IPiSimInspectableModule.h"
#include "IPiSimActuatorInterface.h"
#include "Wheel.generated.h"

UENUM(BlueprintType)
enum class EPiSimWheelMode : uint8
{
    DriveWheel   UMETA(DisplayName = "Tahrikli Tekerlek (Drive Wheel)"),
    FreeWheel    UMETA(DisplayName = "Serbest Sarhoş Tekerlek (Free Caster)")
};

UCLASS(ClassGroup=(PiSim), meta=(BlueprintSpawnableComponent))
class PISIM_API UPiSimWheelComponent : public UActorComponent, public IPiSimInspectableModule, public IPiSimActuatorInterface
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

    // IPiSimActuatorInterface
    virtual void SetActuatorNormalized(float Value) override;
    virtual void SetActuatorDirect(float PhysicalValue) override { SetTargetRPM(PhysicalValue); }
    virtual void SetActuatorPWM(int32 InPWM) override { SetFromPWM(InPWM); }
    virtual float GetActuatorNormalized() const override { return TargetRPM / 250.0f; }
    virtual FName GetTargetBoneName() const override { return TargetBoneName; }
    virtual void SetTargetBoneName(FName InBoneName) override { TargetBoneName = InBoneName; }
    virtual FString GetActuatorType() const override { return TEXT("Wheel"); }
    virtual int32 GetArduPilotChannel() const override { return ArduPilotChannel; }
    virtual void SetArduPilotChannel(int32 InChannel) override { ArduPilotChannel = InChannel; ServoChannel = InChannel; }
    virtual FVector GetActuatorAxis() const override { return LocalSpinAxis; }
    virtual void SetActuatorAxis(const FVector& InAxis) override { LocalSpinAxis = InAxis; }
    virtual void InitializeModuleIdentity(class APiSimVehicle* InVehicle) override;
    virtual void HandleAckMessage(const FString& TopicName, int32 AckCode) override {}

    // @state: WIP - Modül içi araç kimliği ve kanal testi değişkenleri
    UPROPERTY()
    class APiSimVehicle* ParentVehicle = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Identity")
    FString ParentVehicleName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Identity")
    int32 VehicleIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Identity")
    int32 ServoChannel = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Identity")
    float ChannelTestSlider = 0.0f; // -1.0 .. +1.0

    UFUNCTION(BlueprintCallable, Category = "PiSim|Wheel")
    void SetTargetRPM(float InRPM) { TargetRPM = FMath::Clamp(InRPM, -MaxRPM, MaxRPM); }

    UFUNCTION(BlueprintCallable, Category = "PiSim|Wheel")
    void SetSteerAngle(float InAngleDeg) { TargetSteerAngleDeg = FMath::Clamp(InAngleDeg, -MaxSteerAngleDeg, MaxSteerAngleDeg); }

    UFUNCTION(BlueprintCallable, Category = "PiSim|Wheel")
    void SetWheelMode(EPiSimWheelMode InMode) { WheelMode = InMode; }

    UFUNCTION(BlueprintCallable, Category = "PiSim|Wheel")
    void ApplyFrictionToCollisionMesh();

    UFUNCTION(BlueprintCallable, Category = "PiSim|Wheel")
    void SetFromPWM(int32 InPWM, bool bIsSteering = false);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Wheel")
    FName TargetBoneName = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Wheel")
    EPiSimWheelMode WheelMode = EPiSimWheelMode::DriveWheel;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Wheel")
    bool bReverseDirection = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Wheel")
    float DriveTorque = 120.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Wheel")
    float MaxRPM = 1200.0f;

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
    int32 ArduPilotChannel = 1; // Kanal 1: Sürüş Tekerlekleri (İleri / Geri Gaz)

    /** Manuel UI testi açıksa ağ / otopilot komutları değeri ezmez */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Wheel")
    bool bManualOverride = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Wheel")
    FVector LocalSpinAxis = FVector(0.0f, 1.0f, 0.0f); // Roll X ekseni (PiSimModelImporter LocalAxle=(1,0,0) birebir)

protected:
    UPROPERTY()
    class USkeletalMeshComponent* CachedSkeletalMesh = nullptr;

    UPROPERTY()
    class USceneComponent* TargetSceneComponent = nullptr;

    UPROPERTY()
    class UPhysicalMaterial* DynamicWheelPhysMat = nullptr;

    float LastAppliedFriction = -1.0f;

    void ApplyWheelPhysics(float DeltaTime);
};
