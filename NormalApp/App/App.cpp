#include <iostream>
#include <iomanip> // setw
#include <thread> // hardware_concurrency
#include <fstream> // ifstream

#include "../Include/consts.h"
#include "utils/command_handler.h"

// システム情報の表示関数
void displaySystemInfo() {
    std::cout << "[ INFO ] System Information" << std::endl;

    // 利用可能なスレッドの数を取得する
    unsigned int numThreads = std::thread::hardware_concurrency();
    unsigned int workerThreads = numThreads - LOGGER_NUM;
    // スレッド数が0(取得できない)場合
    if(numThreads == 0) {
        std::cout << "\033[31mError: Unable to detect available threads or no threads are available.\033[0m" << std::endl;
        exit(1);
    }
    // ワーカースレッド数が0以下の場合
    if (workerThreads <= 0) {
        std::cout << "\033[31mError: There must be at least one Worker Thread.\033[0m" << std::endl;
        exit(1);
    }
    // ロガースレッドの数が0以下の場合
    if (LOGGER_NUM <= 0) {
        std::cout << "\033[31mError: There must be at least one Logger Thread. (check LOGGER_NUM indicated in consts.h)\033[0m" << std::endl;
        exit(1);
    }
    // ロガースレッドがhardware_concurrency()より多い場合
    if(LOGGER_NUM >= numThreads) {
        std::cout << "\033[31mError: LOGGER_NUM must be less than the total number of available threads.\033[0m" << std::endl;
        exit(1);
    }

    int labelWidth = 20; // Width for the labels
    int valueWidth = 10; // Width for the values
    std::cout << std::right << std::setw(labelWidth) << "SystemName : "        << std::left << std::setw(valueWidth) << "CASSA" << std::endl;
    std::cout << std::right << std::setw(labelWidth) << "Version : "           << std::left << std::setw(valueWidth) << "0.0.1" << std::endl;

    unsigned int maxLoggerThreads = numThreads / 2;
    std::cout << std::right << std::setw(labelWidth) << "Available Threads : " << std::left << std::setw(valueWidth) << numThreads << std::endl;
    std::cout << std::right << std::setw(labelWidth) << "- Worker threads : "  << std::left << std::setw(valueWidth) << workerThreads << std::endl;
    std::cout << std::right << std::setw(labelWidth) << "- Logger threads : "  << std::left << std::setw(valueWidth) << LOGGER_NUM
              << (LOGGER_NUM > maxLoggerThreads ? "\033[33m[!CAUTION!] Logger threads should be within the range [1, " + std::to_string(maxLoggerThreads) + "]!\033[0m" : "") << std::endl;



    // CPUクロック周波数の取得
    unsigned long cpuFreq = 0; // 初期化しておく
    std::ifstream("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq") >> cpuFreq;
    if (!cpuFreq) std::cerr << "Error reading CPU frequency!" << std::endl;
    std::cout << std::right << std::setw(labelWidth) << "CPU Clock : "         << std::left << std::setw(valueWidth) << (std::to_string(cpuFreq/1000) + " MHz")
              << (cpuFreq != CLOCKS_PER_US * 1000 ? "\033[33m[!CAUTION!] CPU Clock is different from CLOCKS_PER_US (" + std::to_string(CLOCKS_PER_US) + " MHz)!\033[0m" : "") << std::endl;
}

// Attestation関数
void performAttestation() {
    std::cout << "Performing Attestation...\n";
    // Attestationの詳細ロジックをここに実装する
}

// Enclaveの初期化関数
void initializeEnclave() {
    std::cout << "Initializing Enclave...\n";
    // Enclaveの初期化ロジックをここに実装する
    // データがある場合はリカバリも行う
}

// ユーザーからの命令を待機する関数
void awaitUserCommands() {
    std::cout << "Awaiting User Commands... (type 'help' for available commands, 'exit' to quit)" << std::endl;
    CommandHandler handler;
    
    while (true) {
        std::cout << "> ";
        std::string command;
        std::getline(std::cin, command);
        
        if (std::cin.eof() || !handler.handleCommand(command)) {
            // EOFが検出された、またはhandler.handleCommandがfalseを返した場合
            break; // whileループを抜ける
        }
    }
}

// Enclaveの破壊関数
void destroyEnclave() {
    std::cout << "Destroying Enclave..." << std::endl;
    // Enclaveの破壊ロジックをここに実装する
}

int main() {
    displaySystemInfo();
    performAttestation();
    initializeEnclave();
    awaitUserCommands();
    destroyEnclave();

    return 0;
}