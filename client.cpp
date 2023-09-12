#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int main() {
    // Create a socket
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        std::cerr << "Error creating socket" << std::endl;
        return 1;
    }

    // Define the server address and port
    sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(12345); // Use the same port number as the server
    server_address.sin_addr.s_addr = inet_addr("127.0.0.1"); // Use the server's IP address

    // Connect to the server
    if (connect(client_socket, (struct sockaddr*)&server_address, sizeof(server_address)) == -1) {
        std::cerr << "Error connecting to server" << std::endl;
        return 1;
    }

    std::cout << "Connected to server" << std::endl;

    // Receive the "Hello" message from the server
    char server_message[1024];
    recv(client_socket, server_message, sizeof(server_message), 0);
    std::cout << "Received message from server: " << server_message << std::endl;

    // Send an acknowledgment back to the server
    const char* acknowledgment = "Acknowledged";
    send(client_socket, acknowledgment, strlen(acknowledgment), 0);

    // Close the client socket
    close(client_socket);

    return 0;
}
