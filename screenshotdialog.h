#pragma once

#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui { class ScreenshotDialog; }
QT_END_NAMESPACE

class ScreenshotDialog : public QDialog
{
	Q_OBJECT

	public:
		ScreenshotDialog(QWidget *parent, QPixmap pixmap);
		~ScreenshotDialog();

	private:
		Ui::ScreenshotDialog *ui;
};
