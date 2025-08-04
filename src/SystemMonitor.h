// SystemMonitor.h
#pragma once

#include <string>

class SystemMonitor {
public:
    SystemMonitor();
    double getCpuUsage();
    double getMemoryUsage();

private:
    struct CpuTimes {
        long long user, nice, system, idle, iowait, irq, softirq;
        long long getTotal() const;
        long long getActive() const;
    };

    CpuTimes previousCpu;
    bool firstRun = true;

    CpuTimes readCpuTimes();
};