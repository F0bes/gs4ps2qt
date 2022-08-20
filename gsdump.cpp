#include "gsdump.h"

#include <QByteArray>
#include <QDataStream>

GSDump::GSDumpHeader GSDump::ReadHeader(QDataStream& dataStream)
{
	dataStream.setByteOrder(QDataStream::LittleEndian);

	uint CRC;
	dataStream >> CRC;
	if (CRC == 0xFFFFFFFF)
	{
		uint header_size;
		dataStream >> header_size;
		return GSDumpHeader(dataStream);
	}
	else
	{
		GSDumpHeader header;
		memset(&header, 0, sizeof(header));
		header.old = 1;
		dataStream >> header.state_size;
		dataStream >> header.state_version;
		header.crc = CRC;
		return header;
	}
}
