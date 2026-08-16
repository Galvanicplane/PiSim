// PiSimGarageWidget.h
// Interactive Visual Garage Editor Panel Widget for PiSim Platform.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PiSimGarageRobot.h"
#include "PiSimGarageWidget.generated.h"

UCLASS()
class PISIM_API UPiSimGarageWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual bool Initialize() override;
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual FReply NativeOnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

    /** Target Garage Robot Pawn bound to this widget UI */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|UI")
    APiSimGarageRobot* TargetGarageRobot = nullptr;

    /** Currently selected sub-mesh joint index */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|UI")
    int32 SelectedJointIndex = 0;

    /** Currently set joint angle in degrees */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|UI")
    float CurrentJointAngle = 0.0f;

    /** Select joint sub-mesh by index */
    UFUNCTION(BlueprintCallable, Category = "PiSim|UI")
    void SelectJoint(int32 JointIndex);

    /** Update selected joint angle directly with exact float value */
    UFUNCTION(BlueprintCallable, Category = "PiSim|UI")
    void SetJointAngleDirect(float DirectAngleDegrees);

    /** Set Min/Max angle limits for selected joint */
    UFUNCTION(BlueprintCallable, Category = "PiSim|UI")
    void SetJointLimits(float MinAngle, float MaxAngle);

    /** Save robot_config.json and broadcast over UDP Port 7402 to Pi 5 */
    UFUNCTION(BlueprintCallable, Category = "PiSim|UI")
    bool SaveAndBroadcastConfig();

    /** Typed exact angle string buffer */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|UI")
    FString InputAngleBuffer = TEXT("0.0");

    /** Assign virtual port binding to selected joint */
    UFUNCTION(BlueprintCallable, Category = "PiSim|UI")
    void AssignVirtualPortToSelectedJoint(EPiSimVirtualPortType PortType, int32 PinOrAddress);

    /** Currently selected actuator index */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|UI")
    int32 SelectedActuatorIndex = 0;

    /** Currently active motor test state (-1 for Min, 0 for Zero, 1 for Max) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|UI")
    int32 ActiveMotorTestState = 0;

    /** Is motor currently running full range sweep animation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|UI")
    bool bIsSweepingMotor = false;

    /** Timer for motor sweep animation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|UI")
    float SweepMotorTimer = 0.0f;

    /** Set of parent joint indices whose children are currently collapsed in the Outliner tree */
    TSet<int32> CollapsedParentIndices;

    /** Currently selected sensor index */

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|UI")
    int32 SelectedSensorIndex = 0;

    /** Is Physics Test Panel Open */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|UI")
    bool bIsTestPanelOpen = false;

    /** Switch active view mode (Visual, Structural, Sensor) */
    UFUNCTION(BlueprintCallable, Category = "PiSim|UI")
    void SwitchViewMode(EGarageViewMode NewMode);

    /** Re-import CAD model and refresh classification */
    UFUNCTION(BlueprintCallable, Category = "PiSim|UI")
    void ReimportModel();

    /** Adjust test slider (-1.0 to 1.0) on active actuator */
    UFUNCTION(BlueprintCallable, Category = "PiSim|UI")
    void SetActuatorSlider(float NewValue);

    /** Toggle Sensor ON/OFF active status */
    UFUNCTION(BlueprintCallable, Category = "PiSim|UI")
    void ToggleSensorActive(int32 SensorIndex);

    /** Toggle Physics Test Mode panel visibility */
    UFUNCTION(BlueprintCallable, Category = "PiSim|UI")
    void ToggleTestPanel();

    /** Select physical test mode (HoldInAir, LockRotation, FreeSim) */
    UFUNCTION(BlueprintCallable, Category = "PiSim|UI")
    void SelectPhysicsTestMode(EPhysicsTestMode Mode);

    /** Reset robot transform to spawn pose */
    UFUNCTION(BlueprintCallable, Category = "PiSim|UI")
    void ResetTestPose();

    /** Is Motor Effect & Binding Modal Panel Open */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|UI")
    bool bIsMotorEffectModalOpen = false;

    /** Add a new independent motor to the robot's actuators list */
    UFUNCTION(BlueprintCallable, Category = "PiSim|UI")
    void AddNewMotor();

    /** Bind currently selected motor to currently selected mesh in hierarchy */
    UFUNCTION(BlueprintCallable, Category = "PiSim|UI")
    void BindMotorToSelectedMesh();

    /** Set PWM channel for currently selected motor */
    UFUNCTION(BlueprintCallable, Category = "PiSim|UI")
    void SetMotorPwmChannel(int32 Channel);

    /** Set driver profile name for currently selected motor */
    UFUNCTION(BlueprintCallable, Category = "PiSim|UI")
    void SetMotorDriverProfile(const FString& ProfileName);

    /** Update CAD Unit Scale Multiplier and reimport model (BlueprintCallable for UI Scale Buttons) */
    UFUNCTION(BlueprintCallable, Category = "PiSim|UI")
    void SetCadScaleMultiplierFromUI(float NewScaleMultiplier);

    /** Scale robot actor overall transform (BlueprintCallable for UI Scale Buttons) */
    UFUNCTION(BlueprintCallable, Category = "PiSim|UI")
    void SetRobotActorScaleFromUI(float UniformScale);
};
