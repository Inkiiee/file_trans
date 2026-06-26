#include <asio.hpp>
#include <asio/experimental/channel.hpp>
#include <iostream>
#include <memory>
#include <vector>
#include <stdexcept>

#include "trans_session.h"

using asio::as_tuple;
using asio::awaitable;
using asio::co_spawn;
using asio::const_buffer;
using asio::detached;
using asio::experimental::channel;
using asio::io_context;
using asio::ip::tcp;
using asio::steady_timer;
namespace filesystem = std::filesystem;
using namespace asio::buffer_literals;
using namespace std::literals::chrono_literals;
using namespace file_trans;

constexpr size_t MAX_PAYLOAD_SIZE = 1024 * 16; // 16 KB

TransSession::TransSession(tcp::socket socket) : socket_(std::move(socket)){
    socket_.set_option(tcp::no_delay(true));

    current_path = filesystem::current_path();
    base_path = current_path;
    is_uploading = false;
}

awaitable<void> TransSession::handle_payload(PacketType packet_type, const std::vector<uint8_t>& payload){
    if(packet_type == PacketType::HEARTBEAT) co_return;
    else if(packet_type == PacketType::REQUEST_FILE_TRANSFER)
        co_await handle_file_transfer_request(payload);
    else if(packet_type == PacketType::REQUEST_FILE_LIST)
        co_await handle_file_list_request(payload);
    else if(packet_type == PacketType::REQUEST_FILE_INFO)
        co_await handle_file_info_request(payload);
    else if(packet_type == PacketType::REQUEST_FILE_DELETE)
        co_await handle_file_delete_request(payload);
    else if(packet_type == PacketType::REQUEST_FILE_RENAME)
        co_await handle_file_rename_request(payload);
    else if(packet_type == PacketType::REQUEST_DIRECTORY_CREATE)
        co_await handle_directory_create_request(payload);
    else if(packet_type == PacketType::REQUEST_FILE_MOVE)
        co_await handle_file_move_request(payload);
    else if(packet_type == PacketType::REQUEST_FILE_COPY)
        co_await handle_file_copy_request(payload);
    else if(packet_type == PacketType::REQUEST_FILE_UPLOAD)
        co_await handle_file_upload_request(payload);
    else if(packet_type == PacketType::REQUEST_SYSTEM_COMMAND)
        co_await handle_system_command_request(payload);
    else if(packet_type == PacketType::REQUEST_CURRENT_DIRECTORY)
        co_await handle_current_directory_request(payload);
    else if(packet_type == PacketType::REQUEST_CHANGE_DIRECTORY)
        co_await handle_change_directory_request(payload);
    else if(packet_type == PacketType::FILE_TRANSFER_DATA)
        co_await handle_file_transfer_data(payload);
    else if(packet_type == PacketType::FILE_TRANSFER_COMPLETE)
        co_await handle_file_transfer_complete(payload);
    else if(packet_type == PacketType::FILE_TRANSFER_ERROR)
        co_await handle_file_transfer_error(payload);
}

awaitable<void> TransSession::write_packet(std::vector<uint8_t> packet){
    co_await write_lock_.async_send();
    co_await asio::async_write(socket_, asio::buffer(packet));
    write_lock_.try_receive([](auto...){});
}
awaitable<void> TransSession::read_packet(){
    try{
        for(;;){
            std::vector<uint8_t> header(8);
            auto [e1, n1] = co_await asio::async_read(socket_, asio::buffer(header), as_tuple);
            if(e1)
                throw std::runtime_error("Failed to read packet header");

            auto [is_header_valid, packet_type, payload_length] = parse_packet_header(header);
            if(!is_header_valid || payload_length > MAX_PAYLOAD_SIZE || payload_length < 0)
                throw std::runtime_error("Invalid packet header");
            
            std::vector<uint8_t> payload(payload_length);
            auto [e2, n2] = co_await asio::async_read(socket_, asio::buffer(payload), as_tuple);
            if(e2)
                throw std::runtime_error("Failed to read packet payload");
            
            co_await handle_payload(packet_type, payload);
        }
    }
    catch(std::exception& e){
        stop();
        std::cerr << "Exception: " << e.what() << "\n";
    }
}
awaitable<void> TransSession::send_heartbeats(){
    steady_timer timer{socket_.get_executor()};
    try{
        for(;;){
            timer.expires_after(1s);
            co_await timer.async_wait();
            
            co_await write_packet(make_heartbeat_packet());
        }
    }
    catch(std::exception& e){
        stop();
        std::cerr << "Exception: " << e.what() << "\n";
    }
}

