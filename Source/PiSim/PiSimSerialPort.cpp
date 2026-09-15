#include "PiSimSerialPort.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

FPiSimSerialPort::FPiSimSerialPort() = default;

FPiSimSerialPort::~FPiSimSerialPort()
{
    Close();
}

bool FPiSimSerialPort::Open(const FString& PortName, int32 BaudRate)
{
    Close();

#if PLATFORM_WINDOWS
    FString DevicePath = PortName;
    if (!DevicePath.StartsWith(TEXT("\\\\.\\")))
    {
        DevicePath = FString::Printf(TEXT("\\\\.\\%s"), *PortName);
    }

    HANDLE Handle = CreateFileW(
        *DevicePath,
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (Handle == INVALID_HANDLE_VALUE)
    {
        UE_LOG(LogTemp, Error, TEXT("[PiSimSerial] Failed to open %s (err=%d)"), *DevicePath, GetLastError());
        return false;
    }

    DCB Dcb = {};
    Dcb.DCBlength = sizeof(DCB);
    if (!GetCommState(Handle, &Dcb))
    {
        CloseHandle(Handle);
        return false;
    }

    Dcb.BaudRate = static_cast<DWORD>(BaudRate);
    Dcb.ByteSize = 8;
    Dcb.Parity = NOPARITY;
    Dcb.StopBits = ONESTOPBIT;
    Dcb.fBinary = 1;
    Dcb.fDtrControl = DTR_CONTROL_ENABLE;
    Dcb.fRtsControl = RTS_CONTROL_ENABLE;
    if (!SetCommState(Handle, &Dcb))
    {
        CloseHandle(Handle);
        return false;
    }

    COMMTIMEOUTS Timeouts = {};
    Timeouts.ReadIntervalTimeout = MAXDWORD;
    Timeouts.ReadTotalTimeoutMultiplier = 0;
    Timeouts.ReadTotalTimeoutConstant = 0;
    Timeouts.WriteTotalTimeoutMultiplier = 0;
    Timeouts.WriteTotalTimeoutConstant = 50;
    SetCommTimeouts(Handle, &Timeouts);
    SetupComm(Handle, 65536, 65536);
    PurgeComm(Handle, PURGE_RXCLEAR | PURGE_TXCLEAR);

    NativeHandle = Handle;
    OpenedPortName = PortName;
    OpenedBaudRate = BaudRate;
    bIsOpen = true;
    UE_LOG(LogTemp, Log, TEXT("[PiSimSerial] Opened %s @ %d baud"), *PortName, BaudRate);
    return true;
#else
    UE_LOG(LogTemp, Warning, TEXT("[PiSimSerial] Serial HITL is only implemented on Windows."));
    return false;
#endif
}

void FPiSimSerialPort::Close()
{
#if PLATFORM_WINDOWS
    if (NativeHandle)
    {
        CloseHandle(static_cast<HANDLE>(NativeHandle));
        NativeHandle = nullptr;
    }
#endif
    bIsOpen = false;
    OpenedPortName.Empty();
}

int32 FPiSimSerialPort::WriteBytes(const TArray<uint8>& Data)
{
    if (!bIsOpen || Data.Num() == 0)
    {
        return 0;
    }

#if PLATFORM_WINDOWS
    DWORD Written = 0;
    if (!WriteFile(static_cast<HANDLE>(NativeHandle), Data.GetData(), static_cast<DWORD>(Data.Num()), &Written, nullptr))
    {
        return 0;
    }
    return static_cast<int32>(Written);
#else
    return 0;
#endif
}

int32 FPiSimSerialPort::ReadBytes(TArray<uint8>& OutData, int32 MaxBytes)
{
    OutData.Reset();
    if (!bIsOpen)
    {
        return 0;
    }

#if PLATFORM_WINDOWS
    DWORD Errors = 0;
    COMSTAT Stat = {};
    ClearCommError(static_cast<HANDLE>(NativeHandle), &Errors, &Stat);
    const int32 ToRead = FMath::Min(MaxBytes, static_cast<int32>(Stat.cbInQue));
    if (ToRead <= 0)
    {
        return 0;
    }

    OutData.SetNumUninitialized(ToRead);
    DWORD ReadCount = 0;
    if (!ReadFile(static_cast<HANDLE>(NativeHandle), OutData.GetData(), static_cast<DWORD>(ToRead), &ReadCount, nullptr))
    {
        OutData.Reset();
        return 0;
    }
    OutData.SetNum(static_cast<int32>(ReadCount), EAllowShrinking::No);
    return static_cast<int32>(ReadCount);
#else
    return 0;
#endif
}

TArray<FString> FPiSimSerialPort::ListAvailablePorts()
{
    TArray<FString> Ports;
#if PLATFORM_WINDOWS
    for (int32 i = 1; i <= 32; ++i)
    {
        const FString Name = FString::Printf(TEXT("COM%d"), i);
        const FString DevicePath = FString::Printf(TEXT("\\\\.\\%s"), *Name);
        HANDLE Handle = CreateFileW(*DevicePath, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (Handle != INVALID_HANDLE_VALUE)
        {
            Ports.Add(Name);
            CloseHandle(Handle);
        }
    }
#endif
    if (Ports.Num() == 0)
    {
        Ports.Add(TEXT("COM3"));
    }
    return Ports;
}
