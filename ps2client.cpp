#include "ps2client.h"
#include "gsdump.h"
#include "crc8.h"

#include <sockpp/tcp_connector.h>
#include <sockpp/inet_address.h>

#include <QDebug>
#include <QObject>
#include <QFile>

void PS2ClientWorker::cmdServerVersion()
{
	char cmd = SERVER_CMD_VER;
	con->write_n(&cmd, 1);


	char verString[128];
	int i = 0;
	char c;
	do
	{
		con->read_n(&c, 1);
		verString[i++] = c;
	} while (c > 0);

	emit retServerVersion(QString(verString));
}

void PS2ClientWorker::connectSocket(QString hostname)
{
	sockpp::socket_initializer* sockInit = new socket_initializer;
	if (!con->connect(inet_address(hostname.toStdString().c_str(), 18196)))
	{
		emit socketDisconnected();
		return;
	}

	emit socketConnected();
}

void PS2ClientWorker::cmdExecuteDump(QByteArray data)
{
	init_crc8();
	shutdownFlag = false;

	QDataStream ds(data);
	ds.setByteOrder(QDataStream::LittleEndian);
	GSDump::GSDumpHeader header = GSDump::ReadHeader(ds);
	ds.skipRawData(header.serial_size);
	ds.skipRawData(header.screenshot_size);

	if (!header.old)
		ds.skipRawData(4); // skip state version

	// Send state data
	//   SERVER_STATE (char)
	//   CRC (char)
	//   SIZE (u32)
	//   DATA
	auto state_size = header.state_size - 4;
	char* state = new char[state_size];
	ds.readRawData(state, state_size);
	unsigned char CRC = 0;
	crc8_buffer(&CRC, reinterpret_cast<unsigned char*>(state), state_size);

_sendStateData:
	char cmd = SERVER_STATE;
	con->write_n(&cmd, 1);
	con->write_n(&CRC, 1);
	printf("CRC is %X\n", CRC);
	con->write_n(&state_size, sizeof(state_size));
	con->write_n(&header.state_version, sizeof(header.state_version));
	con->write_n(state, state_size);


	// Wait for response
	char response;
	con->read_n(&response, 1);

	if (response == SERVER_RETRY)
	{
		qDebug() << "Retrying state data transfer";
		goto _sendStateData;
	}

	delete state;

	char* registers = new char[8192];
	ds.readRawData(registers, 8192);
	CRC = 0;
	crc8_buffer(&CRC, reinterpret_cast<unsigned char*>(registers), 8192);

_sendRegisterData:
	cmd = SERVER_SET_REG;
	con->write_n(&cmd, 1);
	con->write_n(&CRC, 1);
	con->write_n(registers, 8192);

	con->read_n(&response, 1);

	if (response == SERVER_RETRY)
	{
		qDebug() << "Retrying state data transfer";
		goto _sendRegisterData;
	}

	delete registers;
	char* batchedTransfers = new char[0x500000];
_startTransfers:
	uint32_t transfersBatched = 0;
	uint32_t batchedSize = 0;
	auto startPos = ds.device()->pos();
	char tag;
	bool transKicked = false;
	do
	{
		if (shutdownFlag)
			break;
		// We don't have any peeking with QDataStream
		// So when we kick we will have read the next tag
		if (!transKicked)
			ds.readRawData(&tag, sizeof(tag));
		else
			transKicked = false;
		switch (tag)
		{
			case 0: // Transfer
			{
				do
				{
					transfersBatched++;
					stats.trans_cnt++;

					char path; // Transfer Path
					ds.readRawData(&path, sizeof(path));

					uint32_t size; // Transfer Size
					ds.readRawData((char*)&size, sizeof(size));

					ds.readRawData(&batchedTransfers[batchedSize], size);
					batchedSize += size;

					// Read the next tag
					ds.readRawData(&tag, sizeof(tag));

					// If the next instruction isn't a transfer
					// or we have exceeded our batched size, kick it
					// to the PS2
					if (tag != 0 || batchedSize >= 0x100000)
					{
						unsigned char CRC = 0;
						crc8_buffer(&CRC, (unsigned char*)batchedTransfers, batchedSize);

					_sendTransferPacket:
						qDebug() << "Sending batched packet of " << transfersBatched << "packets"
								 << "of size " << batchedSize;
						char cmd = SERVER_TRANSFER;
						con->write_n(&cmd, 1);
						con->write_n(&CRC, 1);
						con->write_n(&batchedSize, 4);
						con->write_n(batchedTransfers, batchedSize);

						char response;
						con->read_n(&response, 1);

						if (response == SERVER_RETRY)
						{
							goto _sendTransferPacket;
						}
						stats.trans_batched_cnt++;
						transKicked = true;
						transfersBatched = 0;
						batchedSize = 0;
						break;
					}

				} while (true);
			}
			break;
			case 1: // Vsync
			{
				qDebug() << "VSYNC";
				stats.vsync_cnt++;
				char cmd = SERVER_WAIT_VSYNC;
				char field;
				ds >> field;

			_sendVsync:
				con->write_n(&cmd, 1);
				con->write_n(&field, 1);

				unsigned char response;
				con->read_n(&response, 1);

				if (response == SERVER_RETRY)
				{
					goto _sendVsync;
				}
				if (response == SERVER_OK_FRAME)
				{
					// We have framebuffer data
					qDebug() << "Receiving framebuffer data";

					uint32_t circuits;
					con->read_n(&circuits, sizeof(circuits));

read_circuit_data:
					Vsync_Frame frame_header;
					con->read_n(&frame_header.Circuit, sizeof(frame_header.Circuit));
					con->read_n(&frame_header.PSM, sizeof(frame_header.PSM));
					con->read_n(&frame_header.Width, sizeof(frame_header.Width));
					con->read_n(&frame_header.Height, sizeof(frame_header.Height));
					con->read_n(&frame_header.Bytes, sizeof(frame_header.Bytes));

					qDebug() << frame_header.Circuit << " <-- received circuit";
					unsigned char* frame = (unsigned char*)malloc(frame_header.Bytes);
					con->read_n(frame, frame_header.Bytes);

					emit frameReceived(frame_header, frame);
					
					if(circuits == 3) // We are reading 2 circuits
					{ 
						circuits = 0;
						goto read_circuit_data;
					}
				}
				else if (response != SERVER_OK)
				{
					qDebug() << "Panic";
					abort();
				}
			}
			break;
			case 2: // FIFO
			{
				qDebug() << "READ FIFO";
				stats.fifo_cnt++;
				char cmd = SERVER_READ_FIFO;
				uint32_t size;
				ds >> size;
			_sendFIFO:
				con->write_n(&cmd, 1);
				con->write_n(&size, sizeof(size));

				char response;
				con->read_n(&response, 1);

				if (response == SERVER_RETRY)
				{
					goto _sendFIFO;
				}
			}
			break;
			case 3: // Set Registers
			{
				qDebug() << "SET REGISTERS";
				stats.reg_cnt++;
				char cmd = SERVER_SET_REG;

				char registers[8192];
				ds.readRawData(registers, 8192);

				unsigned char CRC = 0;
				crc8_buffer(&CRC, (unsigned char*)&registers, 8192);
			_sendRegisters:
				con->write_n(&cmd, 1);
				con->write_n(&CRC, 1);
				con->write_n(registers, 8192);

				char response;
				con->read_n(&response, 1);

				if (response == SERVER_RETRY)
				{
					goto _sendRegisters;
				}
			}
			break;
		}
	} while (!ds.atEnd());

	if (replay && !shutdownFlag)
	{
		stats.replay_cnt++;
		ds.device()->seek(startPos);
		goto _startTransfers;
	}
	unsigned char shutdown = SERVER_SHUTDOWN;
	con->write_n(&shutdown, 1);

	// Unused
	con->read_n(&response, 1);

	delete batchedTransfers;

	if (shutdownFlag)
	{
		emit socketDisconnected();
		con->close();
	}
}

