#include "camera_worker.h"

#include <QMutexLocker>
#include <opencv2/imgproc/imgproc.hpp>

CameraWorker::CameraWorker(const QString &classifierPath, QObject *parent)
    : QThread(parent),
      m_classifierPath(classifierPath),
      m_stop(false)
{
}

CameraWorker::~CameraWorker()
{
    stopCapture();
}

void CameraWorker::startCapture()
{
    if (isRunning())
    {
        return;
    }

    {
        QMutexLocker locker(&m_mutex);
        m_stop = false;
    }

    start();
}

void CameraWorker::stopCapture()
{
    {
        QMutexLocker locker(&m_mutex);
        m_stop = true;
    }

    if (isRunning())
    {
        wait();
    }
}

bool CameraWorker::stopRequested()
{
    QMutexLocker locker(&m_mutex);
    return m_stop;
}

void CameraWorker::run()
{
    cv::VideoCapture camera;
    if (!camera.open(0))
    {
        emit cameraStatus(false, QString::fromUtf8("未检测到摄像头"));
        return;
    }

    camera.set(CV_CAP_PROP_FRAME_WIDTH, 640);
    camera.set(CV_CAP_PROP_FRAME_HEIGHT, 480);

    cv::CascadeClassifier classifier;
    const QByteArray classifierFile = m_classifierPath.toLocal8Bit();
    const bool classifierReady = classifier.load(classifierFile.constData());
    if (!classifierReady)
    {
        emit cameraStatus(true, QString::fromUtf8("摄像头已连接，人脸检测器加载失败"));
    }
    else
    {
        emit cameraStatus(true, QString::fromUtf8("摄像头已连接"));
    }

    while (!stopRequested())
    {
        cv::Mat frame;
        if (!camera.read(frame) || frame.empty())
        {
            msleep(20);
            continue;
        }

        CameraFrameData packet;
        packet.source = frame.clone();

        if (classifierReady)
        {
            cv::Mat gray;
            cv::cvtColor(frame, gray, CV_BGR2GRAY);
            cv::equalizeHist(gray, gray);
            classifier.detectMultiScale(gray, packet.faces, 1.1, 3, 0,
                                        cv::Size(55, 55));

            for (size_t i = 0; i < packet.faces.size(); ++i)
            {
                cv::rectangle(frame, packet.faces[i], cv::Scalar(66, 220, 190), 2);
            }
        }

        cv::Mat rgb;
        cv::cvtColor(frame, rgb, CV_BGR2RGB);
        QImage view(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step),
                    QImage::Format_RGB888);
        packet.image = view.copy();

        emit frameReady(packet);
        msleep(50);
    }

    camera.release();
    emit cameraStatus(false, QString::fromUtf8("摄像头已停止"));
}
