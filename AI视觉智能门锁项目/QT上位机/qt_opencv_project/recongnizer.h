#ifndef RECONGNIZER_H
#define RECONGNIZER_H

#include <opencv2/opencv.hpp>
#include <opencv2/face.hpp>
#include <vector>
#include <QtWidgets>
#include "managerfactory.h"

using namespace cv::face;
using namespace cv;
using namespace std;

class Recongnizer
{
public:
    // 构造函数
    // 主要用于加载模型
    Recongnizer();

    // 模型训练，学习人脸是谁
    // 参数1：src待学习的人脸图像
    // 参数2：人脸区域
    // 参数3：对应的人名
    // 返回值：学习完毕返回1，否则返回0。
    // 在达成学习目标前，需要反复输入学习样本进行学习，直到返回1
    int learnAndtrain(const Mat &src, const vector<Rect> &facevec,const QString &name);

    // 检测是否有已录入的人脸
    // 参数1：人脸图像
    // 参数2：人脸区域
    // 参数3：识别可信度，数字越小可信度越过（要求越严格）。默认为80
    QString checkFace(Mat &src,vector<Rect> &facevec,int conflimit = 80);

    // 设置学习状态，当开始学习后需要将学习状态设置为true
    void setStudyState(bool state){isStudy = state;}

    // 当前已采集到的人脸样本数量
    int studyCount() const { return static_cast<int>(studyVec.size()); }

    // 取消录入并清空未完成的样本
    void cancelStudy();


    ~Recongnizer();

private:
    //创建人脸识别器指针(opencv智能指针)
    Ptr<FaceRecognizer> recognizer;

    //创建人脸矩形容器
    vector<Rect> facevec;
    //保存人脸模型的容器
    vector<Mat> studyVec;

    //标志学习状态
    bool isStudy;
};

/**
 * @brief putTextHusky 中文输出封装，用opencv自带的无法输出中文
 * @param dst 文字显示的源图像
 * @param str 显示的文字
 * @param org 显示的原点位置
 * @param color 字体颜色
 * @param fontSize 字体大小
 * @param fn 字体名，默认是Arial
 * @param italic 倾斜
 * @param underline 下划线
 */
void putTextHusky(Mat &dst, const char *str, Point org, Scalar color, int fontSize,
    const char *fn = "Arial", bool italic = false, bool underline = false);


#endif // RECONGNIZER_H


