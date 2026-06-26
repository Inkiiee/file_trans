#ifndef __TRANS_SESSION_H
#define __TRANS_SESSION_H

#include <asio.hpp>
#include <memory>
#include <asio/experimental/channel.hpp>
#include <cstdint>
#include <filesystem>
#include <fstream>

namespace file_trans{
    enum class PacketType: uint16_t{
        NONE = 0x0000,
        HEARTBEAT = 0x0001,

        REQUEST_FILE_TRANSFER = 0x0002,
        FILE_TRANSFER_DATA = 0x0003,
        FILE_TRANSFER_COMPLETE = 0x0004,
        FILE_TRANSFER_ERROR = 0x0005,

        REQUEST_FILE_LIST = 0x0007,
        REQUEST_FILE_INFO = 0x0008,
        FILE_LIST = 0x0009,
        FILE_INFO = 0x000A,
        FILE_LIST_COMPLETE = 0x000B,
        FILE_LIST_ERROR = 0x000C,
        FILE_INFO_ERROR = 0x000D,

        REQUEST_FILE_DELETE = 0x000E,
        FILE_DELETE_SUCCESS = 0x000F,
        FILE_DELETE_ERROR = 0x0010,

        REQUEST_FILE_RENAME = 0x0011,
        FILE_RENAME_SUCCESS = 0x0012,
        FILE_RENAME_ERROR = 0x0013,

        REQUEST_DIRECTORY_CREATE = 0x0014,
        DIRECTORY_CREATE_SUCCESS = 0x0015,
        DIRECTORY_CREATE_ERROR = 0x0016,

        REQUEST_FILE_MOVE = 0x0017,
        FILE_MOVE_SUCCESS = 0x0018,
        FILE_MOVE_ERROR = 0x0019,

        REQUEST_FILE_COPY = 0x001A,
        FILE_COPY_SUCCESS = 0x001B,
        FILE_COPY_ERROR = 0x001C,

        REQUEST_FILE_UPLOAD = 0x001D,
        FILE_UPLOAD_ACCEPT = 0x001E,
        FILE_UPLOAD_REJECT = 0x001F,

        REQUEST_SYSTEM_COMMAND = 0x0020,
        SYSTEM_COMMAND_SUCCESS = 0x0021,
        SYSTEM_COMMAND_ERROR = 0x0022,

        REQUEST_CURRENT_DIRECTORY = 0x0023,
        CURRENT_DIRECTORY = 0x0024,

        REQUEST_CHANGE_DIRECTORY = 0x0025,
        CHANGE_DIRECTORY_SUCCESS = 0x0026,
        CHANGE_DIRECTORY_ERROR = 0x0027,
    };

    class TransSession: public std::enable_shared_from_this<TransSession>{
    private:
        bool is_uploading = false;
        std::ofstream upload_file;
        std::filesystem::path upload_file_path;
        std::filesystem::path current_path;
        std::filesystem::path base_path;
        asio::ip::tcp::socket socket_;
        asio::experimental::channel<void()> write_lock_{socket_.get_executor(), 1};

        asio::awaitable<void> handle_payload(PacketType packet_type, const std::vector<uint8_t>& payload);
        asio::awaitable<void> write_packet(std::vector<uint8_t> packet);
        asio::awaitable<void> read_packet();
        asio::awaitable<void> send_heartbeats();

        // Request Handlers
        asio::awaitable<void> handle_file_transfer_request(const std::vector<uint8_t>& payload);
        asio::awaitable<void> handle_file_list_request(const std::vector<uint8_t>& payload);
        asio::awaitable<void> handle_file_info_request(const std::vector<uint8_t>& payload);
        asio::awaitable<void> handle_file_delete_request(const std::vector<uint8_t>& payload);
        asio::awaitable<void> handle_file_rename_request(const std::vector<uint8_t>& payload);
        asio::awaitable<void> handle_directory_create_request(const std::vector<uint8_t>& payload);
        asio::awaitable<void> handle_file_move_request(const std::vector<uint8_t>& payload);
        asio::awaitable<void> handle_file_copy_request(const std::vector<uint8_t>& payload);
        asio::awaitable<void> handle_file_upload_request(const std::vector<uint8_t>& payload);
        asio::awaitable<void> handle_system_command_request(const std::vector<uint8_t>& payload);
        asio::awaitable<void> handle_current_directory_request(const std::vector<uint8_t>& payload);
        asio::awaitable<void> handle_change_directory_request(const std::vector<uint8_t>& payload);
        asio::awaitable<void> handle_file_transfer_data(const std::vector<uint8_t>& payload);
        asio::awaitable<void> handle_file_transfer_complete(const std::vector<uint8_t>& payload);
        asio::awaitable<void> handle_file_transfer_error(const std::vector<uint8_t>& payload);

        std::tuple<bool, PacketType, uint32_t> parse_packet_header(const std::vector<uint8_t>& header);
        std::vector<uint8_t> create_packet(PacketType packet_type, const std::vector<uint8_t>& payload);
        std::vector<uint8_t> make_string_packet(PacketType packet_type, const std::string& str);
        std::vector<uint8_t> make_heartbeat_packet();

        bool is_valid_path(const std::filesystem::path& path);
        std::filesystem::path get_absolute_path(const std::filesystem::path& path);
    public:
        TransSession(asio::ip::tcp::socket socket);

        void start();
        void stop();
    };
}


#endif // __TRANS_SESSION_H