/**
 * @file:       database_message_service.cpp
 * @author:     Nigel Sinclair
 * @brief:      MySQL database service abstraction
 */

#include "supervisory/database/database_message_service.hpp"
#include <iostream>

namespace project6::supervisory
{

DBMessageService::DBMessageService(
    const char* url, 
    const char* user, 
    const char* password,
    const char* database 
) noexcept 
{
    kDBUrl = url;
    DBUser = user;
    DBPassword = password;
    DBName = database;

    try {
        DBDriver = sql::mysql::get_driver_instance();
        std::cout << "Creating database session on url: " << url << "...\n" << std::endl;

        DBConnection = DBDriver->connect(kDBUrl, DBUser, DBPassword);

        DBConnection->setSchema(DBName);
    } 
    catch (sql::SQLException& e) {
        /*  handles these:
         * sql::MethodNotImplementedException (derived from sql::SQLException), 
         * sql::InvalidArgumentException (derived from sql::SQLException), 
         * sql::SQLException (derived from std::runtime_error)
         */
        std::cout << "ERROR: SQLEception in " << __FUNCTION__;
        std::cout << "from file " << __FILE__ << "on line " << __LINE__ << std::endl;
        std::cout << "ERROR: " << e.what();
        std::cout << "(MySQL error code: " << e.getErrorCode();
        std::cout << ", SQLState: " << e.getSQLState() << ")" << std::endl;
        exit(EXIT_FAILURE);
    }

}

DBMessageService::~DBMessageService() 
{
    delete DBConnection; // Connect allocates memory
}

std::optional<sql::ResultSet*> DBMessageService::query(const char* query) noexcept 
{
    try {
        std::unique_ptr<sql::Statement>stmt{DBConnection->createStatement()}; 
        return std::optional<sql::ResultSet*>{stmt->executeQuery(query)};
    } catch (sql::SQLException& e) {
        /*  handles these:
         * sql::MethodNotImplementedException (derived from sql::SQLException), 
         * sql::InvalidArgumentException (derived from sql::SQLException), 
         * sql::SQLException (derived from std::runtime_error)
         */
        std::cout << "ERROR: SQLEception in " << __FUNCTION__;
        std::cout << "from file " << __FILE__ << "on line " << __LINE__ << std::endl;
        std::cout << "ERROR: " << e.what();
        std::cout << "(MySQL error code: " << e.getErrorCode();
        std::cout << ", SQLState: " << e.getSQLState() << ")" << std::endl;
        return std::nullopt;
    }
}

} // project6::supervisory
