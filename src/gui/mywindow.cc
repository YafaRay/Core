/****************************************************************************
 *      mywindow.cc: the main window for the yafray GUI
 *      This is part of the yafray package
 *      Copyright (C) 2008 Gustavo Pichorim Boiko
 *		Copyright (C) 2009 Rodrigo Placencia Vazquez
 *
 *      This library is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU Lesser General Public
 *      License as published by the Free Software Foundation; either
 *      version 2.1 of the License, or (at your option) any later version.
 *
 *      This library is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *      Lesser General Public License for more details.
 *
 *      You should have received a copy of the GNU Lesser General Public
 *      License along with this library; if not, write to the Free Software
 *      Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

// YafaRay Headers
#include <gui/yafqtapi.h>
#include "mywindow.h"
#include "worker.h"
#include "qtoutput.h"
#include "renderwidget.h"
#include "ui_windowbase.h"
#include "events.h"
#include "animworking.h"
#include <interface/yafrayinterface.h>
#include <core_api/params.h>
#include <yafraycore/imageOutput.h>

// Embeded Resources:

// Images
#include <resources/yafarayicon.h>
#include <resources/toolbar_z_buffer_icon.h>
#include <resources/toolbar_alpha_icon.h>
#include <resources/toolbar_cancel_icon.h>
#include <resources/toolbar_save_as_icon.h>
#include <resources/toolbar_render_icon.h>
#include <resources/toolbar_show_alpha_icon.h>
#include <resources/toolbar_colorbuffer_icon.h>
#include <resources/toolbar_drawparams_icon.h>
#include <resources/toolbar_savedepth_icon.h>
#include <resources/toolbar_zoomin_icon.h>
#include <resources/toolbar_zoomout_icon.h>
#include <resources/toolbar_quit_icon.h>

// GUI Font
#if !defined(__APPLE__) && defined(YAFQT_EMBEDED_FONT)
	#include <resources/guifont.h>
#endif

// End of resources inclusion

// Standard Headers
#include <iostream>
#include <string>
#include <algorithm>

// Qt Headers
#include <QtGui/QHBoxLayout>
#include <QtGui/QScrollArea>
#include <QtGui/QFileDialog>
#include <QtCore/QDir>
#include <QtGui/QImageWriter>
#include <QtGui/QErrorMessage>
#include <QtGui/QMessageBox>
#include <QtGui/QKeyEvent>
#include <QtGui/QDesktopWidget>
#include <QtGui/QFontDatabase>
#include <QtCore/QSettings>

static QApplication *app=0;

/**************************
 *
 * yafqtapi functions
 *
 *************************/

void initGui()
{
	static int argc=0;

	if(!QApplication::instance())
	{
#if defined(__APPLE__) 
		QApplication::instance()->setAttribute(Qt::AA_MacPluginApplication);
		QApplication::instance()->setAttribute(Qt::AA_DontUseNativeMenuBar);
#endif
		using namespace yafaray;
		Y_INFO << "Starting Qt graphical interface..." << yendl;
		app = new QApplication(argc, 0);
	}
	else app = static_cast<QApplication*>(QApplication::instance());
}

int createRenderWidget(yafaray::yafrayInterface_t *interf, int xsize, int ysize, int bStartX, int bStartY, Settings settings)
{
	MainWindow w(interf, xsize, ysize, bStartX, bStartY, settings);
	w.show();
	w.adjustWindow();
	w.slotRender();

	return app->exec();
}


