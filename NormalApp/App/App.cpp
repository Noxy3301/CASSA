#include <iostream>
#include <iomanip> // setw
#include <thread> // hardware_concurrency
#include <fstream> // ifstream

#include "App.h"

#include "../Include/consts.h"
#include "utils/command_handler.hpp"
#include "utils/parse_command.hpp"
#include "utils/include/logger_affinity.h"

// システム情報の表示関数
void displaySystemInfo() {
    std::cout << "\033[32m" << "[ INFO     ] " << "\033[0m" 
              << "System Information" << std::endl;

    // 利用可能なスレッドの数を取得する
    unsigned int numThreads = std::thread::hardware_concurrency();
    // スレッド数が0(取得できない)場合
    if(numThreads == 0) {
        std::cout << "\033[31m" << "[ ERROR    ] " << "\033[0m" 
                  << "Unable to detect available threads or no threads are available." << std::endl;
        exit(1);
    }
    // ワーカースレッドの数とロガースレッドの数が利用可能なスレッドの数より多い場合
    if (WORKER_NUM + LOGGER_NUM > numThreads) {
        std::cout << "\033[31m" << "[ ERROR    ] " << "\033[0m" 
                  << "The number of Worker Threads and Logger Threads must be less than or equal to the number of available threads." << std::endl;
        exit(1);
    }
    // ワーカースレッド数が0以下の場合
    if (WORKER_NUM <= 0) {
        std::cout << "\033[31m" << "[ ERROR    ] " << "\033[0m" 
                  << "There must be at least one Worker Thread. (check WORKER_NUM indicated in consts.h)" << std::endl;
        exit(1);
    }
    // ロガースレッドの数が0以下の場合
    if (LOGGER_NUM <= 0) {
        std::cout << "\033[31m" << "[ ERROR    ] " << "\033[0m" 
                  << "There must be at least one Logger Thread. (check LOGGER_NUM indicated in consts.h)" << std::endl;
        exit(1);
    }
    // ロガースレッドがhardware_concurrency()より多い場合
    if(LOGGER_NUM >= WORKER_NUM) {
        std::cout << "\033[33m" << "[ WARNING  ] " << "\033[0m" 
                  << "It is recommended to set LOGGER_NUM to be less than WORKER_NUM to avoid the possibility of having unused logger threads." << std::endl;
    }

    // CPUクロック周波数の取得
    unsigned long cpuFreq = 0; // 初期化しておく
    std::ifstream("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq") >> cpuFreq;

    int labelWidth = 20; // Width for the labels
    int valueWidth = 10; // Width for the values
    std::cout << std::left  << std::setw(35) << "  ____    _    ____ ____    _"           << std::right << std::setw(labelWidth) <<        "SystemName : " << std::left << std::setw(valueWidth) << "CASSA" << std::endl;
    std::cout << std::left  << std::setw(35) << " / ___|  / \\  / ___/ ___|  / \\"        << std::right << std::setw(labelWidth) <<           "Version : " << std::left << std::setw(valueWidth) << "0.0.1" << std::endl;
    std::cout << std::left  << std::setw(35) << "| |     / _ \\ \\___ \\___ \\ / _ \\"    << std::right << std::setw(labelWidth) << "Available Threads : " << std::left << std::setw(valueWidth) << numThreads << std::endl;
    std::cout << std::left  << std::setw(35) << "| |___ / ___ \\ ___) |__) / ___ \\"      << std::right << std::setw(labelWidth) <<  "- Worker threads : " << std::left << std::setw(valueWidth) << WORKER_NUM << std::endl;
    std::cout << std::left  << std::setw(35) << " \\____/_/   \\_\\____/____/_/   \\_\\"  << std::right << std::setw(labelWidth) <<  "- Logger threads : " << std::left << std::setw(valueWidth) << LOGGER_NUM << std::endl;
    std::cout << std::right << std::setw(35) <<                              "ver.0.0.1 " << std::right << std::setw(labelWidth) <<         "CPU Clock : " << std::left << std::setw(valueWidth) << (std::to_string(cpuFreq/1000) + " MHz") << std::endl;

    // 警告文を表示する
    if (!cpuFreq) std::cerr << "\033[31m" << "[ ERROR    ] " << "\033[0m" << "reading CPU frequency!" << std::endl;
    if (cpuFreq != CLOCKS_PER_US * 1000) std::cout << "\033[33m[ CAUTION  ] CPU Clock is different from CLOCKS_PER_US (" + std::to_string(CLOCKS_PER_US) + " MHz)!\033[0m" << std::endl;
}

