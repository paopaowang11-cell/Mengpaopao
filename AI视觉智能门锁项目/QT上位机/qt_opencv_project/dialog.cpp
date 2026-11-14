#include "dialog.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QSerialPortInfo>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QMetaType>
#include <QPixmap>

#include "managerfactory.h"

namespace
{
QFrame *createCard(QWidget *parent)
{
    QFrame *card = new QFrame(parent);
    card->setObjectName("card");
    card->setFrameShape(QFrame::NoFrame);
    return card;
}

QLabel *createCaption(const QString &text, QWidget *parent)
{
    QLabel *label = new QLabel(text, parent);
    label->setObjectName("caption");
    return label;
}

QTableWidget *createTable(int columns, const QStringList &headers, QWidget *parent)
{
    QTableWidget *table = new QTableWidget(0, columns, parent);
    table->setHorizontalHeaderLabels(headers);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setAlternatingRowColors(true);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->setShowGrid(false);
    return table;
}
}

Dialog::Dialog(QWidget *parent)
    : QDialog(parent),
      cameraWorker(0),
      videoLabel(0),
      cameraStatusLabel(0),
      serialStatusLabel(0),
      lockStateLabel(0),
      temperatureValueLabel(0),
      recognitionLabel(0),
      enrollmentHintLabel(0),
      userCountLabel(0),
      nameEdit(0),
      serialPortCombo(0),
      serialConnectButton(0),
      enrollButton(0),
      cancelEnrollmentButton(0),
      enrollmentProgress(0),
      usersTable(0),
      recordsTable(0),
      lock_state(0),
      cur_temp(0.0f),
      lockStateKnown(false),
      temperatureKnown(false),
      openPending(false),
      studying(false)
{
    setWindowTitle(QString::fromUtf8("NOVA 智能门锁"));
    setMinimumSize(1020, 700);
    resize(1240, 820);

    buildUi();

    qRegisterMetaType<CameraFrameData>("CameraFrameData");
    connect(&serial, SIGNAL(readyRead()), this, SLOT(serialSlot()));
    connect(&studyTimer, SIGNAL(timeout()), this, SLOT(studyTimerOut()));
    connect(&commandTimer, SIGNAL(timeout()), this, SLOT(commandTimeout()));
    connect(serialConnectButton, SIGNAL(clicked()), this, SLOT(toggleSerialConnection()));
    connect(enrollButton, SIGNAL(clicked()), this, SLOT(beginEnrollment()));
    connect(cancelEnrollmentButton, SIGNAL(clicked()), this, SLOT(cancelEnrollment()));

    studyTimer.setInterval(500);
    commandTimer.setSingleShot(true);
    commandTimer.setInterval(3000);
    recognitionThrottle.start();

    // 确保 SQLite 连接先于用户列表与操作记录初始化。
    ManagerFactory::getMf();
    refreshUsers();
    refreshAccessRecords();
    refreshSerialPorts();
    updateLockStatus();
    updateSerialStatus();

    cameraWorker = new CameraWorker(classifierFilePath(), this);
    connect(cameraWorker, SIGNAL(frameReady(CameraFrameData)),
            this, SLOT(onFrameReady(CameraFrameData)), Qt::QueuedConnection);
    connect(cameraWorker, SIGNAL(cameraStatus(bool,QString)),
            this, SLOT(onCameraStatus(bool,QString)), Qt::QueuedConnection);
    cameraWorker->startCapture();
}

