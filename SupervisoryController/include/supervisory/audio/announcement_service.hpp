/******************************************************************
* announcement_service.hpp - Floor announcement output service
* @brief Non-blocking audio boundary for confirmed floor arrivals.
******************************************************************/

#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>

#include "supervisory/common/spsc_queue.hpp"


namespace project6::supervisory
{

struct sFloorAnnouncement
{
    std::uint8_t floor = 1;
    std::uint64_t sequence = 0;
};

struct sAnnouncementExchange
{
    cSpscQueue<sFloorAnnouncement, 16> requests;
    std::atomic<std::uint64_t> requested{0};
    std::atomic<std::uint64_t> played{0};
    std::atomic<std::uint64_t> failed{0};
};

struct sAnnouncementServiceConfig
{
    /** Set false for tests that only need to capture announcement requests. */
    bool enabled = true;
    /** Directory containing floor1.wav, floor2.wav, and floor3.wav. */
    const char* assetDirectory = "audio";
    /** Device-name fragment; empty selects the platform default. */
    const char* playbackDeviceName = "Headphones";
    float volume = 1.0F;
};

/**
 * @brief Owns audio-device work outside the deterministic control loop.
 *
 * With SUPERVISORY_ENABLE_MINIAUDIO disabled, the worker is a Phase 2 demo
 * sink and logs AUDIO_DEMO_ANNOUNCEMENT records. The same queue and lifecycle
 * are used by the real miniaudio/ALSA implementation. This keeps Windows and
 * unit-test builds independent of Raspberry Pi audio libraries.
 */
class cAnnouncementService
{
public:
    cAnnouncementService(
        const sAnnouncementServiceConfig& config,
        sAnnouncementExchange& exchange);
    ~cAnnouncementService();

    cAnnouncementService(const cAnnouncementService&) = delete;
    cAnnouncementService& operator=(const cAnnouncementService&) = delete;

    [[nodiscard]] bool start();
    void stop();
    [[nodiscard]] bool submit(std::uint8_t floor);

private:
    struct sImplementation;
    void run(std::stop_token stopToken) noexcept;
    bool initializeAudio();
    void playAnnouncement(const sFloorAnnouncement& request);

    const sAnnouncementServiceConfig& config_;
    sAnnouncementExchange& exchange_;
    std::unique_ptr<sImplementation> implementation_;
    std::jthread worker_;
    std::atomic<std::uint64_t> nextSequence_{0};
    std::atomic<bool> initialized_{false};
};

} // namespace project6::supervisory
