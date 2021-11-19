// -*- c++ -*-
//
// commDependence
// Yuyang Jin
//
// This file is to generate code to collect runtime communication data.
//
// To build:
//    ./wrap.py -f commDependence.w > commDependence.cpp
//    mpicc -c commDependence.cpp
//    #ar cr libcommDependence.a commDependence.o
//    #ranlib libcommDependence.a
//
// Link your application with libcommData.a, or build it as a shared lib
// and LD_PRELOAD it to try out this tool.
//
// v1.1 :
// Use index to record all communication traces
//

#include <stdio.h>
#include <string.h>
#include <string>
#include <iostream>
#include <fstream>
#include <libunwind.h>
#include <chrono>
#include <map>
#include <mpi.h>

using namespace std;


#define MODULE_INITED 1
#define LOG_SIZE 10000
#define MAX_STACK_DEPTH 100
#define MAX_CALL_PATH_LEN 1300 // MAX_STACK_DEPTH * 13
#define MAX_NAME_LEN 20
#define MAX_ARGS_LEN 20
#define MAX_COMM_WORLD_SIZE 100000
#define MAX_WAIT_REQ 100
#define MAX_TRACE_SIZE 25000000
#define TRACE_LOG_LINE_SIZE 100

#ifdef MPICH2
	#define SHIFT(COMM_ID) (((COMM_ID&0xf0000000)>>24) + (COMM_ID&0x0000000f))
#else
	#define SHIFT(COMM_ID) -1
#endif

typedef struct CollInfoStruct{
	char call_path[MAX_CALL_PATH_LEN] = {0};
	MPI_Comm comm;
	unsigned long long int count = 0;
	double exe_time = 0.0;
}CIS;

typedef struct P2PInfoStruct{
	char type = 0;  // 'r' 's' :blocking recv/send  | 'R' 'S': non-blocking recv/send
	char call_path[MAX_CALL_PATH_LEN] = {0};
	int request_count = 0;
	int source[MAX_WAIT_REQ] = {0};
	int dest[MAX_WAIT_REQ] = {0};
	int tag[MAX_WAIT_REQ] = {0};
	unsigned long long int count = 0;
	double exe_time = 0.0;
}PIS;

struct RequestConverter
{
    char data[sizeof(MPI_Request)];
    RequestConverter(MPI_Request * mpi_request)
    {
        memcpy(data, mpi_request, sizeof(MPI_Request));
    }
    RequestConverter()
    { }
    RequestConverter(const RequestConverter & req)
    {
		memcpy(data, req.data, sizeof(MPI_Request));
	}
	RequestConverter & operator=(const RequestConverter & req)
	{
		memcpy(data, req.data, sizeof(MPI_Request));
		return *this;
	}
	bool operator<(const RequestConverter & request) const
	{
		for(size_t i=0; i<sizeof(MPI_Request); i++)
		{
			if(data[i]!=request.data[i])
			{
				return data[i]<request.data[i];
			}
		}
		return false;
	}
};


static CIS coll_mpi_info_log[LOG_SIZE];
static PIS p2p_mpi_info_log[LOG_SIZE];
static unsigned long long int coll_mpi_info_log_pointer = 0;
static unsigned long long int p2p_mpi_info_log_pointer = 0;
static unsigned int trace_log[MAX_TRACE_SIZE] = {0};
static unsigned long long int trace_log_pointer = 0;

map <RequestConverter, pair<int,int>> request_converter;

int mpiRank = -1;
static int module_init = 0;
static char* addr_threshold;
bool mpi_finalize_flag = false;

//int mpiRank = 0;

