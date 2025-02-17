#include <iostream>

#include "Protocol.hpp"
#include "config.hpp"

#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

using namespace Protocol;
using namespace tcp;

tcp::error_t recieveMessage(socket_t socket, data_t& buf);
tcp::error_t sendMessage(socket_t socket, const_iterator begin, const_iterator end);


int main(int argc, char* argv[]) {
   // Проверка на правильность аргументов
    
    argc = 3;
    if (argc != 3) {
        std::cerr << "Использование: " << argv[0] << " <адрес> <порт>" << std::endl;
        return 1;
    }

    const char* server_address = "localhost";//argv[1];
    int server_port = 8080;//std::stoi(argv[2]);

    // Создаем сокет
    int client_socket = socket(AF_INET, SOCK_STREAM, SOCK_NONBLOCK | SO_REUSEADDR);
    if (client_socket == -1) {
        std::cerr << "Error connectiong socket" << std::endl;
        return 1;
    }

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
        std::cerr << "Error connectiong to server" << std::endl;
        close(client_socket);
        return 1;
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


        // Если пользователь ввел "exit", выходим из цикла
        if (std::strcmp(buffer, "exit") == 0) {
            break;
        }

        // Отправляем сообщение серверу
        if (send(client_socket, buffer, std::strlen(buffer), 0) == -1) {
            break;
        }

        // Получаем ответ от сервера
        ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received == -1) {
            break;
        }

        buffer[bytes_received] = '\0'; // Завершаем строку
        std::cout << "Ответ от сервера: " << buffer << std::endl;
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
void produce_task(const_iterator begin, const_iterator end, Protocol::command_t cmd);

tcp::error_t process_data(data_t process) {
    auto to_process = std::move(process);
    auto process_it = to_process.cbegin();
    while(process_it + sizeof(Header) <= to_process.cend()) {
        auto o_head = process_head(process_it);
        if(!o_head.has_value()) {
            return -1;
        }

        auto head = *o_head;
        auto begin = process_it + sizeof(Header);

        if(begin + head.payload_size > to_process.end()) {
            break;
        }

        auto end = begin + head.payload_size;
        produce_task(begin, end, head.command);

        process_it = end;
    }

    if(process_it == to_process.cbegin()) {
        process = std::move(to_process);
        return 0;
    }

    std::copy(process_it, to_process.cend(), process.begin());

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

void produce_task(const_iterator begin, const_iterator end, Protocol::command_t cmd) {
    auto [responce, code] = service::processQuery(to_process, cmd, begin, end);
    //no need in synchronization here
    //responces may be executed and sended in different order(close request too) and this is expected behaivour of protocol
    std::lock_guard lock {*socket};
    if(responce.size() != 0) {
        auto send_code = ref.sendMessage(*socket, responce.begin(), responce.end());
        code = send_code < 0 ? send_code : code;
    }

    if(code < 0) {
        ref.closeConnection(*socket);
}

int foo() {
    using namespace Protocol;
    std::cout << "starting client\n";

    while(true) {
        Protocol::command_t cmd;
        std::cout << "Enter Command: \n";

        uint16_t cmd_input;
        std::cin >> cmd_input;
        switch (cmd_input) {
            case command_t::ECHO:
            break;
            case command_t::TIME:
            break;
            case command_t::UPLOAD:
            return 0;
            case command_t::DOWNLOAD:
            return 0;
            case command_t::CLOSE:
            break;
            default:
            throw "AAAAAAAAAAAA";
        }
    }

    return 0;
}