void Dialog::buildUi()
{
    setStyleSheet(
        "QDialog { background:#0b1020; color:#e6edf7; font-family:'Microsoft YaHei UI'; font-size:14px; }"
        "QWidget#topBar { background:#10182a; border-bottom:1px solid #202d44; }"
        "QLabel#brand { color:#f5f8ff; font-size:24px; font-weight:700; }"
        "QLabel#brandSub { color:#8493ad; font-size:10px; letter-spacing:2px; }"
        "QLabel#caption { color:#91a0b8; font-size:12px; }"
        "QLabel#sectionTitle { color:#f1f5fb; font-size:17px; font-weight:600; }"
        "QLabel#metricValue { color:#f5f8ff; font-size:28px; font-weight:700; }"
        "QLabel#metricUnit { color:#8b9ab3; font-size:13px; }"
        "QLabel#statusBadge { background:#1b2940; color:#b8c5d9; padding:7px 12px; border-radius:12px; font-size:12px; }"
        "QFrame#card { background:#131d30; border:1px solid #24324a; border-radius:14px; }"
        "QLabel#videoLabel { background:#080d18; border:1px solid #283751; border-radius:11px; color:#71819a; }"
        "QTabWidget::pane { border:0; background:#0b1020; }"
        "QTabBar::tab { background:transparent; color:#8d9bb3; padding:14px 22px; margin-right:5px; border-bottom:2px solid transparent; }"
        "QTabBar::tab:selected { color:#56e0ca; border-bottom:2px solid #43d9c2; }"
        "QTabBar::tab:hover { color:#dce7f5; }"
        "QLineEdit, QComboBox { background:#0c1424; color:#e6edf7; border:1px solid #2a3952; border-radius:8px; padding:10px 12px; selection-background-color:#178d85; }"
        "QComboBox QAbstractItemView { background:#111b2d; color:#e6edf7; selection-background-color:#1c776f; }"
        "QPushButton { color:#dce7f5; background:#1a2740; border:1px solid #2b3b56; border-radius:8px; padding:10px 15px; font-weight:600; }"
        "QPushButton:hover { background:#243550; border-color:#3b526f; }"
        "QPushButton:pressed { background:#172238; }"
        "QPushButton#primaryButton { background:#16a997; color:#071a1a; border:0; }"
        "QPushButton#primaryButton:hover { background:#26c1ad; }"
        "QPushButton#secondaryButton { background:#202d45; }"
        "QPushButton#dangerButton { color:#f3a0aa; background:#321d2c; border-color:#593042; }"
        "QTableWidget { background:#10192a; alternate-background-color:#131f32; color:#dce7f5; border:1px solid #26354d; border-radius:10px; selection-background-color:#174e55; selection-color:#ffffff; padding:4px; }"
        "QHeaderView::section { background:#17243a; color:#9aabc4; border:0; padding:11px; font-weight:600; }"
        "QProgressBar { background:#0c1424; color:#d8e4f4; border:1px solid #2a3952; border-radius:6px; text-align:center; height:13px; }"
        "QProgressBar::chunk { background:#29c4ad; border-radius:5px; }"
    );

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    QWidget *topBar = new QWidget(this);
    topBar->setObjectName("topBar");
    topBar->setFixedHeight(82);
    QHBoxLayout *topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(28, 10, 28, 10);

    QVBoxLayout *brandLayout = new QVBoxLayout;
    QLabel *brand = new QLabel(QString::fromUtf8("NOVA  /  智能门锁"), topBar);
    brand->setObjectName("brand");
    QLabel *brandSub = new QLabel("SMART ACCESS CONTROL", topBar);
    brandSub->setObjectName("brandSub");
    brandLayout->addWidget(brand);
    brandLayout->addWidget(brandSub);
    topLayout->addLayout(brandLayout);
    topLayout->addStretch();

    cameraStatusLabel = new QLabel(QString::fromUtf8("摄像头 · 启动中"), topBar);
    cameraStatusLabel->setObjectName("statusBadge");
    serialStatusLabel = new QLabel(QString::fromUtf8("串口 · 未连接"), topBar);
    serialStatusLabel->setObjectName("statusBadge");
    topLayout->addWidget(cameraStatusLabel);
    topLayout->addSpacing(10);
    topLayout->addWidget(serialStatusLabel);
    root->addWidget(topBar);

    QTabWidget *tabs = new QTabWidget(this);
    tabs->setDocumentMode(true);
    root->addWidget(tabs, 1);

    // 首页：视频监控与门锁状态
    QWidget *homePage = new QWidget(tabs);
    QHBoxLayout *homeLayout = new QHBoxLayout(homePage);
    homeLayout->setContentsMargins(22, 18, 22, 22);
    homeLayout->setSpacing(16);

    QFrame *videoCard = createCard(homePage);
    QVBoxLayout *videoLayout = new QVBoxLayout(videoCard);
    videoLayout->setContentsMargins(18, 16, 18, 18);
    videoLayout->setSpacing(12);
    QHBoxLayout *videoHeading = new QHBoxLayout;
    QLabel *videoTitle = new QLabel(QString::fromUtf8("实时识别"), videoCard);
    videoTitle->setObjectName("sectionTitle");
    QLabel *videoHint = createCaption(QString::fromUtf8("本地摄像头 · 人脸检测"), videoCard);
    videoHeading->addWidget(videoTitle);
    videoHeading->addStretch();
    videoHeading->addWidget(videoHint);
    videoLayout->addLayout(videoHeading);

    videoLabel = new QLabel(QString::fromUtf8("正在启动摄像头…"), videoCard);
    videoLabel->setObjectName("videoLabel");
    videoLabel->setAlignment(Qt::AlignCenter);
    videoLabel->setMinimumSize(560, 360);
    videoLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    videoLayout->addWidget(videoLabel, 1);

    recognitionLabel = new QLabel(QString::fromUtf8("等待人脸进入画面"), videoCard);
    recognitionLabel->setObjectName("statusBadge");
    recognitionLabel->setMinimumHeight(38);
    videoLayout->addWidget(recognitionLabel);
    homeLayout->addWidget(videoCard, 3);

    QWidget *sideColumn = new QWidget(homePage);
    sideColumn->setMinimumWidth(285);
    sideColumn->setMaximumWidth(360);
    QVBoxLayout *sideLayout = new QVBoxLayout(sideColumn);
    sideLayout->setContentsMargins(0, 0, 0, 0);
    sideLayout->setSpacing(14);

    QFrame *stateCard = createCard(sideColumn);
    QVBoxLayout *stateLayout = new QVBoxLayout(stateCard);
    stateLayout->setContentsMargins(18, 16, 18, 18);
    stateLayout->setSpacing(8);
    stateLayout->addWidget(createCaption(QString::fromUtf8("设备状态"), stateCard));
    lockStateLabel = new QLabel(QString::fromUtf8("等待设备上报"), stateCard);
    lockStateLabel->setObjectName("metricValue");
    lockStateLabel->setWordWrap(true);
    stateLayout->addWidget(lockStateLabel);
    QLabel *lockNote = createCaption(QString::fromUtf8("锁状态通过 STM32 串口更新"), stateCard);
    stateLayout->addWidget(lockNote);
    sideLayout->addWidget(stateCard);

    QFrame *tempCard = createCard(sideColumn);
    QVBoxLayout *tempLayout = new QVBoxLayout(tempCard);
    tempLayout->setContentsMargins(18, 16, 18, 18);
    tempLayout->setSpacing(7);
    tempLayout->addWidget(createCaption(QString::fromUtf8("当前温度"), tempCard));
    QHBoxLayout *tempRow = new QHBoxLayout;
    temperatureValueLabel = new QLabel(QString::fromUtf8("--.-"), tempCard);
    temperatureValueLabel->setObjectName("metricValue");
    QLabel *tempUnit = new QLabel(QString::fromUtf8("°C"), tempCard);
    tempUnit->setObjectName("metricUnit");
    tempRow->addWidget(temperatureValueLabel);
    tempRow->addWidget(tempUnit);
    tempRow->addStretch();
    tempLayout->addLayout(tempRow);
    tempLayout->addWidget(createCaption(QString::fromUtf8("温度数据来自门锁设备"), tempCard));
    sideLayout->addWidget(tempCard);

    QFrame *enrollmentCard = createCard(sideColumn);
    QVBoxLayout *enrollmentLayout = new QVBoxLayout(enrollmentCard);
    enrollmentLayout->setContentsMargins(18, 16, 18, 18);
    enrollmentLayout->setSpacing(10);
    QLabel *enrollmentTitle = new QLabel(QString::fromUtf8("录入新用户"), enrollmentCard);
    enrollmentTitle->setObjectName("sectionTitle");
    enrollmentLayout->addWidget(enrollmentTitle);
    nameEdit = new QLineEdit(enrollmentCard);
    nameEdit->setPlaceholderText(QString::fromUtf8("输入用户姓名"));
    enrollmentLayout->addWidget(nameEdit);
    enrollmentProgress = new QProgressBar(enrollmentCard);
    enrollmentProgress->setRange(0, 10);
    enrollmentProgress->setValue(0);
    enrollmentLayout->addWidget(enrollmentProgress);
    enrollmentHintLabel = createCaption(QString::fromUtf8("请正对摄像头，保持光线充足"), enrollmentCard);
    enrollmentHintLabel->setWordWrap(true);
    enrollmentLayout->addWidget(enrollmentHintLabel);
    QHBoxLayout *enrollButtons = new QHBoxLayout;
    enrollButton = new QPushButton(QString::fromUtf8("开始录入"), enrollmentCard);
    enrollButton->setObjectName("primaryButton");
    cancelEnrollmentButton = new QPushButton(QString::fromUtf8("取消"), enrollmentCard);
    cancelEnrollmentButton->setObjectName("secondaryButton");
    cancelEnrollmentButton->setEnabled(false);
    enrollButtons->addWidget(enrollButton, 2);
    enrollButtons->addWidget(cancelEnrollmentButton, 1);
    enrollmentLayout->addLayout(enrollButtons);
    sideLayout->addWidget(enrollmentCard, 1);
    homeLayout->addWidget(sideColumn, 1);
    tabs->addTab(homePage, QString::fromUtf8("首页"));

    // 用户列表
    QWidget *usersPage = new QWidget(tabs);
    QVBoxLayout *usersLayout = new QVBoxLayout(usersPage);
    usersLayout->setContentsMargins(24, 22, 24, 24);
    QFrame *usersCard = createCard(usersPage);
    QVBoxLayout *usersCardLayout = new QVBoxLayout(usersCard);
    usersCardLayout->setContentsMargins(20, 18, 20, 20);
    QHBoxLayout *usersHeading = new QHBoxLayout;
    QLabel *usersTitle = new QLabel(QString::fromUtf8("已录入用户"), usersCard);
    usersTitle->setObjectName("sectionTitle");
    userCountLabel = createCaption(QString::fromUtf8("0 位用户"), usersCard);
    QPushButton *usersRefreshButton = new QPushButton(QString::fromUtf8("刷新列表"), usersCard);
    usersRefreshButton->setObjectName("secondaryButton");
    connect(usersRefreshButton, SIGNAL(clicked()), this, SLOT(refreshUsers()));
    usersHeading->addWidget(usersTitle);
    usersHeading->addSpacing(10);
    usersHeading->addWidget(userCountLabel);
    usersHeading->addStretch();
    usersHeading->addWidget(usersRefreshButton);
    usersCardLayout->addLayout(usersHeading);
    usersTable = createTable(2, QStringList() << QString::fromUtf8("用户编号")
                                             << QString::fromUtf8("姓名"), usersCard);
    usersCardLayout->addWidget(usersTable, 1);
    usersLayout->addWidget(usersCard, 1);
    tabs->addTab(usersPage, QString::fromUtf8("用户管理"));

    // 开锁记录
    QWidget *recordsPage = new QWidget(tabs);
    QVBoxLayout *recordsLayout = new QVBoxLayout(recordsPage);
    recordsLayout->setContentsMargins(24, 22, 24, 24);
    QFrame *recordsCard = createCard(recordsPage);
    QVBoxLayout *recordsCardLayout = new QVBoxLayout(recordsCard);
    recordsCardLayout->setContentsMargins(20, 18, 20, 20);
    QHBoxLayout *recordsHeading = new QHBoxLayout;
    QLabel *recordsTitle = new QLabel(QString::fromUtf8("开锁与设备记录"), recordsCard);
    recordsTitle->setObjectName("sectionTitle");
    QPushButton *recordsRefreshButton = new QPushButton(QString::fromUtf8("刷新记录"), recordsCard);
    recordsRefreshButton->setObjectName("secondaryButton");
    connect(recordsRefreshButton, SIGNAL(clicked()), this, SLOT(refreshAccessRecords()));
    recordsHeading->addWidget(recordsTitle);
    recordsHeading->addStretch();
    recordsHeading->addWidget(recordsRefreshButton);
    recordsCardLayout->addLayout(recordsHeading);
    recordsTable = createTable(4, QStringList() << QString::fromUtf8("时间")
                                               << QString::fromUtf8("用户")
                                               << QString::fromUtf8("操作")
                                               << QString::fromUtf8("结果"), recordsCard);
    recordsCardLayout->addWidget(recordsTable, 1);
    recordsLayout->addWidget(recordsCard, 1);
    tabs->addTab(recordsPage, QString::fromUtf8("开锁记录"));

    // 设备设置
    QWidget *settingsPage = new QWidget(tabs);
    QVBoxLayout *settingsLayout = new QVBoxLayout(settingsPage);
    settingsLayout->setContentsMargins(24, 22, 24, 24);
    settingsLayout->addStretch();
    QFrame *settingsCard = createCard(settingsPage);
    settingsCard->setMaximumWidth(700);
    QVBoxLayout *settingsCardLayout = new QVBoxLayout(settingsCard);
    settingsCardLayout->setContentsMargins(24, 22, 24, 24);
    settingsCardLayout->setSpacing(15);
    QLabel *settingsTitle = new QLabel(QString::fromUtf8("串口连接"), settingsCard);
    settingsTitle->setObjectName("sectionTitle");
    settingsCardLayout->addWidget(settingsTitle);
    settingsCardLayout->addWidget(createCaption(QString::fromUtf8("选择连接 STM32 的串口。设备通信参数保持 115200 / 8-N-1。"), settingsCard));
    QHBoxLayout *portRow = new QHBoxLayout;
    serialPortCombo = new QComboBox(settingsCard);
    serialPortCombo->setEditable(true);
    serialPortCombo->setMinimumWidth(250);
    QPushButton *refreshPortsButton = new QPushButton(QString::fromUtf8("刷新端口"), settingsCard);
    refreshPortsButton->setObjectName("secondaryButton");
    serialConnectButton = new QPushButton(QString::fromUtf8("连接串口"), settingsCard);
    serialConnectButton->setObjectName("primaryButton");
    portRow->addWidget(serialPortCombo, 1);
    portRow->addWidget(refreshPortsButton);
    portRow->addWidget(serialConnectButton);
    settingsCardLayout->addLayout(portRow);
    connect(refreshPortsButton, SIGNAL(clicked()), this, SLOT(refreshSerialPorts()));
    settingsCardLayout->addWidget(createCaption(QString::fromUtf8("提示：串口被其他程序占用时，请先关闭占用程序。"), settingsCard));
    settingsLayout->addWidget(settingsCard, 0, Qt::AlignHCenter);
    settingsLayout->addStretch();
    tabs->addTab(settingsPage, QString::fromUtf8("设备设置"));
}

