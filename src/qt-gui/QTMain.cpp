#include "QTMain.h"

#include <QApplication>
#include <QFileInfo>

#include <boost/bind.hpp>

#include "MainWindow.h"

using namespace std;

// implementation class (PIMPL idiom)
class QTImpl {
	public:
		QApplication app;
		MainWindow wnd;

		QTImpl(int argc, char **argv, UFTTSettingsRef settings)
			: app(argc, argv), wnd(settings)
		{
			if (app.arguments().empty()) throw std::runtime_error("Invalid argument list");
			QObject::connect(&app, SIGNAL(focusChanged(QWidget*, QWidget*)), &wnd, SLOT(onFocusChanged(QWidget*, QWidget*)));
		};

	private:
		QTImpl(const QTImpl&);
};

QTMain::QTMain(int argc, char **argv, UFTTSettingsRef settings)
{
	#ifdef Q_OS_WIN
	{
		// this should really be inside qt...
		HMODULE hmod = 0;
		if (hmod == 0) hmod = GetModuleHandleA("Qt5Core.dll");
		if (hmod == 0) hmod = GetModuleHandleA("Qt5Cored.dll");
		if (hmod != 0) {
			QString Path;
			wchar_t module_name[256] = { 0 };
			GetModuleFileNameW(hmod, module_name, sizeof(module_name) / sizeof(wchar_t));
			Path = QString::fromUtf16((ushort*) module_name);
			if (!Path.isEmpty()) {
				Path = QFileInfo(Path).absolutePath();
				QCoreApplication::addLibraryPath(Path + "/../plugins");
			}
		}
	}
	#endif
	impl = new QTImpl(argc, argv, settings);
}

QTMain::~QTMain()
{
	delete impl;
}

void QTMain::bindEvents(UFTTCore* core)
{
	impl->wnd.SetBackend(core);
}

int QTMain::run()
{
	impl->wnd.pre_show();
	impl->wnd.show();
	impl->wnd.post_show();
	return impl->app.exec();
}

#define LINK_QT_RESOURCE(name) \
	do { \
		extern int qInitResources_ ## name (); \
		qInitResources_ ## name (); \
	} while (0)

const std::shared_ptr<UFTTGui> UFTTGui::makeGui(int argc, char** argv, UFTTSettingsRef settings) {
	LINK_QT_RESOURCE(icons);
	return std::shared_ptr<UFTTGui>(new QTMain(argc, argv, settings));
}
