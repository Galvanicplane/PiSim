// PiSimSpectator.h
// Serbest Uçuş (Unreal Editör Kamerası), Seçili Araç Orbit ve Araç Üstü FPV Kamera Gözlemci Pawn'ı:
// - Varsayılan olarak serbest uçuş (Free-Flight Editor) modundadır; yeni araç spawn olduğunda odak çalmaz.
// - Kullanıcı sol panelden bir araç seçtiğinde 360° Orbit moduna geçer.
// - Sağ paneldeki "Araç Kamerasına Geç" tuşu ile her tick aracın kendi kamerasının transformuna kilitlenir (FPV).
// - ESC tuşuna basıldığında anında ana serbest uçuş (Editör) kamerasına geri döner.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SpectatorPawn.h"
#include "PiSimSpectator.generated.h"

class USpringArmComponent;
class UCameraComponent;
class APiSimVehicle;
class UPiSimModularGarageWidget;

UCLASS(ClassGroup = (PiSim), BlueprintType, Blueprintable)
class PISIM_API APiSimSpectatorPawn : public ASpectatorPawn
{
    GENERATED_BODY()

public:
    APiSimSpectatorPawn();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    // @state: WIP - Dashboard Unreal arayüzünü açar/kapatır (Tab tuşu)
    UFUNCTION(BlueprintCallable, Category = "PiSim|Spectator")
    void ToggleDashboardVisibility();

    // @state: WIP - Sol panelden seçilen araca 360° Orbit modunda odaklanır
    UFUNCTION(BlueprintCallable, Category = "PiSim|Spectator")
    void SetFocusedVehicle(APiSimVehicle* InVehicle);

    // @state: WIP - Seçili aracın kendi üst kamerasına (FPV) kilitlenir (Her tick aynı transform)
    UFUNCTION(BlueprintCallable, Category = "PiSim|Spectator")
    void EnterVehicleCameraMode(APiSimVehicle* InVehicle);

    // @state: WIP - Şu an odaklanılan aracı döner
    UFUNCTION(BlueprintPure, Category = "PiSim|Spectator")
    APiSimVehicle* GetFocusedVehicle() const { return FocusedVehicle; }

    // @state: WIP - Sahnedeki bir sonraki araca odaklanır (Filo gezinme)
    UFUNCTION(BlueprintCallable, Category = "PiSim|Spectator")
    void FocusNextVehicle();

    // @state: WIP - Seçili araca kilitlenme modunu açar/kapatır
    UFUNCTION(BlueprintCallable, Category = "PiSim|Spectator")
    void ToggleOrbitMode();

    // @state: WIP - Serbest editör uçuş moduna geri döner (ESC)
    UFUNCTION(BlueprintCallable, Category = "PiSim|Spectator")
    void ReturnToFreeFlight();

    // @state: WIP - Orbit mesafesini veya uçuş hızını ayarlar (MouseWheel)
    UFUNCTION(BlueprintCallable, Category = "PiSim|Spectator")
    void AddZoomDelta(float Direction);

    // @state: WIP - Orbit veya serbest bakış açısını ayarlar
    UFUNCTION(BlueprintCallable, Category = "PiSim|Spectator")
    void AddOrbitDelta(float DeltaX, float DeltaY);

    /** Kamera yay kolu */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Camera")
    USpringArmComponent* SpectatorSpringArm;

    /** Gözlemci kamerası */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Camera")
    UCameraComponent* SpectatorCamera;

    /** Spectator'a ait ve tüm filoyu yöneten Dashboard Unreal arayüzü */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|UI")
    UPiSimModularGarageWidget* DashboardWidget = nullptr;

    /** İzlenen / Odaklanılan araç referansı */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Spectator")
    APiSimVehicle* FocusedVehicle = nullptr;

    /** Şu an araca kilitli orbit modunda mıyız? (false ise serbest noclip editör uçuşu) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Spectator")
    bool bIsOrbitingVehicle = false;

    /** Şu an aracın kendi kamerasının (FPV) içinde miyiz? (Live Coding sizeof korumalı) */
    bool IsFpvCameraMode() const;
    void SetFpvCameraMode(bool bEnable);

    /** Hedef araca olan orbit mesafesi (cm) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Spectator")
    float OrbitDistance = 600.0f;

    /** Minimum ve maksimum zoom mesafeleri */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Spectator")
    float MinOrbitDistance = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Spectator")
    float MaxOrbitDistance = 5000.0f;

    /** Serbest editör uçuş hızı (cm/s) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Spectator")
    float FlySpeedMultiplier = 2200.0f;

protected:
    FRotator CurrentOrbitRotation;
    FVector OrbitTargetOffset = FVector(0.0f, 0.0f, 50.0f);

    void HandleMoveForward(float Val);
    void HandleMoveRight(float Val);
    void HandleMoveUp(float Val);
    void HandleTurn(float Val);
    void HandleLookUp(float Val);
    void HandleKeyFocus();
    void HandleKeyFreeFlight();
    void HandleKeyTab();
};