awaitable<void> TransSession::handle_file_transfer_request(const std::vector<uint8_t>& payload){
    std::cout << "REQUEST_FILE_TRANSFER received" << std::endl;
    char file_name[MAX_PAYLOAD_SIZE] = {0};
    std::copy(payload.begin(), payload.end(), file_name);
    std::cout <<" * File Name: " << file_name << std::endl;

    filesystem::path file_path = file_name;
    if(!is_valid_path(file_path)){
        std::cout << " * Invalid file path" << std::endl;
        co_await write_packet(make_string_packet(PacketType::FILE_TRANSFER_ERROR, "Access denied or invalid file path"));
        co_return;
    }
    file_path = get_absolute_path(file_path);

    if(!filesystem::exists(file_path)){
        std::cout << " * File does not exist" << std::endl;
        
        co_await write_packet(make_string_packet(PacketType::FILE_TRANSFER_ERROR, "File does not exist"));
        co_return;
    }

    std::ifstream file(file_path, std::ios::binary);
    if(!file.is_open()){
        std::cout << " * Failed to open file" << std::endl;
        co_await write_packet(make_string_packet(PacketType::FILE_TRANSFER_ERROR, "Failed to open file"));
        co_return;
    }

    std::vector<uint8_t> data(MAX_PAYLOAD_SIZE);
    while(file){
        file.read(reinterpret_cast<char*>(data.data()), MAX_PAYLOAD_SIZE);
        std::streamsize bytes_read = file.gcount();
        if(bytes_read > 0){
            std::vector<uint8_t> chunk(data.begin(), data.begin() + bytes_read);
            co_await write_packet(create_packet(PacketType::FILE_TRANSFER_DATA, chunk));
        }
    }

    co_await write_packet(make_string_packet(PacketType::FILE_TRANSFER_COMPLETE, "File transfer complete"));
    std::cout << " * File transfer complete" << std::endl;
}
awaitable<void> TransSession::handle_file_list_request(const std::vector<uint8_t>& payload){
    std::cout << "REQUEST_FILE_LIST received" << std::endl;
    if(!filesystem::exists(current_path) || !filesystem::is_directory(current_path)){
        std::cout << " * Current path is not a directory" << std::endl;
        co_await write_packet(make_string_packet(PacketType::FILE_LIST_ERROR, "Current path is not a directory"));
        co_return;
    }
    for(const auto& entry : filesystem::directory_iterator(current_path)){
        std::vector<uint8_t> data;
        entry.is_directory() ? data.push_back(1) : data.push_back(0);
        uint64_t file_size = entry.is_directory() ? 0 : entry.file_size();
        data.insert(data.end(), reinterpret_cast<uint8_t*>(&file_size), reinterpret_cast<uint8_t*>(&file_size) + sizeof(file_size));
        
        std::string entry_name = entry.path().filename().string();
        data.insert(data.end(), entry_name.begin(), entry_name.end());

        co_await write_packet(create_packet(PacketType::FILE_LIST, data));
    }
    co_await write_packet(make_string_packet(PacketType::FILE_LIST_COMPLETE, "File list complete"));
}
awaitable<void> TransSession::handle_file_info_request(const std::vector<uint8_t>& payload){
    std::cout << "REQUEST_FILE_INFO received" << std::endl;
    char file_name[MAX_PAYLOAD_SIZE] = {0};
    std::copy(payload.begin(), payload.end(), file_name);
    std::cout <<" * File Name: " << file_name << std::endl;

    filesystem::path file_path = file_name;
    if(!is_valid_path(file_path)){
        std::cout << " * Invalid file path" << std::endl;
        co_await write_packet(make_string_packet(PacketType::FILE_INFO_ERROR, "Access denied or invalid file path"));
        co_return;
    }
    file_path = get_absolute_path(file_path);

    if(!filesystem::exists(file_path)){
        std::cout << " * File does not exist" << std::endl;
        co_await write_packet(make_string_packet(PacketType::FILE_INFO_ERROR, "File does not exist"));
        co_return;
    }

    uint64_t file_size = static_cast<uint64_t>(filesystem::file_size(file_path));
    uint8_t is_directory = filesystem::is_directory(file_path) ? 1 : 0;
    std::vector<uint8_t> payload_info;
    payload_info.push_back(is_directory);
    payload_info.insert(payload_info.end(), reinterpret_cast<uint8_t*>(&file_size), reinterpret_cast<uint8_t*>(&file_size) + sizeof(file_size));
    co_await write_packet(create_packet(PacketType::FILE_INFO, payload_info));
}
awaitable<void> TransSession::handle_file_delete_request(const std::vector<uint8_t>& payload){
    std::cout << "REQUEST_FILE_DELETE received" << std::endl;
    char file_name[MAX_PAYLOAD_SIZE] = {0};
    std::copy(payload.begin(), payload.end(), file_name);
    std::cout <<" * File Name: " << file_name << std::endl;

    filesystem::path file_path = file_name;
    if(!is_valid_path(file_path)){
        std::cout << " * Invalid file path" << std::endl;
        co_await write_packet(make_string_packet(PacketType::FILE_DELETE_ERROR, "Access denied or invalid file path"));
        co_return;
    }
    file_path = get_absolute_path(file_path);

    if(!filesystem::exists(file_path)){
        std::cout << " * File does not exist" << std::endl;
        co_await write_packet(make_string_packet(PacketType::FILE_DELETE_ERROR, "File does not exist"));
        co_return;
    }

    try{
        filesystem::remove(file_path);
        co_await write_packet(make_string_packet(PacketType::FILE_DELETE_SUCCESS, "File deleted successfully"));
        std::cout << " * File deleted successfully" << std::endl;
    }
    catch(const std::exception& e){
        co_await write_packet(make_string_packet(PacketType::FILE_DELETE_ERROR, e.what()));
        std::cout << " * Failed to delete file: " << e.what() << std::endl;
    }
}
awaitable<void> TransSession::handle_file_rename_request(const std::vector<uint8_t>& payload){
    std::cout << "REQUEST_FILE_RENAME received" << std::endl;
    std::string payload_str(payload.begin(), payload.end());
    auto delimiter_pos = payload_str.find('\0');
    if(delimiter_pos == std::string::npos){
        co_await write_packet(make_string_packet(PacketType::FILE_RENAME_ERROR, "Invalid payload format"));
        co_return;
    }
    std::string old_name = payload_str.substr(0, delimiter_pos);
    std::string new_name = payload_str.substr(delimiter_pos + 1);
    std::cout <<" * Old Name: " << old_name << ", New Name: " << new_name << std::endl;

    filesystem::path old_path = old_name;
    filesystem::path new_path = new_name;
    if(!is_valid_path(old_path) || !is_valid_path(new_path)){
        std::cout << " * Invalid file path" << std::endl;
        co_await write_packet(make_string_packet(PacketType::FILE_RENAME_ERROR, "Access denied or invalid file path"));
        co_return;
    }
    old_path = get_absolute_path(old_path);
    new_path = get_absolute_path(new_path);

    if(!filesystem::exists(old_path)){
        std::cout << " * Old file does not exist" << std::endl;
        co_await write_packet(make_string_packet(PacketType::FILE_RENAME_ERROR, "Old file does not exist"));
        co_return;
    }
    if(filesystem::exists(new_path)){
        std::cout << " * New file already exists" << std::endl;
        co_await write_packet(make_string_packet(PacketType::FILE_RENAME_ERROR, "New file already exists"));
        co_return;
    }

    try{
        filesystem::rename(old_path, new_path);
        co_await write_packet(make_string_packet(PacketType::FILE_RENAME_SUCCESS, "File renamed successfully"));
        std::cout << " * File renamed successfully" << std::endl;
    }
    catch(const std::exception& e){
        co_await write_packet(make_string_packet(PacketType::FILE_RENAME_ERROR, e.what()));
        std::cout << " * Failed to rename file: " << e.what() << std::endl;
    }
}
awaitable<void> TransSession::handle_directory_create_request(const std::vector<uint8_t>& payload){
    std::cout << "REQUEST_DIRECTORY_CREATE received" << std::endl;
    char dir_name[MAX_PAYLOAD_SIZE] = {0};
    std::copy(payload.begin(), payload.end(), dir_name);
    std::cout <<" * Directory Name: " << dir_name << std::endl;

    filesystem::path dir_path = dir_name;
    if(!is_valid_path(dir_path)){
        std::cout << " * Invalid directory path" << std::endl;
        co_await write_packet(make_string_packet(PacketType::DIRECTORY_CREATE_ERROR, "Access denied or invalid directory path"));
        co_return;
    }
    dir_path = get_absolute_path(dir_path);

    if(filesystem::exists(dir_path)){
        std::cout << " * Directory already exists" << std::endl;
        co_await write_packet(make_string_packet(PacketType::DIRECTORY_CREATE_ERROR, "Directory already exists"));
        co_return;
    }

    try{
        filesystem::create_directory(dir_path);
        co_await write_packet(make_string_packet(PacketType::DIRECTORY_CREATE_SUCCESS, "Directory created successfully"));
        std::cout << " * Directory created successfully" << std::endl;
    }
    catch(const std::exception& e){
        co_await write_packet(make_string_packet(PacketType::DIRECTORY_CREATE_ERROR, e.what()));
        std::cout << " * Failed to create directory: " << e.what() << std::endl;
    }
}
awaitable<void> TransSession::handle_file_move_request(const std::vector<uint8_t>& payload){
    std::cout << "REQUEST_FILE_MOVE received" << std::endl;
    std::string payload_str(payload.begin(), payload.end());
    auto delimiter_pos = payload_str.find('\0');
    if(delimiter_pos == std::string::npos){
        co_await write_packet(make_string_packet(PacketType::FILE_MOVE_ERROR, "Invalid payload format"));
        co_return;
    }
    std::string source_name = payload_str.substr(0, delimiter_pos);
    std::string dest_name = payload_str.substr(delimiter_pos + 1);
    std::cout <<" * Source Name: " << source_name << ", Destination Name: " << dest_name << std::endl;

    filesystem::path source_path = source_name;
    filesystem::path dest_path = dest_name;
    if(!is_valid_path(source_path) || !is_valid_path(dest_path)){
        std::cout << " * Invalid file path" << std::endl;
        co_await write_packet(make_string_packet(PacketType::FILE_MOVE_ERROR, "Access denied or invalid file path"));
        co_return;
    }
    source_path = get_absolute_path(source_path);
    dest_path = get_absolute_path(dest_path);

    if(!filesystem::exists(source_path)){
        std::cout << " * Source file does not exist" << std::endl;
        co_await write_packet(make_string_packet(PacketType::FILE_MOVE_ERROR, "Source file does not exist"));
        co_return;
    }
    if(filesystem::exists(dest_path)){
        std::cout << " * Destination file already exists" << std::endl;
        co_await write_packet(make_string_packet(PacketType::FILE_MOVE_ERROR, "Destination file already exists"));
        co_return;
    }

    try{
        filesystem::rename(source_path, dest_path);
        co_await write_packet(make_string_packet(PacketType::FILE_MOVE_SUCCESS, "File moved successfully"));
        std::cout << " * File moved successfully" << std::endl;
    }
    catch(const std::exception& e){
        co_await write_packet(make_string_packet(PacketType::FILE_MOVE_ERROR, e.what()));
        std::cout << " * Failed to move file: " << e.what() << std::endl;
    }
}
awaitable<void> TransSession::handle_file_copy_request(const std::vector<uint8_t>& payload){
    std::cout << "REQUEST_FILE_COPY received" << std::endl;
    std::string payload_str(payload.begin(), payload.end());
    auto delimiter_pos = payload_str.find('\0');
    if(delimiter_pos == std::string::npos){
        co_await write_packet(make_string_packet(PacketType::FILE_COPY_ERROR, "Invalid payload format"));
        co_return;
    }
    std::string source_name = payload_str.substr(0, delimiter_pos);
    std::string dest_name = payload_str.substr(delimiter_pos + 1);
    std::cout <<" * Source Name: " << source_name << ", Destination Name: " << dest_name << std::endl;

    filesystem::path source_path = source_name;
    filesystem::path dest_path = dest_name;
    if(!is_valid_path(source_path) || !is_valid_path(dest_path)){
        std::cout << " * Invalid file path" << std::endl;
        co_await write_packet(make_string_packet(PacketType::FILE_COPY_ERROR, "Access denied or invalid file path"));
        co_return;
    }
    source_path = get_absolute_path(source_path);
    dest_path = get_absolute_path(dest_path);

    if(!filesystem::exists(source_path)){
        std::cout << " * Source file does not exist" << std::endl;
        co_await write_packet(make_string_packet(PacketType::FILE_COPY_ERROR, "Source file does not exist"));
        co_return;
    }
    if(filesystem::exists(dest_path)){
        std::cout << " * Destination file already exists" << std::endl;
        co_await write_packet(make_string_packet(PacketType::FILE_COPY_ERROR, "Destination file already exists"));
        co_return;
    }

    try{
        filesystem::copy_file(source_path, dest_path);
        co_await write_packet(make_string_packet(PacketType::FILE_COPY_SUCCESS, "File copied successfully"));
        std::cout << " * File copied successfully" << std::endl;
    }
    catch(const std::exception& e){
        co_await write_packet(make_string_packet(PacketType::FILE_COPY_ERROR, e.what()));
        std::cout << " * Failed to copy file: " << e.what() << std::endl;
    }
}
awaitable<void> TransSession::handle_file_upload_request(const std::vector<uint8_t>& payload){
    std::cout << "REQUEST_FILE_UPLOAD received" << std::endl;
    char file_name[MAX_PAYLOAD_SIZE] = {0};
    std::copy(payload.begin(), payload.end(), file_name);
    std::cout <<" * File Name: " << file_name << std::endl;

    filesystem::path file_path = file_name;
    if(!is_valid_path(file_path)){
        std::cout << " * Invalid file path" << std::endl;
        co_await write_packet(make_string_packet(PacketType::FILE_UPLOAD_REJECT, "Access denied or invalid file path"));
        co_return;
    }
    file_path = get_absolute_path(file_path);

    if(filesystem::exists(file_path)){
        std::cout << " * File already exists" << std::endl;
        co_await write_packet(make_string_packet(PacketType::FILE_UPLOAD_REJECT, "File already exists"));
        co_return;
    }

    co_await write_packet(make_string_packet(PacketType::FILE_UPLOAD_ACCEPT, "File upload accepted"));
    is_uploading = true;
    upload_file_path = file_path;
    upload_file.open(file_path, std::ios::binary);
    if(!upload_file.is_open()){
        std::cout << " * Failed to open file for writing" << std::endl;
        co_await write_packet(make_string_packet(PacketType::FILE_TRANSFER_ERROR, "Failed to open file for writing"));
        is_uploading = false;
        co_return;
    }
}
awaitable<void> TransSession::handle_system_command_request(const std::vector<uint8_t>& payload){
    std::cout << "REQUEST_SYSTEM_COMMAND received" << std::endl;
    std::string command(payload.begin(), payload.end());
    std::cout <<" * Command: " << command << std::endl;

    int result = system(command.c_str());
    if(result == 0){
        co_await write_packet(make_string_packet(PacketType::SYSTEM_COMMAND_SUCCESS, "Command executed successfully"));
        std::cout << " * Command executed successfully" << std::endl;
    }
    else {
        co_await write_packet(make_string_packet(PacketType::SYSTEM_COMMAND_ERROR, "Command execution failed"));
        std::cout << " * Command execution failed" << std::endl;
    }
}
awaitable<void> TransSession::handle_current_directory_request(const std::vector<uint8_t>& payload){
    std::cout << "REQUEST_CURRENT_DIRECTORY received" << std::endl;
    co_await write_packet(make_string_packet(PacketType::CURRENT_DIRECTORY, filesystem::absolute(current_path).string()));
}
awaitable<void> TransSession::handle_change_directory_request(const std::vector<uint8_t>& payload){
    std::cout << "REQUEST_CHANGE_DIRECTORY received" << std::endl;
    std::string new_dir(payload.begin(), payload.end());
    std::cout <<" * New Directory: " << new_dir << std::endl;

    auto new_path = filesystem::path(new_dir);
    if(!is_valid_path(new_path)){
        std::cout << " * Invalid directory path" << std::endl;
        co_await write_packet(make_string_packet(PacketType::CHANGE_DIRECTORY_ERROR, "Access denied or invalid directory path"));
        co_return;
    }

    current_path = get_absolute_path(new_path);
    co_await write_packet(make_string_packet(PacketType::CHANGE_DIRECTORY_SUCCESS, "Directory changed successfully"));
    std::cout << " * Directory changed successfully" << std::endl;
}
awaitable<void> TransSession::handle_file_transfer_data(const std::vector<uint8_t>& payload){
    if(is_uploading){
        std::cout << "FILE_TRANSFER_DATA received" << std::endl;
        // Handle file upload data here
        upload_file.write(reinterpret_cast<const char*>(payload.data()), payload.size());
    }
    co_return;
}
awaitable<void> TransSession::handle_file_transfer_complete(const std::vector<uint8_t>& payload){
    if(is_uploading){
        std::cout << "FILE_TRANSFER_COMPLETE received" << std::endl;
        upload_file.close();
        is_uploading = false;
    }
    co_return;
}
awaitable<void> TransSession::handle_file_transfer_error(const std::vector<uint8_t>& payload){
    if(is_uploading){
        std::cout << "FILE_TRANSFER_ERROR received" << std::endl;
        upload_file.close();
        filesystem::remove(upload_file_path);
        is_uploading = false;
    }
    co_return;
}