QString Dialog::classifierFilePath() const
{
    const QString appPath = QCoreApplication::applicationDirPath()
            + "/haarcascade_frontalface_alt2.xml";
    if (QFile::exists(appPath))
    {
        return appPath;
    }

    QSettings settings;
    const QString configuredPath = settings.value("camera/classifierPath").toString();
    if (!configuredPath.isEmpty() && QFile::exists(configuredPath))
    {
        return configuredPath;
    }

    return "E:/study/soft/opencv/opencv3.4-install/install/etc/haarcascades/haarcascade_frontalface_alt2.xml";
}

void Dialog::onFrameReady(CameraFrameData frame)
{
    if (frame.source.empty() || frame.image.isNull())
    {
        return;
    }

    src = frame.source;
    vec_findfaces = frame.faces;

    const QPixmap pixmap = QPixmap::fromImage(frame.image).scaled(
                videoLabel->size(), Qt::KeepAspectRatio, Qt::FastTransformation);
    videoLabel->setPixmap(pixmap);

    if (studying)
    {
        if (vec_findfaces.empty())
        {
            enrollmentHintLabel->setText(QString::fromUtf8("没有检测到人脸，请调整位置"));
        }
        return;
    }

    if (vec_findfaces.empty())
    {
        recognitionLabel->setText(QString::fromUtf8("等待人脸进入画面"));
        return;
    }

    if (recognitionThrottle.elapsed() >= 250)
    {
        recognitionThrottle.restart();
        processFaceRecognition();
    }
}

