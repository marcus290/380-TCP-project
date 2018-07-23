import java.util.ArrayList;
import java.util.Set;
import java.util.HashMap;
import java.util.Collections;
import java.util.List;
import java.util.Arrays;
//import java.util.Comparator;
//import java.util.regex.*;
import java.util.Scanner;

import java.io.File;
import java.io.IOException;

public class PacketLoss {
	
	public static void main(String args[]) {
		PacketLoss pl = new PacketLoss();
		pl.parse();
	}
	
	/**
	 * Function for analysing the trace file data and identifying packet loss. Instantiates a Tracefile object containing
	 * the data and checks the sequence of packets by their TCP sequence number. If packets are out-of-sequence ("oOS"),
	 * stores them in a hashmap for further sequencing.
	 * 
	 * Prints summary information. Missing packets are identified using the oOS hashmap after processing, whereby those 
	 * remaining packets have not been sequenced successfully, indicating missing packets in the sequence.
	 */	
	public void parse() {
		File f = new File("trace-small.txt");
		TraceData traceInput = new TraceData(f);
		HashMap<List<String>, Integer> connectionsDict = new HashMap<>();
		HashMap<List<String>, HashMap<Integer, Integer[]>> outOfSeq = new HashMap<>(); //list of packets that are out of sequence, value is an ArrayList of trace line indexes
		int counter = 0;
		int packetCounter = 0;
		int byteCounter = 0;
		for (int i = 0; i < traceInput.traceLines.size(); i++) {
			String[] line = traceInput.traceLines.get(i);
			if (line[2].isEmpty() || line[3].isEmpty() || line[4].isEmpty() || line[5].isEmpty()) //if any IP or ports are empty, skip
				continue;
			List<String> connectionTuple = Collections.unmodifiableList(Arrays.asList(line[2], line[3], line[4], line[5]));
			int seqNum = Integer.parseInt(line[13]);
			packetCounter += 1;
			byteCounter += Integer.parseInt(line[8]) + Integer.parseInt(line[11]);
			//counter += 1;
			//if (counter % 10000 == 0) 
			//	System.out.println(counter + " lines parsed.");
			//if (!outOfSeq.isEmpty()) {
			//	System.out.println("OutOfSeq packet(s) from:");
			//	for (List<String> key: outOfSeq.keySet())
			//		System.out.println(key + " " + outOfSeq.get(key));
			//}
			
			if (connectionsDict.containsKey(connectionTuple) && seqNum == connectionsDict.get(connectionTuple)) {
				updateSeqNum(connectionsDict.get(connectionTuple), seqNum + Integer.parseInt(line[8]) + Integer.parseInt(line[11]), 
							connectionTuple, connectionsDict, outOfSeq, Integer.parseInt(line[14]));
				
			} else if (seqNum == 0) {//Handle new connections
				connectionsDict.put(connectionTuple, 1);
			} else { //Handle out-of-sequence packets
				if (!outOfSeq.containsKey(connectionTuple)) 
					outOfSeq.put(connectionTuple, new HashMap<>()); //add connection and new HashMap to outOfSeq HashMap
				outOfSeq.get(connectionTuple).put(seqNum, new Integer[]{seqNum + Integer.parseInt(line[8]) + Integer.parseInt(line[11]), Integer.parseInt(line[14])});
				// add TCP sequence number as key, add next TCP sequence number & connection close flag as value array
			}
			
		}
		System.out.println("\nConnections still open:");
		if (connectionsDict.isEmpty()) {
			System.out.println("None");
		} else {
			for (List<String> key: connectionsDict.keySet())
				System.out.println(key + " expecting packet " + connectionsDict.get(key));
		}
		System.out.println("\nRemaining orphaned out-of-sequence packets:");
		if (outOfSeq.isEmpty()) {
			System.out.println("None");
		} else {
			for (List<String> key: outOfSeq.keySet()) {
				for (Integer packetNum: outOfSeq.get(key).keySet()) {
				System.out.println(key + " with packet no. " + packetNum);
				}
			}
		}
		
		System.out.println("\nSummary:");
		System.out.println(packetCounter + " packets checked containing a total of " + byteCounter + " bytes.");
		int missingBytes = 0;
		if (!outOfSeq.isEmpty()) {
			
			for (List<String> key: outOfSeq.keySet()) { //for each connection with out-of-sequence packets
				Set<Integer> keySetCopy = outOfSeq.get(key).keySet();
				int nextSeqNum = Collections.min(keySetCopy);
				int prevSeqNum;
				//calculate missing bytes between sequenced packets and first oOS packet
				missingBytes += nextSeqNum - connectionsDict.get(key);
				//calculate missing bytes within oOS packets
				while (!keySetCopy.isEmpty()) {
					while (outOfSeq.get(key).containsKey(nextSeqNum)) {
						prevSeqNum = nextSeqNum;
						nextSeqNum = outOfSeq.get(key).get(prevSeqNum)[0];
						keySetCopy.remove(prevSeqNum);
					}
					prevSeqNum = nextSeqNum;
					keySetCopy.remove(prevSeqNum);
					if (!keySetCopy.isEmpty()) {
						nextSeqNum = Collections.min(keySetCopy);
						missingBytes += nextSeqNum - prevSeqNum;
					}
				}
				
			}
		}
		System.out.printf("%d / %d bytes missing from trace sequence (%.2f%% loss).%n%n", missingBytes, byteCounter, missingBytes / (double) byteCounter);
		System.out.println("Subsequent missing packets from " + connectionsDict.size() + " connections could not be determined.");
		
 
	}
	
