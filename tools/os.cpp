#include <QApplication>
#include <QBitmap>
#include <QDesktopWidget>
#include <QDialog>
#include <QDir>
#include <QSettings>
#include <QLibrary>
#include <QPixmap>
#include <QTextEdit>
#include <QTranslator>
#include <QTimer>
#include <QTimeLine>
#include <QWidget>
#include <QGraphicsDropShadowEffect>
#include <string>
#include <QDesktopServices>
#include <QPointer>
#include <QProcess>
#include <QUrl>
#include <QDebug>

#include <QMessageBox>

#include "qtwin.h"

#if defined(Q_WS_WIN)
  #include <qt_windows.h>
  #include <shlobj.h>
#endif

#include "os.h"

void os::addToRecentDocuments(QString fileName)
{
#ifdef Q_WS_WIN
  QT_WA ( {
      SHAddToRecentDocs (0x00000003, QDir::toNativeSeparators(fileName).utf16());
    } , {
      SHAddToRecentDocs (0x00000002, QDir::toNativeSeparators(fileName).toLocal8Bit().data());
  } ); // QT_WA
#else
  Q_UNUSED(fileName)
#endif
}

bool os::aeroGlass(QWidget* target)
{
  if (QtWin::isCompositionEnabled()) {
    QtWin::extendFrameIntoClientArea(target);
    return true;
  }

  return false;
}

void os::setStartup(bool startup, bool hide)
{
  QString lightscreen = QDir::toNativeSeparators(qApp->applicationFilePath());

  if (hide)
    lightscreen.append(" -h");

#ifdef Q_WS_WIN
  // Windows startup settings
  QSettings init("Microsoft", "Windows");
  init.beginGroup("CurrentVersion");
  init.beginGroup("Run");

  if (startup) {
    init.setValue("Lightscreen", lightscreen);
  }
  else {
    init.remove("Lightscreen");
  }

  init.endGroup();
  init.endGroup();
#endif

#if defined(Q_WS_X11)
  QFile desktopFile(QDir::homePath() + "/.config/autostart/lightscreen.desktop");

  desktopFile.remove();

  if (startup) {
    desktopFile.open(QIODevice::WriteOnly);
    desktopFile.write(QString("[Desktop Entry]\nExec=%1\nType=Application").arg(lightscreen).toAscii());
  }
#endif
}

QString os::getDocumentsPath()
{
#ifdef Q_WS_WIN
  TCHAR szPath[MAX_PATH];

  if (SUCCEEDED(SHGetFolderPath(NULL,
                               CSIDL_PERSONAL|CSIDL_FLAG_CREATE,
                               NULL,
                               0,
                               szPath)))
  {
    std::wstring path(szPath);

    return QString::fromWCharArray(path.c_str());
  }

  return QDir::homePath() + QDir::separator() + "My Documents";
#else
  return QDir::homePath() + QDir::separator() + "Documents";
#endif
}

QPixmap os::grabWindow(WId winId)
{
#ifdef Q_WS_WIN
  RECT rcWindow;
  GetWindowRect(winId, &rcWindow);

  if (IsZoomed(winId)) {
    if (QSysInfo::WindowsVersion >= QSysInfo::WV_VISTA) {
      // TODO: WTF!
      rcWindow.right -= 8;
      rcWindow.left += 8;
      rcWindow.top += 8;
      rcWindow.bottom -= 8;
    }
    else {
      rcWindow.right += 4;
      rcWindow.left -= 4;
      rcWindow.top += 4;
      rcWindow.bottom -= 4;
    }
  }

  int width, height;
  width = rcWindow.right - rcWindow.left;
  height = rcWindow.bottom - rcWindow.top;

  RECT rcScreen;
  GetWindowRect(GetDesktopWindow(), &rcScreen);

  RECT rcResult;
  UnionRect(&rcResult, &rcWindow, &rcScreen);

  QPixmap pixmap;

  // Comparing the rects to determine if the window is outside the boundaries of the screen,
  // the window DC method has the disadvantage that it does not show Aero glass transparency,
  // so we'll avoid it for the screenshots that don't need it.

  if (EqualRect(&rcScreen, &rcResult)) {
    // Grabbing the window from the Screen DC.
    HDC hdcScreen = GetDC(NULL);

    BringWindowToTop(winId);

    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hbmCapture = CreateCompatibleBitmap(hdcScreen, width, height);
    SelectObject(hdcMem, hbmCapture);

    BitBlt(hdcMem, 0, 0, width, height, hdcScreen, rcWindow.left, rcWindow.top, SRCCOPY);

    ReleaseDC(winId, hdcMem);
    DeleteDC(hdcMem);

    pixmap = QPixmap::fromWinHBITMAP(hbmCapture);

    DeleteObject(hbmCapture);
  }
  else {
    // Grabbing the window by its own DC
    HDC hdcWindow = GetWindowDC(winId);

    HDC hdcMem = CreateCompatibleDC(hdcWindow);
    HBITMAP hbmCapture = CreateCompatibleBitmap(hdcWindow, width, height);
    SelectObject(hdcMem, hbmCapture);

    BitBlt(hdcMem, 0, 0, width, height, hdcWindow, 0, 0, SRCCOPY);

    ReleaseDC(winId, hdcMem);
    DeleteDC(hdcMem);

    pixmap = QPixmap::fromWinHBITMAP(hbmCapture);

    DeleteObject(hbmCapture);
  }

  return pixmap;
#else
  return QPixmap::grabWindow(winId);
#endif
}