// Dump mpi info log
static void writeCollMpiInfoLog(){
	ofstream outputStream((string("MPID") + to_string(mpiRank) + string(".TXT")), ios_base::app);
	if (!outputStream.good()) {
		cout << "Failed to open sample file\n";
		return;
	}

	int comm_size = 0;
	MPI_Group group1, group2;
	MPI_Comm_group(MPI_COMM_WORLD, &group1);
	MPI_Comm_size(MPI_COMM_WORLD, &comm_size);
	int ranks1[MAX_COMM_WORLD_SIZE] = {0};
	int ranks2[MAX_COMM_WORLD_SIZE] = {0};
	for (int i = 0; i < comm_size; i++) {
		ranks1[i] = i;
	}


	// TODO: output different comm
	for (int i = 0; i < coll_mpi_info_log_pointer; i++){
		MPI_Comm_group(coll_mpi_info_log[i].comm, &group2);
		MPI_Group_translate_ranks(group1, comm_size, ranks1, group2, ranks2);
		outputStream << "c ";
		outputStream << coll_mpi_info_log[i].call_path << " | " ;
		for (int j = 0; j < comm_size; j++){
			outputStream << ranks2[j] << " ";
		}
		outputStream << " | " << coll_mpi_info_log[i].count ;
		outputStream << " | " << coll_mpi_info_log[i].exe_time << '\n';
	}

	coll_mpi_info_log_pointer = 0;

	outputStream.close();
}

static void writeP2PMpiInfoLog(){
	ofstream outputStream((string("MPID") + to_string(mpiRank) + string(".TXT")), ios_base::app);
	if (!outputStream.good()) {
		cout << "Failed to open sample file\n";
		return;
	}

	// TODO: output different comm
	for (int i = 0; i < p2p_mpi_info_log_pointer; i++){
		outputStream << p2p_mpi_info_log[i].type << " " ;
		outputStream << p2p_mpi_info_log[i].call_path << " | " ;
		int request_count = p2p_mpi_info_log[i].request_count;
		for (int j = 0; j < request_count; j++){
			outputStream << p2p_mpi_info_log[i].source[j] << " " << p2p_mpi_info_log[i].dest[j] << " " << p2p_mpi_info_log[i].tag[j] << " , ";
		}
		outputStream << " | " << p2p_mpi_info_log[i].count ;
		outputStream << " | " << p2p_mpi_info_log[i].exe_time << '\n';
	}

	p2p_mpi_info_log_pointer = 0;

	outputStream.close();
}

static void writeTraceLog(){
	ofstream outputStream((string("MPIT") + to_string(mpiRank) + string(".TXT")), ios_base::app);
	if (!outputStream.good()) {
		cout << "Failed to open sample file\n";
		return;
	}

	for (int i = 0; i < trace_log_pointer; i+= TRACE_LOG_LINE_SIZE){
		for (int j = i; j < i + TRACE_LOG_LINE_SIZE && j < trace_log_pointer; j++ ){
			outputStream << trace_log[j] << " " ;
		}
		outputStream << '\n';
	}

	trace_log_pointer = 0;

	outputStream.close();
}

static void init() __attribute__((constructor));
static void init() {
	if(module_init == MODULE_INITED) return ;
	module_init = MODULE_INITED;
	addr_threshold =(char*)malloc( sizeof(char));
}


// Dump mpi info at the end of program's execution
static void fini() __attribute__((destructor));
static void fini(){
	if (!mpi_finalize_flag){
		//writeCollMpiInfoLog();
		//writeP2PMpiInfoLog();
		//printf("mpi_finalize_flag is false\n");
	}
}


// Record mpi info to log

