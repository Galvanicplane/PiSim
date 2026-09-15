// PiSimMavlink.h
// Minimal MAVLink v2 pack/unpack for PX4 SITL and PX4 HITL (HIL_SENSOR, HIL_GPS, HIL_ACTUATOR_CONTROLS).

#pragma once

#include "CoreMinimal.h"

struct FPiSimMavlinkPacket
{
    uint8 SysId = 0;
    uint8 CompId = 0;
    uint8 Seq = 0;
    uint32 MsgId = 0;
    TArray<uint8> Payload;
};

struct FPiSimHilSensor
{
    uint64 TimeUsec = 0;
    float Xacc = 0.0f;
    float Yacc = 0.0f;
    float Zacc = 0.0f;
    float Xgyro = 0.0f;
    float Ygyro = 0.0f;
    float Zgyro = 0.0f;
    float Xmag = 0.0f;
    float Ymag = 0.0f;
    float Zmag = 0.0f;
    float AbsPressureHPa = 1013.25f;
    float DiffPressureHPa = 0.0f;
    float PressureAltM = 0.0f;
    float TemperatureC = 20.0f;
    uint32 FieldsUpdated = 0x1FFF;
    uint8 Id = 0;
};

struct FPiSimHilGps
{
    uint64 TimeUsec = 0;
    int32 LatE7 = 0;
    int32 LonE7 = 0;
    int32 AltMm = 0;
    uint16 Eph = 80;
    uint16 Epv = 110;
    uint16 VelCms = 0;
    int16 VnCms = 0;
    int16 VeCms = 0;
    int16 VdCms = 0;
    uint16 CogCdeg = 0;
    uint8 FixType = 3;
    uint8 SatellitesVisible = 14;
    uint8 Id = 0;
    uint16 YawCdeg = 0;
};

struct FPiSimHilActuatorControls
{
    uint64 TimeUsec = 0;
    uint64 Flags = 0;
    float Controls[16] = {};
    uint8 Mode = 0;
};

class PISIM_API FPiSimMavlink
{
public:
    static constexpr uint8 STX_V2 = 0xFD;
    static constexpr uint32 MSG_HEARTBEAT = 0;
    static constexpr uint32 MSG_HIL_ACTUATOR_CONTROLS = 93;
    static constexpr uint32 MSG_HIL_SENSOR = 107;
    static constexpr uint32 MSG_HIL_GPS = 113;

    static TArray<uint8> PackHeartbeat(uint8 SysId, uint8 CompId, uint8& InOutSeq, uint8 Type, uint8 Autopilot, uint8 BaseMode, uint32 CustomMode, uint8 SystemStatus);
    static TArray<uint8> PackHilSensor(uint8 SysId, uint8 CompId, uint8& InOutSeq, const FPiSimHilSensor& Sensor);
    static TArray<uint8> PackHilGps(uint8 SysId, uint8 CompId, uint8& InOutSeq, const FPiSimHilGps& Gps);

    /** Parse as many complete MAVLink v2 frames as possible from a byte stream. Consumed bytes are removed from Buffer. */
    static void ParseStream(TArray<uint8>& Buffer, TArray<FPiSimMavlinkPacket>& OutPackets);

    static bool DecodeHilActuatorControls(const FPiSimMavlinkPacket& Packet, FPiSimHilActuatorControls& Out);

private:
    static uint16 CrcAccumulate(uint8 Data, uint16 Crc);
    static uint16 CrcCalculate(const uint8* Data, int32 Len, uint8 CrcExtra);
    static TArray<uint8> PackV2(uint8 SysId, uint8 CompId, uint8& InOutSeq, uint32 MsgId, const TArray<uint8>& Payload, uint8 CrcExtra);
    static void AppendU8(TArray<uint8>& Bytes, uint8 V);
    static void AppendU16(TArray<uint8>& Bytes, uint16 V);
    static void AppendU32(TArray<uint8>& Bytes, uint32 V);
    static void AppendU64(TArray<uint8>& Bytes, uint64 V);
    static void AppendF32(TArray<uint8>& Bytes, float V);
    static void AppendI16(TArray<uint8>& Bytes, int16 V);
    static void AppendI32(TArray<uint8>& Bytes, int32 V);
    static uint8 ReadU8(const TArray<uint8>& Bytes, int32& Offset);
    static uint16 ReadU16(const TArray<uint8>& Bytes, int32& Offset);
    static uint32 ReadU32(const TArray<uint8>& Bytes, int32& Offset);
    static uint64 ReadU64(const TArray<uint8>& Bytes, int32& Offset);
    static float ReadF32(const TArray<uint8>& Bytes, int32& Offset);
};