MainWindow::MainWindow(yafaray::yafrayInterface_t *env, int resx, int resy, int bStartX, int bStartY, Settings settings)
: QMainWindow(), interf(env), res_x(resx), res_y(resy), b_x(bStartX), b_y(bStartY), use_zbuf(false)
{
	QCoreApplication::setOrganizationName("YafaRay Team");
	QCoreApplication::setOrganizationDomain("yafaray.org");
	QCoreApplication::setApplicationName("YafaRay Qt Gui");

	QSettings set;

	askUnsaved = set.value("qtGui/askSave", true).toBool();

	QPixmap yafIcon;
	QPixmap zbuffIcon;
	QPixmap alphaIcon;
	QPixmap cancelIcon;
	QPixmap saveAsIcon;
	QPixmap renderIcon;
	QPixmap showAlphaIcon;
	QPixmap showColorIcon;
	QPixmap saveDepthIcon;
	QPixmap drawParamsIcon;
	QPixmap zoomInIcon;
	QPixmap zoomOutIcon;
	QPixmap quitIcon;

	yafIcon.loadFromData(yafarayicon, yafarayicon_size);
	zbuffIcon.loadFromData(z_buf_icon, z_buf_icon_size);
	alphaIcon.loadFromData(alpha_icon, alpha_icon_size);
	cancelIcon.loadFromData(cancel_icon, cancel_icon_size);
	saveAsIcon.loadFromData(saveas_icon, saveas_icon_size);
	renderIcon.loadFromData(render_icon, render_icon_size);
	showAlphaIcon.loadFromData(show_alpha_icon, show_alpha_icon_size);
	showColorIcon.loadFromData(rgb_icon, rgb_icon_size);
	saveDepthIcon.loadFromData(save_z_buf_icon, save_z_buf_icon_size);
	drawParamsIcon.loadFromData(drawparams_icon, drawparams_icon_size);
	zoomInIcon.loadFromData(zoomin_icon, zoomin_icon_size);
	zoomOutIcon.loadFromData(zoomout_icon, zoomout_icon_size);
	quitIcon.loadFromData(quit_icon, quit_icon_size);

#if !defined(__APPLE__) && defined(YAFQT_EMBEDED_FONT)
	int fId = QFontDatabase::addApplicationFontFromData(QByteArray((const char*)guifont, guifont_size));
	QStringList fam = QFontDatabase::applicationFontFamilies(fId);
	QFont gFont = QFont(fam[0]);
	gFont.setPointSize(8);
	QApplication::setFont(gFont);
#else
	#if defined(__APPLE__)
	QFont gFont = QApplication::font();
	gFont.setPointSize(13);
	QApplication::setFont(gFont);
	#endif
#endif

	m_ui = new Ui::WindowBase();
	m_ui->setupUi(this);

	setWindowIcon(QIcon(yafIcon));

	renderSaved = false;
	renderCancelled = false;
	saveWithAlpha = false;
	saveWithDepth = false;

	m_ui->actionAskSave->setChecked(askUnsaved);

	yafaray::paraMap_t *p = interf->getRenderParameters();
	p->getParam("z_channel", use_zbuf);

	m_render = new RenderWidget(m_ui->renderArea, use_zbuf);
	m_output = new QtOutput(m_render);
	m_worker = new Worker(interf, this, m_output);

	m_output->setRenderSize(QSize(resx, resy));

	// animation widget
	anim = new AnimWorking(m_ui->renderArea);
	anim->resize(200,87);

	this->move(20, 20);

	m_ui->renderArea->setWidgetResizable(false);
	m_ui->renderArea->resize(resx, resy);
	m_ui->renderArea->setWidget(m_render);

	QPalette renderAreaPal;
	renderAreaPal = m_ui->renderArea->viewport()->palette();
	renderAreaPal.setColor(QPalette::Window, Qt::black);

	m_ui->renderArea->viewport()->setPalette(renderAreaPal);

	m_ui->cancelButton->setIcon(QIcon(cancelIcon));

	connect(m_ui->cancelButton, SIGNAL(clicked()), this, SLOT(slotCancel()));
	connect(m_worker, SIGNAL(finished()), this, SLOT(slotFinished()));

	// move the animwidget over the render area
	QRect r = anim->rect();
	r.moveCenter(m_ui->renderArea->rect().center());
	anim->move(r.topLeft());

	// Set toolbar icons
	m_ui->actionShowDepth->setIcon(QIcon(zbuffIcon));
	m_ui->actionSaveAlpha->setIcon(QIcon(alphaIcon));
	m_ui->actionCancel->setIcon(QIcon(cancelIcon));
	m_ui->actionSave_As->setIcon(QIcon(saveAsIcon));
	m_ui->actionRender->setIcon(QIcon(renderIcon));
	m_ui->actionShowAlpha->setIcon(QIcon(showAlphaIcon));
	m_ui->actionShowRGB->setIcon(QIcon(showColorIcon));
	m_ui->actionSaveDepth->setIcon(QIcon(saveDepthIcon));
	m_ui->actionDrawParams->setIcon(QIcon(drawParamsIcon));
	m_ui->actionZoom_In->setIcon(QIcon(zoomInIcon));
	m_ui->actionZoom_Out->setIcon(QIcon(zoomOutIcon));
	m_ui->actionQuit->setIcon(QIcon(quitIcon));

	// actions
	connect(m_ui->actionRender, SIGNAL(triggered(bool)),
			this, SLOT(slotRender()));
	connect(m_ui->actionCancel, SIGNAL(triggered(bool)),
			this, SLOT(slotCancel()));
	connect(m_ui->actionSave_As, SIGNAL(triggered(bool)),
			this, SLOT(slotSaveAs()));
	connect(m_ui->actionQuit, SIGNAL(triggered(bool)),
			this, SLOT(close()));
	connect(m_ui->actionZoom_In, SIGNAL(triggered(bool)),
			this, SLOT(zoomIn()));
	connect(m_ui->actionZoom_Out, SIGNAL(triggered(bool)),
			this, SLOT(zoomOut()));
	connect(m_ui->actionSaveAlpha, SIGNAL(triggered(bool)),
			this, SLOT(setAlpha(bool)));
	connect(m_ui->actionShowDepth, SIGNAL(triggered(bool)),
			this, SLOT(showDepth(bool)));
	connect(m_ui->actionShowAlpha, SIGNAL(triggered(bool)),
			this, SLOT(showAlpha(bool)));
	connect(m_ui->actionShowRGB, SIGNAL(triggered(bool)),
			this, SLOT(showColor(bool)));
	connect(m_ui->actionAskSave, SIGNAL(triggered(bool)),
			this, SLOT(setAskSave(bool)));
	connect(m_ui->actionSaveDepth, SIGNAL(triggered(bool)),
			this, SLOT(setSaveDepth(bool)));
	connect(m_ui->actionDrawParams, SIGNAL(triggered(bool)),
			this, SLOT(setDrawParams(bool)));

	m_ui->actionShowRGB->setChecked(true);
	useDrawParams = interf->getDrawParams();
	m_ui->actionDrawParams->setChecked(useDrawParams);

	// offset when using border rendering
	m_render->setRenderBorderStart( QPoint(bStartX, bStartY) );

	autoSave = settings.autoSave;
	autoSaveAlpha = settings.autoSaveAlpha;
	autoClose = settings.closeAfterFinish;
	saveWithAlpha = autoSaveAlpha;

	if (autoSave)
	{
		this->fileName = settings.fileName;
		this->setWindowTitle(this->windowTitle() + QString(" (") + QString(fileName.c_str()) + QString(")"));
	}

	// filter the resize events of the render area to center the animation widget
	m_ui->renderArea->installEventFilter(this);

	m_ui->actionShowDepth->setEnabled(use_zbuf);
	m_ui->actionSaveDepth->setEnabled(use_zbuf);
}

