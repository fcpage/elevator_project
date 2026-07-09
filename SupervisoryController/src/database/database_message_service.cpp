/**
 * @file:       database_message_service.cpp
 * @author:     Nigel Sinclair
 * @brief:      MySQL database service abstraction
 */

#include "supervisory/database/database_message_service.hpp"
#include <memory>

namespace project6::supervisory
{

namespace
{

DBMessageService::DBMessageService(
    const char* url = "tcp://127.0.0.1", 
    const char* user =  "pi", 
    const char* password = "ese",
    const char* database = "elevator_network") noexcept 
{

    kDBUrl = url;
    kDBUser = user;
    kDBPassword = password;
    kDBName = database;
    
    try {
        kDBDriver = sql::mysql::get_driver_instance();
        cout << "Creating database session on url: " << url << "...\n" << std::endl;

        kDBConnection = kDBDriver->connect(kDBUrl, kDBUser, kDBPassword);

        connection->setSchema(kDBName);
    } 
    catch (sql::SQLException e&) {
        /* TODO: handle these:
         * sql::MethodNotImplementedException (derived from sql::SQLException), 
         * sql::InvalidArgumentException (derived from sql::SQLException), 
         * sql::SQLException (derived from std::runtime_error)
         */
    }
    
}

~DBMessageService::DBMessageService() 
{
    delete kDBConnection;
}

std::optional<sql::ResultSet*> DBMessageService::query(const char* query) noexcept 
{
    try {
        std::unique_ptr<sql::Statement>stmt{kDBConnection->createStatement()}; 
        return std::optional<sql::ResultSet*>{stmt->executeQuery(query)};
    } catch (sql::SQLException e&) {
        /* TODO: handle these:
         * sql::MethodNotImplementedException (derived from sql::SQLException), 
         * sql::InvalidArgumentException (derived from sql::SQLException), 
         * sql::SQLException (derived from std::runtime_error)
         */
    }
}


}

} // project6::supervisory
