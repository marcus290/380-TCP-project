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
	unsigned int sourceIP1 = (unsigned int) (connID >> 56);
	unsigned int sourceIP2 = (unsigned int) (connID >> 48) % 0x100;
	unsigned int sourcePort = (unsigned int) (connID >> 32) % 0x10000;
	unsigned int destIP1 = (unsigned int) (connID >> 24) % 0x100;
	unsigned int destIP2 = (unsigned int) (connID >> 16) % 0x100;
	unsigned int destPort = (unsigned int) connID % 0x10000;
	sprintf(str, "192.168.%d.%d/%d to 10.0.%d.%d/%d", 
			sourceIP1, sourceIP2, sourcePort, destIP1, destIP2, destPort);
}

/**
 * Function for handling out of sequence packets from the trace stream. 
 */	
void storeOOSPacket(oOS_ht_hash_table* oOSHT, struct packet currPacket) {
	struct packet* pkt = calloc(1, sizeof(struct packet));
	pkt->seqNum = currPacket.seqNum;
	pkt->timeStamp = currPacket.timeStamp;
	if (currPacket.timeStamp == 0) {puts("Error: Bad packet and invalid timestamp!");exit(0);}
	pkt->payloadSize = currPacket.payloadSize;
	pkt->syn = currPacket.syn;
	pkt->fin = currPacket.fin;
	pkt->connID = currPacket.connID;
	// If connection is not already in oOS buffer, initialize heap and add key(connID) and value(heap):
	if (oOS_ht_search(oOSHT, currPacket.connID) == 0) {
		struct heap* heap = calloc(1, sizeof(struct heap));
		heap_init(heap);
		heap_push(heap, pkt);
		oOS_ht_insert(oOSHT, currPacket.connID, heap);
	// If connection already in oOS buffer
	} else if (oOS_ht_search(oOSHT, currPacket.connID) != 0) {
		struct heap* h = oOS_ht_search(oOSHT, currPacket.connID);
		heap_push(h, pkt);
		//printf("top of heap is seqNum %d\n", heap_front(h)->seqNum);
	}
}

/**
 * Function for updating the sequence number by checking the out-of-sequence packets buffer. 
 */	
int updateSeqNumsFromBuffer(ht_hash_table* connHT, oOS_ht_hash_table* oOSHT, struct packet currPacket) {
	struct heap* connOOSHeap = oOS_ht_search(oOSHT, currPacket.connID);
	int connClosed = 0;
	if (connOOSHeap != 0) {
		struct packet* nextOOSPacket = heap_front(connOOSHeap);
		unsigned long nextOOSSeqNum = nextOOSPacket->seqNum;
		unsigned long prevOOSSeqNum;
		while(nextOOSSeqNum == ht_search(connHT, currPacket.connID)->seqNum && connOOSHeap->count) { // If the buffer contains the next packet
			ht_search(connHT, currPacket.connID)->seqNum = nextOOSSeqNum + nextOOSPacket->payloadSize + nextOOSPacket->fin;
			ht_search(connHT, currPacket.connID)->timeStamp = nextOOSPacket->timeStamp;
			if (nextOOSPacket->fin)	connClosed = 1; // If the sequenced packet from the buffer is FIN, close the connection
			//printf("updateSeqNumsFromBuffer() assigned new seqNum %d at time %.3f!\n", 
			//		nextOOSSeqNum + nextOOSPacket->payloadSize + nextOOSPacket->fin, currPacket.timeStamp);
			do {
				heap_pop(connOOSHeap);
				if (connOOSHeap->count == 0) break;
				prevOOSSeqNum = nextOOSSeqNum;
				nextOOSPacket = heap_front(connOOSHeap);
				nextOOSSeqNum = nextOOSPacket->seqNum;
				//printf("prevOOSSeqNum: %d\nnextOOSSeqNum: %d\n", prevOOSSeqNum, nextOOSSeqNum);
			} while (prevOOSSeqNum == nextOOSSeqNum); // Check for duplicate packets in buffer and remove
		}
		// Clean up and delete the OOS buffer if no more OOS packets or connection closed
		if (connClosed || oOS_ht_search(oOSHT, currPacket.connID)->count == 0) {
			// If connection closed or buffer is empty, remove the OOS buffer
			oOS_ht_delete(oOSHT, currPacket.connID);
			//puts("after oOS delete");
		}
		
	} 
	return connClosed;
}

/**
 * Function for updating the linked list of closed connections. 
 */	
