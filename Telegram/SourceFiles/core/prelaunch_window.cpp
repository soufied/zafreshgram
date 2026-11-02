/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "core/prelaunch_window.h"

#include "core/crash_reports.h"
#include "core/application.h"
#include "core/sandbox.h"
#include "core/update_checker.h"
#include "core/ui_integration.h"
#include "core/version.h"
#include "window/main_window.h"
#include "platform/platform_specific.h"
#include "base/zlib_help.h"

#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMenu>
#include <QtGui/QClipboard>
#include <QtGui/QContextMenuEvent>
#include <QtGui/QFontInfo>
#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>
#include <QtGui/QDesktopServices>
#include <QtCore/QStandardPaths>
#include <QtCore/QTimer>

#include "ayu/ayu_settings.h"


namespace {

constexpr auto kDefaultProxyPort = 80;

} // namespace

PreLaunchWindow *PreLaunchWindowInstance = nullptr;

PreLaunchWindow::PreLaunchWindow(QString title) {
	setWindowIcon(Window::CreateIcon());
	setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);

	setWindowTitle(title.isEmpty() ? u"zaFreshGram"_q : title);

	QPalette p(palette());
	p.setColor(QPalette::Window, QColor(255, 252, 245));
	setPalette(p);

	_size = QFontInfo(font()).pixelSize();

	int paddingVertical = (_size / 2);
	int paddingHorizontal = _size;
	int borderRadius = (_size / 5);
	setStyleSheet(uR"(
QPushButton {
	padding: %1px %2px;
	background-color: #fffcf5;
	border-radius: %3px;
}
QPushButton#confirm:hover,
QPushButton#cancel:hover {
	background-color: #ebecde;
	color: #57651a;
}
QPushButton#confirm {
	color: #57651a;
}
QPushButton#cancel {
	color: #aeaeae;
}
QLineEdit {
	border: 1px solid #e0e0e0;
	padding: 5px;
}
QLineEdit:focus {
	border: 2px solid #57651a;
	padding: 4px;
}
)"_q.arg(paddingVertical).arg(paddingHorizontal).arg(borderRadius));
	if (!PreLaunchWindowInstance) {
		PreLaunchWindowInstance = this;
	}
}

void PreLaunchWindow::activate() {
	setWindowState(windowState() & ~Qt::WindowMinimized);
	setVisible(true);
	Platform::ActivateThisProcess();
	raise();
	activateWindow();
}

PreLaunchWindow *PreLaunchWindow::instance() {
	return PreLaunchWindowInstance;
}

PreLaunchWindow::~PreLaunchWindow() {
	if (PreLaunchWindowInstance == this) {
		PreLaunchWindowInstance = nullptr;
	}
}

PreLaunchLabel::PreLaunchLabel(QWidget *parent) : QLabel(parent) {
	QFont labelFont(font());
	labelFont.setWeight(QFont::DemiBold);
	labelFont.setPixelSize(static_cast<PreLaunchWindow*>(parent)->basicSize());
	setFont(labelFont);

	QPalette p(palette());
	p.setColor(QPalette::WindowText, QColor(27, 28, 23));
	p.setColor(QPalette::Text, QColor(27, 28, 23));
	setPalette(p);
	show();
};

void PreLaunchLabel::setText(const QString &text) {
	QLabel::setText(text);
	updateGeometry();
	resize(sizeHint());
}

