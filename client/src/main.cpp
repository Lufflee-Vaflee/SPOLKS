#include <iostream>

#include "Protocol.hpp"
#include "config.hpp"

#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <fcntl.h>

using namespace Protocol;
using namespace tcp;

tcp::error_t recieveMessage(socket_t socket, data_t& buf);
tcp::error_t sendMessage(socket_t socket, const_iterator begin, const_iterator end);
tcp::error_t sendQuery(socket_t socket);
tcp::error_t process_data(data_t& process, socket_t socket);
bool listening_mode = false;


int main(int argc, char* argv[]) {
   // Проверка на правильность аргументов
    argc = 3;
    if (argc != 3) {
        std::cerr << "Использование: " << argv[0] << " <адрес> <порт>" << std::endl;
        return 1;
    }

    const char* server_address = "0.0.0.0";//argv[1];
    int server_port = 8080;//std::stoi(argv[2]);

    // Создаем сокет
    
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        std::cerr << "Error connection socket " << errno << std::endl;
        return 1;
    }

    int flags = fcntl(client_socket, F_GETFL, 0);
    fcntl(client_socket, F_SETFL, flags | O_NONBLOCK);

    // Устанавливаем адрес сервера
    struct sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);

    if (inet_pton(AF_INET, server_address, &server_addr.sin_addr) <= 0) {
        std::cerr << "Incorrect Address" << std::endl;
        close(client_socket);
        return 1;
    }

    // Подключаемся к серверу
    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        if(errno != EINPROGRESS) {
            std::cerr << "Error connectiong to server" << errno << std::endl;
            close(client_socket);
            return 1;
        }
    }

    std::cout << "Connected to server " << server_address << ":" << server_port << std::endl;

    data_t recieve;
    data_t send;
    while (true) {
        auto code = recieveMessage(client_socket, recieve);
        if (code < 0) {
            std::cout << "critical error during recieve\n";
            break;
        }

        process_data(recieve, client_socket);

        if(!listening_mode && recieve.size() == 0) {
            if(sendQuery(client_socket) < 0) {
                std::cout << "critical error during send query\n";
                break;
            }
        }
    }

    // Закрываем сокет
    close(client_socket);
    std::cout << "Соединение закрыто." << std::endl;

    return 0;
}


tcp::error_t recieveMessage(socket_t socket, data_t& buf) {
    constexpr std::size_t INITIAL_CAPACITY = 2048;

    std::size_t effective_size = buf.size();
    std::size_t amortize_size = INITIAL_CAPACITY;
    do {
        if(effective_size > ULLONG_MAX - amortize_size) {
            amortize_size = ULLONG_MAX - effective_size;
        }

        buf.resize(effective_size + amortize_size);  //couldn't use capacity, because technically its UB
        int bytes_read = read(socket, buf.begin().base() + effective_size, amortize_size);

        if(bytes_read == 0) {
            std::cout << "gracefull socket shutdown: " << socket << "\n";
            return -1;
        }

        if(bytes_read < 0) {
            if(errno == EWOULDBLOCK || errno == EAGAIN) {
                buf.resize(effective_size);
                return 0;
            }

            perror("Error while reading occured");
            return -1;
        }

        effective_size += bytes_read;
        buf.resize(effective_size); //restore real size
        if(buf.size() == ULLONG_MAX) {
            break;
        }

        if(bytes_read == amortize_size)  {
            if(amortize_size >= ((ULLONG_MAX / 2) - 1)) {
                amortize_size = ULLONG_MAX;
            } else {
                amortize_size *= 2;
            }
        }
    } while(true);

    return 0;
}

tcp::error_t sendMessage(socket_t socket, const_iterator begin, const_iterator end) {
    std::size_t size = end - begin;
    auto code = send(socket, begin.base(), size, 0);
    if(code == -1) {
        if(errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }

        perror("send message failed");
        return code;
    }

    return 0;
}

std::optional<Protocol::Header> process_head(const_iterator it);
std::pair<data_t, tcp::error_t> processResponce(command_t command, const_iterator begin, const_iterator end);