MainWindow::~MainWindow()
{
	delete m_output;
	delete m_render;
	delete m_worker;
	delete m_ui;
}

bool MainWindow::event(QEvent *e)
{
	if (e->type() == (QEvent::Type)ProgressUpdate)
	{
		ProgressUpdateEvent *p = static_cast<ProgressUpdateEvent*>(e);
		if (p->min() >= 0)
			m_ui->progressbar->setMinimum(p->min());
		if (p->max() >= 0)
			m_ui->progressbar->setMaximum(p->max());
		m_ui->progressbar->setValue(p->progress());
		return true;
	}

	if (e->type() == (QEvent::Type)ProgressUpdateTag)
	{
		ProgressUpdateTagEvent *p = static_cast<ProgressUpdateTagEvent*>(e);
		if(p->tag().contains("Rendering")) anim->hide();
		m_ui->yafLabel->setText(p->tag());
		return true;
	}

	return QMainWindow::event(e);
}

void MainWindow::closeEvent(QCloseEvent *e)
{
	if(!closeUnsaved())
	{
		e->ignore();
	}
	else
	{
		slotCancel();

		if(renderCancelled) app->exit(1); //check if a render was stopped and exit with the appropiate code
		e->accept();
	}

}

void MainWindow::slotRender()
{
	slotEnableDisable(false);
	m_ui->progressbar->show();
	timeMeasure.start();
	m_ui->yafLabel->setText(tr("Rendering image..."));
	m_render->startRendering();
	m_ui->actionShowRGB->setChecked(true);
	m_ui->actionShowDepth->setChecked(false);
	m_ui->actionShowAlpha->setChecked(false);
	renderSaved = false;
	renderCancelled = false;
	m_worker->start();
}

