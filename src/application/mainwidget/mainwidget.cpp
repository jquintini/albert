// albert - a simple application launcher for linux
// Copyright (C) 2014-2015 Manuel Schneider
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <QFile>
#include <QStandardPaths>
#include <QCursor>
#include <QDir>
#include <QDesktopWidget>
#include <QTimer>
#include <QDebug>
#include <QApplication>
#include <QTimer>
#include <QVBoxLayout>
#include <QAbstractItemModel>
#include "mainwidget.h"

const QString MainWidget::CFG_WND_POS  = "windowPosition";
const QString MainWidget::CFG_CENTERED = "showCentered";
const bool    MainWidget::DEF_CENTERED = true;
const QString MainWidget::CFG_THEME = "theme";
const QString MainWidget::DEF_THEME = "Standard";
const QString MainWidget::CFG_HIDE_ON_FOCUS_LOSS = "hideOnFocusLoss";
const bool    MainWidget::DEF_HIDE_ON_FOCUS_LOSS = true;

/** ***************************************************************************/
MainWidget::MainWidget(QWidget *parent)
    : QWidget(parent) {
	// INITIALIZE UI
    ui.setupUi(this);
    setWindowTitle(qAppName());
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(Qt::Tool
                   | Qt::WindowStaysOnTopHint
                   | Qt::WindowCloseButtonHint // No close event w/o this
                   | Qt::FramelessWindowHint);

    ui.bottomLayout->setSizeConstraint(QLayout::SetFixedSize);

    ui.bottomLayout->setAlignment (Qt::AlignHCenter | Qt::AlignTop);
    ui.topLayout->setAlignment    (Qt::AlignHCenter | Qt::AlignTop);
    ui.contentLayout->setAlignment(Qt::AlignHCenter | Qt::AlignTop);

    ui.bottomFrame->setSizePolicy (QSizePolicy::Expanding, QSizePolicy::Fixed);
    ui.topFrame->setSizePolicy    (QSizePolicy::Expanding, QSizePolicy::Fixed);
    ui.inputLine->setSizePolicy   (QSizePolicy::Expanding, QSizePolicy::Fixed);
    ui.proposalList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // Do not allow context menues (they cause focus out events)
    ui.inputLine->setContextMenuPolicy(Qt::NoContextMenu);
    ui.proposalList->setContextMenuPolicy(Qt::NoContextMenu);

    // Let proposalList not accept keyboard focus
    ui.proposalList->setFocusPolicy(Qt::NoFocus);

    // Let inputLine get focus when proposallist gets it.
    ui.proposalList->setFocusProxy(ui.inputLine);

    // Proposallistview intercepts inputline's events (Navigation with keys, pressed modifiers, etc)
    ui.inputLine->installEventFilter(ui.proposalList);

    // Hide list
    ui.proposalList->hide();

    // Settings
    QSettings s;
    showCentered_ = s.value(CFG_CENTERED, DEF_CENTERED).toBool();
    theme_ = s.value(CFG_THEME, DEF_THEME).toString();
    if (!setTheme(theme_)) {
        qFatal("FATAL: Stylefile not found: %s", theme_.toStdString().c_str());
        exit(EXIT_FAILURE);
    }
    if (s.contains(CFG_WND_POS) && s.value(CFG_WND_POS).canConvert(QMetaType::QPoint))
        move(s.value(CFG_WND_POS).toPoint());
}



/** ***************************************************************************/
MainWidget::~MainWidget() {
    // Save settings
    QSettings s;
    s.setValue(CFG_CENTERED, showCentered_);
    s.setValue(CFG_WND_POS, pos());
    s.setValue(CFG_THEME, theme_);
}



/** ***************************************************************************/
void MainWidget::show() {
    ui.inputLine->clear();
    // Move widget after showing it since QWidget::move works only on widgets
    // that have been shown once. Well as long as this does not introduce ugly
    // flicker this may be okay.
    QWidget::show();
    if (showCentered_){
        QDesktopWidget *dw = QApplication::desktop();
        this->move(dw->availableGeometry(dw->screenNumber(QCursor::pos())).center()
                   -QPoint(rect().right()/2,192 ));
    }
    this->raise();
    this->activateWindow();
    ui.inputLine->setFocus();
    emit widgetShown();
}