void Dialog::onCameraStatus(bool online, const QString &message)
{
    if (online && message.indexOf(QString::fromUtf8("失败")) < 0)
    {
        cameraStatusLabel->setText(QString::fromUtf8("摄像头 · 在线"));
    }
    else
    {
        cameraStatusLabel->setText(QString::fromUtf8("摄像头 · ") + message);
    }
    if (online && message.indexOf(QString::fromUtf8("失败")) < 0)
    {
        cameraStatusLabel->setStyleSheet("background:#123b39; color:#62e4ce; padding:7px 12px; border-radius:12px;");
    }
    else
    {
        cameraStatusLabel->setStyleSheet("background:#39202c; color:#ff9cac; padding:7px 12px; border-radius:12px;");
    }

    if (!online)
    {
        videoLabel->setPixmap(QPixmap());
        videoLabel->setText(message);
    }
}

void Dialog::processFaceRecognition()
{
    if (src.empty() || vec_findfaces.empty() || studying)
    {
        return;
    }

    const QString name = recongnizer.checkFace(src, vec_findfaces);
    if (name.isEmpty())
    {
        recognitionLabel->setText(QString::fromUtf8("检测到人脸 · 正在比对"));
        return;
    }

    // checkFace() 只有在 LBPH 标签能从 SQLite 的 user 表查到姓名时才返回非空姓名。
    // 因此这里不再限制某个固定姓名：数据库中识别成功的用户都可以触发开锁。
    if (lockStateKnown && lock_state == 1)
    {
        recognitionLabel->setText(QString::fromUtf8("已识别：%1 · 门锁已开启").arg(name));
    }
    else
    {
        recognitionLabel->setText(QString::fromUtf8("已识别：%1 · 数据库用户，正在开锁").arg(name));
        if (!lockStateKnown)
        {
            recognitionLabel->setText(QString::fromUtf8("已识别：%1 · 等待门锁状态后开锁").arg(name));
        }
        sendOpenCommand(name);
    }
}