void updateClosedConns(struct node** head, uint64_t connID) {
	struct node* newNode = calloc(1, sizeof(struct node));
	newNode->connID = connID;
	newNode->next = *head;
	*head = newNode;
}

/**
 * Function for updating the seq numbers of connections open. If out of sequence, packet is stored in array.
 * If closing the connection, connection is recorded in closed connections list and connection and associated outOfSeq packets are deleted
 * @param line String array from a line of the trace file
 */	
int updateSeqNums(ht_hash_table* connHT, oOS_ht_hash_table* oOSHT, struct packet currPacket) {
	int connClosed = 0;
	printf("Handling packet no. %d at time %.5f of connection %llx\n", currPacket.seqNum, currPacket.timeStamp, currPacket.connID);
					
	// If packet is from new connection:
	if (ht_search(connHT, currPacket.connID) == NULL) {
		struct connStatus* newConn = calloc(1, sizeof(struct connStatus));
		ht_insert(connHT, currPacket.connID, newConn);
		newConn->seqNum = 1;
		newConn->timeStamp = currPacket.timeStamp;
		if (currPacket.timeStamp == 0) {puts("Error: Bad packet and invalid timestamp!");exit(0);}
		connClosed = updateSeqNumsFromBuffer(connHT, oOSHT, currPacket);
	// Else if packet is from open connection and matches next expected sequence number
	} else if (ht_search(connHT, currPacket.connID)->seqNum == currPacket.seqNum) {
		ht_search(connHT, currPacket.connID)->seqNum = currPacket.seqNum + currPacket.payloadSize + currPacket.fin;
		ht_search(connHT, currPacket.connID)->timeStamp = currPacket.timeStamp;
		connClosed = updateSeqNumsFromBuffer(connHT, oOSHT, currPacket);
		// If connection closed from current packet, clean up any packets from the OOS buffer
		if (currPacket.fin) {
			connClosed = 1;
			// if (oOS_ht_search(oOSHT, currPacket.connID) != NULL)
			// 	oOS_ht_delete(oOSHT, currPacket.connID);
		}
	// Else if packet is out of sequence.
	} else if (ht_search(connHT, currPacket.connID)->seqNum < currPacket.seqNum) {
		// Store packet in buffer if it has a later sequence number
		storeOOSPacket(oOSHT, currPacket);
	}
	//if (ht_search(connHT, (uint64_t) 0x131f4000089bbe)->timeStamp == 0) {puts("Error: Bad packet and invalid timestamp!");exit(0);}
	return connClosed;
}

/**
 * Function for storing information of missing packets over 20s before the end of trace file in a the linked list. 
 */	
void updateWarningNodes(struct warningNode** warningHead, uint64_t connID, double timeStamp, unsigned long bytesMissing) {
	struct warningNode* newNode = calloc(1, sizeof(struct warningNode));
	newNode->connID = connID;
	newNode->timeStamp = timeStamp;
	newNode->bytesMissing = bytesMissing;
	newNode->next = *warningHead;
	*warningHead = newNode;
}

/**
 * Function for outputting the summary statistics.
 */