	/**
	 * Function for updating the next expected TCP sequence number when the current expected packet is found.
	 * Subsequently checks for any packets in the outOfSeq record for any oOS packets that can now be sequenced.
	 * Removes the connection from connectionsDict, if the connection is closed (with all packets in sequence).
	 * @param seqNum current sequence number
	 * @param nextSeqNum next sequence number to replace the current sequence number in the chain
	 * @param connectionTuple identifier for the connection
	 * @param connectionsDict hashmap of all open connections
	 * @param outOfSeq hashmap of all packets which currently do not fall in sequence
	 * @param closedflag flag for whether the packet is closing the connection
	 */	
	public void updateSeqNum(int seqNum, int nextSeqNum, List<String> connectionTuple, HashMap<List<String>, Integer> connectionsDict, 
								HashMap<List<String>, HashMap<Integer, Integer[]>> outOfSeq, int closedFlag) {
		//System.out.println("For " + connectionTuple + " " + connectionsDict.get(connectionTuple) + " is replaced by " + nextSeqNum);
		connectionsDict.replace(connectionTuple, nextSeqNum); //update value to next expected TCP sequence num
		
		//after updating, check if any outOfSeq packets can be reordered
		if (outOfSeq.containsKey(connectionTuple) && outOfSeq.get(connectionTuple).containsKey(nextSeqNum)) {
			Integer[] outOfSeqInfo = outOfSeq.get(connectionTuple).remove(nextSeqNum); //remove the reordered packet info from outOfSeq
			if (outOfSeq.get(connectionTuple).isEmpty())
				outOfSeq.remove(connectionTuple); //remove the connection from outOfSeq if no more outOfSeq packets
			//System.out.println(nextSeqNum + " packet from " + connectionTuple + " which was out of sequence, removed from outOfSeq HashMap");
			updateSeqNum(nextSeqNum, outOfSeqInfo[0], connectionTuple, connectionsDict, outOfSeq, outOfSeqInfo[1]);

		}
		if (closedFlag == 2) {
			//System.out.println(connectionTuple + " closed");
			connectionsDict.remove(connectionTuple); //remove connection if closed from in-sequence packet
		}
	}
	
/**
 * TraceData inner class of TraceViewer to handle the instantiating and processing of data from the trace file.
 */
	private class TraceData {
		private ArrayList<String[]> traceLines = new ArrayList<String[]>();

	/**
	 * Constructor for TraceData object. Takes the input file of trace data and uses a scanner object to 
	 * split the input data into String arrays for each record and adds it to an ArrayList
	 * @param f input file of the trace data.
	 */
		private TraceData(File f) {
			Scanner input = null;
			try {
				input = new Scanner(f);
			}
			catch (IOException ioExc) {
				return; // check if file isn't readable;
			}
			while (input.hasNext()) {
				String currentLine = input.nextLine();
				traceLines.add(currentLine.split("\t"));
			}
			input.close();
		}
	}

}
