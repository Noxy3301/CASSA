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

    // CPUクロック周波数の取得
    unsigned long cpuFreq = 0; // 初期化しておく
    std::ifstream("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq") >> cpuFreq;
    if (!cpuFreq) std::cerr << "Error reading CPU frequency!" << std::endl;

    int width = 20; // Width for the labels
    std::cout << std::right << std::setw(width) << "SystemName : " << "CASSA" << std::endl;
    std::cout << std::right << std::setw(width) << "Version : " << "0.0.1" << std::endl;
    std::cout << std::right << std::setw(width) << "Available Threads : " << numThreads << std::endl;
    std::cout << std::right << std::setw(width) << "CPU Clock : " << cpuFreq/1000 << " MHz " 
              << (cpuFreq != CLOCKS_PER_US * 1000 ? "\033[33m[!CAUTION!] CPU Clock is different from CLOCKS_PER_US (" + std::to_string(CLOCKS_PER_US) + " MHz)!\033[0m" : "") << std::endl;
}

// Attestation関数
void performAttestation() {
    std::cout << "[ INFO ] Performing Attestation...\n";
    // Attestationの詳細ロジックをここに実装する
}

// Enclaveの初期化関数
void initializeEnclave() {
    std::cout << "[ INFO ] Initializing Enclave...\n";
    // Enclaveの初期化ロジックをここに実装する
    // データがある場合はリカバリも行う
}

// ユーザーからの命令を待機する関数
void awaitUserCommands() {
    std::cout << "[ INFO ] Awaiting User Commands... (type 'help' for available commands, 'exit' to quit)" << std::endl;
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
    std::cout << "[ INFO ] Destroying Enclave..." << std::endl;
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