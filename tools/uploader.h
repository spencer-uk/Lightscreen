#ifndef UPLOADER_H
#define UPLOADER_H

#include <QObject>
#include <QMap>
#include "qtimgur.h"

class Uploader : public QObject
{
    Q_OBJECT
public:
  Uploader(QObject *parent = 0);
  static Uploader* instance();
  QString lastUrl();

public slots:
  void upload(QString fileName);
  void uploaded(QString fileName, QString url);
  int  uploading();
  void imgurError(QtImgur::Error e);

signals:
  void done(QString, QString);
  void error();

private:
  static Uploader* mInstance;

  // Filename, url
  QMap<QString, QString> mScreenshots;
  QtImgur *mImgur;

  int mUploading;

};

#endif // UPLOADER_H