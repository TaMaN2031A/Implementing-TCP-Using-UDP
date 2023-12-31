#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fstream>
#include <iostream>
#include <chrono>
#include <thread>
#include <bits/stdc++.h>
typedef struct packet packet;
typedef std::priority_queue<packet, std::vector<packet>, std::greater<packet>> pck_queue;
typedef struct ack_packet ack_packet;
typedef std::priority_queue<ack_packet, std::vector<ack_packet>, std::greater<ack_packet>> ack_queue;
using namespace std;
using namespace std;
void DieWithUserMessage(const char *msg, const char *detail){
    fputs(msg, stderr);
    fputs(": ", stderr);
    fputs(detail, stderr);
    fputc('\n', stderr);
    exit(1);
}
void DieWithSystemMessage(const char *msg){
    perror(msg);
    exit(1);
}
struct ack_packet {
    uint16_t chsum;
    uint16_t len;
    uint16_t ackno;
    bool operator>(const ack_packet& other) const {
        return ackno > other.ackno;
    }
};
struct packet {
    uint16_t chsum;
    uint16_t len;
    uint16_t seqno;
    uint16_t finished;
    char data[500];
    bool operator>(const packet& other) const {
        return seqno > other.seqno;
    }
};
// "Wa la Yasodonnoka A'an A'ayat ElLah ba'd Eth Onzelat Elayk
// Wa Ed'oo Ela Rabek
// Wa la takonan men Al moshreken"
int total_buffer_size = 2*(1e5);
char* buffer = new char[total_buffer_size];
pck_queue minQueue;
pck_queue tempPckQueue;
ack_queue ackQueue;
bool last_pack_sent;
ack_packet formulate_one_ack_packet(ssize_t ackno){
    ack_packet ackPacket{};
    ackPacket.ackno = ackno;
    ackPacket.len = 6;
    ackPacket.chsum = 0;
    return ackPacket;
}
void formulate_ack_packets_queue(ack_queue &ackq, pck_queue &the_temp){
    while(!the_temp.empty()){
        ackq.push(formulate_one_ack_packet(the_temp.top().seqno));
        the_temp.pop();
    }
}
std::ofstream debug("debug.txt", std::ios::app); // Open the file in append mode

