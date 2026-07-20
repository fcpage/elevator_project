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
#include <thread>
#include "supervisory/common/event.hpp"
#include "supervisory/common/result.hpp"
#include "supervisory/common/spsc_queue.hpp"
#include "supervisory/database/database_tables.hpp"

namespace project6::supervisory
{

struct sDBServiceConfig {
    const char*    url      = "tcp://127.0.0.1:3306";
    sql::SQLString user     = "pi";
    sql::SQLString password = "ese";
    sql::SQLString database = "elevatorg1";
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
    cSpscQueue<sSupervisoryEvent, 64> writtenMessages;
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
    // thread data
    sDBMessageExchange& exchange_;
    std::jthread        worker_;

    /*** Private methods ***/
    void run(const std::stop_token& stopToken) const noexcept;
    [[nodiscard]] std::optional<sDBSnapshot> getDBSnapshot() const;
    [[nodiscard]] sSupervisoryEvent snapshotToSupervisoryEvent(sDBSnapshot snap) const;
};

}
