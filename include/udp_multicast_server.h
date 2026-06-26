#ifndef __UDP_MULTICAST_SERVER_H__
#define __UDP_MULTICAST_SERVER_H__

#include <asio.hpp>
#include <memory>
#include <cstdint>
#include <array>
#include <vector>

namespace udp_multicast{
    enum class PacketType: uint16_t{
        UNKNOWN_REQUEST = 0x0000,
        REQUEST_SERVER_INFO = 0x0001,
        SERVER_INFO_RESPONSE = 0x0002,
        SERVER_INFO_ERROR = 0x0003
    };

    class UdpMulticastServer{
    private:
        asio::io_context io_context_;
        asio::ip::udp::socket socket_;
        asio::ip::udp::endpoint sender_endpoint_;
        asio::ip::udp::endpoint multicast_endpoint_;
        std::vector<uint8_t> recv_buffer_;

        std::string target_server_address_;
        std::string target_server_port_;
        std::string id_;

        asio::awaitable<void> start_receive();
        asio::awaitable<void> handle_receive(const std::vector<uint8_t>& data, const asio::ip::udp::endpoint& sender_endpoint);
        asio::awaitable<void> send_packet(std::vector<uint8_t> data, const asio::ip::udp::endpoint& target_endpoint);
        
        std::vector<uint8_t> create_packet(PacketType packet_type, const std::vector<uint8_t>& payload);
        std::vector<uint8_t> create_packet(PacketType packet_type, const std::string& payload_str);
    public:
        UdpMulticastServer(
            const std::string& multicast_address, 
            const std::string& port, 
            const std::string& target_server_address, 
            const std::string& target_server_port,
            const std::string& id
        );
        ~UdpMulticastServer();

        void start();
        void stop();
    };
} // namespace udp_multicast

#endif // __UDP_MULTICAST_SERVER_H__