void Dialog::sendOpenCommand(const QString &name)
{
    const QDateTime now = QDateTime::currentDateTime();
    if (lastOpenAttemptAt.isValid() && lastOpenAttemptAt.secsTo(now) < 5)
    {
        return;
    }
    lastOpenAttemptAt = now;

    if (!serial.isOpen())
    {
        recognitionLabel->setText(QString::fromUtf8("已识别：%1 · 请先连接串口").arg(name));
        addAccessRecord(name, QString::fromUtf8("人脸开锁"), QString::fromUtf8("串口未连接"));
        return;
    }
    // 状态已知且已开锁时不重复发送；状态未知时仍按用户要求发送开锁命令。
    if ((lockStateKnown && lock_state == 1) || openPending)
    {
        return;
    }

    const qint64 written = serial.write("open", 4);
    if (written == 4)
    {
        openPending = true;
        pendingOpenName = name;
        commandTimer.start();
        recognitionLabel->setText(QString::fromUtf8("已识别：%1 · 开锁指令已发送").arg(name));
        addAccessRecord(name, QString::fromUtf8("人脸开锁"), QString::fromUtf8("指令已发送，等待设备确认"));
    }
    else
    {
        recognitionLabel->setText(QString::fromUtf8("开锁指令发送失败"));
        addAccessRecord(name, QString::fromUtf8("人脸开锁"), QString::fromUtf8("串口发送失败"));
    }
}

