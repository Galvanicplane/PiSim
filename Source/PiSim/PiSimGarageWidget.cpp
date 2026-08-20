// PiSimGarageWidget.cpp
#include "PiSimGarageWidget.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"

bool UPiSimGarageWidget::Initialize()
{
    bool bSuccess = Super::Initialize();
    return bSuccess;
}

void UPiSimGarageWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Find APiSimGarageRobot pawn in world if not assigned
    if (!TargetGarageRobot && GetWorld())
    {
        AActor* FoundActor = UGameplayStatics::GetActorOfClass(GetWorld(), APiSimGarageRobot::StaticClass());
        if (FoundActor)
        {
            TargetGarageRobot = Cast<APiSimGarageRobot>(FoundActor);
        }
    }

    if (TargetGarageRobot)
    {
        UE_LOG(LogTemp, Log, TEXT("[UPiSimGarageWidget] Bound to APiSimGarageRobot Pawn '%s' with %d sub-meshes."),
            *TargetGarageRobot->GetName(), TargetGarageRobot->SubMeshComponents.Num());
    }
}

void UPiSimGarageWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
}

FReply UPiSimGarageWidget::NativeOnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
    FKey Key = InKeyEvent.GetKey();

    if (TargetGarageRobot && TargetGarageRobot->SubMeshComponents.Num() > 0)
    {
        if (Key == EKeys::Up)
        {
            SelectJoint((SelectedJointIndex - 1 + TargetGarageRobot->SubMeshComponents.Num()) % TargetGarageRobot->SubMeshComponents.Num());
            return FReply::Handled();
        }
        else if (Key == EKeys::Down)
        {
            SelectJoint((SelectedJointIndex + 1) % TargetGarageRobot->SubMeshComponents.Num());
            return FReply::Handled();
        }
        else if (Key == EKeys::Left)
        {
            CurrentJointAngle -= 1.0f;
            SetJointAngleDirect(CurrentJointAngle);
            return FReply::Handled();
        }
        else if (Key == EKeys::Right)
        {
            CurrentJointAngle += 1.0f;
            SetJointAngleDirect(CurrentJointAngle);
            return FReply::Handled();
        }
        else if (Key == EKeys::S)
        {
            SaveAndBroadcastConfig();
            return FReply::Handled();
        }
        else if (Key == EKeys::Enter)
        {
            if (!InputAngleBuffer.IsEmpty())
            {
                float TypedVal = FCString::Atof(*InputAngleBuffer);
                SetJointAngleDirect(TypedVal);
                InputAngleBuffer.Empty();
            }
            return FReply::Handled();
        }
        else if (Key == EKeys::BackSpace)
        {
            if (InputAngleBuffer.Len() > 0)
            {
                InputAngleBuffer.RemoveAt(InputAngleBuffer.Len() - 1);
            }
            return FReply::Handled();
        }
        else if (Key == EKeys::Hyphen || Key == EKeys::Underscore || Key == EKeys::Subtract)
        {
            if (!InputAngleBuffer.Contains(TEXT("-")))
            {
                InputAngleBuffer = TEXT("-") + InputAngleBuffer;
            }
            return FReply::Handled();
        }
        else if (Key == EKeys::Period || Key == EKeys::Decimal)
        {
            if (!InputAngleBuffer.Contains(TEXT(".")))
            {
                InputAngleBuffer += TEXT(".");
            }
            return FReply::Handled();
        }
        // ---------------------------------------------------------
        // DEVKIT NUMPAD KEYPAD CONTROLS (Numpad 0..9)
        // ---------------------------------------------------------
        if (Key == EKeys::NumPadZero || Key == EKeys::Zero)
        {
            TargetGarageRobot->ToggleDevKitServoAxis();
            return FReply::Handled();
        }
        else if (Key == EKeys::NumPadOne || Key == EKeys::One)
        {
            TargetGarageRobot->SetDevKitServoPosition(SelectedJointIndex, -1.0f);
            return FReply::Handled();
        }
        else if (Key == EKeys::NumPadTwo || Key == EKeys::Two)
        {
            TargetGarageRobot->SetDevKitServoPosition(SelectedJointIndex, 0.0f);
            return FReply::Handled();
        }
        else if (Key == EKeys::NumPadThree || Key == EKeys::Three)
        {
            TargetGarageRobot->SetDevKitServoPosition(SelectedJointIndex, 1.0f);
            return FReply::Handled();
        }
        else if (Key == EKeys::NumPadFour || Key == EKeys::Four)
        {
            TargetGarageRobot->AddDevKitRpm(SelectedJointIndex, 0, 1.0f); // X-Axis RPM +1
            return FReply::Handled();
        }
        else if (Key == EKeys::NumPadFive || Key == EKeys::Five)
        {
            TargetGarageRobot->AddDevKitRpm(SelectedJointIndex, 1, 1.0f); // Y-Axis RPM +1
            return FReply::Handled();
        }
        else if (Key == EKeys::NumPadSix || Key == EKeys::Six)
        {
            TargetGarageRobot->AddDevKitRpm(SelectedJointIndex, 2, 1.0f); // Z-Axis RPM +1
            return FReply::Handled();
        }
        else if (Key == EKeys::NumPadSeven || Key == EKeys::Seven)
        {
            TargetGarageRobot->AddDevKitForceN(SelectedJointIndex, 0, 10.0f); // X-Axis Force +10 N
            return FReply::Handled();
        }
        else if (Key == EKeys::NumPadEight || Key == EKeys::Eight)
        {
            TargetGarageRobot->AddDevKitForceN(SelectedJointIndex, 1, 10.0f); // Y-Axis Force +10 N
            return FReply::Handled();
        }
        else if (Key == EKeys::NumPadNine || Key == EKeys::Nine)
        {
            TargetGarageRobot->AddDevKitForceN(SelectedJointIndex, 2, 10.0f); // Z-Axis Force +10 N
            return FReply::Handled();
        }
        else
        {
            FString KeyStr = Key.GetDisplayName().ToString();
            if (KeyStr.Len() == 1 && FChar::IsDigit(KeyStr[0]))
            {
                InputAngleBuffer += KeyStr;
                return FReply::Handled();
            }
        }
    }

    return Super::NativeOnKeyDown(MyGeometry, InKeyEvent);
}

