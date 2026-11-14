#ifndef USERDAO_H
#define USERDAO_H

#include <QString>
#include <QList>
#include <QStringList>
#include <QSqlQuery>    // 用于操作sql语句
#include <QSqlRecord>
#include <QDebug>

struct UserRecord
{
    int id;
    QString name;
};

struct AccessRecord
{
    QString time;
    QString name;
    QString action;
    QString result;
};


class UserDao
{
public:
    // 构造函数
    // 作用：用于创建用户表，字段：整形id（主键自增）、name字符串
    UserDao();


    // 插入表数据
    // 参数：插入的用户名称
    int insertUser(QString name);


    // 查询
    // 参数：要查询的ID
    // 返回值：返回对应的姓名
    QString selectUser(int id);

    // 返回本地已录入的人脸用户
    QList<UserRecord> listUsers();

    // 记录一次开锁或设备事件
    bool insertAccessRecord(const QString &name, const QString &action,
                            const QString &result);

    // 查询最近的操作记录
    QList<AccessRecord> listAccessRecords(int limit = 200);

    ~UserDao();
};

#endif // USERDAO_H