void MainWindow::slotFinished()
{
	using namespace yafaray;
	QString rt = "";

	if (autoSave)
	{
		Y_INFO << " Image saved to " << fileName;
		if (autoSaveAlpha) std::cout << " with alpha" << yendl;
		else std::cout << " without alpha" << yendl;

		using namespace yafaray;

		interf->paramsClearAll();
		interf->paramsSetString("type", "png");
		interf->paramsSetInt("width", res_x);
		interf->paramsSetInt("height", res_y);
		interf->paramsSetBool("alpha_channel", autoSaveAlpha);
		interf->paramsSetBool("z_channel", use_zbuf);

		imageHandler_t *ih = interf->createImageHandler("saver", false);
		imageOutput_t *out = new imageOutput_t(ih, fileName, b_x, b_y);

		interf->paramsClearAll();

		interf->getRenderedImage(*out);

		renderSaved = true;

		rt = QString("Image Auto-saved. ");

		delete ih;
		delete out;

		if (autoClose)
		{
			if(renderCancelled) app->exit(1);
			else app->quit();
			return;
		}
	}

	int renderTime = timeMeasure.elapsed();
	float timeSec = renderTime / 1000.f;

	int ms = renderTime % 1000;
	renderTime = renderTime / 1000;
	int s = renderTime % 60;
	renderTime = renderTime / 60;
	int m = renderTime % 60;
	int h = renderTime / 60;

	QString timeStr = "";
	QChar fill = '0';
	QString suffix = "";

	if(h > 0)
	{
		timeStr.append(QString("%1:").arg(h));
		suffix = "h.";
	}

	if(m > 0 || h > 0)
	{
		if(h > 0) timeStr.append(QString("%1:").arg(m, 2, 10, fill));
		else timeStr.append(QString("%1:").arg(m));

		if(suffix == "") suffix = "m.";
	}

	if(s < 10 && m == 0 && h == 0) timeStr.append(QString("%1.%2").arg(s).arg(ms, 2, 10, fill));
	else timeStr.append(QString("%1.%2").arg(s, 2, 10, fill).arg(ms, 2, 10, fill));

	if(suffix == "") suffix = "s.";

	timeStr.append(QString(" %1").arg(suffix));

	rt.append(QString("Render time: %1 [%2s.]").arg(timeStr).arg(timeSec, 5));
	m_ui->yafLabel->setText(rt);
	Y_INFO << setColor(Green, true) << "Render completed!" << setColor() << yendl;

	m_render->finishRendering();
	update();

	slotEnableDisable(true);

	if (autoClose)
	{
		if(renderCancelled) app->exit(1);
		else app->quit();
		return;
	}

	m_ui->progressbar->hide();

	QApplication::alert(this);
}


void MainWindow::slotEnableDisable(bool enable)
{
	m_ui->actionRender->setVisible(enable);
	m_ui->cancelButton->setVisible(!enable);
	m_ui->actionCancel->setVisible(!enable);
 	m_ui->actionZoom_In->setEnabled(enable);
 	m_ui->actionZoom_Out->setEnabled(enable);
 	m_ui->actionDrawParams->setEnabled(enable);
}

void MainWindow::setAlpha(bool checked)
{
	saveWithAlpha = checked;
}

void MainWindow::showColor(bool checked)
{
	if(checked)
	{
		m_render->paintColorBuffer();
		m_ui->actionShowDepth->setChecked(false);
		m_ui->actionShowAlpha->setChecked(false);
	}
	else m_ui->actionShowRGB->setChecked(true);
}

void MainWindow::showAlpha(bool checked)
{
	if(checked)
	{
		m_render->paintAlpha();
		m_ui->actionShowDepth->setChecked(false);
		m_ui->actionShowRGB->setChecked(false);
	}
	else m_ui->actionShowAlpha->setChecked(true);
}

void MainWindow::showDepth(bool checked)
{
	if(checked)
	{
		m_render->paintDepth();
		m_ui->actionShowAlpha->setChecked(false);
		m_ui->actionShowRGB->setChecked(false);
	}
	else m_ui->actionShowDepth->setChecked(true);
}

void MainWindow::setAskSave(bool checked)
{
	QSettings set;
	askUnsaved = checked;
	set.setValue("qtGui/askSave", askUnsaved);
}

void MainWindow::setSaveDepth(bool checked)
{
	saveWithDepth = checked;
}

void MainWindow::setDrawParams(bool checked)
{
	useDrawParams = checked;
	if(!m_render->isRendering())
	{
		interf->setDrawParams(useDrawParams);
		interf->getRenderedImage(*m_output);
		showColor(true);
	}
}

/*void MainWindow::slotOpen()
{
	if (m_lastPath.isNull())
		m_lastPath = QDir::currentPath();
	QString fileName = QFileDialog::getOpenFileName(this, tr("Open Yafaray Scene"), m_lastPath, tr("Yafaray Scenes (*.xml)"));

	if (m_worker->isRunning())
		m_worker->terminate();
	delete m_worker;
	m_worker = new Worker(interf, this, m_output);

	m_lastPath = QDir(fileName).absolutePath();
	slotEnableDisable(true);
}*/

/*void MainWindow::slotSave()
{
	if (m_outputPath.isNull())
	{
		saveDlg();
	}
}*/

void MainWindow::slotSaveAs()
{
	saveDlg();
}

