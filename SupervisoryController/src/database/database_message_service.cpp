/**
 * @file:       database_message_service.cpp
 * @author:     Nigel Sinclair
 * @brief:      MySQL database service abstraction
 */

#include "supervisory/database/database_message_service.hpp"
#include "supervisory/common/result.hpp"
#include <iostream>
#include <fstream>

namespace project6::supervisory
{

namespace
{

constexpr std::chrono::milliseconds kIdleDelay{1}; // Not a timeout! See application file.

void recordFault(sDBMessageExchange& exchange, const ecDBServiceFaultReason reason) {
    ecDBServiceFaultReason expected = ecDBServiceFaultReason::None;
    static_cast<void>(exchange.faultReason.compare_exchange_strong(expected, reason));
}

} // namespace


cDBMessageService::cDBMessageService(const sDBServiceConfig& config, sDBMessageExchange& exchange) 
: config_(config), exchange_(exchange)
{
    connection_ = nullptr;
    driver_ = nullptr;
}

cDBMessageService::~cDBMessageService() 
{
    this->stop();
    this->close();
    delete connection_; // Connect allocates memory
    connection_ = nullptr;
}

[[nodiscard]] ecOperationStatus cDBMessageService::start() noexcept
{
    if(connection_ != nullptr && !connection_->isClosed()) {
        // connection_->setSchema(config_.database);
        return ecOperationStatus::InvalidArgument;
    }
    if(worker_.joinable()) {
        return ecOperationStatus::InvalidArgument;
    }

    // Database initialization
    try {
        driver_ = sql::mysql::get_driver_instance();
        std::cout << "Creating database session on url: " << config_.url << "...\n";

        connection_ = driver_->connect(config_.url, config_.user, config_.password);
        connection_->setSchema(config_.database);
        std::cout << "Database connection active." << std::endl;
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

    // Worker thread initialization
    worker_ = std::jthread([this](const std::stop_token stopToken){
        run(stopToken);
    });

    if(!worker_.joinable()) return ecOperationStatus::NotInitialized;
    return ecOperationStatus::Ok;
}

[[nodiscard]] sChoice<ecOperationStatus, sql::ResultSet*> cDBMessageService::query(const char* query) const noexcept
{
    // Unlikely attribute helps branch prediction for unlikely control flow
    if(connection_ == nullptr || driver_ == nullptr) [[unlikely]] {
        std::cout << "ERROR: Uninitialized driver or connection.";
        return { ecOperationStatus::DatabaseException };
    }
    try {
        std::unique_ptr<sql::Statement>stmt{connection_->createStatement()}; 
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

void cDBMessageService::stop() {
    if(worker_.joinable()) {
        worker_.request_stop();
        worker_.join();
    }

    if(exchange_.databaseState.load() != ecDBServiceState::Failed) {
        exchange_.databaseState.store(ecDBServiceState::Stopped);
    }
}

void cDBMessageService::close() {
    std::cout << "Database service terminated." << std::endl;
    if(connection_ == nullptr) return;
    if(!connection_->isClosed()) {
        connection_->close();
    }
}

void cDBMessageService::run(const std::stop_token& stopToken) const noexcept {
    std::ofstream file("test.txt");
    file << "Hello from database thread!" << '\n';
    try 
    {
        exchange_.databaseState.store(ecDBServiceState::Running);

        while(!stopToken.stop_requested()) {
            std::optional<sDBSnapshot> snap = getDBSnapshot();
            if(snap == std::nullopt) throw;
            sDBSnapshot snap_value = snap.value();
            std::cout << snap_value << std::endl;
            // TODO: Send the snapshot to main thread
        }

        exchange_.databaseState.store(ecDBServiceState::Stopped);
    } 
    catch (...) 
    {
        recordFault(exchange_, ecDBServiceFaultReason::ThreadFailure);
        exchange_.databaseState.store(ecDBServiceState::Failed);
    }
}

std::optional<sDBSnapshot> cDBMessageService::getDBSnapshot() const {
    if(auto choice = query("SELECT * FROM elevatorNetwork"); choice.err()) {
        std::cerr << "query failed: " << operationStatusMessage(choice.status()) << '\n';
    } else {
        std::unique_ptr<sql::ResultSet>result{choice.value()};
        // TODO: Handle null result
        sDBSnapshot snap = {
            .index = result->getInt("index"),
            // .date = result->getDate("date"), // TODO: what type should this be?
            // .time = result->getTime("time"), // TODO: what type should this be?
            .nodeID = result->getInt("nodeID"),
            .sender = static_cast<uint8_t>(result->getUInt("sender")),
            .receiver = static_cast<uint8_t>(result->getInt("reciever")),
            .currentFloor = static_cast<uint8_t>(result->getInt("currentFloor")),
            .requestFloor = static_cast<uint8_t>(result->getInt("requestFloor")),
            .status = static_cast<uint8_t>(result->getInt("status")),
            .queued = result->getBoolean("queued"),
            .served = result->getBoolean("served"),
        };
        return std::optional<sDBSnapshot>{snap};
    }
    return std::nullopt;
}

sSupervisoryEvent cDBMessageService::snapshotToSupervisoryEvent(sDBSnapshot snap) const {
     
}

} // project6::supervisory
