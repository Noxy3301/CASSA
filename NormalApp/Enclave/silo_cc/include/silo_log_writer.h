#pragma once

#include <fcntl.h>  // f controlって読むらしい
#include <unistd.h> // for write, fsync, close
#include <iostream>

class PosixWriter {
    private:
        int fd_;
    
    public:
        void open(const std::string &filePath) {
            fd_ = ::open(filePath.c_str(), O_WRONLY|O_CREAT, 0644);
            // std::cout << "fd_ = " << fd_ << " path = " << filePath << std::endl;
            if (fd_ == -1) {
                perror("open failed");
                std::abort();
            }
        }

        void write(void *log_data, size_t size) {
            size_t s = 0;
            while (s < size) {  // writeは最大size - s(byte)だけ書き出すから事故対策でwhile loopしている？
                ssize_t r = ::write(fd_, (char*)log_data + s, size - s);    // returnは書き込んだbyte数
                if (r <= 0) {
                    perror("write failed");
                    std::abort();
                }
                s += r;
            }
        }

        // writeはディスクキャッシュに書き出すだけでディスクに書き出すことを保証していない(kernelが暇なときにやる)から、fsyncでファイル書き込みを保証しているらしい
        // fsyncは同期処理が完了しないと返ってこない
        void sync() {
            if (::fsync(fd_) == -1) {
                perror("fsync failed");
                std::abort();
            }
        }

        void close() {
            if (::close(fd_) == -1) {
                perror("close failed");
                std::abort();
            }
        }
};