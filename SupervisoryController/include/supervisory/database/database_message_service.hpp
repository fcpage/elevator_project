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
#include <cppconn/prepared_statement.h>
#include <thread>
#include "supervisory/common/event.hpp"
#include "supervisory/common/result.hpp"
#include "supervisory/common/spsc_queue.hpp"
#include "supervisory/control/supervisory_state_machine.hpp"
#include "supervisory/database/database_tables.hpp"

namespace project6::supervisory
{

struct sDBServiceConfig {
    const char*    url      = "tcp://127.0.0.1:3306";
    sql::SQLString user     = "pi";
    sql::SQLString password = "ese";
    sql::SQLString database = "elevatorg1";
    std::uint8_t   minFloor = 1;
    std::uint8_t   maxFloor = 3;
};

/** @brief database worker lifecycle state */
enum class ecDBServiceState {
    Stopped,
    Running,
    Failed
};

/** @brief First detected DATABASE failure*/
enum class ecDBServiceFaultReason {
    None,
    InitializationFailed,
    FailedWrite,
    FailedRead,
    InboundQueueFull,
    OutboundQueueFull,
    DatabaseProgressTimeout,
    ThreadFailure
};

/** @brief Lock-free data exchange between DATABASE and CONTROL */
struct sDBMessageExchange {
    /** @brief DATABASE-to-CONTROL events. */
    cSpscQueue<sSupervisoryEvent, 64> readEvents;
    /** @brief CONTROL-to-DATABASE messages. */
    cSpscQueue<sSupervisoryStateSnapshot, 64> writableSnapshots;
    /** Messages read from Database. */
    std::atomic<std::uint64_t> readCount{0};
    /** Events rejected by a full queue. */
    std::atomic<std::uint64_t> droppedEventCount{0};
    /** Messages written to Database. */
    std::atomic<std::uint64_t> writeCount{0};
    /** Failed Database writes. */
    std::atomic<std::uint64_t> writeFailureCount{0};
    /** Current worker state. */
    std::atomic<ecDBServiceState> databaseState{ecDBServiceState::Stopped};
    /** First detected failure. */
    std::atomic<ecDBServiceFaultReason> faultReason{ecDBServiceFaultReason::None};
};

class cDBMessageService
{
public:
    /** @brief: initializes database config info */
    cDBMessageService(const sDBServiceConfig& config, sDBMessageExchange& exchange);
    /** @brief: stops the database connection */
    ~cDBMessageService();
    /** @brief: forbid copying */
    cDBMessageService(const cDBMessageService&) = delete;
    cDBMessageService& operator=(const cDBMessageService&) = delete;
    /** @brief: forbid moving */
    cDBMessageService(const cDBMessageService&&) = delete;
    cDBMessageService& operator=(const cDBMessageService&&) = delete;
    
    /** @brief: start the database connection */
    [[nodiscard]] ecOperationStatus start() noexcept;
    /** 
     * @brief query the database 
     * @param query String literal containing query
     * @return The result of the query wrapped in sResult (contains status
     *          code and optionally the query result - nullopt on error)
     */
    [[nodiscard]] sChoice<ecOperationStatus, sql::ResultSet*> query(
        const char* query) const noexcept;
    /** @brief: stop the worker thread (can be called manually but is
     *          also called automatically by the destructor) 
     */
    void stop();
    /** @brief: close the database connection (can be called manually but is
     *          also called automatically by the destructor) 
     */
    void close();

private:
    // database data
    sql::Driver*            driver_;
    sql::Connection*        connection_;
    const sDBServiceConfig& config_;
    // Prepared statments
    const char* writeSnapshotStmtQuery_ = "\
        INSERT INTO elevatorNetwork(\
            currentFloor,\
            floorRequest1,\
            floorRequest2,\
            floorRequest3,\
            carRequestFloor1,\
            carRequestFloor2,\
            carRequestFloor3,\
            doorsOpen\
        ) VALUES ( ?, ?, ?, ?, ?, ?, ?, ? )";
    std::unique_ptr<sql::PreparedStatement> writeSnapshotStmt_;
    // thread data
    sDBMessageExchange& exchange_;
    std::jthread        worker_;

    /*** Private methods ***/

    /** @brief main function for the thread (also handles thread failures with std::stop_token)*/
    void run(const std::stop_token& stopToken) const noexcept;
    /** @brief read a snapshot from the database (only the gui requests table)*/
    [[nodiscard]] 
    std::optional<sDBInboundSnapshot> readSnapshot() const;
    /** @brief convert inbound snapshot to supervisory event to be sent to control thread */
    [[nodiscard]] 
    std::optional<sSupervisoryEvent> inboundSnapshotToSupervisoryEvent(sDBInboundSnapshot& snap) const;
    /** @brief convert supervisory event to outbound snapshot to be written to database */
    [[nodiscard]] 
    std::optional<sDBOutboundSnapshot> supervisoryStateToOutboundSnapshot(sSupervisoryStateSnapshot& state) const;
    /** @brief write outbound snapshot to database */
    [[nodiscard]] 
    bool writeSnapshot(sDBOutboundSnapshot snap) const;
};

/*** Extras ***/
static inline std::ostream& operator<<(std::ostream& os, const ecDBServiceFaultReason& reason) {
    using namespace project6::supervisory;
    switch (reason) {
        case ecDBServiceFaultReason::DatabaseProgressTimeout: 
        {
            os << "DatabaseProgressTimeout";
            break;
        }
        case ecDBServiceFaultReason::FailedRead: 
        {
            os << "FailedRead";
            break;
        }
        case ecDBServiceFaultReason::FailedWrite: 
        {
            os << "FailedWrite";
            break;
        }
        case ecDBServiceFaultReason::InboundQueueFull: 
        {
            os << "InboundQueueFull";
            break;
        }
        case ecDBServiceFaultReason::InitializationFailed: 
        {
            os << "InitializationFailed";
            break;
        }
        case ecDBServiceFaultReason::OutboundQueueFull: 
        {
            os << "OutboundQueueFull";
            break;
        }
        case ecDBServiceFaultReason::ThreadFailure: 
        {
            os << "ThreadFailure";
            break;
        }
        case ecDBServiceFaultReason::None: 
        {
            os << "None";
            break;
        }
    }
    return os;
}

}
