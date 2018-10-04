#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "avl-conn.h"
#include "avl-oos.h"
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
void storeOOSPacket(oos_avlNode** oosBSTRoot, struct packet currPacket) {
	struct packet* pkt = calloc(1, sizeof(struct packet));
	pkt->seqNum = currPacket.seqNum;
	pkt->timeStamp = currPacket.timeStamp;
	pkt->payloadSize = currPacket.payloadSize;
	pkt->syn = currPacket.syn;
	pkt->fin = currPacket.fin;
	pkt->ping = currPacket.ping;
	pkt->connState = currPacket.connState;
	pkt->connID = currPacket.connID;

	// If connection is not already in oOS buffer, initialize heap and add key(connID) and value(heap):
	if (oos_search(*oosBSTRoot, currPacket.connID) == NULL) {
		struct heap* heap = calloc(1, sizeof(struct heap));
		heap_init(heap);
		heap_push(heap, pkt);
		*oosBSTRoot = oos_insert(*oosBSTRoot, currPacket.connID, heap);
	// If connection already in oOS buffer
	} else {
		struct heap* h = oos_search(*oosBSTRoot, currPacket.connID)->value;
		heap_push(h, pkt);
		//printf("top of heap is seqNum %d\n", heap_front(h)->seqNum);
	}
}

/**
 * Function for updating the sequence number by checking the out-of-sequence packets buffer. 
 */	
