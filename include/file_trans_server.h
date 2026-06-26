/*
Written by: Inki Lee
*/

#ifndef __FILE_TRANS_SERVER_H__
#define __FILE_TRANS_SERVER_H__

#include <asio.hpp>

namespace file_trans{
    class FileTransServer{
    private:
        asio::io_context io_context_;
        asio::ip::tcp::endpoint listen_endpoint_;

        asio::awaitable<void> start_accept(asio::ip::tcp::acceptor& acceptor);
    public:
        FileTransServer(const std::string& address, const std::string& port);
        ~FileTransServer();

        void start();
    };
}

#endif // __FILE_TRANS_SERVER_H__ 