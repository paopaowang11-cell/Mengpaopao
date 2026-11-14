#ifndef MANAGERFACTORY_H
#define MANAGERFACTORY_H

#include <QSqlDatabase>
#include "userdao.h"

class ManagerFactory
{
private:
    // 构造函数
    ManagerFactory();

    // 拷贝构造函数
    ManagerFactory(const ManagerFactory&);

    // 数据库对象
    QSqlDatabase db;

    // 静态成员变量指针
    static ManagerFactory *mf;

public:
    // 静态成员函数，可以使用类名::的方式直接访问。
    static ManagerFactory *getMf();

    // 创建人员表指针
    UserDao *user;

    ~ManagerFactory();
};



#endif // MANAGERFACTORY_H