int updateSeqNumsFromBuffer(avlNode** connBSTRoot, oos_avlNode** oosBSTRoot, struct packet currPacket) {
	int connClosed = 0;
	if (oos_search(*oosBSTRoot, currPacket.connID) == NULL || oos_search(*oosBSTRoot, currPacket.connID)->value->count == 0) {
		return connClosed;
	}

	struct heap* connOOSHeap = oos_search(*oosBSTRoot, currPacket.connID)->value;
	struct packet* nextOOSPacket = heap_front(connOOSHeap);
	unsigned long nextOOSSeqNum = nextOOSPacket->seqNum;
	unsigned long prevOOSSeqNum;
	while(nextOOSSeqNum == search(*connBSTRoot, currPacket.connID)->value->seqNum && connOOSHeap->count) { // If the buffer contains the next packet
		search(*connBSTRoot, currPacket.connID)->value->seqNum = nextOOSSeqNum + nextOOSPacket->payloadSize + nextOOSPacket->fin;
		search(*connBSTRoot, currPacket.connID)->value->timeStamp = nextOOSPacket->timeStamp;
		if (nextOOSPacket->fin || nextOOSPacket->ping || (currPacket.payloadSize == 0 && currPacket.connState == 2))
			connClosed = 1; // If the sequenced packet from the buffer is FIN, close the connection
		//printf("updateSeqNumsFromBuffer() assigned new seqNum %d at time %.3f for conn %llx!\n", 
				// nextOOSSeqNum + nextOOSPacket->payloadSize + nextOOSPacket->fin, currPacket.timeStamp,
				// search(*connBSTRoot, currPacket.connID)->key);
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
	if (connClosed || oos_search(*oosBSTRoot, currPacket.connID)->value->count == 0) {
		// If connection closed or buffer is empty, remove the OOS buffer
		*oosBSTRoot = oos_deleteNode(*oosBSTRoot, currPacket.connID);
		//puts("after oOS delete");
	}
		 
	return connClosed;
}

/**
 * Function for adding to a linked list of connections. 
 */	
void updateLinkedList(struct node** head, uint64_t connID) {
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
int updateSeqNums(avlNode** connBSTRoot, oos_avlNode** oosBSTRoot, struct packet currPacket) {
	int connClosed = 0;
	
	//printf("Handling packet no. %d at time %.5f of connection %llx\n", currPacket.seqNum, currPacket.timeStamp, currPacket.connID);
	
	// If packet is from new connection:
	if (search(*connBSTRoot, currPacket.connID) == NULL && currPacket.syn) {
		struct connStatus* newConn = calloc(1, sizeof(struct connStatus));
		*connBSTRoot = insert(*connBSTRoot, currPacket.connID, newConn);
		newConn->seqNum = 1;
		newConn->timeStamp = currPacket.timeStamp;
		connClosed = updateSeqNumsFromBuffer(connBSTRoot, oosBSTRoot, currPacket);
	// Else if packet is from open connection and matches next expected sequence number
	} else if (search(*connBSTRoot, currPacket.connID) != NULL && search(*connBSTRoot, currPacket.connID)->value->seqNum == currPacket.seqNum) {
		search(*connBSTRoot, currPacket.connID)->value->seqNum = currPacket.seqNum + currPacket.payloadSize + currPacket.fin;
		search(*connBSTRoot, currPacket.connID)->value->timeStamp = currPacket.timeStamp;
		//printf("Assigned seqnum %d, at time %.3f, of conn %llx\n", search(*connBSTRoot, currPacket.connID)->value->seqNum, search(*connBSTRoot, currPacket.connID)->value->timeStamp, search(*connBSTRoot, currPacket.connID)->key);
		connClosed = updateSeqNumsFromBuffer(connBSTRoot, oosBSTRoot, currPacket);
		if (currPacket.fin || currPacket.ping == 1 || (currPacket.payloadSize == 0 && currPacket.connState == 2))
			connClosed = 1;
	
	// Else if packet is not in connections record (and not SYN) or has seq num greater than the current expected seq num, 
	//and is not a 0-bit packet (the FIN packet and FIN ACK packet are included).
	} else if ((search(*connBSTRoot, currPacket.connID) == NULL || search(*connBSTRoot, currPacket.connID)->value->seqNum < currPacket.seqNum) 
				&& (currPacket.payloadSize != 0 || currPacket.connState == 2 || currPacket.fin)) {
		// Store packet in buffer if it has a later sequence number
		storeOOSPacket(oosBSTRoot, currPacket);
	}

	// If connection closed or buffer is empty, remove the OOS buffer
	if (connClosed || (oos_search(*oosBSTRoot, currPacket.connID) != NULL && oos_search(*oosBSTRoot, currPacket.connID)->value->count == 0)) {
		*oosBSTRoot = oos_deleteNode(*oosBSTRoot, currPacket.connID);
		//puts("after oOS delete");
	}
	// if (oos_search(*oosBSTRoot, 0x121f40012cdf01LL) != NULL) {
	// 	printf("192.168.0.18/8000 to 10.0.1.44/57089 (0x121f40012cdf01) in oos buffer! at time %.3f\n", currPacket.timeStamp);
	// }
	// if (oos_search(*oosBSTRoot, 0x121f400114c6cfLL) != NULL) {
	// 	printf("192.168.0.18/8000 to 10.0.1.20/50895 (0x121f400114c6cf) in oos buffer! at time %.3f\n", currPacket.timeStamp);
	// }
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
 * Recursive function for traversing the connections BST and outputting open connection info.
 */
void openConns(avlNode** root, struct warningNode** warningHead, int* connCt, int* openConnCt, FILE *file, double lastTimeStamp) { 
    if (*root != NULL) 
    { 
        openConns(&((*root)->left), warningHead, connCt, openConnCt, file, lastTimeStamp); 

        char ipString[40];
		IDToString(ipString, (*root)->key);
		printf("%s expecting seq num %d since %.3f\n", 
				ipString, (*root)->value->seqNum, (*root)->value->timeStamp);
		fprintf(file, "%s expecting seq num %d since %.3f\n", 
				ipString, (*root)->value->seqNum, (*root)->value->timeStamp);
		if ((*root)->value->timeStamp < lastTimeStamp - 20)
			updateWarningNodes(warningHead, (*root)->key, (*root)->value->timeStamp, 0L);
		(*connCt)++;
		(*openConnCt)++;
		
        openConns(&((*root)->right), warningHead, connCt, openConnCt, file, lastTimeStamp); 
    } 
} 

/**
 * Function for outputting the summary statistics.
 */
void summary(avlNode** connBSTRoot, struct node* closedConnHead, oos_avlNode** oosBSTRoot, int packetCt, unsigned long long byteCt, double lastTimeStamp, const char* outputFilename) {
	FILE *file;
	file = fopen(outputFilename, "w");
	if(file == NULL) {
      perror("Error opening output file");
   	}		
	int connCt = 0;
	int openConnCt = 0;
	struct warningNode* warningHead = NULL;
	struct node* synMissingList = NULL;
	int over60sWarningFlag = 0;
	char ipString[40];


	// Delete completed connections
	struct node* nodePtr = closedConnHead;
	printf("\nDeleting completed connections... ");
	while (nodePtr != NULL) {
		if (search(*connBSTRoot, nodePtr->connID) != NULL) connCt++;
		*connBSTRoot = deleteNode(*connBSTRoot, nodePtr->connID);
		nodePtr = nodePtr->next;
	}
	puts("Done.\n");
	

	puts("\nParse finished! Analysing trace statistics...\n");
	puts("======================================================================================");
	printf("* OUTPUT FROM PACKET LOSS ANALYSIS to %s\n", outputFilename);
	puts("======================================================================================");
	fputs("======================================================================================\n", file);
	fprintf(file, "* OUTPUT FROM PACKET LOSS ANALYSIS of %s\n", outputFilename);
	fputs("======================================================================================\n", file);
	

	// Collate missing packets and print to terminal
	struct heap* h = calloc(1, sizeof(struct heap));
	unsigned long lastSeqNum = 0;
	unsigned long prevSeqNum;
	unsigned long totalMissingBytes = 0;
	struct packet* nextOOSPacket = calloc(1, sizeof(struct packet));
	oos_avlNode* oosBSTPtr = *oosBSTRoot;
	
	while (oosBSTPtr != NULL) {
		h = oosBSTPtr->value;

		// If oos packet in buffer is a phantom byte during connection close, ignore and skip.
		if (heap_front(h)->seqNum - search(*connBSTRoot, oosBSTPtr->key)->value->seqNum == 1
			&& heap_front(h)->connState == 2) {
			heap_pop(h);
			break;
		}

		IDToString(ipString, oosBSTPtr->key);
		printf("\n\nBytes missing from %s: \n", ipString);
		fprintf(file, "\n\nBytes missing from %s: \n", ipString);
		
		// fprintf(file, "heap count: %d;", h->count);
		// fprintf(file, "heap size: %d; connID: %s\n", h->size, ipString);
		nextOOSPacket = heap_front(h);
		// printf("Packet seqnum: %d; Timestamp: %.3f\n", nextOOSPacket->seqNum, nextOOSPacket->timeStamp);

		// Check expected seqNum
		if (search(*connBSTRoot, oosBSTPtr->key) != NULL) {
			lastSeqNum = search(*connBSTRoot, oosBSTPtr->key)->value->seqNum;
		}
		while (h->count != 0) {
			// printf("Last sequence number: %d \n", lastSeqNum);
			nextOOSPacket = heap_front(h);
			if (lastSeqNum == 0) {
				// If not in connBST, SYN is missing
				totalMissingBytes += nextOOSPacket->seqNum - 1;
				printf("%d missing bytes between start of connection and seq num %d (incl. SYN phantom byte) at time %.3f\n",
						nextOOSPacket->seqNum, nextOOSPacket->seqNum, nextOOSPacket->timeStamp);
				fprintf(file, "%d missing bytes between start of connection and seq num %d (incl. SYN phantom byte) at time %.3f\n",
						nextOOSPacket->seqNum, nextOOSPacket->seqNum, nextOOSPacket->timeStamp);
				if (nextOOSPacket->timeStamp < lastTimeStamp - 20)
					updateWarningNodes(&warningHead, oosBSTPtr->key, nextOOSPacket->timeStamp, nextOOSPacket->seqNum);	
				// Increment connection counters because not in connBST but connection is incomplete
				openConnCt++;
				connCt++;
				updateLinkedList(&synMissingList, oosBSTPtr->key);		
			} else if (lastSeqNum < nextOOSPacket->seqNum) { 
				totalMissingBytes += nextOOSPacket->seqNum - lastSeqNum;
				printf("%d missing bytes between seq num %d and %d at time %.3f\n",
						nextOOSPacket->seqNum - lastSeqNum, lastSeqNum, nextOOSPacket->seqNum, nextOOSPacket->timeStamp);
				fprintf(file, "%d missing bytes between seq num %d and %d at time %.3f\n",
						nextOOSPacket->seqNum - lastSeqNum, lastSeqNum, nextOOSPacket->seqNum, nextOOSPacket->timeStamp);
				if (nextOOSPacket->timeStamp < lastTimeStamp - 20)
					updateWarningNodes(&warningHead, oosBSTPtr->key, nextOOSPacket->timeStamp, nextOOSPacket->seqNum - lastSeqNum);
				//fprintf(file, "lastseq before %d; nextOOSPacket->seqNum: %d; nextOOSPacket->payloadSize: %d\n", lastSeqNum, nextOOSPacket->seqNum, nextOOSPacket->payloadSize);
			}
			if (lastSeqNum < nextOOSPacket->seqNum + nextOOSPacket->payloadSize + nextOOSPacket->fin)
				lastSeqNum = nextOOSPacket->seqNum + nextOOSPacket->payloadSize + nextOOSPacket->fin;
			//fprintf(file, "lastseq after assignemnt: %d; heap count: %d; connID: %llx; packet time: %.3f\n", lastSeqNum, h->count, oosBSTPtr->key, nextOOSPacket->timeStamp);
			//puts("finished while loop (before pop)");
			heap_pop(h);
		}
		oosBSTPtr = oos_deleteNode(oosBSTPtr, oosBSTPtr->key);
	}

	// Print open connections
	puts("\n\nConnections open or with missing packets:");
	fputs("\n\nConnections open or with missing packets:\n", file);
	openConns(connBSTRoot, &warningHead, &connCt, &openConnCt, file, lastTimeStamp);
	// Print connections where SYN is missing
	while (synMissingList != NULL) {
		IDToString(ipString, synMissingList->connID);
		printf("%s with packets received but missing SYN packet\n", ipString);
		fprintf(file, "%s with packets received but missing SYN packet\n", ipString);
		synMissingList = synMissingList->next;
	}

	if (openConnCt == 0) {
		puts("None.");
		fputs("None.\n", file);
	}

	// Print summary statistics
	puts("\n\nSummary:");
	printf("%d packets checked containing a total of %lld bytes from %d connections.\n\n", packetCt, byteCt, connCt);
	printf("%d / %lld bytes missing from trace sequence (%.3f%% loss).\n\n", totalMissingBytes, byteCt + totalMissingBytes, totalMissingBytes / ((double) byteCt + totalMissingBytes) * 100);
	
	fputs("======================================================================================\n", file);
	fputs("\n\nSummary:\n", file);
	fprintf(file, "%d packets checked containing a total of %lld bytes from %d connections.\n\n", packetCt, byteCt, connCt);
	fprintf(file, "%d / %lld bytes missing from trace sequence (%.3f%% loss).\n\n", totalMissingBytes, byteCt + totalMissingBytes, totalMissingBytes / ((double) byteCt + totalMissingBytes) * 100);
	
	if (openConnCt) {
		printf("Subsequent packets from %d open connection(s) could not be analysed.\n\n", openConnCt);
		fprintf(file, "Subsequent packets from %d open connection(s) could not be analysed.\n\n", openConnCt);
	}

	// Print warning for missing bytes (i) 60s before trace end and (ii) 20s before trace end
	puts("\n======================================================================================");
	fputs("\n======================================================================================\n", file);
	if (warningHead != NULL) {
		puts("* Warning! Packet(s) missing or connection(s) open since before last 20 s of trace!\n*");
		fputs("* Warning! Packet(s) missing or connection(s) open since before last 20 s of trace!\n*\n", file);
		while (warningHead != NULL) {
			IDToString(ipString, warningHead->connID);
			if (warningHead->bytesMissing == 0) {
				printf("* %s open since %.3f\n", ipString, warningHead->timeStamp);
				fprintf(file, "* %s open since %.3f\n", ipString, warningHead->timeStamp);
			} else {
				printf("* %d bytes missing from %s since %.3f\n", warningHead->bytesMissing, ipString, warningHead->timeStamp);
				fprintf(file, "* %d bytes missing from %s since %.3f\n", warningHead->bytesMissing, ipString, warningHead->timeStamp);
			}
			
			if (warningHead->timeStamp < lastTimeStamp - 60) over60sWarningFlag = 1;
			warningHead = warningHead->next;
		}
		if (over60sWarningFlag) {
			puts("\n*\n* Warning! Packet(s) missing or connection(s) open since before last 60 s of trace!\n*");
			fputs("*\n* Warning! Packet(s) missing or connection(s) open since before last 60 s of trace!\n*\n", file);
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
	FILE *file;
	file = fopen(filename, "r");
	if(file == NULL) {
      perror("Error opening file");
   	}	
	char buff[16] = {0};
	int batchCt = 0;
	int batchSize = 50000;
	int packetCt = 0;
	unsigned long long byteCt = 0;
	int lineCt = 0;
	int charCt = 0;
	int dataComplete = 1;
	struct packet currPacket;
	struct connection currConnection;
	int connClosed = 0;
	double lastTimeStamp;
	char *outputSuffix = "-PacketLoss.txt";
	char outputFile[50] = {0};

//Initialize IP address buffers and counters for parsing trace file
	char ip[5] = {0};
	unsigned long ipLong = 0L;
	int ctr = 0;
	int i = 0;
	int j = 0;	
	
//Initialize data structures for containing connections and out-of-sequence packet buffer
	avlNode* connBSTRoot = NULL;
	oos_avlNode* oosBSTRoot = NULL;
	struct node* closedConnHead = NULL;
	puts("Initialised parse!");
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
					byteCt += currPacket.syn;
					break;
				
				case 11 :
					currPacket.fin = atoi(buff);
					byteCt += currPacket.fin;
					break;

				case 12 :
					currPacket.ping = atoi(buff);

				case 13 :
					currPacket.seqNum = atoi(buff);
					break;

				case 14 :
					currPacket.connState = atoi(buff);
			}
			if (c == '\n') {
				if (dataComplete) {
					connClosed = updateSeqNums(&connBSTRoot, &oosBSTRoot, currPacket);
				}
				if (connClosed) {
					updateLinkedList(&closedConnHead, currPacket.connID);
					connClosed = 0;
				}
				lineCt = 0;
				dataComplete = 1;
				batchCt++;
				packetCt++;
				if (packetCt % 20000 == 0) printf(".");
				if (packetCt % 1000000 == 0) printf("\n%d packets parsed.\n", packetCt);
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
	fclose(file);
	// Create the output file name XXX-PacketLoss.txt
	strcat(outputFile, filename);
	int len = strlen(outputFile);
	for (int i = len - 1; i > len - 5; i--) outputFile[i] = 0; //delete the ".txt" suffix
	strcat(outputFile, outputSuffix);
	summary(&connBSTRoot, closedConnHead, &oosBSTRoot, packetCt, byteCt, lastTimeStamp, outputFile);
}


int main(int argc, char *argv[]) {
    char defaultFile[] = "trace-small.txt";
    
	const char* filename = (argc > 1) ? argv[1] : defaultFile;
	printf("Starting analysis of %s!\n", filename);
	
	parse(filename);
	puts("Finished and exiting!");
	return(0);	
}