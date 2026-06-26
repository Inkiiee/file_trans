#include <iostream>
#include <string>
#include <thread>

#include "file_trans_server.h"
#include "udp_multicast_server.h"

int main(int argc, char* argv[]){
    if(argc != 4){
        std::cerr << "Usage: " << argv[0] << " <address> <port> <id>" << std::endl;
        return 1;
    }

    std::jthread udp_multicast_thread([argv](){
        try{
            udp_multicast::UdpMulticastServer udp_multicast_server("239.255.0.1", "30001", argv[1], argv[2], argv[3]);
            udp_multicast_server.start();
        }
        catch(const std::exception& e){
            std::cerr << "Exception in UDP Multicast Server: " << e.what() << std::endl;
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