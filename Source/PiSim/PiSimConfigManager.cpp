// PiSimConfigManager.cpp
#include "PiSimConfigManager.h"
#include "JsonObjectConverter.h"

FString UPiSimConfigManager::GetDefaultConfigPath()
{
    return FPaths::ProjectSavedDir() / TEXT("Robots/Config/robot_config.json");
}

bool UPiSimConfigManager::SaveRobotConfigToFile(const FPiSimRobotConfig& Config, const FString& FilePath)
{
    FString TargetPath = FilePath.IsEmpty() ? GetDefaultConfigPath() : FilePath;
    FString JsonStr = RobotConfigToJsonString(Config);

    if (JsonStr.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("[UPiSimConfigManager] Failed to serialize RobotConfig to JSON."));
        return false;
    }

    bool bSuccess = FFileHelper::SaveStringToFile(JsonStr, *TargetPath);
    if (bSuccess)
    {
        UE_LOG(LogTemp, Log, TEXT("[UPiSimConfigManager] Successfully saved robot config to: %s"), *TargetPath);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[UPiSimConfigManager] Failed to save robot config file to: %s"), *TargetPath);
    }

    return bSuccess;
}

bool UPiSimConfigManager::LoadRobotConfigFromFile(const FString& FilePath, FPiSimRobotConfig& OutConfig)
{
    FString TargetPath = FilePath.IsEmpty() ? GetDefaultConfigPath() : FilePath;
    
    if (!FPaths::FileExists(TargetPath))
    {
        UE_LOG(LogTemp, Warning, TEXT("[UPiSimConfigManager] Config file does not exist: %s. Creating default."), *TargetPath);
        
        // Generate and save default config
        OutConfig = FPiSimRobotConfig();
        
        FPiSimVirtualPort Pwm0;
        Pwm0.PortID = TEXT("PWM_0");
        Pwm0.PortType = EPiSimVirtualPortType::PWM;
        Pwm0.TargetComponent = TEXT("LeftWheelMotor");
        Pwm0.PinOrAddress = 18;
        Pwm0.MinValue = 1000;
        Pwm0.MaxValue = 2000;

        FPiSimVirtualPort Pwm1;
        Pwm1.PortID = TEXT("PWM_1");
        Pwm1.PortType = EPiSimVirtualPortType::PWM;
        Pwm1.TargetComponent = TEXT("RightWheelMotor");
        Pwm1.PinOrAddress = 19;
        Pwm1.MinValue = 1000;
        Pwm1.MaxValue = 2000;

        FPiSimVirtualPort I2c1;
        I2c1.PortID = TEXT("I2C_1");
        I2c1.PortType = EPiSimVirtualPortType::I2C;
        I2c1.TargetComponent = TEXT("ImuSensor_MPU6050");
        I2c1.PinOrAddress = 0x68;

        FPiSimVirtualPort Cam0;
        Cam0.PortID = TEXT("CAM_0");
        Cam0.PortType = EPiSimVirtualPortType::CAMERA;
        Cam0.TargetComponent = TEXT("FpvCameraCapture");
        Cam0.PinOrAddress = 5000;

        OutConfig.VirtualPorts.Add(Pwm0);
        OutConfig.VirtualPorts.Add(Pwm1);
        OutConfig.VirtualPorts.Add(I2c1);
        OutConfig.VirtualPorts.Add(Cam0);

        SaveRobotConfigToFile(OutConfig, TargetPath);
        return true;
    }

    FString JsonStr;
    if (!FFileHelper::LoadFileToString(JsonStr, *TargetPath))
    {
        UE_LOG(LogTemp, Error, TEXT("[UPiSimConfigManager] Failed to read config file: %s"), *TargetPath);
        return false;
    }

    return JsonStringToRobotConfig(JsonStr, OutConfig);
}

