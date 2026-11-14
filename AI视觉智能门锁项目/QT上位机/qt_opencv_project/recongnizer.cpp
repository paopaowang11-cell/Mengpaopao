#include "recongnizer.h"

// 构造函数
Recongnizer::Recongnizer()
{
    // 默认学习状态为false
    isStudy = false;

    // 创建文件对象
    QFile file("face.xml");
    // 判断文件是否存在
    if(file.exists() == true)
    {
        // 如果文件存在则加载已有模型文件
        // FaceRecognizer:OpenCV 人脸识别器基类，包含静态加载函数load
        // <LBPHFaceRecognizer>:模板参数，告诉编译器：我要加载的模型是 LBPH 算法 的人脸模型
        // load<>:是静态模板函数
        // 功能：读取本地 XML 模型文件，生成一个 LBPH 人脸识别器对象，返回Ptr<LBPHFaceRecognizer>智能指针。
        recognizer = FaceRecognizer::load<LBPHFaceRecognizer>("face.xml");
        qDebug() << "call initFaceRecognizer load";
    }
    else
    {
        // 如果文件不存在就创建空的人脸识别器
        recognizer = LBPHFaceRecognizer::create();
        qDebug() << "call initFaceRecognizer create";
    }
}

Recongnizer::~Recongnizer()
{

}

void Recongnizer::cancelStudy()
{
    isStudy = false;
    studyVec.clear();
}

int Recongnizer::learnAndtrain(const Mat &src, const vector<Rect> &facevec, const QString &name)
{
    // 判断是否识别到人脸，如果没有人脸，直接返回
    if(facevec.size() <= 0)
    {
        return 0;
    }

    // 判断学习的人脸个数
    if(studyVec.size() == 10)
    {
        // 获取输入的名字
        // 创建标签容器(数据库)
        vector<int> labels;
        // 将名称丢入到数据库中
        // 将返回的id进行存储
        int id = ManagerFactory::getMf()->user->insertUser(name);

        if (id < 0)
        {
            isStudy = false;
            studyVec.clear();
            return -1;
        }

        // 创建出10个一样的id，存放到labels中。
        // 录入id为2，则创建10个2存放到labels中
        // 为什么创建这个？因为LBPH要求输入的图片数与标签数要一致
        labels.insert(labels.begin(), 10, id);

        // update：机器学习并更新（不会覆盖已有的人脸）
        // 参数1：学习的人脸
        // 参数2：人脸的标签
        recognizer->update(studyVec,labels);

        // 将学习好的人脸模型进行保存
        recognizer->save("face.xml");

        // 停止学习
        isStudy = false;

        // 清空数据
        studyVec.clear();

        // 返回1，训练完成
        return 1;
    }
    else
    {
        // 将人脸区域截取
        // 将第一个识别到的人脸，进行学习
        Mat f = src(facevec[0]);

        // 灰度转换
        cvtColor(f,f,CV_BGR2GRAY);

        // 统一尺寸
        cv::resize(f,f,Size(100,100));

        studyVec.push_back(f);

        // 打印已经学习的人脸数量
        qDebug() << studyVec.size();

        // 还没学习完毕，返回0
        return 0;
    }
}

QString Recongnizer::checkFace(Mat &src, vector<Rect> &facevec, int conflimit)
{
    // 判断
    // 条件1：如果人脸数量小于等于0
    // 条件2：在学习人脸中
    // 条件3：人脸识别器为空
    // 如果满足上面的任何一个条件，直接返回
    if(facevec.size() <= 0 || isStudy || recognizer->empty())
    {
        // 返回空字符串
        return QString();
    }

    // 截取人脸区域
    // 默认只识别第一个检测到的人脸
    Mat f = src(facevec[0]);

    // 灰度处理
    cvtColor(f,f,CV_BGR2GRAY);

    // 修改成机器学习一样的尺寸
    cv::resize(f,f,Size(100,100));

    // 保存结果的值
    // 检测到人脸id
    int label = -1;
    // 检测到的人脸置信度
    double confidence = 0;

    // 预测
    recognizer->predict(f,label,confidence);
    // 将置信度进行输出
    qDebug() << confidence;

    // 判断是否找到人脸
    // id不是-1，-1表示没找到
    // 置信度不高于阈值（越高置信度越低）
    if(label != -1 && confidence < conflimit)
    {
        // 获取名字
        QString name = ManagerFactory::getMf()->user->selectUser(label);

        return name;
    }

    // 否则返回空字符串
    return QString();
}

/****************************下面是封装的中文处理接口***************************************/
//和上面训练没有关系
void GetStringSize(HDC hDC, const char* str, int* w, int* h)
{
    SIZE size;
    GetTextExtentPoint32A(hDC, str, strlen(str), &size);
    if (w != 0) *w = size.cx;
    if (h != 0) *h = size.cy;
}

