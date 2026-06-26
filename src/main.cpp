#include <iostream>
#include <string>
#include <thread>

#include "file_trans_server.h"
#include "udp_broadcast_server.h"

int main(int argc, char* argv[]){
    if(argc != 4){
        std::cerr << "Usage: " << argv[0] << " <address> <port> <id>" << std::endl;
        return 1;
    }

    std::jthread udp_broadcast_thread([argv](){
        try{
            udp_broadcast::UdpBroadcastServer udp_broadcast_server("30001", argv[1], argv[2], argv[3]);
            udp_broadcast_server.start();
        }
        catch(const std::exception& e){
            std::cerr << "Exception in UDP Broadcast Server: " << e.what() << std::endl;
        }
    });

    try{
        file_trans::FileTransServer server(argv[1], argv[2]);
        server.start();
    }
    catch(const std::exception& e){
        std::cerr << "Exception in File Transfer Server: " << e.what() << std::endl;
    }
    return 0;
}