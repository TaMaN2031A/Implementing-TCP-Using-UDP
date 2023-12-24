//
// Created by taman on 22/12/23.
//
#include <cstring>
#include "Formatter.h"

packet Formatter::formulate_packet(int i, std::ifstream& file, long fileSize, int total_no_packets) {
    packet pkt;
    pkt.seqno = static_cast<uint16_t>(i);
    file.seekg(i * sizeof(pkt.data), std::ios::beg);
    size_t bytesToRead = sizeof(pkt.data);
    if (fileSize - i * sizeof(pkt.data) < sizeof(pkt.data)) {
        bytesToRead = fileSize - i * sizeof(pkt.data);
    }
    file.read(pkt.data, bytesToRead);
    // Calculate chsum
    pkt.chsum = 77;
    pkt.len = static_cast<uint16_t>(bytesToRead);
    pkt.finished = (i==total_no_packets-1);
    return pkt;
}
void Formatter::push_packets_to_queue(pck_queue &minQueue,
                           unsigned long limit_of_queue_size,
                           int &pointer_on_the_next_not_in_queue,
                           std::ifstream& file,
                           long fileSize,
                           int total_no_packets){
    while(minQueue.size() < limit_of_queue_size){
        minQueue.push(formulate_packet(pointer_on_the_next_not_in_queue++, file, fileSize, total_no_packets));
    }

}
void Formatter::formulate_buffer(pck_queue & minQueue, pck_queue & tempQueue,
                      char* buffer, size_t& bufferSize) {
    size_t offset = 0;
    while (!minQueue.empty()) {
        packet currentPacket = minQueue.top();
        minQueue.pop();
        memcpy(buffer + offset, &currentPacket, sizeof(packet));
        offset += sizeof(packet);
        tempQueue.push(currentPacket);
    }
    while (!tempQueue.empty()) {
        minQueue.push(tempQueue.top());
        tempQueue.pop();
    }
    bufferSize = offset;
}
ack_queue Formatter::formulate_ack_queue_from_buffer(const char* buffer, size_t bufferSize) {
    ack_queue ackPacketQueue;
    size_t numAckPackets = bufferSize / sizeof(ack_packet);
    for (size_t i = 0; i < numAckPackets; ++i) {
        ack_packet ackPacket;
        memcpy(&ackPacket, buffer + i * sizeof(ack_packet), sizeof(ack_packet));
        ackPacketQueue.push(ackPacket);
    }
    return ackPacketQueue;
}
// Function to remove acknowledged packets from minQueue
void Formatter::remove_acknowledged_packets_from_minQueue(pck_queue& minQueue
        , ack_queue& ackPacketQueue
        , int &sent_and_acked) {
    pck_queue newMinQueue;  // Create a new priority queue to store the non-acknowledged packets
    while (!minQueue.empty()) {
        if(minQueue.top().seqno != ackPacketQueue.top().ackno) {
            newMinQueue.push(minQueue.top());
        }else{
            sent_and_acked++;
            ackPacketQueue.pop();
        }
        minQueue.pop();
    }

    while (!newMinQueue.empty()) {
        minQueue.push(newMinQueue.top());
        newMinQueue.pop();
    }
}