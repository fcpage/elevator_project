/**
 * @file:       database_message_service.cpp
 * @author:     Nigel Sinclair
 * @brief:      MySQL database service abstraction
 */

#include "supervisory/database/database_message_service.hpp"
#include <iostream>
#include "supervisory/common/result.hpp"

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
    DBConnection = nullptr;
    DBDriver = nullptr;
}

DBMessageService::~DBMessageService() 
{
    this->stop();
    delete DBConnection; // Connect allocates memory
    DBConnection = nullptr;
}

[[nodiscard]] ecOperationStatus DBMessageService::start() noexcept
{
    if(DBConnection != nullptr && !DBConnection->isClosed()) {
        DBConnection->setSchema(DBName);
        return ecOperationStatus::Ok;
    }
    try {
        DBDriver = sql::mysql::get_driver_instance();
        std::cout << "Creating database session on url: " << kDBUrl << "...\n" << std::endl;

        DBConnection = DBDriver->connect(kDBUrl, DBUser, DBPassword);

        DBConnection->setSchema(DBName);
        return ecOperationStatus::Ok;
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
        return ecOperationStatus::DatabaseException;
    }
}


ecResult<sql::ResultSet*> DBMessageService::query(const char* query) noexcept 
{
    if(DBConnection == nullptr || DBDriver == nullptr) [[unlikely]] {
        std::cout << "ERROR: Uninitialized driver or connection.";
        return { ecOperationStatus::DatabaseException };
    }
    try {
        std::unique_ptr<sql::Statement>stmt{DBConnection->createStatement()}; 
        return { stmt->executeQuery(query) };
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
        return { ecOperationStatus::DatabaseException };
    }
}

void DBMessageService::stop() {
    if(DBConnection == nullptr) return;
    if(!DBConnection->isClosed()) {
        DBConnection->close();
    }
}

} // project6::supervisory