void TRACE_COLL(MPI_Comm comm, double exe_time){
  void *buffer[MAX_STACK_DEPTH] = {0};
  unsigned int i, depth = 0;
	//memset(buffer, 0, sizeof(buffer));
	depth = unw_backtrace(buffer, MAX_STACK_DEPTH);
  char call_path[MAX_CALL_PATH_LEN] = {0};
	int offset = 0;

  for (i = 0; i < depth; ++ i)
	{
		if( (void*)buffer[i] != NULL && (char*)buffer[i] < addr_threshold ){ 
			offset+=snprintf(call_path+offset, MAX_CALL_PATH_LEN - offset - 4 ,"%lx ",(void*)(buffer[i]-2));
		}
	}

	bool hasRecordFlag = false;

	for (i = 0; i < coll_mpi_info_log_pointer; i++){
		if (strcmp(call_path, coll_mpi_info_log[i].call_path) == 0 ){
			int cmp_result;
			MPI_Comm_compare(comm, coll_mpi_info_log[i].comm, &cmp_result);
			if (cmp_result == MPI_CONGRUENT) {
				coll_mpi_info_log[i].count ++;
				coll_mpi_info_log[i].exe_time += exe_time;
				trace_log[trace_log_pointer ++ ] = i * 2;
				hasRecordFlag = true;
				break;
			}
		}
	}

	if(hasRecordFlag == false){
		if(strlen(call_path) >= MAX_CALL_PATH_LEN){
			//LOG_WARN("Call path string length (%d) is longer than %d", strlen(call_path),MAX_CALL_PATH_LEN);
		}else{
			strcpy(coll_mpi_info_log[coll_mpi_info_log_pointer].call_path, call_path);
  		MPI_Comm_dup(comm, &coll_mpi_info_log[coll_mpi_info_log_pointer].comm);
			coll_mpi_info_log[coll_mpi_info_log_pointer].count = 1;
			coll_mpi_info_log[coll_mpi_info_log_pointer].exe_time = exe_time;
			trace_log[trace_log_pointer ++ ] = coll_mpi_info_log_pointer * 2;
			coll_mpi_info_log_pointer++;
		}
	}

  if(coll_mpi_info_log_pointer >= LOG_SIZE-5){
		writeCollMpiInfoLog();
	}
	if(trace_log_pointer >= MAX_TRACE_SIZE - 5){
		writeTraceLog();
	}
}

void TRACE_P2P(char type, int request_count, int *source, int *dest, int *tag, double exe_time){
  void *buffer[MAX_STACK_DEPTH] = {0};
  unsigned int i, j, depth = 0;
	depth = unw_backtrace(buffer, MAX_STACK_DEPTH);
  char call_path[MAX_CALL_PATH_LEN] = {0};
	int offset = 0;

  for (i = 0; i < depth; ++ i)
	{
		if( (void*)buffer[i] != NULL && (char*)buffer[i] < addr_threshold ){ 
			offset+=snprintf(call_path+offset, MAX_CALL_PATH_LEN - offset - 4 ,"%lx ",(void*)(buffer[i]-2));
		}
	}

	bool hasRecordFlag = false;

	for (i = 0; i < p2p_mpi_info_log_pointer; i++){
		if (p2p_mpi_info_log[i].type == type && strcmp(call_path, p2p_mpi_info_log[i].call_path) == 0 ){
			if (p2p_mpi_info_log[i].request_count == request_count){
				for (j = 0; j < request_count; j++){
					if (p2p_mpi_info_log[i].source[j] != source[j] || p2p_mpi_info_log[i].dest[j] != dest[j] || p2p_mpi_info_log[i].tag[j] != tag[j]) {
						goto k1;
					}
				}
				p2p_mpi_info_log[i].count ++;
				p2p_mpi_info_log[i].exe_time += exe_time;
				hasRecordFlag = true;
				trace_log[trace_log_pointer ++ ] = i * 2 + 1;
				break;
k1:
				continue;
			}
		}
	}

	if(hasRecordFlag == false){
		if(strlen(call_path) >= MAX_CALL_PATH_LEN){
			//LOG_WARN("Call path string length (%d) is longer than %d", strlen(call_path),MAX_CALL_PATH_LEN);
		}else{
			p2p_mpi_info_log[p2p_mpi_info_log_pointer].type = type;
			strcpy(p2p_mpi_info_log[p2p_mpi_info_log_pointer].call_path, call_path);
			p2p_mpi_info_log[p2p_mpi_info_log_pointer].request_count = request_count;
			for (i = 0; i < request_count; i++){
				p2p_mpi_info_log[p2p_mpi_info_log_pointer].source[i] = source[i];
				p2p_mpi_info_log[p2p_mpi_info_log_pointer].dest[i] = dest[i];
				p2p_mpi_info_log[p2p_mpi_info_log_pointer].tag[i] = tag[i];
			}
			p2p_mpi_info_log[p2p_mpi_info_log_pointer].count = 1;
			p2p_mpi_info_log[p2p_mpi_info_log_pointer].exe_time = exe_time;
			trace_log[trace_log_pointer ++ ] = p2p_mpi_info_log_pointer * 2 + 1;
			p2p_mpi_info_log_pointer++;
		}
	}

  if(p2p_mpi_info_log_pointer >= LOG_SIZE-5){
		writeP2PMpiInfoLog();
	}
	if(trace_log_pointer >= MAX_TRACE_SIZE - 5){
		writeTraceLog();
	}
}