void Dialog::serialSlot()
{
    rxBuffer.append(serial.readAll());

    while (rxBuffer.size() >= static_cast<int>(sizeof(cmd_t)))
    {
        cmd_t command;
        memcpy(&command, rxBuffer.constData(), sizeof(command));

        const bool validLock = command.type == LOCK_STATE
                && (command.lock_state == 0 || command.lock_state == 1);
        const bool validTemp = command.type == TEMP_VAL
                && command.temp >= -40.0f && command.temp <= 125.0f;

        if (validLock || validTemp)
        {
            rxBuffer.remove(0, static_cast<int>(sizeof(command)));
            handleSerialCommand(command);
        }
        else
        {
            // STM32 启动日志与二进制状态帧共用串口，逐字节寻找有效帧边界。
            rxBuffer.remove(0, 1);
        }
    }

    if (rxBuffer.size() > 128)
    {
        rxBuffer.remove(0, rxBuffer.size() - static_cast<int>(sizeof(cmd_t)) + 1);
    }
}

void Dialog::handleSerialCommand(const cmd_t &command)
{
    if (command.type == LOCK_STATE)
    {
        lock_state = command.lock_state;
        lockStateKnown = true;
        updateLockStatus();

        if (openPending && lock_state == 1)
        {
            openPending = false;
            commandTimer.stop();
            recognitionLabel->setText(QString::fromUtf8("门锁已开启 · 设备状态已确认"));
            addAccessRecord(pendingOpenName, QString::fromUtf8("开锁回执"), QString::fromUtf8("设备确认已开启"));
            pendingOpenName.clear();
        }
    }
    else if (command.type == TEMP_VAL)
    {
        cur_temp = command.temp;
        temperatureKnown = true;
        temperatureValueLabel->setText(QString::number(cur_temp, 'f', 1));
    }
}

