// PiSimSpectator.h
// Serbest Uçuş ve Filo Gözlemci Pawn'ı (Spectator):
// Kullanıcıyı tek bir araca hapsetmeden 3D dünyada serbest dolaşım (WASD + Mouse) sağlar.
// İstenen araca (FocusedVehicle) kilitlenerek 360° orbit/takip yapabilir.

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

    // @state: WIP - Odaklanılan aracı günceller
    UFUNCTION(BlueprintCallable, Category = "PiSim|Spectator")
    void SetFocusedVehicle(APiSimVehicle* InVehicle);

    // @state: WIP - Şu an odaklanılan aracı döner
    UFUNCTION(BlueprintPure, Category = "PiSim|Spectator")
    APiSimVehicle* GetFocusedVehicle() const { return FocusedVehicle; }

    // @state: WIP - Sahnedeki bir sonraki araca odaklanır (Filo gezinme)
    UFUNCTION(BlueprintCallable, Category = "PiSim|Spectator")
    void FocusNextVehicle();

    // @state: WIP - Seçili araca kilitlenme modunu açar/kapatır
    UFUNCTION(BlueprintCallable, Category = "PiSim|Spectator")
    void ToggleOrbitMode();

    // @state: WIP - Serbest uçuş moduna geri döner
    UFUNCTION(BlueprintCallable, Category = "PiSim|Spectator")
    void ReturnToFreeFlight();

    // @state: WIP - Orbit mesafesini ayarlar (Zoom)
    UFUNCTION(BlueprintCallable, Category = "PiSim|Spectator")
    void AddZoomDelta(float Direction);

    // @state: WIP - Orbit açısını ayarlar
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

    /** Şu an araca kilitli orbit modunda mıyız? (false ise serbest noclip uçuş) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Spectator")
    bool bIsOrbitingVehicle = false;

    /** Hedef araca olan orbit mesafesi (cm) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Spectator")
    float OrbitDistance = 600.0f;

    /** Minimum ve maksimum zoom mesafeleri */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Spectator")
    float MinOrbitDistance = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Spectator")
    float MaxOrbitDistance = 5000.0f;

    /** Serbest uçuş hızı çarpanı */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Spectator")
    float FlySpeedMultiplier = 2500.0f;

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
