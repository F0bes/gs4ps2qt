#pragma once

#include <QObject>
#include <QThread>
#include <QFuture>
#include <optional>
#include <sockpp/tcp_connector.h>

using namespace sockpp;

class PS2ClientWorker : public QObject
{
	Q_OBJECT

	enum SERVER_CMD
	{
		SERVER_CMD_VER = 0x00,
		SERVER_TRANSFER = 0x01,
		SERVER_WAIT_VSYNC = 0x02,
		SERVER_READ_FIFO = 0x03,
		SERVER_SET_REG = 0x04,
		SERVER_STATE = 0x05,
		SERVER_SHUTDOWN = 0xFF,
	};

	enum SERVER_RESP
	{
		SERVER_OK_FRAME = 0x79,
		SERVER_OK = 0x80,
		SERVER_RETRY = 0x81,
	};

public:
	struct Stats
	{
		uint64_t trans_cnt;
		uint64_t trans_batched_cnt;
		uint64_t fifo_cnt;
		uint64_t vsync_cnt;
		uint64_t reg_cnt;
		uint64_t replay_cnt;
	};

	struct Vsync_Frame
	{
		unsigned char Circuit;
		unsigned char PSM;
		uint32_t Width;
		uint32_t Height;
		uint32_t Bytes;
	};

	PS2ClientWorker(tcp_connector* con, QObject* parent = nullptr)
		: con(con)
		, QObject{parent}
		, stats{0}
	{
	}

	bool replay = true;
	Stats stats;
	bool shutdownFlag = false;
	tcp_connector* con;
signals:
	void retServerVersion(QString ver);
	void socketConnected();
	void socketDisconnected();
	void frameReceived(PS2ClientWorker::Vsync_Frame frame, unsigned char* data);
public slots:
	void cmdServerVersion();
	void cmdExecuteDump(QByteArray data);
	void connectSocket(QString hostname);
};

class PS2ClientController : public QObject
{
	Q_OBJECT
	PS2ClientWorker worker;
	QThread workerThread;
	tcp_connector con;

public:
	PS2ClientController();
	~PS2ClientController();
	void retrieveServerVersion();
	void setReplay(bool replay)
	{
		worker.replay = replay;
	};

	PS2ClientWorker::Stats* fetchWorkerStats()
	{
		return &worker.stats;
	};

	void startDump(QString file);

	void connectSocket(QString hostname);
	void disconnectSocket()
	{
		worker.shutdownFlag = true;
	};
signals:
	void socketDisconnected();
	void socketConnected();
	void retServerVersion(QString ver);
	void frameReceived(PS2ClientWorker::Vsync_Frame frame, unsigned char* data);
};