void Dialog::updateLockStatus()
{
    if (!lockStateKnown)
    {
        lockStateLabel->setText(QString::fromUtf8("状态未知"));
        lockStateLabel->setStyleSheet("color:#a7b5ca; font-size:25px; font-weight:700;");
        return;
    }

    if (lock_state == 1)
    {
        lockStateLabel->setText(QString::fromUtf8("已开启"));
        lockStateLabel->setStyleSheet("color:#56e0ca; font-size:28px; font-weight:700;");
    }
    else
    {
        lockStateLabel->setText(QString::fromUtf8("已上锁"));
        lockStateLabel->setStyleSheet("color:#f4c76c; font-size:28px; font-weight:700;");
    }
}

void Dialog::refreshSerialPorts()
{
    const QString oldPort = serialPortCombo->currentText();
    serialPortCombo->clear();
    const QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    for (int i = 0; i < ports.size(); ++i)
    {
        serialPortCombo->addItem(ports.at(i).portName(), ports.at(i).portName());
    }

    QSettings settings;
    QString preferredPort = oldPort;
    if (preferredPort.isEmpty())
    {
        preferredPort = settings.value("serial/port", "COM15").toString();
    }

    const int index = serialPortCombo->findText(preferredPort, Qt::MatchFixedString);
    if (index >= 0)
    {
        serialPortCombo->setCurrentIndex(index);
    }
    else
    {
        serialPortCombo->setEditText(preferredPort);
    }
}

void Dialog::toggleSerialConnection()
{
    if (serial.isOpen())
    {
        serial.close();
        rxBuffer.clear();
        openPending = false;
        commandTimer.stop();
        updateSerialStatus();
        addAccessRecord(QString::fromUtf8("系统"), QString::fromUtf8("串口连接"), QString::fromUtf8("已断开"));
        return;
    }

    const QString portName = serialPortCombo->currentText().trimmed();
    if (portName.isEmpty())
    {
        serialStatusLabel->setToolTip(QString::fromUtf8("请先选择串口"));
        serialPortCombo->setFocus();
        return;
    }

    serial.setPortName(portName);
    serial.setBaudRate(QSerialPort::Baud115200);
    serial.setDataBits(QSerialPort::Data8);
    serial.setStopBits(QSerialPort::OneStop);
    serial.setParity(QSerialPort::NoParity);
    serial.setFlowControl(QSerialPort::NoFlowControl);

    if (serial.open(QIODevice::ReadWrite))
    {
        rxBuffer.clear();
        QSettings settings;
        settings.setValue("serial/port", portName);
        updateSerialStatus();
        addAccessRecord(QString::fromUtf8("系统"), QString::fromUtf8("串口连接"),
                        QString::fromUtf8("已连接 %1").arg(portName));
    }
    else
    {
        QMessageBox::warning(this, QString::fromUtf8("串口连接失败"),
                             QString::fromUtf8("无法打开 %1：%2")
                             .arg(portName).arg(serial.errorString()));
        updateSerialStatus();
    }
}

void Dialog::updateSerialStatus()
{
    if (serial.isOpen())
    {
        const QString portName = serial.portName();
        serialStatusLabel->setText(QString::fromUtf8("串口 · 已连接 %1").arg(portName));
        serialStatusLabel->setStyleSheet("background:#123b39; color:#62e4ce; padding:7px 12px; border-radius:12px;");
        serialConnectButton->setText(QString::fromUtf8("断开串口"));
    }
    else
    {
        serialStatusLabel->setText(QString::fromUtf8("串口 · 未连接"));
        serialStatusLabel->setStyleSheet("background:#39202c; color:#ff9cac; padding:7px 12px; border-radius:12px;");
        serialConnectButton->setText(QString::fromUtf8("连接串口"));
    }
}

void Dialog::beginEnrollment()
{
    const QString name = nameEdit->text().trimmed();
    if (name.isEmpty())
    {
        enrollmentHintLabel->setText(QString::fromUtf8("请先输入用户姓名"));
        nameEdit->setFocus();
        return;
    }
    if (src.empty())
    {
        enrollmentHintLabel->setText(QString::fromUtf8("摄像头尚未提供画面，请检查摄像头"));
        return;
    }

    recongnizer.cancelStudy();
    recongnizer.setStudyState(true);
    studying = true;
    enrollmentProgress->setValue(0);
    enrollmentHintLabel->setText(QString::fromUtf8("采集中，请正对摄像头并缓慢调整角度"));
    enrollButton->setEnabled(false);
    cancelEnrollmentButton->setEnabled(true);
    nameEdit->setEnabled(false);
    studyTimer.start();
}