void PreLaunchLabel::contextMenuEvent(QContextMenuEvent *e) {
	const auto flags = textInteractionFlags();
	const auto selectable = flags
		& (Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
	if (!selectable) {
		e->ignore();
		return;
	}
	const auto accel = [](QKeySequence::StandardKey key) {
		return QCoreApplication::testAttribute(
				Qt::AA_DontShowShortcutsInContextMenus)
			? QString()
			: QChar('\t')
				+ QKeySequence(key).toString(QKeySequence::NativeText);
	};
	const auto menu = new QMenu(this);
	menu->setAttribute(Qt::WA_DeleteOnClose);

	const auto copy = menu->addAction(
		u"&Copy"_q + accel(QKeySequence::Copy));
	copy->setEnabled(hasSelectedText());
	connect(copy, &QAction::triggered, this, [=] {
		if (hasSelectedText()) {
			QGuiApplication::clipboard()->setText(selectedText());
		}
	});

	menu->addSeparator();

	const auto selectAll = menu->addAction(
		u"Select All"_q + accel(QKeySequence::SelectAll));
	selectAll->setEnabled(!text().isEmpty());
	connect(selectAll, &QAction::triggered, this, [=] {
		setSelection(0, text().size());
	});

	e->accept();
	menu->popup(e->globalPos());
}

PreLaunchInput::PreLaunchInput(QWidget *parent, bool password) : QLineEdit(parent) {
	QFont logFont(font());
	logFont.setPixelSize(static_cast<PreLaunchWindow*>(parent)->basicSize());
	setFont(logFont);

	QPalette p(palette());
	p.setColor(QPalette::Window, QColor(255, 252, 245));
	p.setColor(QPalette::Base, QColor(255, 252, 245));
	p.setColor(QPalette::WindowText, QColor(27, 28, 23));
	p.setColor(QPalette::Text, QColor(27, 28, 23));
	setPalette(p);

	QLineEdit::setTextMargins(0, 0, 0, 0);
	setContentsMargins(0, 0, 0, 0);
	if (password) {
		setEchoMode(QLineEdit::Password);
	}
	show();
};

PreLaunchLog::PreLaunchLog(QWidget *parent) : QTextEdit(parent) {
	QFont logFont(font());
	logFont.setPixelSize(static_cast<PreLaunchWindow*>(parent)->basicSize());
	setFont(logFont);

	QPalette p(palette());
	p.setColor(QPalette::WindowText, QColor(96, 96, 96));
	p.setColor(QPalette::Text, QColor(96, 96, 96));
	setPalette(p);

	setReadOnly(true);
	setFrameStyle(int(QFrame::NoFrame) | QFrame::Plain);
	viewport()->setAutoFillBackground(false);
	setContentsMargins(0, 0, 0, 0);
	document()->setDocumentMargin(0);
	show();
};

PreLaunchButton::PreLaunchButton(QWidget *parent, bool confirm) : QPushButton(parent) {
	setFlat(true);

	setObjectName(confirm ? "confirm" : "cancel");

	QFont closeFont(font());
	closeFont.setWeight(QFont::DemiBold);
	closeFont.setPixelSize(static_cast<PreLaunchWindow*>(parent)->basicSize());
	setFont(closeFont);

	setCursor(Qt::PointingHandCursor);
	show();
};

void PreLaunchButton::setText(const QString &text) {
	QPushButton::setText(text);
	updateGeometry();
	resize(sizeHint());
}

PreLaunchCheckbox::PreLaunchCheckbox(QWidget *parent) : QCheckBox(parent) {
	setTristate(false);
	setCheckState(Qt::Checked);

	QFont closeFont(font());
	closeFont.setWeight(QFont::DemiBold);
	closeFont.setPixelSize(static_cast<PreLaunchWindow*>(parent)->basicSize());
	setFont(closeFont);

	QPalette p(palette());
	p.setColor(QPalette::WindowText, QColor(96, 96, 96));
	p.setColor(QPalette::Text, QColor(96, 96, 96));
	setPalette(p);

	setCursor(Qt::PointingHandCursor);
	show();
};

void PreLaunchCheckbox::setText(const QString &text) {
	QCheckBox::setText(text);
	updateGeometry();
	resize(sizeHint());
}

NotStartedWindow::NotStartedWindow()
: _label(this)
, _log(this)
, _close(this) {
	_label.setText(u"Could not start zaFreshGram!\nYou can see complete log below:"_q);

	_log.setPlainText(Logs::full());

	connect(&_close, &QPushButton::clicked, [=] { close(); });
	_close.setText(u"Close"_q);

	QRect scr(QApplication::primaryScreen()->availableGeometry());
	move(scr.x() + (scr.width() / 6), scr.y() + (scr.height() / 6));
	updateControls();
	show();
}

void NotStartedWindow::updateControls() {
	_label.show();
	_log.show();
	_close.show();

	QRect scr(QApplication::primaryScreen()->availableGeometry());
	QSize s(scr.width() / 2, scr.height() / 2);
	if (s == size()) {
		resizeEvent(0);
	} else {
		resize(s);
	}
}

void NotStartedWindow::closeEvent(QCloseEvent *e) {
	deleteLater();
	Core::Quit();
}

void NotStartedWindow::resizeEvent(QResizeEvent *e) {
	int padding = _size;
	_label.setGeometry(padding, padding, width() - 2 * padding, _label.sizeHint().height());
	_log.setGeometry(padding, padding * 2 + _label.sizeHint().height(), width() - 2 * padding, height() - 4 * padding - _label.height() - _close.height());
	_close.setGeometry(width() - padding - _close.width(), height() - padding - _close.height(), _close.width(), _close.height());
}
