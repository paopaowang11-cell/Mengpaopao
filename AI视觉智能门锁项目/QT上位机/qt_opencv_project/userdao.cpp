#include "userdao.h"
#include <QDateTime>
#include <QSqlError>

UserDao::UserDao()
{
    // 创建数据库的操作对象
    QSqlQuery query;

    // 定义sql语句
    // exec执行sql语句
    // 参数：要执行的sql语句
    // create table if not exists user:查看是否存在user这个表
    // 如果不存在则创建，存在则不创建
    // 表中存在两个内容：id与name
    // id integer primary key autoincrement：表示id为自增主键
    // name varchar(20)):name 的长度为20
    // 返回值：bool类型，true表示创建成功，false表示创建失败
    bool ok = query.exec("create table if not exists user("
                   "id integer primary key autoincrement,"
                   "name varchar(20));");


    if(ok == true)
    {
        qDebug() << "建表成功";
    }
    else
    {
        qDebug() << "建表失败";
    }

    ok = query.exec("create table if not exists access_log("
                    "id integer primary key autoincrement,"
                    "created_at varchar(32),"
                    "user_name varchar(64),"
                    "action varchar(64),"
                    "result varchar(128));");
    if (!ok)
    {
        qDebug() << "创建开锁记录表失败:" << query.lastError().text();
    }


}

// 插入功能
// 参数：插入的姓名
int UserDao::insertUser(QString name)
{
    // 创建数据库操作对象
    QSqlQuery query;

    // 准备sql语句
    // user(name) values(?);:占位符
    query.prepare("insert into user(name) values(?);");

    // 绑定数据
    // 参数1：位置
    // 参数2：内容
    query.bindValue(0,name);

    // 执行sql语句
    bool ok = query.exec();

    int id = -1;

    if(ok == true)
    {
        // 返回本次数据库插入的id
        query.exec("select last_insert_rowid();");

        if(query.next())
        {
            // 取出查询出来的id，转换为int类型
            id = query.record().value(0).toInt();

            // 将id进行展示
            qDebug () << id;
        }

    }
    else
    {
        qDebug() << "插入表失败";
    }

    // 返回插入的id
    return id;
}

// 查询功能(按照id查找)
// 返回：姓名
QString UserDao::selectUser(int id)
{
    // 创建操作数据库的类对象
    QSqlQuery query;

    // 准备sql语句
    query.prepare("select name from user where id = ?;");

    // 绑定数据
    query.bindValue(0,id);

    // 执行sql语句
    bool ok = query.exec();

    QString name;

    if(ok == true)
    {
        // 查看是否查找到了结果
        if(query.next())
        {
            // 取出查找到的姓名
            name = query.record().value(0).toString();

            qDebug() << name;
        }
    }
    else
    {
        qDebug() << "查询表失败";
    }

    return name;
}

QList<UserRecord> UserDao::listUsers()
{
    QList<UserRecord> users;
    QSqlQuery query;
    if (!query.exec("select id, name from user order by id desc"))
    {
        qDebug() << "查询用户列表失败:" << query.lastError().text();
        return users;
    }

    while (query.next())
    {
        UserRecord record;
        record.id = query.value(0).toInt();
        record.name = query.value(1).toString();
        users.append(record);
    }
    return users;
}

bool UserDao::insertAccessRecord(const QString &name, const QString &action,
                                const QString &result)
{
    QSqlQuery query;
    query.prepare("insert into access_log(created_at, user_name, action, result) "
                  "values(?, ?, ?, ?)");
    query.addBindValue(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
    query.addBindValue(name);
    query.addBindValue(action);
    query.addBindValue(result);

    if (!query.exec())
    {
        qDebug() << "写入开锁记录失败:" << query.lastError().text();
        return false;
    }
    return true;
}

QList<AccessRecord> UserDao::listAccessRecords(int limit)
{
    QList<AccessRecord> records;
    if (limit < 1)
    {
        limit = 1;
    }
    if (limit > 1000)
    {
        limit = 1000;
    }

    QSqlQuery query;
    const QString sql = QString("select created_at, user_name, action, result "
                                "from access_log order by id desc limit %1").arg(limit);
    if (!query.exec(sql))
    {
        qDebug() << "查询开锁记录失败:" << query.lastError().text();
        return records;
    }

    while (query.next())
    {
        AccessRecord record;
        record.time = query.value(0).toString();
        record.name = query.value(1).toString();
        record.action = query.value(2).toString();
        record.result = query.value(3).toString();
        records.append(record);
    }
    return records;
}

UserDao::~UserDao()
{

}


