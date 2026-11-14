#include "managerfactory.h"
#include <QSqlError>

ManagerFactory* ManagerFactory::mf = NULL;


ManagerFactory::ManagerFactory() : user(NULL)
{
    // 加载驱动
    db = QSqlDatabase::addDatabase("QSQLITE");

    // 设置数据库主机名称
    db.setHostName("test");

    // 设置数据库名称
    db.setDatabaseName("test.db");

    // 打开数据库
    if (!db.open())
    {
        qDebug() << "打开本地数据库失败:" << db.lastError().text();
    }

    // 创建数据库操作对象
    user = new UserDao;
}

ManagerFactory *ManagerFactory::getMf()
{
    if(mf == NULL)
    {
        mf = new ManagerFactory;
    }
    return mf;
}


ManagerFactory::~ManagerFactory()
{
    // 关闭数据库
    db.close();
    delete user;
}


