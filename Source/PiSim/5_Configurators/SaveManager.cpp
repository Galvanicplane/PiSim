// SaveManager.cpp

#include "SaveManager.h"
#include "Vehicle.h"
#include "Wheel.h"
#include "Servo.h"
#include "Propeller.h"
#include "LiftBody.h"
#include "MotorConfigurator.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// @state: WIP - Kayıt yöneticisi yapıcısı
UPiSimSaveManager::UPiSimSaveManager()
{
}

// @state: WIP - Aracı verilen profil ismiyle JSON dosyasına kaydeder
bool UPiSimSaveManager::SaveProfile(APiSimVehicle* TargetVehicle, const FString& ProfileName, FString& OutMessage)
{
    if (!TargetVehicle || ProfileName.IsEmpty())
    {
        OutMessage = TEXT("Hata: Geçersiz araç veya boş profil ismi.");
        return false;
    }

    TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject());
    RootObject->SetStringField(TEXT("ProfileName"), ProfileName);
    RootObject->SetNumberField(TEXT("TotalMassKg"), TargetVehicle->TotalMassKg);

    TArray<TSharedPtr<FJsonValue>> ModulesArray;

    TArray<UActorComponent*> Comps;
    TargetVehicle->GetComponents(Comps);

    for (UActorComponent* Comp : Comps)
    {
        TSharedPtr<FJsonObject> ModObj = MakeShareable(new FJsonObject());

        if (UPiSimWheelComponent* Wheel = Cast<UPiSimWheelComponent>(Comp))
        {
            ModObj->SetStringField(TEXT("Type"), TEXT("Wheel"));
            ModObj->SetStringField(TEXT("BoneName"), Wheel->TargetBoneName.ToString());
            ModObj->SetNumberField(TEXT("RadiusCm"), Wheel->RadiusCm);
            ModObj->SetNumberField(TEXT("Friction"), Wheel->FrictionCoefficient);
            ModulesArray.Add(MakeShareable(new FJsonValueObject(ModObj)));
        }
        else if (UPiSimServoComponent* Servo = Cast<UPiSimServoComponent>(Comp))
        {
            ModObj->SetStringField(TEXT("Type"), TEXT("Servo"));
            ModObj->SetStringField(TEXT("BoneName"), Servo->TargetBoneName.ToString());
            ModObj->SetNumberField(TEXT("MinAngle"), Servo->MinAngleDeg);
            ModObj->SetNumberField(TEXT("MaxAngle"), Servo->MaxAngleDeg);
            ModObj->SetNumberField(TEXT("Channel"), Servo->ArduPilotChannel);
            ModObj->SetStringField(TEXT("Function"), Servo->ArduPilotFunction);
            ModulesArray.Add(MakeShareable(new FJsonValueObject(ModObj)));
        }
        else if (UPiSimPropellerComponent* Prop = Cast<UPiSimPropellerComponent>(Comp))
        {
            ModObj->SetStringField(TEXT("Type"), TEXT("Propeller"));
            ModObj->SetStringField(TEXT("BoneName"), Prop->TargetBoneName.ToString());
            ModObj->SetNumberField(TEXT("MaxThrust"), Prop->MaxThrustNewtons);
            ModObj->SetNumberField(TEXT("Channel"), Prop->ArduPilotChannel);
            ModulesArray.Add(MakeShareable(new FJsonValueObject(ModObj)));
        }
        else if (UPiSimLiftBodyComponent* Lift = Cast<UPiSimLiftBodyComponent>(Comp))
        {
            ModObj->SetStringField(TEXT("Type"), TEXT("LiftBody"));
            ModObj->SetStringField(TEXT("BoneName"), Lift->TargetBoneName.ToString());
            ModObj->SetNumberField(TEXT("WingArea"), Lift->WingAreaSquareMeters);
            ModulesArray.Add(MakeShareable(new FJsonValueObject(ModObj)));
        }
    }

    RootObject->SetArrayField(TEXT("Modules"), ModulesArray);

    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);

    FString SaveDir = FPaths::ProjectSavedDir() / TEXT("Vehicles");
    IFileManager::Get().MakeDirectory(*SaveDir, true);
    FString FilePath = SaveDir / (ProfileName + TEXT(".json"));

    if (FFileHelper::SaveStringToFile(OutputString, *FilePath))
    {
        OutMessage = FString::Printf(TEXT("Profil başarıyla kaydedildi: %s"), *FilePath);
        return true;
    }

    OutMessage = TEXT("Hata: Dosya diske yazılamadı.");
    return false;
}

