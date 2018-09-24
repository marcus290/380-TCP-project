#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "hash-table.h"

/**
 * Struct for storing data of the tracefile line.
 */
struct packet {
	unsigned int seqNum;
	double timeStamp;
	unsigned int payloadSize;
	int syn;
	int fin;
	uint64_t connID;
};

struct connection {
	unsigned long sourceIP;
	unsigned long sourcePort;
	unsigned long destIP;
	unsigned long destPort;
};

/**
 * Function for creating a 64-bit long identifier for each connection. The 64-bits are allocated as follows:
 * Source IP: 16 bits for last 2 numbers of IP address
 * Source port: 16 bits
 * Dest IP: 16 bits for last 2 numbers of IP address
 * Dest port: 16 bits
 * NOTE: This implementation assumes source IP always starts with "192.168." and dest IP always 
 * starts with "10.0." so only last 16 bits are used to identify IP addresses.
 * @param currConnection custom connection struct holding the connection IPs and ports
 */	
uint64_t makeID(struct connection currConnection) {
	uint64_t connID;
	connID = 	(uint64_t) (currConnection.sourceIP % 0x10000L << 48 +
				currConnection.sourcePort << 32 +
				currConnection.destIP % 0x10000L << 16 +
				currConnection.destPort);
	return connID;
}

/**
 * Function for updating the seq numbers of connections open. If out of sequence, packet is stored in array.
 * If closing the connection, connection is recorded in closed connections list and connection and associated outOfSeq packets are deleted
 * @param line String array from a line of the trace file
 */	
void updateSeqNums(ht_hash_table* connHT, struct packet currPacket) {
	//printf("handling packet no. %d at time %.5f\n", currPacket.seqNum, currPacket.timeStamp);
				
	// If packet is from new connection:
	if (ht_search(connHT, currPacket.connID) == NULL) {
		ht_insert(connHT, currPacket.connID, 1);
	// Else if packet is from open connection and matches next expected sequence number
	} else if (ht_search(connHT, currPacket.connID) == currPacket.seqNum) {
		printf("next sequence number of %d will be replaced by updateSeqNums() !!", currPacket.seqNum);
		ht_insert(connHT, currPacket.connID, currPacket.seqNum + currPacket.payloadSize);
	// Else if packet is out of sequence.
	} else {
		storeOOSPacket(currPacket);
	}
}

/**
 * Function for handling out of sequence packets from the trace stream. 
 */	
void storeOOSPacket(struct packet currPacket) {
	// If packet is from connections open:
	;
}

/**
 * Function for parsing the tcp input file.
 */
void parse(const char* filename) {
	puts("parse function entered!");
	FILE *file;
	file = fopen(filename, "r");
	if(file == NULL) {
      perror("Error opening file");
   	}	
	char buff[16] = {0};
	int batchCt = 0;
	int batchSize = 50000;
	int lineCt = 0;
	int charCt = 0;
	struct packet currPacket;
	struct connection currConnection;

//Initialize IP address buffers and counters for parsing trace file
	char ip[5] = {0};
	unsigned long ipLong = 0L;
	int ctr = 0;
	int i = 0;
	int j = 0;			

//Initialize data structures for containing connections and out-of-sequence packet buffer
	ht_hash_table* connHT = ht_new();

	do {
		int c = fgetc(file);
		if(c == EOF) 
			break;
		if(c == '\n' || c == '\t') {
			charCt = 0;
			puts(buff);
			switch (lineCt) {
				case 1 : //timestamp
					currPacket.timeStamp = atof(buff);
					break;
				
				case 2 :
					while(ctr < 4) {
						if(buff[i] != '.') {
							ip[j++] = buff[i++];
						} else {
							ipLong += atol(ip) * (long) ((3 - ctr++) * 0x100);
							j = 0;
							memset(ip, 0, sizeof ip);
						}
					}
					currConnection.sourceIP = ipLong;
					break;
				
				case 3 :
					currConnection.sourcePort = atol(buff);
					break;
				
				case 4 :
					char ip[5] = {0};
					unsigned long ipLong = 0L;
					int ctr = 0;
					int i = 0;
					int j = 0;
					while(ctr < 4) {
						if(buff[i] != '.') {
							ip[j++] = buff[i++];
						} else {
							ipLong += atol(ip) * (long) ((3 - ctr++) * 0x100);
							j = 0;
							memset(ip, 0, sizeof ip);
						}
					}
					currConnection.sourceIP = ipLong;
					break;
				
				case 5 :
					currConnection.destPort = atol(buff);
					currPacket.connID = makeID(currConnection);
					printf("Connection ID: %I64d\n", currPacket.connID);
					break;

				case 8 :
					currPacket.payloadSize = atoi(buff);
					break;
				
				case 9 :
					currPacket.syn = atoi(buff);
					break;
				
				case 11 :
					currPacket.fin = atoi(buff);
					break;

				case 13 :
					currPacket.seqNum = atoi(buff);
			}
			if (c == '\n') {
				updateSeqNums(connHT, currPacket);
				lineCt = 0;
				batchCt += 1;
				printf("Batch counter: %d\n", batchCt);
			} else {
				lineCt += 1;
			}
			memset(buff, 0, sizeof buff);
			charCt = 0;
		} else {
			buff[charCt++] = c;
		}
		
	} while(batchCt < batchSize);

	fclose(file);

}


int main(int argc, char *argv[]) {
    //HashMap<Long, Integer> connectionsDict = new HashMap<>();
	//HashMap<Long, HashMap<Integer, oOSPacket>> outOfSeq = new HashMap<>(); //hashmap of packets that are out of sequence
	//ArrayList<Long> closedConnections = new ArrayList<Long>(); //record of connections which have all packets accounted for in sequence and closed
	puts("program started!");
	char defaultFile[] = "trace-small.txt";
    unsigned int packetCounter = 0;
	unsigned int byteCounter = 0;
	unsigned int connectionCounter = 0;
	unsigned int missingBytes = 0;

	const char* filename = (argc > 1) ? argv[1] : defaultFile;
	puts(filename);


	parse(filename);

	

	return(0);	
}