bool MainWindow::saveDlg()
{
	QString formats;
	std::vector<std::string> formatList = interf->listImageHandlers();
	std::vector<std::string> formatDesc = interf->listImageHandlersFullName();

	std::sort(formatList.begin(), formatList.end());
	std::sort(formatDesc.begin(), formatDesc.end());

	for (size_t i = 0; i < formatList.size(); ++i)
	{
		formats += QString::fromStdString(formatDesc[i]) + " (*." + QString::fromStdString(formatList[i]) + ")";
		if(i < formatList.size() - 1 ) formats += ";;";
	}

	if (m_lastPath.isNull())
		m_lastPath = QDir::currentPath();

	QString selectedFilter;
	renderSaved = false;
	QString fileName = QFileDialog::getSaveFileName(this, tr("YafaRay Save Image"), m_lastPath,
			formats, &selectedFilter);

	// "re"extract the actual file ending
	selectedFilter.remove(0, selectedFilter.indexOf("."));
	selectedFilter.remove(selectedFilter.indexOf(")"), 2);

	if (!fileName.endsWith(selectedFilter, Qt::CaseInsensitive))
	{
		fileName.append(selectedFilter.toLower());
	}

	selectedFilter.remove(0, 1); // Remove the dot "."

	if (!fileName.isNull())
	{
		using namespace yafaray;
		interf->paramsClearAll();
		interf->paramsSetString("type", selectedFilter.toStdString().c_str());
		interf->paramsSetInt("width", res_x);
		interf->paramsSetInt("height", res_y);
		interf->paramsSetBool("alpha_channel", saveWithAlpha);
		interf->paramsSetBool("z_channel", saveWithDepth);

		m_lastPath = QDir(fileName).absolutePath();

		imageHandler_t *ih = interf->createImageHandler("saver", false);
		imageOutput_t *out = new imageOutput_t(ih, m_lastPath.toStdString(), b_x, b_y);

		interf->paramsClearAll();

		interf->setDrawParams(useDrawParams);

		interf->getRenderedImage(*out);

		renderSaved = true;

		QString savemesg;
		savemesg.append("Render ");
		savemesg.append(( (use_zbuf) ? "(RGBA + Z) " : "(RGBA) " ));
		savemesg.append("saved.");

		m_ui->yafLabel->setText(savemesg);

		delete ih;
		delete out;
	}

	return renderSaved;
}

bool MainWindow::closeUnsaved()
{
	if(!renderSaved && !m_render->isRendering() && askUnsaved)
	{
		QMessageBox msgBox(QMessageBox::Question, "YafaRay Question", "The render hasn't been saved, if you close, it will be lost.",
						   QMessageBox::NoButton, this);

		msgBox.setInformativeText("Do you want to save your render before closing?");
		QPushButton *discard = msgBox.addButton("Close without Saving", QMessageBox::DestructiveRole);
		QPushButton *save = msgBox.addButton("Save", QMessageBox::AcceptRole);
		QPushButton *cancel = msgBox.addButton("Cancel", QMessageBox::RejectRole);
		msgBox.setDefaultButton(discard);

		msgBox.exec();

		if(msgBox.clickedButton() == save) return saveDlg();
		else if(msgBox.clickedButton() == cancel) return false;
	}
	return true;
}

void MainWindow::slotCancel()
{
	// cancel the render and cleanup, especially wait for the worker to finish up
	// (otherwise the app will crash (if this is followed by a quit))
	if(m_render->isRendering()) renderCancelled = true;

	interf->abort();
	m_worker->wait();
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
	if (event->key() == Qt::Key_Escape)	close();
}

bool MainWindow::eventFilter(QObject *obj, QEvent* event)
{
	if (event->type() == QEvent::Resize)
	{
		// move the animwidget over the render area
		QRect r = anim->rect();
		r.moveCenter(m_ui->renderArea->rect().center());
		anim->move(r.topLeft());
	}
	return QMainWindow::eventFilter(obj, event);
}

void MainWindow::zoomOut()
{
	m_render->zoomOut(QPoint(0,0));
}

void MainWindow::zoomIn()
{
	m_render->zoomIn(QPoint(0,0));
}

void MainWindow::adjustWindow()
{
 	QRect scrGeom = QApplication::desktop()->availableGeometry();

	int w = std::min(res_x + 10, scrGeom.width()-60);
	int h = std::min(res_y + 10, scrGeom.height()-160);

	m_ui->renderArea->setMaximumSize(w, h);
	m_ui->renderArea->setMinimumSize(w, h);

	adjustSize();
	resize(minimumSize());

	m_ui->renderArea->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
	m_ui->renderArea->setMinimumSize(0, 0);
}
