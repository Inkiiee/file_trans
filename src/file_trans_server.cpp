#include <asio.hpp>
#include <iostream>
#include <memory>

#include "file_trans_server.h"
#include "trans_session.h"

using namespace file_trans;
using asio::awaitable;
using asio::co_spawn;
using asio::detached;
using asio::io_context;
using asio::ip::tcp;
using asio::as_tuple;

FileTransServer::FileTransServer(const std::string& address, const std::string& port){
    listen_endpoint_ = *tcp::resolver(io_context_).resolve(address, port, tcp::resolver::passive).begin();
}
FileTransServer::~FileTransServer(){}

awaitable<void> FileTransServer::start_accept(tcp::acceptor& acceptor){
    for(;;){
        auto [e, socket] = co_await acceptor.async_accept(as_tuple);
        if(!e){
            std::make_shared<TransSession>(std::move(socket))->start();
        }
    }
}

void FileTransServer::start(){
    try{
        tcp::acceptor acceptor(io_context_, listen_endpoint_);
        co_spawn(io_context_, start_accept(acceptor), detached);
        io_context_.run();
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << "\n";
    }
}