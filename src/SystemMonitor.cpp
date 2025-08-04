// SystemMonitor.cpp
#include "SystemMonitor.h"
#include <fstream>
#include <sstream>
#include <sys/sysinfo.h>

// 생성자
SystemMonitor::SystemMonitor() : firstRun(true) {
    // 첫 실행 시 previousCpu를 초기화합니다.
    previousCpu = readCpuTimes();
}

// getTotal, getActive 멤버 함수 구현
long long SystemMonitor::CpuTimes::getTotal() const {
    return user + nice + system + idle + iowait + irq + softirq;
}
long long SystemMonitor::CpuTimes::getActive() const {
    return user + nice + system + irq + softirq;
}

// CPU 및 메모리 사용률 계산 함수 (기존 코드와 동일)
double SystemMonitor::getCpuUsage() {
    CpuTimes currentCpu = readCpuTimes();
    long long totalDiff = currentCpu.getTotal() - previousCpu.getTotal();
    long long activeDiff = currentCpu.getActive() - previousCpu.getActive();
    double cpuPercent = 0.0;
    if (totalDiff > 0) {
        cpuPercent = (static_cast<double>(activeDiff) / totalDiff) * 100.0;
    }
    previousCpu = currentCpu;
    return cpuPercent;
}

double SystemMonitor::getMemoryUsage() {
    struct sysinfo info;
    if (sysinfo(&info) != 0) return -1.0;
    unsigned long totalRam = info.totalram * info.mem_unit;
    unsigned long freeRam = info.freeram * info.mem_unit;
    unsigned long usedRam = totalRam - freeRam;
    return (static_cast<double>(usedRam) / totalRam) * 100.0;
}

SystemMonitor::CpuTimes SystemMonitor::readCpuTimes() {
    std::ifstream file("/proc/stat");
    std::string line;
    CpuTimes times = {0, 0, 0, 0, 0, 0, 0};
    if (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string cpu;
        iss >> cpu >> times.user >> times.nice >> times.system
            >> times.idle >> times.iowait >> times.irq >> times.softirq;
    }
    return times;
}