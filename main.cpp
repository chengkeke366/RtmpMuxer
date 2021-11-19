#include <iostream>

#include <QtWidgets/QtWidgets>
#include <QtWidgets/QApplication>
#include "MainForm.h"

int main(int argc , char *argv[])
{
	QApplication a(argc, argv);
	MainForm form;
	form.show();
	a.exec();
	return 0;
}