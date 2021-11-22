#include <iostream>

#include <QtWidgets/QtWidgets>
#include <QtWidgets/QApplication>
#include "MainForm.h"


int main(int argc , char *argv[])
{
	QApplication a(argc, argv);
	std::cout << "main thread id:" << std::this_thread::get_id() << std::endl;;
	MainForm form;
	form.show();
	return a.exec();
}
