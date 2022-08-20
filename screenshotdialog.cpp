#include "screenshotdialog.h"

#include "ui_screenshotdialog.h"

ScreenshotDialog::ScreenshotDialog(QWidget *parent, QPixmap pixmap)
	: QDialog(parent), ui(new Ui::ScreenshotDialog)
{
	ui->setupUi(this);
	ui->lblScreenshot->setPixmap(pixmap);
	
	this->resize(pixmap.width(), pixmap.height());
}

ScreenshotDialog::~ScreenshotDialog()
{
	delete ui;
}