PS2ClientController::PS2ClientController()
	: worker(&con)
{
	worker.moveToThread(&workerThread);
	connect(&worker, &PS2ClientWorker::retServerVersion, this, &PS2ClientController::retServerVersion);
	connect(&worker, &PS2ClientWorker::socketConnected, this, &PS2ClientController::socketConnected);
	connect(&worker, &PS2ClientWorker::socketDisconnected, this, &PS2ClientController::socketDisconnected);
	connect(&worker, &PS2ClientWorker::frameReceived, this, &PS2ClientController::frameReceived);
	workerThread.setObjectName("PS2Client Thread");
	workerThread.start();
}

PS2ClientController::~PS2ClientController()
{
	assert(1);
}

void PS2ClientController::connectSocket(QString hostname)
{
	QMetaObject::invokeMethod(&worker, "connectSocket", Qt::QueuedConnection, Q_ARG(QString, hostname));
}


void PS2ClientController::retrieveServerVersion()
{
	QString versionString;
	// Qt::QueuedConnection ensures that the slot is invoked in its own thread
	QMetaObject::invokeMethod(&worker, "cmdServerVersion", Qt::QueuedConnection);
};

void PS2ClientController::startDump(QString filepath)
{
	worker.stats.fifo_cnt = 0;
	worker.stats.reg_cnt = 0;
	worker.stats.replay_cnt = 0;
	worker.stats.trans_cnt = 0;
	worker.stats.trans_batched_cnt = 0;
	worker.stats.vsync_cnt = 0;

	QFile file(filepath);
	if (file.open(QIODevice::ReadOnly))
	{
		// TODO: don't load everything at once
		QByteArray data = file.readAll();
		QMetaObject::invokeMethod(&worker, "cmdExecuteDump", Qt::QueuedConnection, Q_ARG(QByteArray, data));
	}
	else
	{
		qDebug() << "Unable to open file ???";
	}
}
