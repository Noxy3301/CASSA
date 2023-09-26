# 最低要求バージョンの設定
cmake_minimum_required(VERSION 3.13)

# プロジェクト名とC++のバージョンの設定
project(CASSA_NormalApp)
set(CMAKE_CXX_STANDARD 17)

# スレッドライブラリの検索
find_package(Threads REQUIRED)

# ソースコードの設定
file(GLOB_RECURSE CASSA_SOURCES
    "App/*.cpp"
    "App/**/*.cpp"
    "Enclave/*.cpp"
    "Enclave/**/*.cpp"
)

# 実行可能ファイルの生成
add_executable(cassa.exe ${CASSA_SOURCES})

# コンパイルオプションの設定
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
# デバッグビルドのためのコンパイルオプション
target_compile_options(cassa.exe PUBLIC -O0 -g -std=c++11 -m64)

# pepochファイルの生成
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/log0/pepoch
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/log0
    COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_BINARY_DIR}/log0/pepoch
    COMMENT "Creating pepoch file in log0 directory"
)
add_custom_target(
    create_pepoch_file ALL
    DEPENDS ${CMAKE_BINARY_DIR}/log0/pepoch
)
add_dependencies(cassa.exe create_pepoch_file)
