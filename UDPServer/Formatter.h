//
// Created by eyad on 22/12/23.
//

#ifndef UDPSERVER_FORMATTER_H
#define UDPSERVER_FORMATTER_H

#include <fstream>
#include <queue>
typedef struct packet packet;
typedef std::priority_queue<packet, std::vector<packet>, std::greater<packet>> pck_queue;
typedef struct ack_packet ack_packet;
typedef std::priority_queue<ack_packet, std::vector<ack_packet>, std::greater<ack_packet>> ack_queue;
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
using namespace std;
class Formatter{
public:
    packet formulate_packet(int i, std::ifstream& file, long fileSize, int total_no_packets);
    void push_packets_to_queue(pck_queue &minQueue,
                               unsigned long limit_of_queue_size,
                               int &pointer_on_the_next_not_in_queue,
                               std::ifstream& file,
                               long fileSize,
                               int total_no_packets);
    void formulate_buffer(pck_queue & minQueue, pck_queue & tempQueue,
                          char* buffer, size_t& bufferSize);
    ack_queue formulate_ack_queue_from_buffer(const char* buffer, size_t bufferSize);
    void remove_acknowledged_packets_from_minQueue(pck_queue& minQueue
            , ack_queue& ackPacketQueue
            , int &sent_and_acked);
};
#endif //UDPSERVER_FORMATTER_H