// Attestation関数
void performAttestation() {
    std::cout << "\033[32m" << "[ INFO     ] " << "\033[0m" << "Performing Attestation...\n";
    // Attestationの詳細ロジックをここに実装する
}

// Enclaveの初期化関数
void initializeEnclave(size_t num_worker_threads, size_t num_logger_threads) {
    std::cout << "\033[32m" << "[ INFO     ] " << "\033[0m" << "Initializing Enclave...\n";
    // Enclaveの初期化ロジックをここに実装する
    // データがある場合はリカバリも行う
    

    // Enclave内のグローバル変数を初期化しておく
    // TODO: Epochに関してはpepochとかで書きだしているはずだから、それを読み込めるようにする
    ecall_initializeGlobalVariables(num_worker_threads, num_logger_threads);
}

void worker_thread_work(size_t worker_thid, size_t logger_thid) {
    ecall_worker_thread_work(worker_thid, logger_thid);
}

void logger_thread_work(size_t logger_thid) {
    ecall_logger_thread_work(logger_thid);
}

// Worker/Loggerスレッドの初期化と配置
void createThreads(std::vector<std::thread>& worker_threads, std::vector<std::thread>& logger_threads) {
    std::cout << "\033[32m" << "[ INFO     ] " << "\033[0m" << "create threads...\n";
    // WorkerとLoggerのスレッドを均等にCPUコアに割り当て、グループ化するアフィニティを設定する
    // アフィニティに基づいてWorkerとLoggerのスレッドを起動する
    LoggerAffinity affin;
    affin.init(WORKER_NUM, LOGGER_NUM);
    size_t w_thid = 0;  // Workerのthread ID
    size_t l_thid = 0;  // Loggerのthread ID, Workerのgroup IDとしても機能する
    for (auto itr = affin.nodes_.begin(); itr != affin.nodes_.end(); itr++, l_thid++) {
        logger_threads.emplace_back(logger_thread_work, l_thid);
        for (auto wcpu = itr->worker_cpu_.begin(); wcpu != itr->worker_cpu_.end(); wcpu++, w_thid++) {
            worker_threads.emplace_back(worker_thread_work, w_thid, l_thid);
        }
    }

    // Enclave内で全てのスレッドが準備完了になるまで待機する
    ecall_waitForReady();
}