FString UPiSimConfigManager::RobotConfigToJsonString(const FPiSimRobotConfig& Config)
{
    TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject());
    RootObject->SetStringField(TEXT("robot_name"), Config.RobotName);
    RootObject->SetStringField(TEXT("glb_mesh_path"), Config.GlbMeshPath);
    RootObject->SetNumberField(TEXT("mass_kg"), Config.MassKg);

    TArray<TSharedPtr<FJsonValue>> PortsArray;
    for (const FPiSimVirtualPort& Port : Config.VirtualPorts)
    {
        TSharedPtr<FJsonObject> PortObj = MakeShareable(new FJsonObject());
        PortObj->SetStringField(TEXT("port_id"), Port.PortID);
        
        FString PortTypeStr = UEnum::GetValueAsString(Port.PortType);
        PortObj->SetStringField(TEXT("port_type"), PortTypeStr);
        PortObj->SetStringField(TEXT("target_component"), Port.TargetComponent);
        PortObj->SetNumberField(TEXT("pin_or_address"), Port.PinOrAddress);
        PortObj->SetNumberField(TEXT("min_value"), Port.MinValue);
        PortObj->SetNumberField(TEXT("max_value"), Port.MaxValue);

        PortsArray.Add(MakeShareable(new FJsonValueObject(PortObj)));
    }

    RootObject->SetArrayField(TEXT("virtual_ports"), PortsArray);

    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    if (FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer))
    {
        return OutputString;
    }

    return TEXT("");
}

bool UPiSimConfigManager::JsonStringToRobotConfig(const FString& JsonStr, FPiSimRobotConfig& OutConfig)
{
    TSharedPtr<FJsonObject> RootObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);

    if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("[UPiSimConfigManager] Failed to parse JSON string."));
        return false;
    }

    OutConfig.RobotName = RootObject->GetStringField(TEXT("robot_name"));
    OutConfig.GlbMeshPath = RootObject->GetStringField(TEXT("glb_mesh_path"));
    OutConfig.MassKg = static_cast<float>(RootObject->GetNumberField(TEXT("mass_kg")));

    OutConfig.VirtualPorts.Empty();
    const TArray<TSharedPtr<FJsonValue>>* PortsArray = nullptr;
    if (RootObject->TryGetArrayField(TEXT("virtual_ports"), PortsArray) && PortsArray)
    {
        for (const TSharedPtr<FJsonValue>& Val : *PortsArray)
        {
            TSharedPtr<FJsonObject> PortObj = Val->AsObject();
            if (!PortObj.IsValid()) continue;

            FPiSimVirtualPort Port;
            Port.PortID = PortObj->GetStringField(TEXT("port_id"));
            Port.TargetComponent = PortObj->GetStringField(TEXT("target_component"));
            Port.PinOrAddress = static_cast<int32>(PortObj->GetNumberField(TEXT("pin_or_address")));
            Port.MinValue = static_cast<int32>(PortObj->GetNumberField(TEXT("min_value")));
            Port.MaxValue = static_cast<int32>(PortObj->GetNumberField(TEXT("max_value")));

            FString PortTypeStr = PortObj->GetStringField(TEXT("port_type"));
            if (PortTypeStr.Contains(TEXT("PWM"))) Port.PortType = EPiSimVirtualPortType::PWM;
            else if (PortTypeStr.Contains(TEXT("I2C"))) Port.PortType = EPiSimVirtualPortType::I2C;
            else if (PortTypeStr.Contains(TEXT("SPI"))) Port.PortType = EPiSimVirtualPortType::SPI;
            else if (PortTypeStr.Contains(TEXT("UART"))) Port.PortType = EPiSimVirtualPortType::UART;
            else if (PortTypeStr.Contains(TEXT("CAMERA"))) Port.PortType = EPiSimVirtualPortType::CAMERA;
            else Port.PortType = EPiSimVirtualPortType::GPIO;

            OutConfig.VirtualPorts.Add(Port);
        }
    }

    return true;
}