// MPI_Init does all the communicator setup
//
{{fn func MPI_Init}}{
    // First call PMPI_Init()
    {{callfn}}
    PMPI_Comm_rank(MPI_COMM_WORLD, &mpiRank);
}{{endfn}}

// P2P communication
{{fn func MPI_Send}}{
    // First call P2P communication
		auto st = chrono::system_clock::now();
    {{callfn}}
		auto ed = chrono::system_clock::now();
		double time = chrono::duration_cast<chrono::microseconds>(ed - st).count();

		int source_list[1];
		int dest_list[1];
		int tag_list[1];

		source_list[0] = mpiRank;
		dest_list[0] = dest;
		tag_list[0] = tag;
#ifdef DEBUG
		printf("%s\n", "{{func}}");
#endif
		TRACE_P2P('s', 1, source_list, dest_list, tag_list, time);
}{{endfn}}

{{fn func MPI_Isend}}{
    // First call P2P communication
		auto st = chrono::system_clock::now();
    {{callfn}}
		auto ed = chrono::system_clock::now();
		double time = chrono::duration_cast<chrono::microseconds>(ed - st).count();

		int source_list[1] = {-1};
		int dest_list[1] = {-1};
		int tag_list[1] = {-1};

		source_list[0] = mpiRank;
		dest_list[0] = dest;
		tag_list[0] = tag;
		//if (mpiRank == 0){
		//printf("isend record %x %d %d\n",request, mpiRank , tag);}
		request_converter[RequestConverter(request)] = pair<int, int>(mpiRank, tag);
#ifdef DEBUG
		printf("%s\n", "{{func}}");
#endif
		TRACE_P2P('S', 1, source_list, dest_list, tag_list, time);

}{{endfn}}

{{fn func MPI_Recv}}{
    // First call P2P communication
		auto st = chrono::system_clock::now();
    {{callfn}}
		auto ed = chrono::system_clock::now();
		double time = chrono::duration_cast<chrono::microseconds>(ed - st).count();

		int source_list[1];
		int dest_list[1];
		int tag_list[1];

		source_list[0] = source;
		dest_list[0] = mpiRank;
		tag_list[0] = tag;
#ifdef DEBUG
		printf("%s\n", "{{func}}");
#endif
		TRACE_P2P('r', 1, source_list, dest_list, tag_list, time);
}{{endfn}}

{{fn func MPI_Irecv}}{
    // First call P2P communication
		auto st = chrono::system_clock::now();
    {{callfn}}
		auto ed = chrono::system_clock::now();
		double time = chrono::duration_cast<chrono::microseconds>(ed - st).count();

		int source_list[1] = {-1};
		int dest_list[1] = {-1};
		int tag_list[1] = {-1};

		source_list[0] = source;
		dest_list[0] = mpiRank;
		tag_list[0] = tag;
#ifdef DEBUG		
		if (mpiRank == 0){
		printf("irecv record %x %d %d\n", request, source , tag);
		}
#endif
		request_converter[RequestConverter(request)] = pair<int, int>(source, tag);
#ifdef DEBUG
		printf("%s\n", "{{func}}");
#endif
		TRACE_P2P('R', 1, source_list, dest_list, tag_list, time);
}{{endfn}}