void UPiSimGarageWidget::SelectJoint(int32 JointIndex)
{
    if (!TargetGarageRobot || !TargetGarageRobot->SubMeshComponents.IsValidIndex(JointIndex))
    {
        return;
    }

    SelectedJointIndex = JointIndex;
    FString JointName = TargetGarageRobot->SubMeshNames.IsValidIndex(JointIndex) ? TargetGarageRobot->SubMeshNames[JointIndex] : TEXT("Unknown");

    if (TargetGarageRobot->JointLimitsList.IsValidIndex(JointIndex))
    {
        CurrentJointAngle = TargetGarageRobot->JointLimitsList[JointIndex].CurrentAngle;
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(200, 3.0f, FColor(0, 240, 255),
            FString::Printf(TEXT(">>> [GARAJ HİYERARŞİ SEÇİMİ] EKLEM [%d / %d] : %s <<<"),
                SelectedJointIndex, TargetGarageRobot->SubMeshComponents.Num() - 1, *JointName));
    }
}

void UPiSimGarageWidget::SetJointAngleDirect(float DirectAngleDegrees)
{
    if (!TargetGarageRobot)
    {
        return;
    }

    CurrentJointAngle = DirectAngleDegrees;
    TargetGarageRobot->SetJointAngleClamped(SelectedJointIndex, CurrentJointAngle, FVector(0, 1, 0));
}

void UPiSimGarageWidget::SetJointLimits(float MinAngle, float MaxAngle)
{
    if (!TargetGarageRobot || !TargetGarageRobot->JointLimitsList.IsValidIndex(SelectedJointIndex))
    {
        return;
    }

    TargetGarageRobot->JointLimitsList[SelectedJointIndex].MinAngle = MinAngle;
    TargetGarageRobot->JointLimitsList[SelectedJointIndex].MaxAngle = MaxAngle;

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Green,
            FString::Printf(TEXT(">>> [AÇI LİMİTİ KAYDEDİLDİ] [%d] AÇI SINIRLARI: [ %.1f°  -  %.1f° ] <<<"),
                SelectedJointIndex, MinAngle, MaxAngle));
    }
}

bool UPiSimGarageWidget::SaveAndBroadcastConfig()
{
    if (!TargetGarageRobot)
    {
        return false;
    }

    bool bSaved = TargetGarageRobot->SaveConfig();

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
            FString::Printf(TEXT(">>> [GARAJ CONFIG] CONFIGURASYON KAYDEDİLDİ VE PI 5'E GÖNDERİLDİ! (%s) <<<"),
                bSaved ? TEXT("BAŞARILI") : TEXT("BAŞARISIZ")));
    }

    return bSaved;
}