// ユーザーからの命令を待機する関数
void awaitUserCommands() {
    std::cout << "\033[32m" << "[ INFO     ] " << "\033[0m" 
              << "Awaiting User Commands... (type '/help' for available commands, '/exit' to quit)" << std::endl;

    CommandHandler handler;
    std::vector<std::string> procedure;
    bool in_procedure = false;
    bool in_transaction = false;

    while (true) {
        // show procedure prompt if in procedure
        if (in_procedure) {
            std::cout << "\033[32m" << "=== start of procedure ===" << std::endl;
            for (auto proc : procedure) {
                std::cout << proc << std::endl;
            }
            std::cout << "===  end of procedure  ===" << "\033[0m" << std::endl;
        }

        // get user input
        std::cout << "> ";
        std::string command;
        std::getline(std::cin, command);

        // exit the loop if EOF is detected or if "/exit" command is entered by the user
        if (std::cin.eof() || command == "/exit") {
            procedure.clear();  // 念のため
            break;
        }

        // handle /help command
        if (command == "/help") {
            handler.printHelp();
            continue;
        }

        // handle /mkproc command
        if (command == "/mkproc") {
            if (in_procedure) {
                std::cout << "\033[31m" << "[ ERROR    ] " << "\033[0m" 
                          << "Error: Already in a procedure. Please end the current procedure first." << std::endl;
            } else {
                in_procedure = true;
                procedure.clear();
            }
            continue;
        }

        // handle /endproc command
        if (command == "/endproc") {
            if (!in_procedure) {
                std::cout << "\033[31m" << "[ ERROR    ] " << "\033[0m" 
                          << "Error: Not in a procedure. Please begin a procedure first." << std::endl;
            } else {
                if (in_transaction) {
                    std::cout << "\033[31m" << "[ ERROR    ] " << "\033[0m" 
                              << "Error: Still in a transaction. Please end the current transaction first." << std::endl;
                } else {
                    in_procedure = false;
                    if (procedure.size() > 0) {
                        // create json object for procedure
                        nlohmann::json procedure_json = parseCommand(procedure);
                        
                        // dump json object to string
                        std::string procedure_json_string = procedure_json.dump();
                        uint8_t *dumped_json_data = reinterpret_cast<const uint8_t*>(procedure_json_string.data());
                        size_t dumped_json_size = procedure_json_string.size();

                        // send procedure to enclave
                        size_t worker_thid = handler.rnd() % WORKER_NUM;
                        ecall_push_tx(worker_thid, handler.txIDcounter_++, dumped_json_data, dumped_json_size);

                        // clear procedure
                        procedure.clear();
                    }
                }
            }
            continue;
        }

        if (in_procedure) {
            if (command == "BEGIN_TRANSACTION") {
                if (in_transaction) {
                    std::cout << "\033[31m" << "[ ERROR    ] " << "\033[0m" 
                              << "Error: Already in a transaction. Please end the current transaction first." << std::endl;
                } else {
                    in_transaction = true;
                    procedure.push_back(command);
                }
            } else if (command == "END_TRANSACTION") {
                if (!in_transaction) {
                    std::cout << "\033[31m" << "[ ERROR    ] " << "\033[0m" 
                              << "Error: Not in a transaction. Please begin a transaction first." << std::endl;
                } else {
                    in_transaction = false;
                    procedure.push_back(command);
                }
            } else if (in_transaction) {
                    std::pair<bool, std::string> check_syntax_result = handler.checkOperationSyntax(command);
                    if (check_syntax_result.first) {
                        procedure.push_back(command);
                    } else {
                        std::cout << "\033[31m" << "[ ERROR    ] " << "\033[0m" 
                                  << check_syntax_result.second << std::endl;
                    }
            } else if (!in_transaction) {
                    std::cout << "\033[31m" << "[ ERROR    ] " << "\033[0m" 
                              << "Error: Not in a transaction. Please begin a transaction first." << std::endl;
            } else {
                std::cout << "\033[31m" << "[ ERROR    ] " << "\033[0m" 
                          << "Error: Invalid command. Please type '/help' for available commands." << std::endl;
            }
            continue;
        }

        // show error message because valid command was not entered here
        std::cout << "\033[31m" << "[ ERROR    ] " << "\033[0m" 
                  << "Error: Invalid command. Please type '/help' for available commands." << std::endl;
    }
}

void destroyThreads(std::vector<std::thread>& worker_threads, std::vector<std::thread>& logger_threads) {
    std::cout << "\033[32m" << "[ INFO     ] " << "\033[0m" << "Destroying Threads..." << std::endl;

    // Enclaveにスレッドの終了を通知する
    ecall_terminateThreads();

    // すべてのロガーとワーカースレッドが完了するのを待つ
    for (auto &th : logger_threads) th.join();
    for (auto &th : worker_threads) th.join();
}

// Enclaveの破壊関数
void destroyEnclave() {
    std::cout << "\033[32m" << "[ INFO     ] " << "\033[0m" << "Destroying Enclave..." << std::endl;
    // Enclaveの破壊ロジックをここに実装する
}

void ocall_print_commit_message(size_t txIDcounter, size_t num_process_record) {
    std::cout << "\r";
    std::cout << "\033[32m" << "Transaction " << txIDcounter << " committed." << "\033[0m"
              << " ( " << std::to_string(num_process_record) + " record" << (num_process_record >= 2 ? "s" : "") << " processed. )" << std::endl;
    std::cout << "> ";
    std::cout.flush();
}

int main() {
    // 利用可能なスレッドの数を取得する
    std::vector<std::thread> worker_threads;
    std::vector<std::thread> logger_threads;

    displaySystemInfo();
    performAttestation();
    initializeEnclave(WORKER_NUM, LOGGER_NUM);
    createThreads(worker_threads, logger_threads);
    awaitUserCommands();
    destroyThreads(worker_threads, logger_threads);
    destroyEnclave();

    return 0;
}