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
		if (val.length() > 5)
			ui->lblVersion->setText("Invalid\n");
		else
			ui->lblVersion->setText(val);
	});
	connect(ctrl, &PS2ClientController::frameReceived, this, &MainWindow::frameReceived);
	connect(ui->chkReplay, &QCheckBox::toggled, ctrl, &PS2ClientController::setReplay);

	readSettings();
}

MainWindow::~MainWindow()
{
	writeSettings();
	delete ui;
}

void MainWindow::readSettings()
{
	QSettings settings("Fobes", "GS4PS2QT");

	settings.beginGroup("MainWindow");
	const auto geometry = settings.value("geometry", QByteArray()).toByteArray();
	if (!geometry.isEmpty())
		restoreGeometry(geometry);


	const auto dumpPath = settings.value("dump-path", "").toString();
	ui->txtDumpPath->setText(dumpPath);

	const auto hostAddress = settings.value("host-address").toString();
	ui->txtHost->setText(hostAddress);

	const auto replay = settings.value("toggle-replay", true).toBool();
	ui->chkReplay->setChecked(replay);
}

void MainWindow::writeSettings()
{
	QSettings settings("Fobes", "GS4PS2QT");

	settings.beginGroup("MainWindow");

	settings.setValue("geometry", saveGeometry());
	settings.setValue("dump-path", ui->txtDumpPath->text());
	settings.setValue("host-path", ui->txtHost->text());
	settings.setValue("toggle-replay", ui->chkReplay->isChecked());
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
			ui->lblDumpPreview->setPixmap(QPixmap::fromImage(image.convertedTo(QImage::Format_RGB32)));
		}
		else
		{
			ui->lblDumpPreview->setPixmap(QPixmap(0, 0));
		}
	}
}

void MainWindow::listDumpFiles_itemDoubleClicked(QListWidgetItem* item)
{
	// Clear the frames
	c1Pixmaps.clear();
	c2Pixmaps.clear();
	ui->listC1Frames->clear();
	ui->listC2Frames->clear();
	ctrl->startDump(ui->txtDumpPath->text() + "/" + item->text());
}

void MainWindow::screenshotWidget_Clicked()
{
	QPixmap pixmap = ui->lblDumpPreview->pixmap();
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
	if (ctrl == nullptr)
		return;

	auto workerStats = ctrl->fetchWorkerStats();

	QString status("TRANS (BATCHED): %0 | TRANS (TOTAL): %1 | VSYNC: %2 | FIFO: %3 | REPLAY: %4");
	status = status.arg(workerStats->trans_batched_cnt).arg(workerStats->trans_cnt).arg(workerStats->vsync_cnt).arg(workerStats->fifo_cnt).arg(workerStats->replay_cnt);

	ui->mainStatusBar->showMessage(status);
}

void MainWindow::frameReceived(PS2ClientWorker::Vsync_Frame frame, unsigned char* data)
{
	QImage image;
	if (frame.PSM == 0)
	{
		image = QImage(frame.Width, frame.Height, QImage::Format_RGBA8888);
	}
	else if (frame.PSM == 1)
	{
		image = QImage(frame.Width, frame.Height, QImage::Format_RGB888);
	}
	else if (frame.PSM == 2)
	{
		image = QImage(frame.Width, frame.Height, QImage::Format_RGB16);
	}

	memcpy(image.bits(), data, frame.Bytes);
	if (frame.Circuit == 1)
	{
		int frame_index = c1Pixmaps.size();
		c1Pixmaps.emplace_back(QPixmap::fromImage(image.convertedTo(QImage::Format_RGB32)));
		QByteArray frameData((char*)data, frame.Bytes);

		QString frameInfoString("%0");
		frameInfoString = frameInfoString.arg(QString::number(qChecksum(frameData), 16), 4);

		QListWidgetItem* currentFrameItem = ui->listC1Frames->item(frame_index);
		if (currentFrameItem == nullptr)
			ui->listC1Frames->addItem(frameInfoString);
		else
			currentFrameItem->setText(frameInfoString);

		if (ui->btnPreviewC1->isChecked())
		{
			frameChangedByUser = false;
			ui->listC1Frames->setCurrentRow(frame_index);
		}
	}
	else
	{
		int frame_index = c2Pixmaps.size();
		c2Pixmaps.emplace_back(QPixmap::fromImage(image.convertedTo(QImage::Format_RGB32)));
		QByteArray frameData((char*)data, frame.Bytes);

		QString frameInfoString("%0");
		frameInfoString = frameInfoString.arg(QString::number(qChecksum(frameData), 16), 4);

		QListWidgetItem* currentFrameItem = ui->listC2Frames->item(frame_index);
		if (currentFrameItem == nullptr)
			ui->listC2Frames->addItem(frameInfoString);
		else
			currentFrameItem->setText(frameInfoString);

		if (ui->btnPreviewC2->isChecked())
		{
			frameChangedByUser = false;
			ui->listC2Frames->setCurrentRow(frame_index);
		}
	}


	delete data;
}

void MainWindow::listFrame_currentItemChanged(QListWidgetItem* item)
{
	if (!frameChangedByUser)
		frameChangedByUser = true;
	else
	{
		ui->btnPreviewC1->setAutoExclusive(false);
		ui->btnPreviewC1->setChecked(false);
		ui->btnPreviewC1->setAutoExclusive(true);

		ui->btnPreviewC2->setAutoExclusive(false);
		ui->btnPreviewC2->setChecked(false);
		ui->btnPreviewC2->setAutoExclusive(true);
	}

	QListWidget* caller = static_cast<QListWidget*>(sender());

	// currentRow returns -1 on no selected item (the list widget is cleared)
	if (caller->currentRow() < 0)
	{
		ui->lblCurrentFrameImg->setPixmap(QPixmap());
		return;
	}

	if (caller == ui->listC1Frames)
	{
		ui->lblCurrentFrameImg->setPixmap(c1Pixmaps.at(ui->listC1Frames->currentRow()));
	}
	else
	{
		ui->lblCurrentFrameImg->setPixmap(c2Pixmaps.at(ui->listC2Frames->currentRow()));
	}
}
