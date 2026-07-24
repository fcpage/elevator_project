/******************************************************************
* announcement_service.cpp - Floor announcement output service
******************************************************************/

#include "supervisory/audio/announcement_service.hpp"

#include <array>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

#ifdef SUPERVISORY_ENABLE_MINIAUDIO
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#endif

namespace project6::supervisory
{

struct cAnnouncementService::sImplementation
{
#ifdef SUPERVISORY_ENABLE_MINIAUDIO
    ma_context context{};
    ma_device device{};
    ma_engine engine{};
    std::array<ma_sound, 3> sounds{};
    bool contextInitialized = false;
    bool deviceInitialized = false;
    bool engineInitialized = false;
    std::array<bool, 3> soundInitialized{};
#endif
};

namespace
{

bool validFloor(const std::uint8_t floor)
{
    return floor >= 1 && floor <= 3;
}

#ifdef SUPERVISORY_ENABLE_MINIAUDIO
void playbackDataCallback(
    ma_device* device,
    void* output,
    const void* input,
    const ma_uint32 frameCount)
{
    static_cast<void>(input);
    auto* engine = static_cast<ma_engine*>(device->pUserData);
    if (engine == nullptr)
    {
        return;
    }

    static_cast<void>(ma_engine_read_pcm_frames(engine, output, frameCount, nullptr));
}
#endif

} // namespace

cAnnouncementService::cAnnouncementService(
    const sAnnouncementServiceConfig& config,
    sAnnouncementExchange& exchange)
    : config_(config), exchange_(exchange), implementation_(std::make_unique<sImplementation>())
{
}

cAnnouncementService::~cAnnouncementService()
{
    stop();
}

bool cAnnouncementService::start()
{
    if (worker_.joinable())
    {
        return false;
    }

    if (!config_.enabled)
    {
        initialized_.store(true);
    }
    else if (!initializeAudio())
    {
#ifdef SUPERVISORY_ENABLE_MINIAUDIO
        std::cerr << "AUDIO_INIT_FAILED announcements disabled" << '\n';
        exchange_.failed.fetch_add(1);
        // Audio is an accessibility feature, not a motion-safety dependency.
        // Keep the queue alive so Phase 2 diagnostics still show requests.
        initialized_.store(true);
#else
        initialized_.store(true);
#endif
    }

    // The non-miniaudio demo sink and a successfully initialized real device
    // both expose the same queue contract to CONTROL.
    initialized_.store(true);

    worker_ = std::jthread([this](const std::stop_token stopToken) { run(stopToken); });
    return worker_.joinable();
}

void cAnnouncementService::stop()
{
    if (worker_.joinable())
    {
        worker_.request_stop();
        worker_.join();
    }

#ifdef SUPERVISORY_ENABLE_MINIAUDIO
    for (std::size_t index = 0; index < implementation_->sounds.size(); ++index)
    {
        if (implementation_->soundInitialized[index])
        {
            ma_sound_uninit(&implementation_->sounds[index]);
            implementation_->soundInitialized[index] = false;
        }
    }
    if (implementation_->engineInitialized)
    {
        ma_engine_uninit(&implementation_->engine);
        implementation_->engineInitialized = false;
    }
    if (implementation_->deviceInitialized)
    {
        ma_device_uninit(&implementation_->device);
        implementation_->deviceInitialized = false;
    }
    if (implementation_->contextInitialized)
    {
        ma_context_uninit(&implementation_->context);
        implementation_->contextInitialized = false;
    }
#endif

    initialized_.store(false);
}

bool cAnnouncementService::submit(const std::uint8_t floor)
{
    if (!validFloor(floor) || !initialized_.load())
    {
        exchange_.failed.fetch_add(1);
        return false;
    }

    const sFloorAnnouncement request{floor, nextSequence_.fetch_add(1) + 1};
    if (!exchange_.requests.tryPush(request))
    {
        exchange_.failed.fetch_add(1);
        std::cerr << "AUDIO_QUEUE_FULL floor=" << static_cast<unsigned int>(floor) << '\n';
        return false;
    }

    exchange_.requested.fetch_add(1);
    return true;
}

void cAnnouncementService::run(const std::stop_token stopToken) noexcept
{
    while (!stopToken.stop_requested())
    {
        sFloorAnnouncement request{};
        if (exchange_.requests.tryPop(request))
        {
            playAnnouncement(request);
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds{2});
        }
    }
}

