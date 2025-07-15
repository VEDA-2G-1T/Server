#include "AudioNotifier.h"
#include <alsa/asoundlib.h>
#include <fstream>
#include <iostream>

// WAV 파일 헤더 구조체 (제공해주신 코드와 동일)
struct WAVHeader {
    char riff_tag[4];
    uint32_t riff_length;
    char wave_tag[4];
    char fmt_tag[4];
    uint32_t fmt_length;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char data_tag[4];
    uint32_t data_length;
};

AudioNotifier::AudioNotifier() {}

// 소멸자: 프로그램 종료 시 스레드가 남아있으면 안전하게 종료될 때까지 기다림
AudioNotifier::~AudioNotifier() {
    if (player_thread_.joinable()) {
        player_thread_.join();
    }
}

// 재생 중인지 상태를 반환
bool AudioNotifier::isPlaying() const {
    return is_playing_.load();
}

// play 함수: 재생 중이 아니면 새 스레드를 만들어 음성 재생 시작
void AudioNotifier::play(const std::string& filename) {
    // 이미 재생 중이면 아무것도 하지 않고 즉시 반환
    if (is_playing_.load()) {
        return;
    }

    // 이전에 끝난 스레드가 있다면 정리
    if (player_thread_.joinable()) {
        player_thread_.join();
    }

    // play_wav_file 함수를 새 스레드에서 실행
    player_thread_ = std::thread(&AudioNotifier::play_wav_file, this, filename);
}


// private 함수: 실제 WAV 파일 재생 로직 (제공해주신 코드 기반)
void AudioNotifier::play_wav_file(const std::string& filename) {
    // 재생 시작 플래그 설정
    is_playing_ = true;

    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Cannot open WAV file: " << filename << std::endl;
        is_playing_ = false; // 에러 발생 시 플래그 원위치
        return;
    }

    WAVHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(WAVHeader));

    snd_pcm_t *pcm_handle;
    if (snd_pcm_open(&pcm_handle, "plughw:CARD=sndrpihifiberry,DEV=0", SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        std::cerr << "Error opening PCM device: plughw:CARD=sndrpihifiberry,DEV=0" << std::endl;
        is_playing_ = false; // 에러 발생 시 플래그 원위치
        return;
    }

    snd_pcm_hw_params_t *params;
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(pcm_handle, params);
    snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    
    snd_pcm_format_t format;
    switch (header.bits_per_sample) {
        case 16: format = SND_PCM_FORMAT_S16_LE; break;
        default:
            std::cerr << "Unsupported bit depth: " << header.bits_per_sample << std::endl;
            snd_pcm_close(pcm_handle);
            is_playing_ = false;
            return;
    }

    snd_pcm_hw_params_set_format(pcm_handle, params, format);
    snd_pcm_hw_params_set_channels(pcm_handle, params, header.num_channels);
    snd_pcm_hw_params_set_rate_near(pcm_handle, params, &header.sample_rate, 0);
    snd_pcm_hw_params(pcm_handle, params);
    
    snd_pcm_prepare(pcm_handle);

    const size_t buffer_size = 4096;
    char buffer[buffer_size];
    while (file.read(buffer, buffer_size) || file.gcount() > 0) {
        int frames_to_write = file.gcount() / (header.bits_per_sample / 8);
        if (snd_pcm_writei(pcm_handle, buffer, frames_to_write) < 0) {
            snd_pcm_recover(pcm_handle, -EPIPE, 1);
        }
    }

    snd_pcm_drain(pcm_handle);
    snd_pcm_close(pcm_handle);

    // 재생 완료 후 플래그 원위치
    is_playing_ = false;
}