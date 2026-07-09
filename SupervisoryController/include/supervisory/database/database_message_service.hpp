/**
 * @file:       database_message_service.hpp
 * @author:     Nigel Sinclair
 * @brief:      MySQL database service abstraction
 */

#pragma once

#include <string>
#include <iostream>
#include <optional>
#include <mysql/jdbc.h>

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
    );
    /** @brief: stops the database connection */
    ~DBMessageService();
    /** @brief: forbid copying */
    DBMessageService(const DBMessageService&) = delete;
    /** @brief: forbid moving */
    DBMessageService(const DBMessageService&&) = delete;
    DBMessageService& operator=(const DBMessageService&&) = delete;

    /** @brief: query the database */
    std::optional<sql::ResultSet*> query(const char* query);

private:
    const sql::Driver*  kDBDriver;
    sql::Connection*    kDBConnection;
    const char*         kDBUrl;
    const std::string   kDBUser;
    const std::string   kDBPassword;
    const std::string   kDBName;
};

}
