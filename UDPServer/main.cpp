#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <sstream>
#include <vector>
#include <sys/wait.h>
#include <bits/stdc++.h>
#include "Formatter.h"

typedef struct packet packet;
typedef std::priority_queue<packet, std::vector<packet>, std::greater<packet>> pck_queue;
typedef struct ack_packet ack_packet;
typedef std::priority_queue<ack_packet, std::vector<ack_packet>, std::greater<ack_packet>> ack_queue;
using namespace std;

void DieWithUserMessage(const char *msg, const char *detail) {
    fputs(msg, stderr);
    fputs(": ", stderr);
    fputs(detail, stderr);
    fputc('\n', stderr);
    exit(1);
}

void DieWithSystemMessage(const char *msg) {
    perror(msg);
    exit(1);
}
// "Esbero, Wa Sabero, Wa rabeto, Wa Ittaqo Allah la'alkom tofle7on"
pck_queue minQueue;
pck_queue tempQueue;
int total_buffer_size = 2 * (1e5);
char *buffer = new char[total_buffer_size];
ack_queue ackPacketQueue;
enum State{SLOW_START, CONGESTION_AVOIDANCE};
enum Type{TCP_RENO, TCP_TAHOE};
const int MSS = 3;
int get_new_size(State state, int windowSize, Type type, bool is_time_out){
    if(is_time_out){
        return type == TCP_TAHOE ? MSS : windowSize/2;
    }else{
        return state == CONGESTION_AVOIDANCE ? windowSize + (int)ceil(double(MSS)*(double(MSS)/double(windowSize)))
        : windowSize + MSS;
    }
}
State get_new_state(State state, int windowSize, int ss_thresh, bool is_time_out){
    if(is_time_out)
        return SLOW_START;
    else
        return (state == SLOW_START && windowSize > ss_thresh) ? CONGESTION_AVOIDANCE : state;
}
int main(int argc, char *argv[]) {
    if (argc != 2) // Test for the correct number of arguments
        DieWithUserMessage("Parameter(s)",
                           "<Server Address/Name> <Input File Address>");

    const char *inputFileName = argv[1];
    std::ifstream inputFile(inputFileName);
    if (!inputFile.is_open()) {
        DieWithUserMessage("Wrong File",
                           "File Doesn't Exist");
    }

    char line1[256];
    inputFile.getline(line1, sizeof(line1));

    int seed;
    inputFile >> seed; // Read the second line as an int

    double prob;
    inputFile >> prob; // Read the third line as a double
    cout << prob << endl;
    int t;
    inputFile >> t;
    cout << t << endl;


    // Create an engine and seed it
    std::mt19937 rng(seed);

    // Define a distribution (e.g., for integers between 1 and 100)
    std::uniform_int_distribution<int> distribution(1, 100);



    // Construct the server address structure
    struct addrinfo addrCriteria; // Criteria for address
    memset(&addrCriteria, 0, sizeof(addrCriteria)); // Zero out structure
    addrCriteria.ai_family = AF_INET; // Any address family
    addrCriteria.ai_flags = AI_PASSIVE; // Accept on any address/port
    addrCriteria.ai_socktype = SOCK_DGRAM; // Only datagram socket
    addrCriteria.ai_protocol = IPPROTO_UDP; // Only UDP socket

    struct addrinfo *servAddr; // List of server addresses
    int rtnVal = getaddrinfo(NULL, line1, &addrCriteria, &servAddr);
    if (rtnVal != 0)
        DieWithUserMessage("getaddrinfo() failed", gai_strerror(rtnVal));
    // Create socket for incoming connections
    int sock = socket(AF_INET, SOCK_DGRAM,
                      IPPROTO_UDP);
    if (sock < 0)
        DieWithSystemMessage("socket() failed");

    if (bind(sock, servAddr->ai_addr, servAddr->ai_addrlen) < 0)
        DieWithSystemMessage("bind() failed");

    freeaddrinfo(servAddr);

    for (;;) {
        struct sockaddr clntAddr{};
        socklen_t clntAddrLen = sizeof(clntAddr);;
        char buff[508];
        ssize_t numBytesRcvd = recvfrom(sock, buff, 6, 0,
                                        &clntAddr, &clntAddrLen);
        std::istringstream iss(buff);
        std::string address;
        iss >> address;
        if (numBytesRcvd < 0)
            DieWithSystemMessage("recvfrom() failed");
        pid_t pid = fork();
        if (pid == -1) {
            DieWithSystemMessage("fork() failed");
        } else if (pid == 0) {

            /** We're now in a whole new process,
             * let's open a socket for it and use
             * the address of the client saved in
             * addressOfClient sockaddr in the child
             * Client doesn't need a new socket at all,
             * it will just keep on listening on his socket
             * regardless who's sending.
             * You may ask why are we binding sockets to ports
             * here and not in client, actually we don't have to
             * bind here in child, we'll just send to the waiting client
             * and he'll get to know our address form the receive_from he's
             * using.
             * In parent server we must have bind to some socket
             * so that the client could have something fixed to send
             * the first request to.
             * */
            int newSock = socket(AF_INET, SOCK_DGRAM,
                                 0);
            if (newSock < 0)
                DieWithSystemMessage("socket() failed");

            /**
             * Reading the file*/
            cout << address << endl;
            ifstream file(address, ios::binary);
            if (!file) {
                cerr << "Error opening file." << endl;
                return 1;
            }
            file.seekg(0, ios::end);
            streampos fileSize = file.tellg();
            file.seekg(0, ios::beg);
            cout << fileSize << endl;
            vector<char> fileContent(fileSize);
            file.read(fileContent.data(), fileSize);

            /**
             * Sending the file to the client saved in
             * the black pearl address addressOfClientBlackPearl
             * */
            struct timeval timeout;
            timeout.tv_sec = 0;
            timeout.tv_usec = 40;
            setsockopt(newSock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

            /** Stop and wait*/

            const size_t chunkSize = 500;

///**Stop&wait*/{
//                for (uint16_t seqno = 0; seqno < fileSize / chunkSize; ++seqno) {
//                    packet pkt;
//                    pkt.seqno = (seqno);
//                    pkt.len = (chunkSize + 8);
//                    pkt.chsum = 88;
//                    pkt.finished = 0;
//                    memcpy(pkt.data, fileContent.data() + seqno * chunkSize, chunkSize);
//                    if (seqno == fileSize / chunkSize - 1 && fileSize % chunkSize == 0)
//                        pkt.finished = 1;
//                    /**
//                    * The real business starts*/
//                    ssize_t b = sendto(newSock, &pkt, pkt.len, 0,
//                                       &clntAddr, clntAddrLen);
//                    if (b < 0)
//                        perror("sendto() failed");
//                    ack_packet receivedPacket{};
//                    ssize_t received = recvfrom(newSock, (void *) &receivedPacket, 6,
//                                                MSG_WAITALL,
//                                                &clntAddr,
//                                                &clntAddrLen);
//                    if (received == -1) {
//                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
//                            // Timeout occurred
//                            std::cerr << "Timeout occurred during receive" << std::endl;
//                            seqno--;
//                            continue;
//                        } else {
//                            std::cerr << "Error in sendto: " << strerror(errno) << std::endl;
//                        }
//                    }
//                    if ((receivedPacket.ackno) != seqno) {
//                        cout << "EEEEEEE" << endl;
//                        cout << receivedPacket.ackno << " " << seqno << endl;
//                        exit(-1);
//                        seqno--;
//                    }
//                }
//
//                size_t lastChunkSize = fileSize % chunkSize;
//                if (lastChunkSize > 0) {
//                    packet pkt;
//                    pkt.seqno = fileSize / chunkSize; // Finished
//                    pkt.len = (lastChunkSize + 8);
//                    memcpy(pkt.data, fileContent.data() + fileSize / chunkSize * chunkSize, lastChunkSize);
//                    pkt.chsum = 0;
//                    pkt.finished = 1;
//                    ssize_t b = sendto(newSock, &pkt, 508, 0,
//                                       &clntAddr, clntAddrLen);
//                    ack_packet receivedPacket{};
//                    recvfrom(newSock, (void *) &receivedPacket, 6, MSG_WAITALL,
//                             &clntAddr, &clntAddrLen);
//                    cout << "Acknowledged message " << receivedPacket.ackno << endl;
//                }
//                cout << fileSize / chunkSize << endl;
//                exit(EXIT_SUCCESS);
//            }
/**SR*/         {
                int pointer_on_the_next_not_in_queue = 0;
                int sent_and_ack = 0;
                unsigned long totalPackets = fileSize / chunkSize + (fileSize % chunkSize != 0);
                size_t to_be_sent_size;
                int u = 0;
                Formatter formatter = Formatter();
                int ss_thresh = 25;
                int windowSize = MSS;
                State state = SLOW_START;
                Type type = t == 1 ? TCP_TAHOE : TCP_RENO;
                int round_number = 0;
                bool time_out_happened = false;
                while (sent_and_ack < totalPackets) {
                    round_number++;
                    windowSize = windowSize < totalPackets - sent_and_ack ?
                                 windowSize : totalPackets - sent_and_ack;
                    formatter.push_packets_to_queue(minQueue, (windowSize < totalPackets - sent_and_ack) ?
                                                              windowSize : totalPackets - sent_and_ack,
                                                    pointer_on_the_next_not_in_queue, file, fileSize, totalPackets);
                    // receivefrom() hena: Resource temporarily unavailable
                    pck_queue to_be_sent, temp;
                    int i = 0;
                    while (i < windowSize) {
                        int random_number = distribution(rng);
                        if (double(random_number) / 100 >= prob) {
                            to_be_sent.push(minQueue.top());
                        }
                        temp.push(minQueue.top());
                        minQueue.pop();
                        i++;
                    }
                    while (!temp.empty()) {
                        minQueue.push(temp.top());
                        temp.pop();
                    }
                    unsigned long n = to_be_sent.size();
                    if(n == 0)
                        continue;
                    ssize_t b = sendto(newSock, &n,
                                       sizeof(n), 0,
                                       &clntAddr, clntAddrLen);
                    string s = "sending the n failed" + to_string(u++);
                    if (b < 0)
                        cout << s << endl;
                    ssize_t r = recvfrom(newSock, buffer,
                                         2, MSG_WAITALL,
                                         &clntAddr, &clntAddrLen);
                    if (r < 0)
                        perror("receivefrom() hena");
                    formatter.formulate_buffer(to_be_sent, tempQueue, buffer, to_be_sent_size);
                    sendto(newSock, buffer, to_be_sent_size,
                           0, &clntAddr, clntAddrLen);

                    unsigned long ack_packets_to_received_size = to_be_sent.size() * 6;
                    recvfrom(newSock, buffer, ack_packets_to_received_size,
                             MSG_WAITALL, &clntAddr, &clntAddrLen);
                    ackPacketQueue = formatter.formulate_ack_queue_from_buffer(buffer, ack_packets_to_received_size);
                    formatter.remove_acknowledged_packets_from_minQueue(minQueue, ackPacketQueue, sent_and_ack);
                    unsigned long dropped = windowSize - n;
                    string x = type == TCP_TAHOE ? "Tahoe_." : "Reno_";
                    x += to_string(prob);
                    x += ".txt";
                    std::ofstream debug(x, std::ios::app); // Open the file in append mode
                    if(dropped != 0){
                        ssize_t v = recvfrom(newSock, buffer, (dropped) * 6,
                                             MSG_WAITALL, &clntAddr, &clntAddrLen);
                        if (v == -1) {
                            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                time_out_happened = true;
                                continue;
                            } else {
                                std::cerr << "Error in sendto: " << strerror(errno) << std::endl;
                            }
                        }
                    }
                    cout << windowSize << " " << (type == TCP_TAHOE ? "TCP_TAHOE" : "TCP_RENO")
                    << " " << (state == CONGESTION_AVOIDANCE? "CONGESTION_AVOIDANCE":"SLOW_START")
                    << endl;
                    debug << windowSize << " " << round_number << endl;
                    int tempWindowSize = windowSize;
                    // 30
                    // ssthreash 20
                    // Congestion Control
                    windowSize = min(get_new_size(state, windowSize, type, time_out_happened), 50);
                    state = get_new_state(state, windowSize, ss_thresh, time_out_happened);
                    ss_thresh = time_out_happened ? tempWindowSize/2 : ss_thresh;
                    time_out_happened = false;
                }
                cout << "Done" << endl;
                cout << sent_and_ack << endl;
                file.close();
                close(newSock);
                delete[] buffer;
                exit(0);
            }
        } else {
            int status;
            pid_t terminatedChildPid = waitpid(pid, &status, 0);

            if (terminatedChildPid == -1) {
                // Handle waitpid error
                perror("waitpid");
                return 1;
            }

            if (WIFEXITED(status)) {
                std::cout << "Child process (PID: " << terminatedChildPid << ") exited with status: "
                          << WEXITSTATUS(status) << std::endl;
            } else {
                std::cerr << "Child process did not exit normally." << std::endl;
                return 1;
            }

            std::cout << "Parent process is done." << std::endl;

        }
    }

    // NOT REACHED
}
