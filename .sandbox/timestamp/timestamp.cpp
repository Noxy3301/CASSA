#include <iostream>
#include <ctime>
#include <cstdio>
#include <sstream>
#include <iomanip>

std::string timespecToString(const timespec &ts) {
    struct tm t;
    char buf[32];

    // Convert into local and parsed time
    localtime_r(&ts.tv_sec, &t);

    // Create string with strftime
    strftime(buf, 32, "%Y/%m/%d %H:%M:%S", &t);

    // Add milliseconds
    int msec = ts.tv_nsec / 1000000;
    std::stringstream ss;
    ss << buf << "." << std::setfill('0') << std::setw(3) << msec;

    return ss.str();
}

timespec stringToTimespec(const std::string &timeStr) {
    struct tm t;
    int msec;
    timespec ts;
    std::stringstream ss(timeStr);

    ss >> std::get_time(&t, "%Y/%m/%d %H:%M:%S");
    ss.ignore(); // skip '.'
    ss >> msec;

    // Convert to timespec
    ts.tv_sec = mktime(&t);
    ts.tv_nsec = msec * 1000000;

    return ts;
}

// compare two timestamps
int compare_timestamps(const timespec& ts1, const timespec& ts2) {
    if (ts1.tv_sec < ts2.tv_sec) {
        return -1; // ts1 < ts2
    } else if (ts1.tv_sec > ts2.tv_sec) {
        return 1; // ts1 > ts2
    } else if (ts1.tv_nsec < ts2.tv_nsec) {
        return -1; // ts1 < ts2
    } else if (ts1.tv_nsec > ts2.tv_nsec) {
        return 1; // ts1 > ts2
    }
    return 0; // ts1 == ts2
}

int main() {
    timespec ts1, ts2;

    // get current time twice
    clock_gettime(CLOCK_REALTIME, &ts1);
    clock_gettime(CLOCK_REALTIME, &ts2);

    // check the timestamps (seconds and nanoseconds)
    std::cout << "ts1: " << ts1.tv_sec << "." << ts1.tv_nsec << "(" << timespecToString(ts1) << ")" << std::endl;
    std::cout << "ts2: " << ts2.tv_sec << "." << ts2.tv_nsec << "(" << timespecToString(ts2) << ")" << std::endl;

    // compare the timestamps
    int result = compare_timestamps(ts1, ts2);

    if (result < 0) {
        std::cout << "ts1 < ts2" << std::endl;
    } else if (result > 0) {
        std::cout << "ts1 > ts2" << std::endl;
    } else {
        std::cout << "ts1 == ts2" << std::endl;
    }

    return 0;
}
