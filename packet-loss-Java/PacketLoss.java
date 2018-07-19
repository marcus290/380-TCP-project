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
		PacketLoss plp = new PacketLoss();
		plp.start();
	}
	
	public void start() {
		File f = new File("trace-small2.txt");
		TraceData traceInput = new TraceData(f);
		HashMap<List<String>, Integer> connectionsDict = new HashMap<>();
		HashMap<List<String>, HashMap<Integer, Integer>> outOfSeq = new HashMap<>(); //list of packets that are out of sequence, value is an ArrayList of trace line indexes
		int counter = 0;
		for (int i = 0; i < traceInput.traceLines.size(); i++) {
			String[] line = traceInput.traceLines.get(i);
			if (line[2].isEmpty() || line[3].isEmpty() || line[4].isEmpty() || line[5].isEmpty()) //if any IP or ports are empty, skip
				continue;
			List<String> connectionTuple = Collections.unmodifiableList(Arrays.asList(line[2], line[3], line[4], line[5]));
			int packetSeqNum = Integer.parseInt(line[13]);
			//counter += 1;
			//if (counter % 10000 == 0) 
			//	System.out.println(counter + " lines parsed.");
			for (List<String> key: outOfSeq.keySet())
				System.out.println(key); 
			
			if (connectionsDict.containsKey(connectionTuple) && packetSeqNum == connectionsDict.get(connectionTuple)) {
				updateSeqNum(connectionsDict.get(connectionTuple), line, connectionTuple, connectionsDict, outOfSeq);
			} else if (packetSeqNum == 0) {//Handle new connections
				connectionsDict.put(connectionTuple, 1);
			} else { //Handle out-of-sequence packets
				if (!outOfSeq.containsKey(connectionTuple)) 
					outOfSeq.put(connectionTuple, new HashMap<>()); //add connection and new HashMap to outOfSeq HashMap
				outOfSeq.get(connectionTuple).put(packetSeqNum, packetSeqNum + Integer.parseInt(line[8]));// add TCP sequence number and index of trace line to HashMap 
			}
			
		}
 
	}
	
	public void updateSeqNum(int seqNum, String[] line, List<String> connectionTuple, HashMap<List<String>, Integer> connectionsDict, 
								HashMap<List<String>, HashMap<Integer, Integer>> outOfSeq) {
		int nextSeqNum = seqNum + Integer.parseInt(line[8]);
		System.out.println("For " + connectionTuple + " " + connectionsDict.get(connectionTuple) + " is replaced by " + nextSeqNum);
		connectionsDict.replace(connectionTuple, nextSeqNum); //update value to next expected TCP sequence num
		if (outOfSeq.containsKey(connectionTuple)) {
			if (outOfSeq.get(connectionTuple).containsKey(nextSeqNum)) {
				updateSeqNum(nextSeqNum, line, connectionTuple, connectionsDict, outOfSeq);
				outOfSeq.remove(nextSeqNum);
				System.out.println(nextSeqNum + " packet which was out of sequence, removed from outOfSeq HashMap");
			}
		}
	}
	
/**
 * TraceData inner class of TraceViewer to handle the instantiating and processing of data from the trace file.
 */
	private class TraceData {
		private ArrayList<String[]> traceLines = new ArrayList<String[]>();

	/**
	 * Constructor for TraceData object. Takes the input file of trace data and processes it.
	 * @param f input file of the trace data.
	 */
		private TraceData(File f) {
			parseFile(f);
		}
	
	/**
	 * Takes the input trace data file and uses a scanner object to split the input data into
	 * String arrays for each record and adds it to the ArrayList of string arrays.
	 * @param file input file of the trace data passed by the constructor.
	 */
		private void parseFile(File file) {
			int counter = 0;
			Scanner input = null;
			try {
				input = new Scanner(file);
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
