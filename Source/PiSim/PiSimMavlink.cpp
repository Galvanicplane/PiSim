#include "PiSimMavlink.h"

namespace
{
    constexpr uint8 CrcExtraHeartbeat = 50;
    constexpr uint8 CrcExtraHilActuator = 47;
    constexpr uint8 CrcExtraHilSensor = 108;
    constexpr uint8 CrcExtraHilGps = 124;
}

uint16 FPiSimMavlink::CrcAccumulate(uint8 Data, uint16 Crc)
{
    uint8 Tmp = Data ^ static_cast<uint8>(Crc & 0xFF);
    Tmp ^= (Tmp << 4);
    return static_cast<uint16>((Crc >> 8) ^ (Tmp << 8) ^ (Tmp << 3) ^ (Tmp >> 4));
}

uint16 FPiSimMavlink::CrcCalculate(const uint8* Data, int32 Len, uint8 CrcExtra)
{
    uint16 Crc = 0xFFFF;
    for (int32 i = 0; i < Len; ++i)
    {
        Crc = CrcAccumulate(Data[i], Crc);
    }
    Crc = CrcAccumulate(CrcExtra, Crc);
    return Crc;
}

void FPiSimMavlink::AppendU8(TArray<uint8>& Bytes, uint8 V) { Bytes.Add(V); }

void FPiSimMavlink::AppendU16(TArray<uint8>& Bytes, uint16 V)
{
    Bytes.Add(static_cast<uint8>(V & 0xFF));
    Bytes.Add(static_cast<uint8>((V >> 8) & 0xFF));
}

void FPiSimMavlink::AppendU32(TArray<uint8>& Bytes, uint32 V)
{
    Bytes.Add(static_cast<uint8>(V & 0xFF));
    Bytes.Add(static_cast<uint8>((V >> 8) & 0xFF));
    Bytes.Add(static_cast<uint8>((V >> 16) & 0xFF));
    Bytes.Add(static_cast<uint8>((V >> 24) & 0xFF));
}

void FPiSimMavlink::AppendU64(TArray<uint8>& Bytes, uint64 V)
{
    for (int32 i = 0; i < 8; ++i)
    {
        Bytes.Add(static_cast<uint8>((V >> (8 * i)) & 0xFF));
    }
}

void FPiSimMavlink::AppendF32(TArray<uint8>& Bytes, float V)
{
    uint32 Bits = 0;
    FMemory::Memcpy(&Bits, &V, sizeof(float));
    AppendU32(Bytes, Bits);
}

void FPiSimMavlink::AppendI16(TArray<uint8>& Bytes, int16 V)
{
    AppendU16(Bytes, static_cast<uint16>(V));
}

void FPiSimMavlink::AppendI32(TArray<uint8>& Bytes, int32 V)
{
    AppendU32(Bytes, static_cast<uint32>(V));
}

uint8 FPiSimMavlink::ReadU8(const TArray<uint8>& Bytes, int32& Offset)
{
    return Bytes.IsValidIndex(Offset) ? Bytes[Offset++] : 0;
}

uint16 FPiSimMavlink::ReadU16(const TArray<uint8>& Bytes, int32& Offset)
{
    const uint16 Lo = ReadU8(Bytes, Offset);
    const uint16 Hi = ReadU8(Bytes, Offset);
    return static_cast<uint16>(Lo | (Hi << 8));
}

uint32 FPiSimMavlink::ReadU32(const TArray<uint8>& Bytes, int32& Offset)
{
    uint32 V = 0;
    for (int32 i = 0; i < 4; ++i)
    {
        V |= (static_cast<uint32>(ReadU8(Bytes, Offset)) << (8 * i));
    }
    return V;
}

uint64 FPiSimMavlink::ReadU64(const TArray<uint8>& Bytes, int32& Offset)
{
    uint64 V = 0;
    for (int32 i = 0; i < 8; ++i)
    {
        V |= (static_cast<uint64>(ReadU8(Bytes, Offset)) << (8 * i));
    }
    return V;
}

float FPiSimMavlink::ReadF32(const TArray<uint8>& Bytes, int32& Offset)
{
    const uint32 Bits = ReadU32(Bytes, Offset);
    float V = 0.0f;
    FMemory::Memcpy(&V, &Bits, sizeof(float));
    return V;
}

TArray<uint8> FPiSimMavlink::PackV2(uint8 SysId, uint8 CompId, uint8& InOutSeq, uint32 MsgId, const TArray<uint8>& Payload, uint8 CrcExtra)
{
    TArray<uint8> Packet;
    Packet.Reserve(12 + Payload.Num());
    Packet.Add(STX_V2);
    Packet.Add(static_cast<uint8>(Payload.Num()));
    Packet.Add(0); // incompat
    Packet.Add(0); // compat
    Packet.Add(InOutSeq++);
    Packet.Add(SysId);
    Packet.Add(CompId);
    Packet.Add(static_cast<uint8>(MsgId & 0xFF));
    Packet.Add(static_cast<uint8>((MsgId >> 8) & 0xFF));
    Packet.Add(static_cast<uint8>((MsgId >> 16) & 0xFF));
    Packet.Append(Payload);

    const uint16 Crc = CrcCalculate(Packet.GetData() + 1, Packet.Num() - 1, CrcExtra);
    AppendU16(Packet, Crc);
    return Packet;
}

