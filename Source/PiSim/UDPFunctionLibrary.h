// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UDPMessage.h"
#include "AirplaneUDPMessage.h"
#include "LandVehicleUDPMessage.h"
#include "UDPFunctionLibrary.generated.h"

/**
 * Generic blueprint function library for UDP communication and vehicle message serialization.
 */
UCLASS()
class PISIM_API UUDPFunctionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // --- Generic UDP Functions ---

    /**
     * Send raw binary bytes as a UDP packet to the specified IP address and port.
     */
    UFUNCTION(BlueprintCallable, Category = "UDP")
    static bool SendUDPBytes(const FString& IPAddress, int32 Port, const TArray<uint8>& Bytes);

    /**
     * Receive raw binary UDP bytes on LocalPort with timeout.
     */
    UFUNCTION(BlueprintCallable, Category = "UDP")
    static bool ReceiveUDPBytes(int32 LocalPort, float TimeoutSeconds, TArray<uint8>& OutBytes);

    /**
     * Send a raw UTF-8 string as a UDP datagram to the specified IP and port.
     */
    UFUNCTION(BlueprintCallable, Category = "UDP")
    static bool SendUDPMessage(const FString& IPAddress, int32 Port, const FString& Message);

    /**
     * Receive a raw UTF-8 UDP text packet on LocalPort with timeout.
     */
    UFUNCTION(BlueprintCallable, Category = "UDP")
    static bool ReceiveUDPMessage(int32 LocalPort, float TimeoutSeconds, FString& OutMessage);

    /**
     * Read the message type name (first serialized string field) from raw binary UDP bytes.
     * Returns false if the byte array is invalid or corrupted.
     */
    UFUNCTION(BlueprintCallable, Category = "UDP")
    static bool GetUDPMessageType(const TArray<uint8>& Bytes, FString& OutMessageType);

    // --- Struct <-> Bytes Converters ---

    /**
     * Convert an FAirplaneUDPMessage struct to binary bytes.
     */
    UFUNCTION(BlueprintCallable, Category = "UDP")
    static bool AirplaneMessageToBytes(const FAirplaneUDPMessage& Message, TArray<uint8>& OutBytes);

    /**
     * Convert binary bytes to an FAirplaneUDPMessage struct.
     */
    UFUNCTION(BlueprintCallable, Category = "UDP")
    static bool BytesToAirplaneMessage(const TArray<uint8>& Bytes, FAirplaneUDPMessage& OutMessage);

    /**
     * Convert an FLandVehicleUDPMessage struct to binary bytes.
     */
    UFUNCTION(BlueprintCallable, Category = "UDP")
    static bool LandVehicleMessageToBytes(const FLandVehicleUDPMessage& Message, TArray<uint8>& OutBytes);

    /**
     * Convert binary bytes to an FLandVehicleUDPMessage struct.
     */
    UFUNCTION(BlueprintCallable, Category = "UDP")
    static bool BytesToLandVehicleMessage(const TArray<uint8>& Bytes, FLandVehicleUDPMessage& OutMessage);
};