void Dialog::cancelEnrollment()
{
    studyTimer.stop();
    recongnizer.cancelStudy();
    studying = false;
    enrollmentProgress->setValue(0);
    enrollmentHintLabel->setText(QString::fromUtf8("录入已取消"));
    enrollButton->setEnabled(true);
    cancelEnrollmentButton->setEnabled(false);
    nameEdit->setEnabled(true);
}

void Dialog::studyTimerOut()
{
    if (src.empty() || vec_findfaces.empty())
    {
        enrollmentHintLabel->setText(QString::fromUtf8("请将人脸放入画面框内"));
        return;
    }

    const int result = recongnizer.learnAndtrain(src, vec_findfaces, nameEdit->text().trimmed());
    const int count = recongnizer.studyCount();
    enrollmentProgress->setValue(count);

    if (result < 0)
    {
        studyTimer.stop();
        studying = false;
        recongnizer.cancelStudy();
        enrollmentHintLabel->setText(QString::fromUtf8("用户数据保存失败，请检查本地数据库"));
        enrollButton->setEnabled(true);
        cancelEnrollmentButton->setEnabled(false);
        nameEdit->setEnabled(true);
        return;
    }

    if (result == 1)
    {
        studyTimer.stop();
        studying = false;
        enrollmentProgress->setValue(10);
        enrollmentHintLabel->setText(QString::fromUtf8("人脸录入完成，模型已保存"));
        enrollButton->setEnabled(true);
        cancelEnrollmentButton->setEnabled(false);
        nameEdit->setEnabled(true);
        addAccessRecord(nameEdit->text().trimmed(), QString::fromUtf8("人脸录入"), QString::fromUtf8("成功"));
        nameEdit->clear();
        refreshUsers();
        return;
    }

    enrollmentHintLabel->setText(QString::fromUtf8("正在采集人脸样本：%1 / 10").arg(count));
}

void Dialog::refreshUsers()
{
    const QList<UserRecord> users = ManagerFactory::getMf()->user->listUsers();
    usersTable->setRowCount(users.size());
    for (int row = 0; row < users.size(); ++row)
    {
        QTableWidgetItem *idItem = new QTableWidgetItem(QString::number(users.at(row).id));
        QTableWidgetItem *nameItem = new QTableWidgetItem(users.at(row).name);
        idItem->setTextAlignment(Qt::AlignCenter);
        usersTable->setItem(row, 0, idItem);
        usersTable->setItem(row, 1, nameItem);
    }
    userCountLabel->setText(QString::fromUtf8("%1 位用户").arg(users.size()));
}

void Dialog::refreshAccessRecords()
{
    const QList<AccessRecord> records = ManagerFactory::getMf()->user->listAccessRecords(200);
    recordsTable->setRowCount(records.size());
    for (int row = 0; row < records.size(); ++row)
    {
        recordsTable->setItem(row, 0, new QTableWidgetItem(records.at(row).time));
        recordsTable->setItem(row, 1, new QTableWidgetItem(records.at(row).name));
        recordsTable->setItem(row, 2, new QTableWidgetItem(records.at(row).action));
        recordsTable->setItem(row, 3, new QTableWidgetItem(records.at(row).result));
    }
}

void Dialog::addAccessRecord(const QString &name, const QString &action, const QString &result)
{
    ManagerFactory::getMf()->user->insertAccessRecord(name, action, result);
    refreshAccessRecords();
}

void Dialog::commandTimeout()
{
    if (openPending)
    {
        openPending = false;
        recognitionLabel->setText(QString::fromUtf8("未收到门锁确认，请检查设备状态"));
        addAccessRecord(pendingOpenName, QString::fromUtf8("开锁回执"), QString::fromUtf8("超时，未收到状态确认"));
        pendingOpenName.clear();
    }
}

Dialog::~Dialog()
{
    studyTimer.stop();
    commandTimer.stop();
    if (cameraWorker != 0)
    {
        cameraWorker->stopCapture();
    }
    if (serial.isOpen())
    {
        serial.close();
    }
}
