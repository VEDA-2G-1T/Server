/*
#pragma once
#include <string>

bool play_wav_file(const std::string& filename, const std::string& device = "plughw:0,0");
*/

#pragma once
#include <string>
#include <atomic>
#include <thread>

class AudioNotifier {
public:
    AudioNotifier();
    ~AudioNotifier();

    // 이 함수를 호출하면 새 스레드에서 음성 재생을 시작합니다.
    void play(const std::string& filename);

    // 현재 음성이 재생 중인지 확인합니다.
    bool isPlaying() const;

private:
    void play_wav_file(const std::string& filename); // 실제 재생 로직을 담을 private 함수

    std::string device_ = "default"; // 사용할 사운드 장치 이름
    std::atomic<bool> is_playing_{false}; // 재생 상태 (스레드 충돌 방지)
    std::thread player_thread_; // 음성 재생을 위한 스레드 객체
};