void UPiSimGarageWidget::AssignVirtualPortToSelectedJoint(EPiSimVirtualPortType PortType, int32 PinOrAddress)
{
    if (!TargetGarageRobot)
    {
        return;
    }

    FString JointName = TargetGarageRobot->SubMeshNames.IsValidIndex(SelectedJointIndex) ? TargetGarageRobot->SubMeshNames[SelectedJointIndex] : TEXT("Joint");

    // Update existing port assignment for this joint
    bool bFoundExisting = false;
    for (FPiSimVirtualPort& Port : TargetGarageRobot->RobotConfig.VirtualPorts)
    {
        if (Port.TargetComponent == JointName)
        {
            Port.PortID = FString::Printf(TEXT("PWM_%d"), PinOrAddress);
            Port.PortType = PortType;
            Port.PinOrAddress = PinOrAddress;
            bFoundExisting = true;
            break;
        }
    }

    if (!bFoundExisting)
    {
        FPiSimVirtualPort NewPort;
        NewPort.PortID = FString::Printf(TEXT("PWM_%d"), PinOrAddress);
        NewPort.PortType = PortType;
        NewPort.TargetComponent = JointName;
        NewPort.PinOrAddress = PinOrAddress;
        NewPort.MinValue = 1000;
        NewPort.MaxValue = 2000;

        TargetGarageRobot->RobotConfig.VirtualPorts.Add(NewPort);
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Green,
            FString::Printf(TEXT(">>> [PWM GÜNCELLENDİ] '%s' PARÇASI PWM_%d PORTUNA BAĞLANDI! <<<"),
                *JointName, PinOrAddress));
    }
}

void UPiSimGarageWidget::SwitchViewMode(EGarageViewMode NewMode)
{
    if (TargetGarageRobot)
    {
        TargetGarageRobot->SetGarageViewMode(NewMode);
    }
}

void UPiSimGarageWidget::ReimportModel()
{
    if (TargetGarageRobot)
    {
        TargetGarageRobot->ReimportCadModel();
    }
}

void UPiSimGarageWidget::SetActuatorSlider(float NewValue)
{
    if (TargetGarageRobot)
    {
        TargetGarageRobot->ApplyActuatorTestValue(SelectedActuatorIndex, NewValue);
    }
}

void UPiSimGarageWidget::ToggleSensorActive(int32 SensorIndex)
{
    if (TargetGarageRobot && TargetGarageRobot->SensorsList.IsValidIndex(SensorIndex))
    {
        TargetGarageRobot->SensorsList[SensorIndex].bSensorActive = !TargetGarageRobot->SensorsList[SensorIndex].bSensorActive;
        TargetGarageRobot->UpdateSensorRaycasts();
    }
}

void UPiSimGarageWidget::ToggleTestPanel()
{
    bIsTestPanelOpen = !bIsTestPanelOpen;
}

void UPiSimGarageWidget::SelectPhysicsTestMode(EPhysicsTestMode Mode)
{
    if (TargetGarageRobot)
    {
        TargetGarageRobot->SetPhysicsTestMode(Mode);
    }
}

void UPiSimGarageWidget::ResetTestPose()
{
    if (TargetGarageRobot)
    {
        TargetGarageRobot->ResetRobotPose();
    }
}

void UPiSimGarageWidget::AddNewMotor()
{
    if (TargetGarageRobot)
    {
        TargetGarageRobot->AddNewMotorActuator();
        SelectedActuatorIndex = TargetGarageRobot->ActuatorsList.Num() - 1;
    }
}

void UPiSimGarageWidget::BindMotorToSelectedMesh()
{
    if (TargetGarageRobot)
    {
        TargetGarageRobot->BindSelectedMotorToMesh(SelectedActuatorIndex, SelectedJointIndex);
        bIsMotorEffectModalOpen = true;
    }
}

void UPiSimGarageWidget::SetMotorPwmChannel(int32 Channel)
{
    if (TargetGarageRobot && TargetGarageRobot->ActuatorsList.IsValidIndex(SelectedActuatorIndex))
    {
        TargetGarageRobot->ActuatorsList[SelectedActuatorIndex].AssignedPwmChannel = Channel;
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green,
                FString::Printf(TEXT(">>> [MOTOR PWM KANALI ATANDI] '%s' -> PWM Kanalı: [P%d] <<<"),
                    *TargetGarageRobot->ActuatorsList[SelectedActuatorIndex].MotorName, Channel));
        }
    }
}

void UPiSimGarageWidget::SetMotorDriverProfile(const FString& ProfileName)
{
    if (TargetGarageRobot && TargetGarageRobot->ActuatorsList.IsValidIndex(SelectedActuatorIndex))
    {
        TargetGarageRobot->ActuatorsList[SelectedActuatorIndex].DriverProfileName = ProfileName;
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan,
                FString::Printf(TEXT(">>> [MOTOR SÜRÜCÜ PROFİLİ SEÇİLDİ] '%s' -> Profil: [%s] <<<"),
                    *TargetGarageRobot->ActuatorsList[SelectedActuatorIndex].MotorName, *ProfileName));
        }
    }
}

void UPiSimGarageWidget::SetCadScaleMultiplierFromUI(float NewScaleMultiplier)
{
    if (TargetGarageRobot)
    {
        TargetGarageRobot->CadUnitScaleMultiplier = NewScaleMultiplier;
        TargetGarageRobot->ReimportCadModel();
    }
}

void UPiSimGarageWidget::SetRobotActorScaleFromUI(float UniformScale)
{
    if (TargetGarageRobot)
    {
        TargetGarageRobot->SetActorScale3D(FVector(UniformScale, UniformScale, UniformScale));
    }
}