void putTextHusky(Mat &dst, const char* str, Point org, Scalar color, int fontSize, const char* fn, bool italic, bool underline)//后俩参数：斜体，下划线
{
    CV_Assert(dst.data != 0 && (dst.channels() == 1 || dst.channels() == 3));

    int x, y, r, b;
    if (org.x > dst.cols || org.y > dst.rows) return;
    x = org.x < 0 ? -org.x : 0;
    y = org.y < 0 ? -org.y : 0;

    LOGFONTA lf;
    lf.lfHeight = -fontSize;
    lf.lfWidth = 0;
    lf.lfEscapement = 0;
    lf.lfOrientation = 0;
    lf.lfWeight = 5;
    lf.lfItalic = italic;   //斜体
    lf.lfUnderline = underline; //下划线
    lf.lfStrikeOut = 0;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = 0;
    lf.lfClipPrecision = 0;
    lf.lfQuality = PROOF_QUALITY;
    lf.lfPitchAndFamily = 0;
    strcpy_s(lf.lfFaceName, fn);

    HFONT hf = CreateFontIndirectA(&lf);
    HDC hDC = CreateCompatibleDC(0);
    HFONT hOldFont = (HFONT)SelectObject(hDC, hf);

    int strBaseW = 0, strBaseH = 0;
    int singleRow = 0;
    char buf[1 << 12];
    strcpy_s(buf, str);
    char *bufT[1 << 12];  // 这个用于分隔字符串后剩余的字符，可能会超出。
    //处理多行
    {
        int nnh = 0;
        int cw, ch;

        const char* ln = strtok_s(buf, "\n", bufT);
        while (ln != 0)
        {
            GetStringSize(hDC, ln, &cw, &ch);
            strBaseW = max(strBaseW, cw);
            strBaseH = max(strBaseH, ch);

            ln = strtok_s(0, "\n", bufT);
            nnh++;
        }
        singleRow = strBaseH;
        strBaseH *= nnh;
    }

    if (org.x + strBaseW < 0 || org.y + strBaseH < 0)
    {
        SelectObject(hDC, hOldFont);
        DeleteObject(hf);
        DeleteObject(hDC);
        return;
    }

    r = org.x + strBaseW > dst.cols ? dst.cols - org.x - 1 : strBaseW - 1;
    b = org.y + strBaseH > dst.rows ? dst.rows - org.y - 1 : strBaseH - 1;
    org.x = org.x < 0 ? 0 : org.x;
    org.y = org.y < 0 ? 0 : org.y;

    BITMAPINFO bmp = { 0 };
    BITMAPINFOHEADER& bih = bmp.bmiHeader;
    int strDrawLineStep = strBaseW * 3 % 4 == 0 ? strBaseW * 3 : (strBaseW * 3 + 4 - ((strBaseW * 3) % 4));

    bih.biSize = sizeof(BITMAPINFOHEADER);
    bih.biWidth = strBaseW;
    bih.biHeight = strBaseH;
    bih.biPlanes = 1;
    bih.biBitCount = 24;
    bih.biCompression = BI_RGB;
    bih.biSizeImage = strBaseH * strDrawLineStep;
    bih.biClrUsed = 0;
    bih.biClrImportant = 0;

    void* pDibData = 0;
    HBITMAP hBmp = CreateDIBSection(hDC, &bmp, DIB_RGB_COLORS, &pDibData, 0, 0);

    CV_Assert(pDibData != 0);
    HBITMAP hOldBmp = (HBITMAP)SelectObject(hDC, hBmp);

    //color.val[2], color.val[1], color.val[0]
    SetTextColor(hDC, RGB(255, 255, 255));
    SetBkColor(hDC, 0);
    //SetStretchBltMode(hDC, COLORONCOLOR);

    strcpy_s(buf, str);
    const char* ln = strtok_s(buf, "\n", bufT);
    int outTextY = 0;
    while (ln != 0)
    {
        TextOutA(hDC, 0, outTextY, ln, strlen(ln));
        outTextY += singleRow;
        ln = strtok_s(0, "\n", bufT);
    }
    uchar* dstData = (uchar*)dst.data;
    int dstStep = dst.step / sizeof(dstData[0]);
    unsigned char* pImg = (unsigned char*)dst.data + org.x * dst.channels() + org.y * dstStep;
    unsigned char* pStr = (unsigned char*)pDibData + x * 3;
    for (int tty = y; tty <= b; ++tty)
    {
        unsigned char* subImg = pImg + (tty - y) * dstStep;
        unsigned char* subStr = pStr + (strBaseH - tty - 1) * strDrawLineStep;
        for (int ttx = x; ttx <= r; ++ttx)
        {
            for (int n = 0; n < dst.channels(); ++n) {
                double vtxt = subStr[n] / 255.0;
                int cvv = vtxt * color.val[n] + (1 - vtxt) * subImg[n];
                subImg[n] = cvv > 255 ? 255 : (cvv < 0 ? 0 : cvv);
            }

            subStr += 3;
            subImg += dst.channels();
        }
    }

    SelectObject(hDC, hOldBmp);
    SelectObject(hDC, hOldFont);
    DeleteObject(hf);
    DeleteObject(hBmp);
    DeleteDC(hDC);
}