{{fn func MPI_Wait}}{
		int source_list[1] = {-1};
		int dest_list[1] = {-1};
		int tag_list[1] = {-1};

		dest_list[0] = mpiRank;
		bool valid_flag = false;

		map<RequestConverter, pair<int, int>> :: iterator iter;
		iter = request_converter.find(RequestConverter(request));
		if (iter != request_converter.end()){
			pair<int, int> p = request_converter[RequestConverter(request)];
			//if (p.first >= 0 && p.second >= 0){
			source_list[0] = p.first;
			tag_list[0] = p.second;
			//printf("%d %d\n", p.first, p.second);
			valid_flag = true;
			//}
			request_converter.erase(RequestConverter(request));
		}
		
		auto st = chrono::system_clock::now();
		int ret_val = {{callfn}}
		auto ed = chrono::system_clock::now();
		double time = chrono::duration_cast<chrono::microseconds>(ed - st).count();

		if (valid_flag == false){
			source_list[0] = status->MPI_SOURCE;
			tag_list[0] = status->MPI_TAG;
#ifdef DEBUG
			//printf("%d %d\n", status->MPI_SOURCE, status->MPI_TAG);
#endif
		}

#ifdef DEBUG
		printf("%s\n", "{{func}}");
#endif
		TRACE_P2P('w', 1, source_list, dest_list, tag_list, time);

		return ret_val;

}{{endfn}}

{{fn func MPI_Waitall}}{


		int *source_list = (int *)malloc (count * sizeof(int));
		int *dest_list = (int *)malloc (count * sizeof(int));
		int *tag_list = (int *)malloc (count * sizeof(int));

		char *valid_flag = (char *)malloc (count * sizeof(char));
		memset(valid_flag,0,count * sizeof(char));

		for (int i = 0 ; i < count; i ++){
			dest_list[i] = mpiRank;

			map<RequestConverter, pair<int, int>> :: iterator iter;
			iter = request_converter.find(RequestConverter(&array_of_requests[i]));
			if (iter != request_converter.end()){
				pair<int, int> p = request_converter[RequestConverter(&array_of_requests[i])];

				//if (p.first >= 0 && p.second >= 0){
				source_list[i] = p.first;
				tag_list[i] = p.second;
#ifdef DEBUG
				if (mpiRank == 0){
					printf("convert wait %d %d\n", p.first, p.second);
				}
#endif
				valid_flag[i] = 1;
				
				//}
				request_converter.erase(RequestConverter(&array_of_requests[i]));
			}
		}

			auto st = chrono::system_clock::now();
		    int ret_val = {{callfn}}
			auto ed = chrono::system_clock::now();
			double time = chrono::duration_cast<chrono::microseconds>(ed - st).count();

		for(int i = 0; i < count; i++){
			if (valid_flag[i] == 0){
				source_list[i] = array_of_statuses[i].MPI_SOURCE;
				tag_list[i] = array_of_statuses[i].MPI_TAG;
				//printf("status wait %d %d\n", array_of_statuses[i].MPI_SOURCE, array_of_statuses[i].MPI_TAG);
#ifdef DEBUG
				if (mpiRank == 0){
					printf("status wait %d %d\n", array_of_statuses[i].MPI_SOURCE, array_of_statuses[i].MPI_TAG);
				}
#endif
			}
		}

		TRACE_P2P('w', count, source_list, dest_list, tag_list, time);

#ifdef DEBUG
		printf("%s\n", "{{func}}");
#endif

		free(source_list);
		free(dest_list);
		free(tag_list);


		return ret_val;

}{{endfn}}

// collective communication
{{fn func MPI_Reduce MPI_Alltoall MPI_Allreduce MPI_Bcast MPI_Scatter MPI_Gather MPI_Allgather}}{
    // First call collective communication
		auto st = chrono::system_clock::now();
    {{callfn}} 
		auto ed = chrono::system_clock::now();
		double time = chrono::duration_cast<chrono::microseconds>(ed - st).count();

#ifdef DEBUG
		printf("%s\n", "{{func}}");
#endif
		TRACE_COLL(comm, time);
}{{endfn}}


{{fn func MPI_Finalize}}{
		mpi_finalize_flag = true;
		writeCollMpiInfoLog();
		writeP2PMpiInfoLog();
		writeTraceLog();
#ifdef DEBUG
		printf("%s\n", "{{func}}");
#endif
		// Finally call MPI_Finalize
    {{callfn}}
}{{endfn}}
