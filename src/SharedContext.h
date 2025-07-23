#pragma once

#include <string>
#include <mutex>
#include <atomic>

// 현재 작동 모드를 저장하는 전역 변수 (예: "detect", "blur")
extern std::string g_current_mode;

// g_current_mode에 대한 스레드 안전 접근을 보장하는 뮤텍스
extern std::mutex g_mode_mutex;

// 메인 루프를 종료시키기 위한 원자적 불리언 변수
extern std::atomic<bool> g_keep_running;

// 영상 분석을 통해 위험 상황이 감지되었는지 나타내는 플래그
extern std::atomic<bool> g_visual_alert_active;

// 음향 분석을 통해 이상 소음이 감지되었는지 나타내는 플래그
extern std::atomic<bool> g_audio_alert_active;