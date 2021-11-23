#ifndef FRMSWITCHBUTTON_H
#define FRMSWITCHBUTTON_H

#include <QtWidgets/QWidget>
#include <thread>
#include <memory>
#include <atomic>
#include <mutex>
namespace Ui
{
class MainForm;
}

class AVFormatContext;

class MainForm : public QWidget
{
	Q_OBJECT

public:
	explicit MainForm(QWidget *parent = 0);
	~MainForm();

public slots:
	void on_start_clicked();
	void on_stop_clicked();
private:
	Ui::MainForm*ui;
	AVFormatContext* m_input_fmt_ctx{nullptr};
	AVFormatContext* m_output_fmt_ctx{nullptr};
	QString m_output_filename;

	std::shared_ptr<std::thread> m_write_mp4_thread;
	bool m_bexit_record{ false };
};

#endif // frmSwitchButton_H