tcp::error_t process_data(data_t& process, socket_t socket) {
    auto to_process = std::move(process);
    std::size_t processed = 0;
    std::size_t size = to_process.size();
    while(processed + sizeof(Header) <= to_process.size()) {
        auto o_head = process_head(to_process.begin() + processed);
        if(!o_head.has_value()) {
            return -1;
        }

        auto head = *o_head;
        std::size_t payload_start = processed + sizeof(Header);

        if(payload_start + head.payload_size > to_process.size()) {
             break;
        }

        auto begin = to_process.begin() + payload_start;
        auto end = begin + head.payload_size;
        processResponce(head.command, begin, end);

        processed = payload_start + head.payload_size;
    }

    if(processed == to_process.size()) {
        return 0;
    }

    std::copy(to_process.cbegin() + processed, to_process.cend(), std::back_inserter(process));

    return 0;
}

using const_iterator = data_t::const_iterator;

std::optional<Protocol::Header> process_head(const_iterator it) {
    using namespace Protocol;
    auto [head, _] = deserialize<Header>(it);

    if(head.version != CUR_VERSION || head.command == command_t::CLOSE) {
        data_t data;
        serialize(std::back_inserter(data), Header{
            CUR_VERSION,
            command_t::CLOSE,
            0
        });

        return {};
    }

    return head;
}

std::pair<data_t, tcp::error_t> processTime(command_t command, const_iterator begin, const_iterator end);
std::pair<data_t, tcp::error_t> processEcho(command_t command, const_iterator begin, const_iterator end);

std::pair<data_t, tcp::error_t> processResponce(command_t command, const_iterator begin, const_iterator end) {
    try {
        switch(command) {
        case TIME:
            processTime(command, begin, end);
            break;
        case ECHO:
            processEcho(command, begin, end);
            break;
        case CLOSE:
            std::cout << "close responce recieved\n";
            break;
        case UPLOAD:
            break;
        case DOWNLOAD:
            break;
        }
    } catch(std::exception exc) {
        std::cout << exc.what();
    }

    listening_mode = false;

    return {{}, 0};
}

std::pair<data_t, tcp::error_t> processTime(command_t command, const_iterator begin, const_iterator end) {
    std::cout << "time responce recieved\n";
    if(end - begin != sizeof(time_t)) {
        throw "aaaa";
    }

    auto [time, _] = deserialize<time_t>(begin);
    auto tm = localtime(&time);
    std::cout << "year: " << tm->tm_year + 1900 << ", month: " << tm->tm_mon + 1 << ", day: " << tm->tm_mday 
        << ", hour: " << tm->tm_hour << ", minute: " << tm->tm_min << ", second: " << tm->tm_sec << ", GMT: " << tm->tm_zone << '\n';

    return {{}, 0};
}

std::pair<data_t, tcp::error_t> processEcho(command_t command, const_iterator begin, const_iterator end) {
    std::cout << "echo responce recieved\n";

    std::size_t size = end - begin;

    std::string echo_msg;
    std::copy(begin, end, std::back_inserter(echo_msg));
    std::cout << echo_msg << '\n';
    return {{}, 0};
}

tcp::error_t sendQuery(socket_t socket) {
    std::cout << "enter command code:\n";

    uint16_t cmd;
    std::cin >> cmd;

    data_t query;
    std::string echo;
    try {
        switch(cmd) {
        case TIME:
            serialize(back_inserter(query), Header {
                CUR_VERSION,
                command_t::TIME,
                0
            });
            break;
        case ECHO:
            std::cout << "enter echo message\n";
            std::cin >> echo;
            serialize(std::back_inserter(query), Header {
                CUR_VERSION,
                command_t::ECHO,
                (uint32_t)echo.size()
            });

            std::copy(echo.cbegin(), echo.cend(), std::back_inserter(query));
            break;
        case CLOSE:
            serialize(std::back_inserter(query), Header{
                CUR_VERSION,
                command_t::CLOSE,
                0
            });
            break;
        case UPLOAD:
            break;
        case DOWNLOAD:
            break;
        default:
            std::cout << "invalid command code\n";
            return -1;
        }
    } catch(std::exception exc) {
        std::cout << exc.what();
    }

    listening_mode = true;
    std::cout << query.size() << "\n";
    return sendMessage(socket, query.begin(), query.end());
}



