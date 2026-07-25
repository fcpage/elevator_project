/**
 * @file:       database_message_service.cpp
 * @author:     Nigel Sinclair
 * @brief:      MySQL database service abstraction
 */

#include "supervisory/database/database_message_service.hpp"
#include "supervisory/common/result.hpp"
#include <iostream>
#include <optional>

namespace project6::supervisory
{

namespace
{

void recordFault(sDBMessageExchange& exchange, const ecDBServiceFaultReason reason) {
    ecDBServiceFaultReason expected = ecDBServiceFaultReason::None;
    static_cast<void>(exchange.faultReason.compare_exchange_strong(expected, reason));
}

bool isValidFloor(const std::uint8_t floor, const sDBServiceConfig& config)
{
    return floor >= config.minFloor && floor <= config.maxFloor;
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
    stop();
    close();
    delete connection_; // Connect allocates memory
    connection_ = nullptr;
}

[[nodiscard]] ecOperationStatus cDBMessageService::start() noexcept
{
    // If it is already running then stop before restarting
    if(worker_.joinable()) {
        stop();
    }
    if(connection_ != nullptr && !connection_->isClosed()) {
        close();
    }

    // Database initialization
    try {
        driver_ = sql::mysql::get_driver_instance();
        std::cout << "Creating database session on url: " << config_.url << "...\n";

        connection_ = driver_->connect(config_.url, config_.user, config_.password);
        connection_->setSchema(config_.database);
        std::cout << "Database connection active." << std::endl;
        // Initialize prepared statements
        writeSnapshotStmt_ = std::unique_ptr<sql::PreparedStatement>(
            connection_->prepareStatement(writeSnapshotStmtQuery_)
        );
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

    // TODO: Consider adding date check

    // Worker thread initialization
    worker_ = std::jthread([this](const std::stop_token stopToken){
        run(stopToken);
    });

    if(!worker_.joinable()) return ecOperationStatus::NotInitialized;
    return ecOperationStatus::Ok;
}

[[nodiscard]] sChoice<ecOperationStatus, sql::ResultSet*> cDBMessageService::query(const char* query) const noexcept
{
    sql::ResultSet* result;
    // Unlikely attribute helps branch prediction for unlikely control flow
    if(connection_ == nullptr || driver_ == nullptr) [[unlikely]] {
        std::cerr << "ERROR: Uninitialized driver or connection.";
        return { ecOperationStatus::DatabaseException };
    }
    try {
        std::unique_ptr<sql::Statement>stmt{connection_->createStatement()}; 
        result = stmt->executeQuery(query);
    } catch (sql::SQLException& e) {
        /*  handles these:
         * sql::MethodNotImplementedException (derived from sql::SQLException), 
         * sql::InvalidArgumentException (derived from sql::SQLException), 
         * sql::SQLException (derived from std::runtime_error)
         */
        std::cerr << "ERROR: SQLEception in " << __FUNCTION__;
        std::cerr << " from file " << __FILE__ << " on line " << __LINE__ << '\n';
        std::cerr << "ERROR: " << e.what();
        std::cerr << "(MySQL error code: " << e.getErrorCode();
        std::cerr << ", SQLState: " << e.getSQLState() << ")" << std::endl;
        return { ecOperationStatus::DatabaseException };
    } catch (...) {
        std::cerr << "ERROR: Unspecified exception" << std::endl;
        return { ecOperationStatus::DatabaseException };
    }
    return { result };
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
    try 
    {
        exchange_.databaseState.store(ecDBServiceState::Running);

        while(!stopToken.stop_requested()) 
        {
            /*** Read ***/
            std::optional<sDBInboundSnapshot> maybeInSnap = readSnapshot();
            if(!maybeInSnap.has_value()) 
            {
                exchange_.droppedEventCount.fetch_add(1);
                recordFault(exchange_, ecDBServiceFaultReason::FailedRead);
                continue;
            }
            sDBInboundSnapshot snap = maybeInSnap.value();
            std::optional<sSupervisoryEvent> maybeEvent = inboundSnapshotToSupervisoryEvent(snap);
            if(!maybeEvent.has_value())
            {
                exchange_.droppedEventCount.fetch_add(1);
                recordFault(exchange_, ecDBServiceFaultReason::FailedRead);
                continue;
            }
            sSupervisoryEvent event = maybeEvent.value();
            if(!exchange_.readEvents.tryPush(event)) 
            {
                exchange_.droppedEventCount.fetch_add(1);         
                recordFault(exchange_, ecDBServiceFaultReason::InboundQueueFull);
            } else {
                exchange_.readCount.fetch_add(1);
            }

            /*** Write ***/
            sSupervisoryStateSnapshot state;
            if(!exchange_.writableSnapshots.tryPop(state)) {
                exchange_.writeFailureCount.fetch_add(1);
                recordFault(exchange_, ecDBServiceFaultReason::OutboundQueueFull);
            } else {
                std::optional<sDBOutboundSnapshot> maybeOutSnap = supervisoryStateToOutboundSnapshot(state);
                if(!maybeOutSnap.has_value())
                {
                    exchange_.writeFailureCount.fetch_add(1);
                    recordFault(exchange_, ecDBServiceFaultReason::FailedWrite);
                    continue;
                }
                sDBOutboundSnapshot outSnap = maybeOutSnap.value();
                if(!writeSnapshot(outSnap))
                {
                    exchange_.writeFailureCount.fetch_add(1);
                    recordFault(exchange_, ecDBServiceFaultReason::FailedWrite);
                    continue;
                }
            }

            std::chrono::milliseconds delay{1000};
            std::this_thread::sleep_for(delay);
        }

        exchange_.databaseState.store(ecDBServiceState::Stopped);
    } 
    catch (...) 
    {
        recordFault(exchange_, ecDBServiceFaultReason::ThreadFailure);
        exchange_.databaseState.store(ecDBServiceState::Failed);
    }
}

std::optional<sDBInboundSnapshot> cDBMessageService::readSnapshot() const {
    sDBInboundSnapshot snap{};

    if(auto choice = query("SELECT * FROM guiRequests;"); choice.err()) {
        std::cerr << "query failed: " << operationStatusMessage(choice.status()) << '\n';
    } else {
        try {
            std::unique_ptr<sql::ResultSet>result{choice.value()};
            while (result->next()) {
                snap.index = result->getUInt("index");
                snap.requestedFloor = result->getUInt("requestedFloor");
            }
        } catch (...) {
            std::cerr << "ERROR: Invalid field from query." << std::endl;
            return std::nullopt;
        }
    }

    return std::optional<sDBInboundSnapshot>{snap};
}

// TODO: This function may need to be passed the supervisory controllers state as well
std::optional<sSupervisoryEvent> cDBMessageService::inboundSnapshotToSupervisoryEvent(sDBInboundSnapshot& snap) const {
    std::optional<sSupervisoryEvent> event;
    if(!isValidFloor(snap.requestedFloor, config_)) {
        event = std::nullopt;
    } else {
        event = {
            .type = ecEventType::DatabaseFloorRequest,
            .requestedFloor = snap.requestedFloor,
            // TODO: .reportedFloor -- how do I get the current floor?
            // TODO: .reportedDirection -- get direction from reportedFloor and floor request
            // TODO: .timestampMs -- get from date/time fields from snapshot
        };
    }
    return event;
}

std::optional<sDBOutboundSnapshot> 
cDBMessageService::supervisoryStateToOutboundSnapshot(sSupervisoryStateSnapshot& state) const { 
    std::optional<sDBOutboundSnapshot> snap;
    if(state.isFaulted) {
        snap = std::nullopt;
    } else {
        snap = {
            // TODO: .index -- not sure what to do with this
            // WARNING: not sure if currentFloor may be 0
            .currentFloor = state.currentFloor,
            /* TODO: Update to include queue history */
            .floorRequest1 = state.targetFloor == 1 ? true : false,
            .floorRequest2 = state.targetFloor == 2 ? true : false,
            .floorRequest3 = state.targetFloor == 3 ? true : false,
            .carRequestFloor1 = state.targetFloor == 1 ? true : false,
            .carRequestFloor2 = state.targetFloor == 2 ? true : false,
            .carRequestFloor3 = state.targetFloor == 3 ? true : false,
            .doorsOpen = state.isDoorOpen,
        };
    }
    return snap;
}

bool cDBMessageService::writeSnapshot(sDBOutboundSnapshot snap) const {
    try {
        writeSnapshotStmt_->setUInt(1, snap.currentFloor);
        writeSnapshotStmt_->setBoolean(2, snap.floorRequest1);
        writeSnapshotStmt_->setBoolean(3, snap.floorRequest2);
        writeSnapshotStmt_->setBoolean(4, snap.floorRequest3);
        writeSnapshotStmt_->setBoolean(5, snap.carRequestFloor1);
        writeSnapshotStmt_->setBoolean(6, snap.carRequestFloor2);
        writeSnapshotStmt_->setBoolean(7, snap.carRequestFloor3);
        writeSnapshotStmt_->setBoolean(8, snap.doorsOpen);
        writeSnapshotStmt_->executeUpdate();
    } catch (...) {
        std::cerr << "Error writing snapshot" << std::endl;
        return false; 
    }
    return true;
}

} // project6::supervisory