void formulate_packet_from_buffer(char* buffer, size_t numBytes, pck_queue & pckQueue, pck_queue &temp) {
    const size_t chunkSize = 508;
    size_t offset = 0;
    while (offset < numBytes) {
        packet pkt;
        pkt.chsum = *reinterpret_cast<uint16_t*>(buffer + offset);
        pkt.len = *reinterpret_cast<uint16_t*>(buffer + offset + 2);
        pkt.seqno = *reinterpret_cast<uint16_t*>(buffer + offset + 4);
        pkt.finished = *reinterpret_cast<uint16_t*>(buffer + offset + 6);
        debug << pkt.seqno << " " << pkt.len << " " << pkt.finished << std::endl;
        last_pack_sent = (last_pack_sent) || (pkt.finished == 1);
        std::memcpy(pkt.data, buffer + offset + 8, pkt.len);
        // Here chsum determines whether to put in ackQueue
        pckQueue.push(pkt);
        temp.push(pkt);
        offset += chunkSize; // SUs
    }
}
void formulate_buffer_from_ack_queue(ack_queue& ackPacketQueue, char* ackBuffer) {
    size_t bufferOffset = 0;
    while (!ackPacketQueue.empty()) {
        const ack_packet& ackPkt = ackPacketQueue.top();
        std::memcpy(ackBuffer + bufferOffset, &ackPkt, sizeof(ack_packet));
        bufferOffset += sizeof(ack_packet);
        ackPacketQueue.pop();
    }
}
void try_to_write_in_file(uint16_t &waiting_for_package_x,
                          pck_queue &pckQueue,
                          ofstream &outputFile) {
    while (!pckQueue.empty() && pckQueue.top().seqno == waiting_for_package_x) {
        const packet& pkt = pckQueue.top();
        outputFile.write(pkt.data, pkt.len);
        waiting_for_package_x++;
        pckQueue.pop();
    }
    //outputFile.close();
}
void selective_repeat(int sock, char fileName[]){
    ofstream outputFile(fileName, ios::binary);
    struct sockaddr temp{};
    socklen_t sz = sizeof(temp);
    uint16_t waiting_for_package_x = 0;
    unsigned long n, no;
    long l = 0;
    while(true){
        ssize_t r = recvfrom(sock, (void*)&no, sizeof(no),
                             MSG_WAITALL,&temp, &sz);
        n = (no);
        if(r < 0)
            perror("receivefrom() failed");
        ssize_t b = sendto(sock, "ok", sizeof("ok"),
                           0,&temp, sz);
        if(b < 0)
            perror("sendto() failed");
        int received_packets_size = n*508;
        l += recvfrom(sock, buffer, received_packets_size,
                 MSG_WAITALL, &temp, &sz);
        l-=8*n;
        formulate_packet_from_buffer(buffer, received_packets_size, minQueue, tempPckQueue);
        formulate_ack_packets_queue(ackQueue, tempPckQueue);
        formulate_buffer_from_ack_queue(ackQueue, buffer);
        int sent_packets_size = n*6;
        ssize_t se = sendto(sock, buffer, sent_packets_size,
                            0,&temp, sz);
        if(se < 0)
            perror("sendto() failed");
        try_to_write_in_file(waiting_for_package_x, minQueue, outputFile);
        cout << minQueue.size() << " " << last_pack_sent << endl;
        if(minQueue.empty() && last_pack_sent) {
            cout << "Done" << endl;
            cout << waiting_for_package_x << endl;
            break;
        }
    }
    outputFile.close();
    delete[] buffer;
}
void stop_and_wait(int sock, char fileName[]){
    ofstream outputFile(fileName, ios::binary);
    ssize_t numBytes;
    int s = 0;
    struct sockaddr temp{};
    socklen_t sz;
    uint16_t waiting_for_package_x = 0;
    int i = 0;
    while(true){
        i++;
        packet receivedPacket{};
        numBytes = recvfrom(sock, (void*)&receivedPacket, 508, MSG_WAITALL,
                            &temp, &sz);
        if(numBytes < 8){
            if (numBytes < 0)
                DieWithSystemMessage("recvfrom() failed");
            exit(-1);
        }
        s+=numBytes-8;
        if(receivedPacket.seqno != waiting_for_package_x){
            exit(-1);
        }
        waiting_for_package_x++;
        outputFile.write(receivedPacket.data, receivedPacket.len-8);
        ack_packet ackPacket{};
        ackPacket.chsum= (0);
        ackPacket.ackno= (receivedPacket.seqno);
        ackPacket.len = (6);
        sendto(sock, &ackPacket, 6,
                   0, &temp, sz);
        if(receivedPacket.finished == 1)
            break;
    }
    cout << i << " iterations" << endl;
    cout << "Real size is " << s << endl;
    outputFile.close();
}
int main(int argc, char *argv[]) {

    if (argc != 2) // Test for correct number of arguments
        DieWithUserMessage("Parameter(s)",
                           "<Server Address/Name> <Input File Address>");

    const char* inputFileName = argv[1];
    std::ifstream inputFile(inputFileName);
    if (!inputFile.is_open()) {
        DieWithUserMessage("Wrong File",
                           "FIle Doesn't Exist");
    }
    char line1[256], line2[256], fileName[256];
    inputFile.getline(line1, sizeof(line1));
    inputFile.getline(line2, sizeof(line2));
    inputFile.getline(fileName, sizeof(fileName));

    inputFile.close();
    size_t length1 = strlen(line1);
    if (length1 > 0 && line1[length1 - 1] == '\n') {
        line1[length1 - 1] = '\0';
    }

    size_t length2 = strlen(line2);
    if (length2 > 0 && line2[length2 - 1] == '\n') {
        line2[length2 - 1] = '\0';
    }

    size_t length3 = strlen(fileName);
    if (length3 > 0 && fileName[length3 - 1] == '\n') {
        fileName[length3 - 1] = '\0';
    }

    char *server = line1;
    char *servPort = line2;
    struct addrinfo addrCriteria;
    memset(&addrCriteria, 0, sizeof(addrCriteria));
    addrCriteria.ai_family = AF_INET;
    addrCriteria.ai_socktype = SOCK_DGRAM;
    addrCriteria.ai_protocol = IPPROTO_UDP;
    struct addrinfo *servAddr;
    /**
     * The client here is trying to reach the server
     * through the system call getaddinfo*/
    int rtnVal = getaddrinfo(server, servPort, &addrCriteria, &servAddr);
    if (rtnVal != 0)
        DieWithUserMessage("getaddrinfo() failed", gai_strerror(rtnVal));
    int sock = socket(servAddr->ai_family, servAddr->ai_socktype,
                      servAddr->ai_protocol); // Socket descriptor for client
    if (sock < 0)
        DieWithSystemMessage("socket() failed");

    ssize_t numBytes = sendto(sock, fileName, strlen(fileName), 0,
                                  servAddr->ai_addr, servAddr->ai_addrlen);
    while (numBytes == -1){
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // Timeout occurred
            std::cerr << "Timeout occurred during sendto" << std::endl;
        } else {
            std::cerr << "Error in sendto: " << strerror(errno) << std::endl;
        }
        numBytes = sendto(sock, fileName, strlen(fileName), 0,
                          servAddr->ai_addr, servAddr->ai_addrlen);
    }
    selective_repeat(sock, fileName);

    close(sock);
    exit(0);
}
