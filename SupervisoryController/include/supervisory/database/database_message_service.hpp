/**
 * @file:       database_message_service.hpp
 * @author:     Nigel Sinclair
 * @brief:      MySQL database service abstraction
 */

#pragma once

#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include "supervisory/common/result.hpp"

namespace project6::supervisory
{

class DBMessageService
{
public:
    /** @brief: initializes database config info */
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
    DBMessageService& operator=(const DBMessageService&) = delete;
    /** @brief: forbid moving */
    DBMessageService(const DBMessageService&&) = delete;
    DBMessageService& operator=(const DBMessageService&&) = delete;
    
    /** @brief: start the database connection */
    [[nodiscard]] ecOperationStatus start() noexcept;
    /** 
     * @brief query the database 
     * @param query String literal containing query
     * @return The result of the query wrapped in ecResult (contains status
     *          code and optionally the query result - nullopt on error)
     */
    [[nodiscard]] ecResult<sql::ResultSet*> query(const char* query) noexcept;
    /** @brief: stop the database connection (can be called manually but is
     *          also called automatically by the destructor) 
     */
    void stop();

private:
    sql::Driver*        DBDriver;
    sql::Connection*    DBConnection;
    const char*         kDBUrl;
    sql::SQLString      DBUser;
    sql::SQLString      DBPassword;
    sql::SQLString      DBName;
};

}