TArray<uint8> FPiSimMavlink::PackHeartbeat(uint8 SysId, uint8 CompId, uint8& InOutSeq, uint8 Type, uint8 Autopilot, uint8 BaseMode, uint32 CustomMode, uint8 SystemStatus)
{
    TArray<uint8> Payload;
    AppendU32(Payload, CustomMode);
    AppendU8(Payload, Type);
    AppendU8(Payload, Autopilot);
    AppendU8(Payload, BaseMode);
    AppendU8(Payload, SystemStatus);
    AppendU8(Payload, 3); // mavlink_version
    return PackV2(SysId, CompId, InOutSeq, MSG_HEARTBEAT, Payload, CrcExtraHeartbeat);
}

TArray<uint8> FPiSimMavlink::PackHilSensor(uint8 SysId, uint8 CompId, uint8& InOutSeq, const FPiSimHilSensor& Sensor)
{
    TArray<uint8> Payload;
    AppendU64(Payload, Sensor.TimeUsec);
    AppendF32(Payload, Sensor.Xacc);
    AppendF32(Payload, Sensor.Yacc);
    AppendF32(Payload, Sensor.Zacc);
    AppendF32(Payload, Sensor.Xgyro);
    AppendF32(Payload, Sensor.Ygyro);
    AppendF32(Payload, Sensor.Zgyro);
    AppendF32(Payload, Sensor.Xmag);
    AppendF32(Payload, Sensor.Ymag);
    AppendF32(Payload, Sensor.Zmag);
    AppendF32(Payload, Sensor.AbsPressureHPa);
    AppendF32(Payload, Sensor.DiffPressureHPa);
    AppendF32(Payload, Sensor.PressureAltM);
    AppendF32(Payload, Sensor.TemperatureC);
    AppendU32(Payload, Sensor.FieldsUpdated);
    AppendU8(Payload, Sensor.Id);
    return PackV2(SysId, CompId, InOutSeq, MSG_HIL_SENSOR, Payload, CrcExtraHilSensor);
}

TArray<uint8> FPiSimMavlink::PackHilGps(uint8 SysId, uint8 CompId, uint8& InOutSeq, const FPiSimHilGps& Gps)
{
    TArray<uint8> Payload;
    AppendU64(Payload, Gps.TimeUsec);
    AppendI32(Payload, Gps.LatE7);
    AppendI32(Payload, Gps.LonE7);
    AppendI32(Payload, Gps.AltMm);
    AppendU16(Payload, Gps.Eph);
    AppendU16(Payload, Gps.Epv);
    AppendU16(Payload, Gps.VelCms);
    AppendI16(Payload, Gps.VnCms);
    AppendI16(Payload, Gps.VeCms);
    AppendI16(Payload, Gps.VdCms);
    AppendU16(Payload, Gps.CogCdeg);
    AppendU8(Payload, Gps.FixType);
    AppendU8(Payload, Gps.SatellitesVisible);
    AppendU8(Payload, Gps.Id);
    AppendU16(Payload, Gps.YawCdeg);
    return PackV2(SysId, CompId, InOutSeq, MSG_HIL_GPS, Payload, CrcExtraHilGps);
}

void FPiSimMavlink::ParseStream(TArray<uint8>& Buffer, TArray<FPiSimMavlinkPacket>& OutPackets)
{
    int32 Index = 0;
    while (Index < Buffer.Num())
    {
        if (Buffer[Index] != STX_V2)
        {
            ++Index;
            continue;
        }

        if (Buffer.Num() - Index < 12)
        {
            break;
        }

        const uint8 PayloadLen = Buffer[Index + 1];
        const int32 FrameLen = 10 + PayloadLen + 2;
        if (Buffer.Num() - Index < FrameLen)
        {
            break;
        }

        const uint8 CrcExtra = (Buffer[Index + 7] == 93 && Buffer[Index + 8] == 0 && Buffer[Index + 9] == 0)
            ? CrcExtraHilActuator
            : ((Buffer[Index + 7] == 0 && Buffer[Index + 8] == 0 && Buffer[Index + 9] == 0) ? CrcExtraHeartbeat : 0);

        bool bCrcOk = true;
        if (CrcExtra != 0)
        {
            const uint16 Calc = CrcCalculate(Buffer.GetData() + Index + 1, 9 + PayloadLen, CrcExtra);
            const uint16 Got = static_cast<uint16>(Buffer[Index + 10 + PayloadLen] | (Buffer[Index + 11 + PayloadLen] << 8));
            bCrcOk = (Calc == Got);
        }

        if (bCrcOk)
        {
            FPiSimMavlinkPacket Pkt;
            Pkt.Seq = Buffer[Index + 4];
            Pkt.SysId = Buffer[Index + 5];
            Pkt.CompId = Buffer[Index + 6];
            Pkt.MsgId = static_cast<uint32>(Buffer[Index + 7] | (Buffer[Index + 8] << 8) | (Buffer[Index + 9] << 16));
            Pkt.Payload.Append(Buffer.GetData() + Index + 10, PayloadLen);
            OutPackets.Add(MoveTemp(Pkt));
        }

        Index += FrameLen;
    }

    if (Index > 0)
    {
        Buffer.RemoveAt(0, Index);
    }
}

bool FPiSimMavlink::DecodeHilActuatorControls(const FPiSimMavlinkPacket& Packet, FPiSimHilActuatorControls& Out)
{
    if (Packet.MsgId != MSG_HIL_ACTUATOR_CONTROLS || Packet.Payload.Num() < 81)
    {
        return false;
    }

    int32 Offset = 0;
    Out.TimeUsec = ReadU64(Packet.Payload, Offset);
    Out.Flags = ReadU64(Packet.Payload, Offset);
    for (int32 i = 0; i < 16; ++i)
    {
        Out.Controls[i] = ReadF32(Packet.Payload, Offset);
    }
    Out.Mode = ReadU8(Packet.Payload, Offset);
    return true;
}
