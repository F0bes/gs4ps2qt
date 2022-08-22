#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "screenshotdialog.h"

#include <QtConcurrent/QtConcurrent>
#include <QFileDialog>
#include <QListWidget>
#include <QImage>
#include <QTimer>

#include "gsdump.h"

#ifdef WIN32
#include <Windows.h>
#endif

MainWindow::MainWindow(QWidget* parent)
	: QMainWindow(parent)
	, ui(new Ui::MainWindow)
	, workerTimer(new QTimer())
	, ctrl(new PS2ClientController())
{
	ui->setupUi(this);
	connect(workerTimer, &QTimer::timeout, this, &MainWindow::workerTimerStats);

	connect(ctrl, &PS2ClientController::socketConnected, this, &MainWindow::socketConnected);
	connect(ctrl, &PS2ClientController::socketDisconnected, this, &MainWindow::socketDisconnected);
	connect(ctrl, &PS2ClientController::retServerVersion, this, [this](QString val) {
		if(val.length() > 5)
			ui->lblVersion->setText("Invalid\n");
		else
			ui->lblVersion->setText(val);
	});
	connect(ui->chkReplay, &QCheckBox::toggled, ctrl, &PS2ClientController::setReplay);
}

MainWindow::~MainWindow()
{
	delete ui;
}

void MainWindow::chkUDPTTY_Changed(int state)
{
#ifdef WIN32
	if (state == Qt::Checked)
	{
		AllocConsole();
	}
	else
	{
		FreeConsole();
	}
#endif
}

void MainWindow::btnConnect_Clicked()
{
	if (ui->txtHost->text().isEmpty())
	{
		qDebug() << "There's no hostname!";
		return;
	}

	ctrl->connectSocket(ui->txtHost->text());

	ui->lblState->setText("Connecting");
}

void MainWindow::btnDisconnect_Clicked()
{
	ctrl->disconnectSocket();
}

void MainWindow::btnBrowse_Clicked()
{
	QString fileName = QFileDialog::getExistingDirectory(this, tr("Open Directory"),
		"",
		QFileDialog::ShowDirsOnly);
	ui->txtDumpPath->setText(fileName);
}

void MainWindow::txtDumpPath_TextChanged(QString text)
{
	QDir dir(text);
	QStringList filters;
	filters << "*.gs";
	dir.setNameFilters(filters);
	dumpFiles = dir.entryList();
	ui->listDumpFiles->clear();
	ui->listDumpFiles->addItems(dumpFiles);
}

void MainWindow::listDumpFiles_itemClicked(QListWidgetItem* item)
{
	if (currentSelectedItem != nullptr && currentSelectedItem == item)
		return;

	currentSelectedItem = item;

	QFile file(ui->txtDumpPath->text() + "/" + item->text());
	if (file.open(QIODevice::ReadOnly))
	{
		// TODO: don't load everything at once
		QByteArray data = file.readAll();
		QDataStream ds(data);
		ui->lblDumpSize->setText(QString::number(data.size()));

		GSDump::GSDumpHeader header = GSDump::ReadHeader(ds);
		ui->lblDumpType->setText(header.old ? "Old" : "New");
		ui->lblDumpCRC->setText(QString::number(header.crc, 16).toUpper());
		ui->lblDumpStateSize->setText(QString::number(header.state_size));
		ui->lblDumpStateVersion->setText(QString::number(header.state_version));
		if (header.screenshot_size > 0 && !header.old)
		{
			QImage image = QImage(header.screenshot_width, header.screenshot_height, QImage::Format_RGBA8888);

			memcpy(image.bits(), data.data() + header.screenshot_offset, header.screenshot_size);
			// Remove the alpha channel
			ui->screenshotWidget->setPixmap(QPixmap::fromImage(image.convertedTo(QImage::Format_RGB32)));
		}
		else
		{
			ui->screenshotWidget->setPixmap(QPixmap(0, 0));
		}
	}
}

void MainWindow::listDumpFiles_itemDoubleClicked(QListWidgetItem* item)
{
	ctrl->startDump(ui->txtDumpPath->text() + "/" + item->text());
}

void MainWindow::screenshotWidget_Clicked()
{
	QPixmap pixmap = ui->screenshotWidget->pixmap();
	ScreenshotDialog* dialog = new ScreenshotDialog(nullptr, pixmap);
	dialog->exec();
}

void MainWindow::socketDisconnected()
{
	workerTimer->stop();
	ui->lblState->setText("Not Connected");
}

void MainWindow::socketConnected()
{
	workerTimer->start(500);
	ui->lblState->setText("Connected");
	ctrl->retrieveServerVersion();
}

void MainWindow::workerTimerStats()
{
	if(ctrl == nullptr)
		return;

	auto workerStats = ctrl->fetchWorkerStats();

	ui->lblTransferCntBatched->setText(QString::number(workerStats->trans_batched_cnt));
	ui->lblTransferCntTotal->setText(QString::number(workerStats->trans_cnt));
	ui->lblVsyncCnt->setText(QString::number(workerStats->vsync_cnt));
	ui->lblFIFOCnt->setText(QString::number(workerStats->fifo_cnt));
	ui->lblReplayCnt->setText(QString::number(workerStats->replay_cnt));
	ui->lblPrivilegedSet->setText(QString::number(workerStats->reg_cnt));
}