bool cAnnouncementService::initializeAudio()
{
#ifndef SUPERVISORY_ENABLE_MINIAUDIO
    return true;
#else
    const ma_backend backends[] = {ma_backend_alsa};
    if (ma_context_init(backends, 1, nullptr, &implementation_->context) != MA_SUCCESS)
    {
        return false;
    }
    implementation_->contextInitialized = true;

    ma_device_info* playbackDevices = nullptr;
    ma_uint32 playbackCount = 0;
    if (ma_context_get_devices(
            &implementation_->context,
            &playbackDevices,
            &playbackCount,
            nullptr,
            nullptr) != MA_SUCCESS)
    {
        return false;
    }

    const std::string requestedName =
        config_.playbackDeviceName == nullptr ? "" : config_.playbackDeviceName;
    const ma_device_id* selectedDevice = nullptr;
    for (ma_uint32 index = 0; index < playbackCount; ++index)
    {
        std::clog << "AUDIO_DEVICE index=" << index
                  << " name=\"" << playbackDevices[index].name << "\"\n";
        if (selectedDevice == nullptr &&
            (requestedName.empty() ||
             std::string(playbackDevices[index].name).find(requestedName) != std::string::npos))
        {
            selectedDevice = &playbackDevices[index].id;
        }
    }

    if (selectedDevice == nullptr && playbackCount > 0)
    {
        selectedDevice = &playbackDevices[0].id;
        std::clog << "AUDIO_DEVICE_FALLBACK index=0\n";
    }
    if (selectedDevice == nullptr)
    {
        return false;
    }

    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.pDeviceID = selectedDevice;
    deviceConfig.playback.format = ma_format_f32;
    deviceConfig.playback.channels = 2;
    deviceConfig.sampleRate = 48000;
    deviceConfig.dataCallback = playbackDataCallback;
    deviceConfig.pUserData = &implementation_->engine;

    if (ma_device_init(
            &implementation_->context,
            &deviceConfig,
            &implementation_->device) != MA_SUCCESS)
    {
        return false;
    }
    implementation_->deviceInitialized = true;

    ma_engine_config engineConfig = ma_engine_config_init();
    engineConfig.pDevice = &implementation_->device;
    if (ma_engine_init(&engineConfig, &implementation_->engine) != MA_SUCCESS)
    {
        return false;
    }
    implementation_->engineInitialized = true;
    ma_engine_set_volume(&implementation_->engine, config_.volume);

    for (std::uint8_t floor = 1; floor <= 3; ++floor)
    {
        const std::filesystem::path assetPath =
            std::filesystem::path(config_.assetDirectory == nullptr ? "audio" : config_.assetDirectory) /
            ("floor" + std::to_string(floor) + ".wav");
        if (ma_sound_init_from_file(
                &implementation_->engine,
                assetPath.string().c_str(),
                MA_SOUND_FLAG_DECODE,
                nullptr,
                nullptr,
                &implementation_->sounds[floor - 1]) != MA_SUCCESS)
        {
            std::cerr << "AUDIO_ASSET_MISSING path=\"" << assetPath.string() << "\"\n";
            continue;
        }
        implementation_->soundInitialized[floor - 1] = true;
    }

    if (ma_device_start(&implementation_->device) != MA_SUCCESS)
    {
        return false;
    }

    std::clog << "AUDIO_STARTED backend=alsa requested_device=\""
              << requestedName << "\"\n";
    return true;
#endif
}

void cAnnouncementService::playAnnouncement(const sFloorAnnouncement& request)
{
#ifndef SUPERVISORY_ENABLE_MINIAUDIO
    std::clog << "AUDIO_DEMO_ANNOUNCEMENT floor="
              << static_cast<unsigned int>(request.floor)
              << " sequence=" << request.sequence << '\n';
    exchange_.played.fetch_add(1);
#else
    if (!validFloor(request.floor) ||
        !implementation_->soundInitialized[request.floor - 1])
    {
        exchange_.failed.fetch_add(1);
        return;
    }

    ma_sound* sound = &implementation_->sounds[request.floor - 1];
    static_cast<void>(ma_sound_stop(sound));
    static_cast<void>(ma_sound_seek_to_pcm_frame(sound, 0));
    if (ma_sound_start(sound) != MA_SUCCESS)
    {
        exchange_.failed.fetch_add(1);
        return;
    }
    exchange_.played.fetch_add(1);
#endif
}

} // namespace project6::supervisory
