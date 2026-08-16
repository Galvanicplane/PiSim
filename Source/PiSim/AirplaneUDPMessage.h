// Airplane-specific UDP message definition for PiSim / Raspberry Pi interoperability.
#pragma once

#include "CoreMinimal.h"
#include "AirplaneUDPMessage.generated.h"

USTRUCT(BlueprintType)
struct PISIM_API FAirplaneUDPMessage
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "UDP")
    FString MessageType = TEXT("AirplaneTelemetry");

    UPROPERTY(BlueprintReadWrite, Category = "UDP")
    FVector Location = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "UDP")
    float Altitude = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "UDP")
    float Airspeed = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "UDP")
    FString Payload = TEXT("");

    bool ToBinary(TArray<uint8>& OutBytes) const
    {
        OutBytes.Empty();

        FTCHARToUTF8 MessageTypeUtf8(*MessageType);
        FTCHARToUTF8 PayloadUtf8(*Payload);

        auto AppendUint32 = [&OutBytes](uint32 Value)
        {
            int32 Start = OutBytes.AddUninitialized(sizeof(uint32));
            FMemory::Memcpy(OutBytes.GetData() + Start, &Value, sizeof(uint32));
        };

        auto AppendFloat = [&OutBytes](float Value)
        {
            int32 Start = OutBytes.AddUninitialized(sizeof(float));
            FMemory::Memcpy(OutBytes.GetData() + Start, &Value, sizeof(float));
        };

        AppendUint32(MessageTypeUtf8.Length());
        if (MessageTypeUtf8.Length() > 0)
        {
            OutBytes.Append(reinterpret_cast<const uint8*>(MessageTypeUtf8.Get()), MessageTypeUtf8.Length());
        }

        AppendFloat(Location.X);
        AppendFloat(Location.Y);
        AppendFloat(Location.Z);
        AppendFloat(Altitude);
        AppendFloat(Airspeed);

        AppendUint32(PayloadUtf8.Length());
        if (PayloadUtf8.Length() > 0)
        {
            OutBytes.Append(reinterpret_cast<const uint8*>(PayloadUtf8.Get()), PayloadUtf8.Length());
        }

        return true;
    }

    static bool FromBinary(const TArray<uint8>& Bytes, FAirplaneUDPMessage& OutMessage)
    {
        const uint8* Data = Bytes.GetData();
        int32 Remaining = Bytes.Num();

        auto ReadUint32 = [&Data, &Remaining](uint32& OutValue) -> bool
        {
            if (Remaining < static_cast<int32>(sizeof(uint32)))
            {
                return false;
            }
            FMemory::Memcpy(&OutValue, Data, sizeof(uint32));
            Data += sizeof(uint32);
            Remaining -= sizeof(uint32);
            return true;
        };

        auto ReadFloat = [&Data, &Remaining](float& OutValue) -> bool
        {
            if (Remaining < static_cast<int32>(sizeof(float)))
            {
                return false;
            }
            FMemory::Memcpy(&OutValue, Data, sizeof(float));
            Data += sizeof(float);
            Remaining -= sizeof(float);
            return true;
        };

        auto ReadString = [&Data, &Remaining, &ReadUint32](FString& OutString) -> bool
        {
            uint32 Length = 0;
            if (!ReadUint32(Length))
            {
                return false;
            }
            if (Remaining < static_cast<int32>(Length))
            {
                return false;
            }
            if (Length > 0)
            {
                const ANSICHAR* Utf8Data = reinterpret_cast<const ANSICHAR*>(Data);
                FUTF8ToTCHAR Converter(Utf8Data, Length);
                OutString = FString(Converter.Get(), Converter.Length());
                Data += Length;
                Remaining -= Length;
            }
            else
            {
                OutString = TEXT("");
            }
            return true;
        };

        if (!ReadString(OutMessage.MessageType))
        {
            return false;
        }
        float LocX = 0.0f;
        float LocY = 0.0f;
        float LocZ = 0.0f;
        if (!ReadFloat(LocX))
        {
            return false;
        }
        if (!ReadFloat(LocY))
        {
            return false;
        }
        if (!ReadFloat(LocZ))
        {
            return false;
        }
        OutMessage.Location = FVector(LocX, LocY, LocZ);
        if (!ReadFloat(OutMessage.Altitude))
        {
            return false;
        }
        if (!ReadFloat(OutMessage.Airspeed))
        {
            return false;
        }
        if (!ReadString(OutMessage.Payload))
        {
            return false;
        }

        return true;
    }
};