void os::setForegroundWindow(QWidget *window)
{
#ifdef Q_WS_WIN
  ShowWindow(window->winId(), SW_RESTORE);
  SetForegroundWindow(window->winId());
#else
  Q_UNUSED(window)
#endif
}

QPixmap os::cursor()
{
#ifdef Q_WS_WIN
  /*
   * Taken from: git://github.com/arrai/mumble-record.git � src � mumble � Overlay.cpp
   * BSD License.
   */
  QPixmap pm;

  CURSORINFO cursorInfo;
  cursorInfo.cbSize = sizeof(cursorInfo);
  ::GetCursorInfo(&cursorInfo);

  HICON c = cursorInfo.hCursor;

  ICONINFO info;
  ZeroMemory(&info, sizeof(info));
  if (::GetIconInfo(c, &info)) {
          if (info.hbmColor) {
                  pm = QPixmap::fromWinHBITMAP(info.hbmColor);
                  pm.setMask(QBitmap(QPixmap::fromWinHBITMAP(info.hbmMask)));
          }
          else {
                  QBitmap orig(QPixmap::fromWinHBITMAP(info.hbmMask));
                  QImage img = orig.toImage();

                  int h = img.height() / 2;
                  int w = img.bytesPerLine() / sizeof(quint32);

                  QImage out(img.width(), h, QImage::Format_MonoLSB);
                  QImage outmask(img.width(), h, QImage::Format_MonoLSB);

                  for (int i=0;i<h; ++i) {
                          const quint32 *srcimg = reinterpret_cast<const quint32 *>(img.scanLine(i + h));
                          const quint32 *srcmask = reinterpret_cast<const quint32 *>(img.scanLine(i));

                          quint32 *dstimg = reinterpret_cast<quint32 *>(out.scanLine(i));
                          quint32 *dstmask = reinterpret_cast<quint32 *>(outmask.scanLine(i));

                          for (int j=0;j<w;++j) {
                                  dstmask[j] = srcmask[j];
                                  dstimg[j] = srcimg[j];
                          }
                  }
                  pm = QBitmap::fromImage(out);
          }

          if (info.hbmMask)
                  ::DeleteObject(info.hbmMask);

          if (info.hbmColor)
                  ::DeleteObject(info.hbmColor);
  }

  return pm;
#else
  return QPixmap();
#endif
}

void os::translate(QString language)
{
  static QTranslator *translator = 0;

  if ((language.compare("English", Qt::CaseInsensitive) == 0
      || language.isEmpty()) && translator) {
    qApp->removeTranslator(translator);
    return;
  }

  if (translator)
    delete translator;

  translator = new QTranslator(qApp);

  if (translator->load(language, ":/translations"))
    qApp->installTranslator(translator);
}

void os::effect(QObject* target, const char *slot, int frames, int duration, const char* cleanup)
{
  QTimeLine* timeLine = new QTimeLine(duration);
  timeLine->setFrameRange(0, frames);

  timeLine->connect(timeLine, SIGNAL(frameChanged(int)), target, slot);

  if (cleanup != 0)
    timeLine->connect(timeLine, SIGNAL(finished()), target, SLOT(cleanup()));

  timeLine->connect(timeLine, SIGNAL(finished()), timeLine, SLOT(deleteLater()));


  timeLine->start();
}

QGraphicsEffect* os::shadow(QColor color, int blurRadius, int offset) {
  QGraphicsDropShadowEffect* shadowEffect = new QGraphicsDropShadowEffect;
  shadowEffect->setBlurRadius(blurRadius);
  shadowEffect->setOffset(offset);
  shadowEffect->setColor(color);

  return shadowEffect;
}