/** ***************************************************************************/
void MainWidget::hide() {
    QWidget::hide();
    emit widgetHidden();
}



/** ***************************************************************************/
void MainWidget::toggleVisibility() {
    this->isVisible() ? this->hide() : this->show();
}



/** ***************************************************************************/
void MainWidget::setModel(QAbstractItemModel *m) {
    QItemSelectionModel *sm = ui.proposalList->selectionModel();
    ui.proposalList->setModel(m);
    delete sm;
}



/** ***************************************************************************/
void MainWidget::setShowCentered(bool b) {
    showCentered_ = b;
}



/** ***************************************************************************/
bool MainWidget::showCentered() const {
    return showCentered_;
}



/** ***************************************************************************/
const QString &MainWidget::theme() const {
    return theme_;
}



/** ***************************************************************************/
bool MainWidget::setTheme(const QString &theme) {
    theme_ = theme;
    QFileInfoList themes;
    QStringList themeDirs = QStandardPaths::locateAll(
        QStandardPaths::DataLocation, "themes", QStandardPaths::LocateDirectory);
    for (QDir d : themeDirs)
        themes << d.entryInfoList(QStringList("*.qss"), QDir::Files | QDir::NoSymLinks);
    // Find and apply the theme
    bool success = false;
    for (QFileInfo fi : themes) {
        if (fi.baseName() == theme_) {
            QFile f(fi.canonicalFilePath());
            if (f.open(QFile::ReadOnly)) {
                qApp->setStyleSheet(f.readAll());
                f.close();
                success = true;
                break;
            }
        }
    }
    return success;
}



/** ***************************************************************************/
bool MainWidget::hideOnFocusLoss() const {
    return hideOnFocusLoss_;
}



/** ***************************************************************************/
void MainWidget::setHideOnFocusLoss(bool b) {
    hideOnFocusLoss_ = b;
}



/** ***************************************************************************/
void MainWidget::closeEvent(QCloseEvent *event) {
    event->accept();
    qApp->quit();
}



/** ***************************************************************************/
void MainWidget::keyPressEvent(QKeyEvent *e) {
    // Hide window on escape key
    if (e->modifiers() == Qt::NoModifier && e->key() == Qt::Key_Escape ) {
        hide();
        e->accept();
    }
    QWidget::keyPressEvent(e);
}



/** ***************************************************************************/
bool MainWidget::event(QEvent *event) {
    if (event->type() == QEvent::WindowDeactivate) {
        qDebug() << "deactivated";
        /* This is a horribly hackish but working solution.

         A triggered key grab on X11 steals the focus of the window for short
         period of time. This may result in the following annoying behaviour:
         When the hotkey is pressed and X11 steals the focus there arises a
         race condition between the hotkey event and the focus out event.
         When the app is visible and the focus out event is delivered the app
         gets hidden. Finally when the hotkey is received the app gets shown
         again although the user intended to hide the app with the hotkey.

         Solutions:
         Although X11 differs between the two focus out events, qt does not.
         One might install a native event filter and use the XCB structs to
         decide which type of event is delivered, but this approach is not
         platform independent (unless designed so explicitely, but its a
         hassle). The behaviour was expected when the app hides on:

         (mode==XCB_NOTIFY_MODE_GRAB && detail==XCB_NOTIFY_DETAIL_NONLINEAR)||
          (mode==XCB_NOTIFY_MODE_NORMAL && detail==XCB_NOTIFY_DETAIL_NONLINEAR)
         (Check Xlib Programming Manual)

         The current, much simpler but less elegant solution is to delay the
         hiding a few milliseconds, so that the hotkey event will always be
         handled first. */
        if (hideOnFocusLoss_){
            QTimer::singleShot(50, this, &MainWidget::hide);
        }
    }
    return QWidget::event(event);
}