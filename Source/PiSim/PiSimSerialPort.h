// PiSimSerialPort.h
// Non-blocking Win32 COM port wrapper for PX4 HITL (USB CDC, typically 921600 baud).

#pragma once

#include "CoreMinimal.h"

class PISIM_API FPiSimSerialPort
{
public:
    FPiSimSerialPort();
    ~FPiSimSerialPort();

    bool Open(const FString& PortName, int32 BaudRate = 921600);
    void Close();
    bool IsOpen() const { return bIsOpen; }

    int32 WriteBytes(const TArray<uint8>& Data);
    int32 ReadBytes(TArray<uint8>& OutData, int32 MaxBytes = 2048);

    static TArray<FString> ListAvailablePorts();

    FString GetPortName() const { return OpenedPortName; }
    int32 GetBaudRate() const { return OpenedBaudRate; }

private:
    bool bIsOpen = false;
    FString OpenedPortName;
    int32 OpenedBaudRate = 921600;
    void* NativeHandle = nullptr;
};