void summary(ht_hash_table* connHT, struct node* head, oOS_ht_hash_table* oOSHT, int packetCt, int byteCt, double lastTimeStamp, const char* outputFilename) {
	FILE *file;
	file = fopen(outputFilename, "w");
	if(file == NULL) {
      perror("Error opening output file");
   	}		
	
	puts("\nParse finished! Analysing trace statistics...");
	fputs("======================================================================================\n", file);
	fprintf(file, "* OUTPUT FROM PACKET LOSS ANALYSIS of %s\n", outputFilename);
	fputs("======================================================================================\n\n", file);
	
	int connCt = 0;
	int openConnCt = 0;
	struct warningNode* warningHead = NULL;
	int over60sWarningFlag = 0;

	// Delete completed connections
	struct node* nodePtr = head;
	printf("\nDeleting completed connections... ");
	while (nodePtr != NULL) {
		ht_delete(connHT, nodePtr->connID);
		connCt++;
		nodePtr = nodePtr->next;
	}
	puts("Done.\n");

	// Print open connections
	char ipString[30];
	puts("\nConnections still open:");
	fputs("\nConnections still open:\n", file);
	for (int i = 0; i < connHT->size; i++) {
		if (connHT->items[i] == NULL || connHT->items[i]->key == 0L) continue;
		IDToString(ipString, connHT->items[i]->key);
		printf("%s expecting seq num %d since %.3f\n", 
				ipString, connHT->items[i]->value->seqNum, connHT->items[i]->value->timeStamp);
		fprintf(file, "%s expecting seq num %d since %.3f\n", 
				ipString, connHT->items[i]->value->seqNum, connHT->items[i]->value->timeStamp);
		if (connHT->items[i]->value->timeStamp < lastTimeStamp - 20)
			updateWarningNodes(&warningHead, connHT->items[i]->key, connHT->items[i]->value->timeStamp, 0L);
		connCt++;
		openConnCt++;
	}
	if (openConnCt == 0) {
		puts("None.");
		fputs("None.\n", file);
	}

	// Collate missing packets and print to terminal
	struct heap* h = calloc(1, sizeof(struct heap));
	unsigned long lastSeqNum;
	unsigned long prevSeqNum;
	unsigned long totalMissingBytes = 0;
	struct packet* nextOOSPacket = calloc(1, sizeof(struct packet));
	// printf("oOSHT count: %d\n", oOSHT->count);
	// printf("oOSHT size: %d\n", oOSHT->size);
	for (int i = 0; i < oOSHT->size; i++) {
		if (oOSHT->items[i] == NULL || oOSHT->items[i]->key == 0L) continue;
		IDToString(ipString, oOSHT->items[i]->key);
		printf("\nBytes missing from %s: \n", ipString);
		fprintf(file, "\nBytes missing from %s: \n", ipString);
		h = oOSHT->items[i]->value;
		// printf("heap count: %d\n", h->count);
		// printf("heap size: %d\n", h->size);
		nextOOSPacket = heap_front(h);
		// printf("Packet seqnum: %d; Timestamp: %.3f\n", nextOOSPacket->seqNum, nextOOSPacket->timeStamp);
		// Check expected seqNum
		lastSeqNum = ht_search(connHT, oOSHT->items[i]->key)->seqNum;
		while (h->count != 0) {
			// printf("Last sequence number: %d \n", lastSeqNum);
			nextOOSPacket = heap_front(h);
			if (!lastSeqNum) {
				totalMissingBytes += lastSeqNum;
				printf("%d missing bytes between start of connection and seq num %d (incl. SYN phantom byte) at time %.3f\n",
						nextOOSPacket->seqNum, nextOOSPacket->seqNum, nextOOSPacket->timeStamp);
				fprintf(file, "%d missing bytes between start of connection and seq num %d (incl. SYN phantom byte) at time %.3f\n",
						nextOOSPacket->seqNum, nextOOSPacket->seqNum, nextOOSPacket->timeStamp);
				if (nextOOSPacket->timeStamp < lastTimeStamp - 20)
					updateWarningNodes(&warningHead, oOSHT->items[i]->key, nextOOSPacket->timeStamp, nextOOSPacket->seqNum);			
			} else if (lastSeqNum != nextOOSPacket->seqNum) { 
				totalMissingBytes += nextOOSPacket->seqNum - lastSeqNum;
				printf("%d missing bytes between seq num %d and seq num %d at time %.3f\n",
						nextOOSPacket->seqNum - lastSeqNum, lastSeqNum, nextOOSPacket->seqNum, nextOOSPacket->timeStamp);
				fprintf(file, "%d missing bytes between seq num %d and seq num %d at time %.3f\n",
						nextOOSPacket->seqNum - lastSeqNum, lastSeqNum, nextOOSPacket->seqNum, nextOOSPacket->timeStamp);
				if (nextOOSPacket->timeStamp < lastTimeStamp - 20)
					updateWarningNodes(&warningHead, oOSHT->items[i]->key, nextOOSPacket->timeStamp, nextOOSPacket->seqNum - lastSeqNum);
			}
			lastSeqNum = nextOOSPacket->seqNum + nextOOSPacket->payloadSize;
			//puts("finished while loop (before pop)");
			heap_pop(h);
		}
		
	}

	// Print summary statistics
	puts("\n\nSummary:");
	printf("%d packets checked containing a total of %d bytes from %d connections.\n\n", packetCt, byteCt, connCt);
	printf("%d / %d bytes missing from trace sequence (%.3f%% loss).\n\n", totalMissingBytes, byteCt, totalMissingBytes / (double) byteCt);
	printf("Subsequent packets from %d open connection(s) could not be analysed.\n\n\n", openConnCt);

	fputs("======================================================================================\n", file);
	fputs("\n\nSummary:\n", file);
	fprintf(file, "%d packets checked containing a total of %d bytes from %d connections.\n\n", packetCt, byteCt, connCt);
	fprintf(file, "%d / %d bytes missing from trace sequence (%.3f%% loss).\n\n", totalMissingBytes, byteCt, totalMissingBytes / (double) byteCt);
	fprintf(file, "Subsequent packets from %d open connection(s) could not be analysed.\n\n\n", openConnCt);

	// Print warning for missing bytes (i) 60s before trace end and (ii) 20s before trace end
	puts("======================================================================================");
	fputs("======================================================================================\n", file);
	if (warningHead != NULL) {
		puts("* Warning! Packets missing/connections open since before last 20 s of trace!\n");
		fputs("* Warning! Packets missing/connections open since before last 20 s of trace!\n", file);
		while (warningHead != NULL) {
			IDToString(ipString, warningHead->connID);
			if (warningHead->bytesMissing == 0) {
				printf("%s open since %.3f\n", ipString, warningHead->timeStamp);
				fprintf(file, "%s open since %.3f\n", ipString, warningHead->timeStamp);
			} else {
				printf("%d bytes missing from %s since %.3f\n", warningHead->bytesMissing, ipString, warningHead->timeStamp);
				fprintf(file, "%d bytes missing from %s since %.3f\n", warningHead->bytesMissing, ipString, warningHead->timeStamp);
			}
			
			if (warningHead->timeStamp < lastTimeStamp - 60) over60sWarningFlag = 1;
			warningHead = warningHead->next;
		}
		if (over60sWarningFlag) {
			puts("*\n* Warning! Packets are missing since before last 60 s of trace!\n*\n");
			fputs("*\n* Warning! Packets are missing since before last 60 s of trace!\n*\n\n", file);
		}
	} else {
		puts("* No packets missing before last 20 s of trace.");
		fputs("* No packets missing before last 20 s of trace.\n", file);
	}
	puts("======================================================================================");
	puts("\n");
	fputs("======================================================================================\n", file);
	fputs("\n", file);


	fclose(file);
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
	int packetCt = 0;
	int byteCt = 0;
	int lineCt = 0;
	int charCt = 0;
	int dataComplete = 1;
	struct packet currPacket;
	struct connection currConnection;
	int connClosed = 0;
	double lastTimeStamp;
	char *outputSuffix = "-PacketLoss.txt";
	char outputFile[30] = {0};

//Initialize IP address buffers and counters for parsing trace file
	char ip[5] = {0};
	unsigned long ipLong = 0L;
	int ctr = 0;
	int i = 0;
	int j = 0;	
	
//Initialize data structures for containing connections and out-of-sequence packet buffer
	ht_hash_table* connHT = ht_new();
	oOS_ht_hash_table* oOSHT = oOS_ht_new();
	struct node* head = NULL;

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
					byteCt += currPacket.payloadSize;
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
					connClosed = updateSeqNums(connHT, oOSHT, currPacket);
				}
				if (connClosed) {
					updateClosedConns(&head, currPacket.connID);
					connClosed = 0;
				}
				lineCt = 0;
				dataComplete = 1;
				batchCt++;
				packetCt++;
				if (packetCt % 1000 == 0) {
					printf("%d packets parsed.\n", packetCt);
				}
			} else {
				lineCt += 1;
			}
			memset(buff, 0, sizeof buff);
			charCt = 0;
		} else {
			buff[charCt++] = c;
		}
		
	} while(1);

	lastTimeStamp = currPacket.timeStamp;

	// Create the output file name XXX-PacketLoss.txt
	strcat(outputFile, filename);
	int len = strlen(outputFile);
	for (int i = len - 1; i > len - 5; i--) outputFile[i] = 0; //delete the .txt suffix
	puts(outputFile);
	strcat(outputFile, outputSuffix);
	summary(connHT,head, oOSHT, packetCt, byteCt, lastTimeStamp, outputFile);
	puts("summary exited!");
	fclose(file);

}


int main(int argc, char *argv[]) {
    puts("program started!");
	char defaultFile[] = "trace-small.txt";
    unsigned int packetCounter = 0;
	unsigned int byteCounter = 0;
	unsigned int connectionCounter = 0;
	unsigned int missingBytes = 0;

	const char* filename = (argc > 1) ? argv[1] : defaultFile;
	puts(filename);

	parse(filename);
	puts("parse exited!");
	return(0);	
}