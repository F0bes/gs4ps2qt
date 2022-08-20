#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidgetItem>

#include "ps2client.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	MainWindow(QWidget *parent = nullptr);
	~MainWindow();

private slots:
	void chkUDPTTY_Changed(int state);
	void btnConnect_Clicked();
	void btnDisconnect_Clicked();
	void btnBrowse_Clicked();
	void txtDumpPath_TextChanged(QString text);

	void listDumpFiles_itemClicked(QListWidgetItem *item);
	void listDumpFiles_itemDoubleClicked(QListWidgetItem *item);

	void screenshotWidget_Clicked();

	void socketConnected();
	void socketDisconnected();

	void workerTimerStats();
private:
	Ui::MainWindow *ui;

	PS2ClientController* ctrl = nullptr;
	QStringList dumpFiles;
	QListWidgetItem* currentSelectedItem;
	QTimer* workerTimer;
};
#endif // MAINWINDOW_H
