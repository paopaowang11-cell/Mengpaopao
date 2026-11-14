#ifndef CAMERA_WORKER_H
#define CAMERA_WORKER_H

#include <QImage>
#include <QMetaType>
#include <QMutex>
#include <QThread>
#include <QString>
#include <opencv2/opencv.hpp>
#include <vector>

struct CameraFrameData
{
    QImage image;
    cv::Mat source;
    std::vector<cv::Rect> faces;
};

Q_DECLARE_METATYPE(CameraFrameData)

class CameraWorker : public QThread
{
    Q_OBJECT

public:
    explicit CameraWorker(const QString &classifierPath, QObject *parent = 0);
    ~CameraWorker();

    void startCapture();
    void stopCapture();

signals:
    void frameReady(CameraFrameData frame);
    void cameraStatus(bool online, const QString &message);

protected:
    void run();

private:
    bool stopRequested();

    QString m_classifierPath;
    QMutex m_mutex;
    bool m_stop;
};

#endif // CAMERA_WORKER_H
