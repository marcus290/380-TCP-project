#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "hash-table.h"
#include "min-heap.h"
#include "PacketLoss.h"


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
	connID = 	((uint64_t) (currConnection.sourceIP % 0x10000L) << 48) +
				((uint64_t) currConnection.sourcePort << 32) +
				((uint64_t) (currConnection.destIP % 0x10000L) << 16) +
				(uint64_t) currConnection.destPort;
	return connID;
}

/**
 * Function for reverting the connection ID to a string.
 */	
void IDToString(char *str, uint64_t connID) {
	memset(str, 0, sizeof str);
	int sourceIP1 = (int) (connID >> 56);
	int sourceIP2 = (int) (connID >> 48) % 0x100;
	int sourcePort = (int) (connID >> 32) % 0x10000;
	int destIP1 = (int) (connID >> 24) % 0x100;
	int destIP2 = (int) (connID >> 16) % 0x100;
	int destPort = (int) connID % 0x10000;
	sprintf(str, "192.168.%d.%d/%d to 10.0.%d.%d/%d", 
			sourceIP1, sourceIP2, sourcePort, destIP1, destIP2, destPort);
}

/**
 * Function for handling out of sequence packets from the trace stream. 
 */	
void storeOOSPacket(oOS_ht_hash_table* oOSHT, struct packet currPacket) {
	// If connection is not already in oOS buffer connection:
	if (ht_search(oOSHT, currPacket.connID) == 0L) {

		ht_insert(oOSHT, currPacket.connID, 1);
	}
	 
}


/**
 * Function for updating the seq numbers of connections open. If out of sequence, packet is stored in array.
 * If closing the connection, connection is recorded in closed connections list and connection and associated outOfSeq packets are deleted
 * @param line String array from a line of the trace file
 */	
void updateSeqNums(ht_hash_table* connHT, oOS_ht_hash_table* oOSHT, struct packet currPacket) {
	printf("handling packet no. %d at time %.5f of connection %llx\n", currPacket.seqNum, currPacket.timeStamp, currPacket.connID);
	// If packet is from new connection:
	if (ht_search(connHT, currPacket.connID) == 0L) {
		ht_insert(connHT, currPacket.connID, 1);
	// Else if packet is from open connection and matches next expected sequence number
	} else if (ht_search(connHT, currPacket.connID) == currPacket.seqNum) {
		printf("next sequence number of %d will be replaced by updateSeqNums() !!\n", currPacket.seqNum);
		ht_insert(connHT, currPacket.connID, currPacket.seqNum + currPacket.payloadSize);
	// Else if packet is out of sequence.
	} else {
		storeOOSPacket(oOSHT, currPacket);
	}
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
	int dataComplete = 1;
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
	oOS_ht_hash_table* oOSHT = oOS_ht_new();

	do {
		int c = fgetc(file);
		if(c == EOF) 
			break;
		if(c == '\n' || c == '\t') {
			switch (lineCt) {
				case 1 : //timestamp
					currPacket.timeStamp = atof(buff);
					break;
				
				case 2 :
					if (charCt == 0)
						dataComplete = 0;
					while(ctr < 4) {
						if(buff[i] == '.' || buff[i] == 0) {
							ipLong += atol(ip) << ((3 - ctr++) * 8);
							j = 0;
							i++;
							memset(ip, 0, sizeof ip);
						} else {
							ip[j++] = buff[i++];
						}
					}
					currConnection.sourceIP = ipLong;
					ctr = 0;
					i = 0;
					ipLong = 0L;
					break;
				
				case 3 :
					if (charCt ==0)
						dataComplete = 0;
					currConnection.sourcePort = atol(buff);
					break;
				
				case 4 :
					if (charCt == 0)
						dataComplete = 0;
					while(ctr < 4) {
						if(buff[i] == '.' || buff[i] == 0) {
							ipLong += atol(ip) << ((3 - ctr++) * 8);
							j = 0;
							i++;
							memset(ip, 0, sizeof ip);
						} else {
							ip[j++] = buff[i++];
						}
					}
					currConnection.destIP = ipLong;
					ctr = 0;
					i = 0;
					ipLong = 0L;
					break;
				
				case 5 :
					if (charCt ==0)
						dataComplete = 0;
					currConnection.destPort = atol(buff);
					currPacket.connID = makeID(currConnection);
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
				if (dataComplete) {
					updateSeqNums(connHT, oOSHT, currPacket);
				}
				lineCt = 0;
				dataComplete = 1;
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