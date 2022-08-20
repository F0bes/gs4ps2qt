#pragma once

#include <QByteArray>
#include <QDataStream>
#include <QImage>
#include <QString>

class GSDump
{

public:
	struct GSDumpHeader
	{
		uint32_t old;
		uint32_t state_version;
		uint32_t state_size;
		uint32_t serial_offset;
		uint32_t serial_size;
		uint32_t crc;
		uint32_t screenshot_width;
		uint32_t screenshot_height;
		uint32_t screenshot_offset;
		uint32_t screenshot_size;
		GSDumpHeader(QDataStream& ds)
		{
			ds.setByteOrder(QDataStream::LittleEndian);
			ds >> state_version;
			ds >> state_size;
			ds >> serial_offset;
			ds >> serial_size;
			ds >> crc;
			ds >> screenshot_width;
			ds >> screenshot_height;
			ds >> screenshot_offset;
			ds >> screenshot_size;
			old = 0;
		}
		GSDumpHeader() = default;
	};

	enum GSDumpInstruction
	{
		Transfer,
		VSYNC,
		FIFO,
		Registers,
		Freeze,
		Unknown
	};

	static GSDumpInstruction ParseInstruction(QDataStream& data)
	{
		
	}
	
	static GSDumpHeader ReadHeader(QDataStream& data);
};
