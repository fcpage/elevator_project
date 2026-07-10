/**
 * @file:       database_message_service.hpp
 * @author:     Nigel Sinclair
 * @brief:      MySQL database service abstraction
 */

#pragma once

#include <optional>
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>

namespace project6::supervisory
{

class DBMessageService
{
public:
    /** @brief: starts the database connection */
    DBMessageService(
        const char* url = "tcp://127.0.0.1:3306", 
        const char* user =  "pi", 
        const char* password = "ese",
        const char* database = "elevtor_network"
    ) noexcept;
    /** @brief: stops the database connection */
    ~DBMessageService();
    /** @brief: forbid copying */
    DBMessageService(const DBMessageService&) = delete;
    /** @brief: forbid moving */
    DBMessageService(const DBMessageService&&) = delete;
    DBMessageService& operator=(const DBMessageService&&) = delete;

    /** @brief: query the database */
    std::optional<sql::ResultSet*> query(const char* query) noexcept;

private:
    sql::Driver*        DBDriver;
    sql::Connection*    DBConnection;
    const char*         kDBUrl;
    sql::SQLString      DBUser;
    sql::SQLString      DBPassword;
    sql::SQLString      DBName;
};

}