// @state: WIP - Diskteki JSON profilini okuyarak aracı yeniden yapılandırır
bool UPiSimSaveManager::LoadProfile(APiSimVehicle* TargetVehicle, const FString& ProfileName, FString& OutMessage)
{
    if (!TargetVehicle || ProfileName.IsEmpty())
    {
        OutMessage = TEXT("Hata: Geçersiz araç veya boş profil ismi.");
        return false;
    }

    FString FilePath = FPaths::ProjectSavedDir() / TEXT("Vehicles") / (ProfileName + TEXT(".json"));
    FString JsonString;
    if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
    {
        OutMessage = FString::Printf(TEXT("Hata: Profil dosyası bulunamadı: %s"), *FilePath);
        return false;
    }

    TSharedPtr<FJsonObject> RootObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
    {
        OutMessage = TEXT("Hata: JSON dosyası çözümlenemedi.");
        return false;
    }

    TargetVehicle->TotalMassKg = RootObject->GetNumberField(TEXT("TotalMassKg"));

    const TArray<TSharedPtr<FJsonValue>>* ModulesArray;
    if (RootObject->TryGetArrayField(TEXT("Modules"), ModulesArray))
    {
        for (const TSharedPtr<FJsonValue>& Val : *ModulesArray)
        {
            TSharedPtr<FJsonObject> ModObj = Val->AsObject();
            if (!ModObj.IsValid()) continue;

            FString Type = ModObj->GetStringField(TEXT("Type"));
            FName BoneName = FName(*ModObj->GetStringField(TEXT("BoneName")));

            UActorComponent* NewComp = UPiSimMotorConfigurator::AssignMotorRoleToBone(TargetVehicle, BoneName, Type);

            if (UPiSimWheelComponent* Wheel = Cast<UPiSimWheelComponent>(NewComp))
            {
                Wheel->RadiusCm = ModObj->GetNumberField(TEXT("RadiusCm"));
                Wheel->FrictionCoefficient = ModObj->GetNumberField(TEXT("Friction"));
            }
            else if (UPiSimServoComponent* Servo = Cast<UPiSimServoComponent>(NewComp))
            {
                Servo->MinAngleDeg = ModObj->GetNumberField(TEXT("MinAngle"));
                Servo->MaxAngleDeg = ModObj->GetNumberField(TEXT("MaxAngle"));
                Servo->ArduPilotChannel = ModObj->GetIntegerField(TEXT("Channel"));
                Servo->ArduPilotFunction = ModObj->GetStringField(TEXT("Function"));
            }
            else if (UPiSimPropellerComponent* Prop = Cast<UPiSimPropellerComponent>(NewComp))
            {
                Prop->MaxThrustNewtons = ModObj->GetNumberField(TEXT("MaxThrust"));
                Prop->ArduPilotChannel = ModObj->GetIntegerField(TEXT("Channel"));
            }
            else if (UPiSimLiftBodyComponent* Lift = Cast<UPiSimLiftBodyComponent>(NewComp))
            {
                Lift->WingAreaSquareMeters = ModObj->GetNumberField(TEXT("WingArea"));
            }
        }
    }

    OutMessage = FString::Printf(TEXT("Profil başarıyla yüklendi: %s"), *ProfileName);
    return true;
}

// @state: WIP - Saved/Vehicles klasöründeki tüm JSON profil dosyalarını listeler
void UPiSimSaveManager::GetSavedProfileNames(TArray<FString>& OutProfileNames)
{
    OutProfileNames.Empty();
    FString SaveDir = FPaths::ProjectSavedDir() / TEXT("Vehicles");

    TArray<FString> FoundFiles;
    IFileManager::Get().FindFiles(FoundFiles, *SaveDir, TEXT("json"));

    for (const FString& FileName : FoundFiles)
    {
        OutProfileNames.Add(FPaths::GetBaseFilename(FileName));
    }
}