std::tuple<bool, PacketType, uint32_t> TransSession::parse_packet_header(const std::vector<uint8_t>& header){
    if(header.size() != 8)
        throw std::invalid_argument("Header size must be 8 bytes");

    bool is_valid = (header[0] == 'I' && header[1] == 'K');
    if(!is_valid)
        return std::make_tuple(is_valid, PacketType::NONE, 0);
    
    uint16_t packet_type = * reinterpret_cast<const uint16_t*>(header.data() + 2);
    uint32_t packet_length = * reinterpret_cast<const uint32_t*>(header.data() + 4);
    return std::make_tuple(is_valid, static_cast<PacketType>(packet_type), packet_length);
}

std::vector<uint8_t> TransSession::create_packet(PacketType packet_type, const std::vector<uint8_t>& payload){
    std::vector<uint8_t> packet(8 + payload.size());
    packet[0] = 'I';
    packet[1] = 'K';
    *reinterpret_cast<uint16_t*>(packet.data() + 2) = static_cast<uint16_t>(packet_type);
    *reinterpret_cast<uint32_t*>(packet.data() + 4) = static_cast<uint32_t>(payload.size());
    std::copy(payload.begin(), payload.end(), packet.begin() + 8);

    return packet;
}

std::vector<uint8_t> TransSession::make_string_packet(PacketType packet_type, const std::string& str){
    std::vector<uint8_t> payload(str.begin(), str.end());
    return create_packet(packet_type, payload);
}
std::vector<uint8_t> TransSession::make_heartbeat_packet(){
    PacketType packet_type = PacketType::HEARTBEAT;
    std::vector<uint8_t> payload = {'H', 'E', 'L', 'L', 'O'};
    return create_packet(packet_type, payload);
}

bool TransSession::is_valid_path(const std::filesystem::path& path){
    filesystem::path target_path;
    if(path.is_absolute())
        target_path = path;
    else 
        target_path = current_path / path;

    auto abs_path = std::filesystem::weakly_canonical(target_path);
    auto abs_base_path = std::filesystem::weakly_canonical(base_path);

    auto base_it = abs_base_path.begin();
    auto path_it = abs_path.begin();
    for (; base_it != abs_base_path.end(); ++base_it, ++path_it) {
        if (path_it == abs_path.end() || *base_it != *path_it) {
            return false;
        }
    }

    return true;
}
filesystem::path TransSession::get_absolute_path(const filesystem::path& path){
    if(path.is_absolute())
        return filesystem::weakly_canonical(path);
    else
        return filesystem::weakly_canonical(current_path / path);
}

void TransSession::start(){
    co_spawn(socket_.get_executor(), [self = shared_from_this()]{
        return self->read_packet();
    }, detached);
    co_spawn(socket_.get_executor(), [self = shared_from_this()]{
        return self->send_heartbeats();
    }, detached);
}
void TransSession::stop(){
    socket_.close();
    write_lock_.cancel();
}