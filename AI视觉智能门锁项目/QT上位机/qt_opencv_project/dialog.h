#ifndef DIALOG_H
#define DIALOG_H

#include <QDialog>
#include <QByteArray>
#include <QDateTime>
#include <QElapsedTimer>
#include <QSerialPort>
#include <QTimer>
#include <opencv2/opencv.hpp>
#include <vector>

#include "camera_worker.h"
#include "recongnizer.h"

class QLabel;
class QLineEdit;
class QComboBox;
class QPushButton;
class QProgressBar;
class QTableWidget;

struct cmd_t
{
    int type;
    int lock_state;
    float temp;
};

enum CMD_TYPE
{
    LOCK_STATE = 0,
    TEMP_VAL
};

class Dialog : public QDialog
{
    Q_OBJECT

public:
    explicit Dialog(QWidget *parent = 0);
    ~Dialog();

private slots:
    void onFrameReady(CameraFrameData frame);
    void onCameraStatus(bool online, const QString &message);
    void serialSlot();
    void refreshSerialPorts();
    void toggleSerialConnection();
    void beginEnrollment();
    void cancelEnrollment();
    void studyTimerOut();
    void refreshUsers();
    void refreshAccessRecords();
    void commandTimeout();

private:
    void buildUi();
    QString classifierFilePath() const;
    void updateLockStatus();
    void updateSerialStatus();
    void processFaceRecognition();
    void sendOpenCommand(const QString &name);
    void addAccessRecord(const QString &name, const QString &action,
                         const QString &result);
    void handleSerialCommand(const cmd_t &command);

    CameraWorker *cameraWorker;
    QSerialPort serial;
    QByteArray rxBuffer;

    cv::Mat src;
    std::vector<cv::Rect> vec_findfaces;
    Recongnizer recongnizer;

    QTimer studyTimer;
    QTimer commandTimer;
    QElapsedTimer recognitionThrottle;

    QLabel *videoLabel;
    QLabel *cameraStatusLabel;
    QLabel *serialStatusLabel;
    QLabel *lockStateLabel;
    QLabel *temperatureValueLabel;
    QLabel *recognitionLabel;
    QLabel *enrollmentHintLabel;
    QLabel *userCountLabel;
    QLineEdit *nameEdit;
    QComboBox *serialPortCombo;
    QPushButton *serialConnectButton;
    QPushButton *enrollButton;
    QPushButton *cancelEnrollmentButton;
    QProgressBar *enrollmentProgress;
    QTableWidget *usersTable;
    QTableWidget *recordsTable;

    int lock_state;
    float cur_temp;
    bool lockStateKnown;
    bool temperatureKnown;
    bool openPending;
    bool studying;
    QString pendingOpenName;
    QString lastRecognitionLogged;
    QDateTime lastRecognitionLoggedAt;
    QDateTime lastOpenAttemptAt;
};

#endif // DIALOG_H
