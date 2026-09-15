// PiSimAeroWingComponent.h
// Modular Aerodynamic Force Body (Wing / Lifting Surface Component)
// Placed at bone socket coordinates (e.g. B_Wing_R, B_Wing_L) to calculate and apply
// dynamic lift and drag forces using the Khan & Nahon model.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "PiSimAeroWingComponent.generated.h"

class UPiSimActuatorComponent;

UCLASS(ClassGroup=(PiSim), meta=(BlueprintSpawnableComponent))
class PISIM_API UPiSimAeroWingComponent : public USceneComponent
{
    GENERATED_BODY()

public:
    UPiSimAeroWingComponent();

    // --- Aerodynamic Profile Parameters ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Aero")
    FString WingName = TEXT("MainWing");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Aero")
    float WingArea = 0.13f; // Yarı kanat için genelde 0.13 m² (Toplam 0.26 m²)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Aero")
    float Wingspan = 0.575f; // Yarı kanat açıklığı (m)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Aero")
    float CL0 = 0.18f; // Sıfır hücum açısı kaldırma katsayısı

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Aero")
    float CLAlpha = 5.5f; // Kaldırma eğimi (rad⁻¹)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Aero")
    float CD0 = 0.020f; // Sıfır kaldırma sürükleme katsayısı

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Aero")
    float StallAngleDeg = 15.0f; // Perdövites açısı

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Aero")
    float ElevonEffectiveness = 0.50f; // Elevon kontrol etkinliği

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Aero")
    bool bIsRightWing = false;

    // --- Live Inputs ---
    /** Bağlı kontrol yüzeyinin sapma açısı (derece) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Aero|Live")
    float ControlSurfaceDeflectionDeg = 0.0f;

    // --- Live Telemetry State ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Aero|State")
    float AirspeedKmh = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Aero|State")
    float AirspeedMs = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Aero|State")
    float AngleOfAttackDeg = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Aero|State")
    float CurrentLiftN = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Aero|State")
    float CurrentDragN = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Aero|State")
    FVector LastAppliedForceWorld = FVector::ZeroVector;

    // --- Aerodynamic Calculation & Force Application ---
    /** Kök fizik gövdesine tam bu kanat kemiği konumunda aerodinamik kuvvet uygular */
    void CalculateAndApplyAeroForces(float DeltaTime, UPrimitiveComponent* RootBody, float AirDensityKgM3 = 1.225f);
};
