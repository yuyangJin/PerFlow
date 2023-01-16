#include <fstream>
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

#define MAX_WAIT_REQ 500 // increase it if we test larger process scale
#define MAX_NPROCS 10000 // increase it if we test larger process scale

//#define DEBUG

typedef struct CollInfoStruct {
  string call_path;
  string comm;
  string count;
  string exe_time;
} CIS;

typedef struct P2PInfoStruct {
  char type = 0;
  string call_path;
  int request_count = 0;
  int source[MAX_WAIT_REQ] = {0};
  int dest[MAX_WAIT_REQ] = {0};
  int tag[MAX_WAIT_REQ] = {0};
  string count;
  string exe_time;
} PIS;

typedef struct CommDepEdge {
  char dest_type = 0;
  char src_type = 0;
  string dest_callpath;
  string src_callpath;
  int dest_pid = 0;
  int src_pid = 0;
  double exe_time = 0;
} CDE;

// unordered_map<unsigned long long, CIS*> coll_info[];
vector<unordered_map<unsigned long long, CIS *>> coll_info;
vector<vector<PIS *>> p2p_info;
vector<unsigned int> coll_info_pointer;
vector<unsigned int> p2p_info_pointer;
// vector<int> trace_log[MAX_NPROCS];
vector<vector<int>> trace_log;
vector<CDE *> comm_dep_edge;

void readMPIInfo(string file_name, int pid) {
  coll_info_pointer[pid] = 0;
  p2p_info_pointer[pid] = 0;
  ifstream inputStream(file_name.c_str(), ios_base::in);
  if (!inputStream.good()) {
    cout << "Failed to open " << file_name.c_str() << " file\n";
    return;
  }
  size_t pos = 0;
  string delimiter = "";
  for (string line; getline(inputStream, line);) {
    // Parse - type
    delimiter = " ";
    pos = line.find(delimiter);
    string type_str = line.substr(0, pos);
    line.erase(0, pos + delimiter.length());

    // Parse - call path
    delimiter = "|";
    pos = line.find(delimiter);
    string call_path_str = line.substr(0, pos);
    line.erase(0, pos + delimiter.length());

    // Parse - info
    pos = line.find(delimiter);
    string info_str = line.substr(0, pos);
    line.erase(0, pos + delimiter.length());

    // Parse - count
    pos = line.find(delimiter);
    string count_str = line.substr(0, pos);
    line.erase(0, pos + delimiter.length());

    // Parse -  time
    delimiter = "\n";
    pos = line.find(delimiter);
    string exe_time_str = line.substr(0, pos);
    line.erase(0, pos + delimiter.length());

    if (!type_str.compare(string("c"))) {
      // CIS * one_coll_info = (CIS*) malloc(sizeof(CIS));
      CIS *one_coll_info = new CIS;
      // delimiter = " ";
      // while ((pos = call_path_str.find(delimiter)) != std::string::npos) {
      //   string addr = call_path_str.substr(0, pos);
      //   call_path_str.erase(0, pos + delimiter.length());
      //   stringstream stristre =
      // }

      one_coll_info->call_path = call_path_str;
      one_coll_info->comm = info_str;
      one_coll_info->count = count_str;
      one_coll_info->exe_time = exe_time_str;
      coll_info[pid].insert(make_pair(coll_info_pointer[pid], one_coll_info));
      coll_info_pointer[pid]++;
    } else if (!type_str.compare(string("s")) ||
               !type_str.compare(string("S")) ||
               !type_str.compare(string("r")) ||
               !type_str.compare(string("R")) ||
               !type_str.compare(string("w"))) {
      // PIS * one_p2p_info = (PIS*) malloc(sizeof(PIS));
      PIS *one_p2p_info = new PIS;

      one_p2p_info->type = type_str.c_str()[0];
      one_p2p_info->call_path = call_path_str;
      one_p2p_info->count = count_str;
      one_p2p_info->exe_time = exe_time_str;

      // Analyze
      int request_count_tmp = 0;
      delimiter = ",";
      while ((pos = info_str.find(delimiter)) != std::string::npos) {
        string src_dst_tag = info_str.substr(0, pos);
        info_str.erase(0, pos + delimiter.length());
        stringstream stristre;
        int src = -1, dst = -1, tag = -1;
        stristre << src_dst_tag;
        stristre >> src >> dst >> tag;
        one_p2p_info->source[request_count_tmp] = src;
        one_p2p_info->dest[request_count_tmp] = dst;
        one_p2p_info->tag[request_count_tmp] = tag;
        request_count_tmp++;
      }
      one_p2p_info->request_count = request_count_tmp;
      pair<unsigned long long, PIS *> pair_tmp(p2p_info_pointer[pid],
                                               one_p2p_info);
      p2p_info[pid].push_back(one_p2p_info);
      // p2p_info[pid].insert(make_pair(p2p_info_pointer[pid], one_p2p_info));
      p2p_info_pointer[pid]++;
    }
  }
  inputStream.close();
}

void readTraceInfo(string file_name, int pid) {
  ifstream inputStream(file_name.c_str(), ios_base::in);
  if (!inputStream.good()) {
    cout << "Failed to open " << file_name.c_str() << " file\n";
    return;
  }
  size_t pos = 0;
  string delimiter = " ";
  for (string line; getline(inputStream, line);) {
    while ((pos = line.find(delimiter)) != std::string::npos) {
      string index = line.substr(0, pos);
      line.erase(0, pos + delimiter.length());
      trace_log[pid].push_back(stoi(index));
    }
  }

  inputStream.close();
}

bool existCDE(int dest_type, int src_type, string dest_callpath,
              string src_callpath, int dest_pid, int src_pid) {
  for (auto cde : comm_dep_edge) {
    if (cde->dest_type == dest_type && cde->src_type == src_type &&
        !cde->dest_callpath.compare(dest_callpath) &&
        !cde->src_callpath.compare(src_callpath) && cde->dest_pid == dest_pid &&
        cde->src_pid == src_pid) {
      return true;
    }
  }
  return false;
}

// output inter-process communication dependence edge : (dest , src , edge
// value) -> (type | call path | pid , type | call path | pid , exe_time)
void commOpMatch(int pid) {
  vector<int>::iterator iter;
  for (iter = trace_log[pid].begin(); iter != trace_log[pid].end(); ++iter) {
    int index = (*iter);
    // p2p
    if (index % 2 == 1) {
      // recv('r') or wait('w')
      char type = p2p_info[pid][index / 2]->type;
      if (type == 'r' || type == 'w') {
        int request_count = p2p_info[pid][index / 2]->request_count;
        for (int req_id = 0; req_id < request_count; req_id++) {
          int src = p2p_info[pid][index / 2]->source[req_id];
          int dest = p2p_info[pid][index / 2]->dest[req_id];
          int tag = p2p_info[pid][index / 2]->tag[req_id];

          // wait for
          if (src == dest || src < 0 || tag < 0) {
            continue;
          }

          // search, record, and delete corresponding send op('s' with same tag)
          // in trace_log[src]
          vector<int>::iterator src_iter;
          for (src_iter = trace_log[src].begin();
               src_iter != trace_log[src].end(); ++src_iter) {
            int src_index = (*src_iter);
            // p2p
            if (src_index % 2 == 1) {
              char src_type = p2p_info[src][src_index / 2]->type;
              int src_src = p2p_info[src][src_index / 2]->source[0];
              int src_dest = p2p_info[src][src_index / 2]->dest[0];
              int src_tag = p2p_info[src][src_index / 2]->tag[0];
              if ((src_type == 's' || src_type == 'S') && src_src == src &&
                  src_dest == dest && src_tag == tag) {
                char dest_type = p2p_info[pid][index / 2]->type;
                // char src_type = p2p_info[src][src_index / 2]->type;
                string dest_callpath = p2p_info[pid][index / 2]->call_path;
                string src_callpath = p2p_info[src][src_index / 2]->call_path;
                int dest_pid = pid;
                int src_pid = src;
                double exe_time = stod(p2p_info[pid][index / 2]->exe_time);

                // delete it
                trace_log[src].erase(src_iter);
                if (existCDE(dest_type, src_type, dest_callpath, src_callpath,
                             dest_pid, src_pid)) {
                  // trace_log[src].erase(src_iter);
                  break;
                }
                // record
                CDE *one_comm_dep_edge = new CDE;
                one_comm_dep_edge->dest_type = dest_type;
                one_comm_dep_edge->src_type = src_type;
                one_comm_dep_edge->dest_callpath = dest_callpath;
                one_comm_dep_edge->src_callpath = src_callpath;
                one_comm_dep_edge->dest_pid = dest_pid;
                one_comm_dep_edge->src_pid = src_pid;
                one_comm_dep_edge->exe_time = exe_time;
                comm_dep_edge.push_back(one_comm_dep_edge);

#ifdef DEBUG
                cout << dest_type << " | " << dest_callpath << " | " << dest_pid
                     << ", ";
                cout << src_type << " | " << src_callpath << " | " << src_pid
                     << ", ";
                cout << exe_time << endl;
#endif

                break;
              }
            }
          }
        }
      }
    }
  }
}

void commOpMatchWithMPIInfo(int pid) {
  vector<PIS *>::iterator iter;
  for (iter = p2p_info[pid].begin(); iter != p2p_info[pid].end(); ++iter) {
    char type = (*iter)->type;
    if (type == 'r' || type == 'w') {
      int request_count = (*iter)->request_count;
      for (int req_id = 0; req_id < request_count; req_id++) {
        int src = (*iter)->source[req_id];
        int dest = (*iter)->dest[req_id];
        int tag = (*iter)->tag[req_id];
        if (src == dest || src < 0 || tag < 0) {
          continue;
        }
        vector<PIS *>::iterator src_iter;
        for (src_iter = p2p_info[src].begin(); src_iter != p2p_info[src].end();
             ++src_iter) {
          char src_type = (*src_iter)->type;
          int src_src = (*src_iter)->source[0];
          int src_dest = (*src_iter)->dest[0];
          int src_tag = (*src_iter)->tag[0];
          if ((src_type == 's' || src_type == 'S') && src_src == src &&
              src_dest == dest && src_tag == tag) {
            char dest_type = (*iter)->type;
            // char src_type = ( *src_iter ) ->type;
            string dest_callpath = (*iter)->call_path;
            string src_callpath = (*src_iter)->call_path;
            int dest_pid = pid;
            int src_pid = src;
            double exe_time = 0.0;
            if (!(*iter)->exe_time.empty()) {
              exe_time = stod((*iter)->exe_time);
            }

            // delete it
            p2p_info[src].erase(src_iter);
            if (existCDE(dest_type, src_type, dest_callpath, src_callpath,
                         dest_pid, src_pid)) {
              // trace_log[src].erase(src_iter);
              break;
            }
            // record
            CDE *one_comm_dep_edge = new CDE;
            one_comm_dep_edge->dest_type = dest_type;
            one_comm_dep_edge->src_type = src_type;
            one_comm_dep_edge->dest_callpath = dest_callpath;
            one_comm_dep_edge->src_callpath = src_callpath;
            one_comm_dep_edge->dest_pid = dest_pid;
            one_comm_dep_edge->src_pid = src_pid;
            one_comm_dep_edge->exe_time = exe_time;
            comm_dep_edge.push_back(one_comm_dep_edge);

            if (src_type == 's') {
              double src_exe_time = 0.0;
              if (!(*src_iter)->exe_time.empty()) {
                src_exe_time = stod((*src_iter)->exe_time);
              }
              CDE *one_reverse_comm_dep_edge = new CDE;
              one_reverse_comm_dep_edge->dest_type = src_type;
              one_reverse_comm_dep_edge->src_type = dest_type;
              one_reverse_comm_dep_edge->dest_callpath = src_callpath;
              one_reverse_comm_dep_edge->src_callpath = dest_callpath;
              one_reverse_comm_dep_edge->dest_pid = src_pid;
              one_reverse_comm_dep_edge->src_pid = dest_pid;
              one_reverse_comm_dep_edge->exe_time = src_exe_time;
              comm_dep_edge.push_back(one_reverse_comm_dep_edge);
            }
            break;
          }
        }
      }
    }
  }
}

void outputCommDepEdges(ofstream &fout) {
  fout << "0" << endl;
  fout << comm_dep_edge.size() << endl;
  for (auto &cdp : comm_dep_edge) {
    //#ifdef DEBUG

    fout << cdp->src_callpath << " | " << cdp->dest_callpath << " | ";
    fout << cdp->exe_time << " | ";
    fout << cdp->src_pid << " | " << cdp->dest_pid << " | ";
    fout << "0 | 0" << endl;
    // fout.flush();
  }
  //#endif
}

int main(int argc, char *argv[]) {
  int nprocs = atoi(argv[1]);

  coll_info.resize(nprocs);
  p2p_info.resize(nprocs);
  // trace_log.resize(nprocs);
  coll_info_pointer.resize(nprocs);
  p2p_info_pointer.resize(nprocs);

  for (int pid = 0; pid < nprocs; pid++) {
    readMPIInfo(string(argv[2]) + string("/dynamic_data/MPID") +
                    to_string(pid) + string(".TXT"),
                pid);
    // readTraceInfo(string("./MPIT") + to_string(pid) + string(".TXT"), pid);
  }

  for (int pid = 0; pid < nprocs; pid++) {
    commOpMatchWithMPIInfo(pid);
  }
  std::string output_filename =
      string(argv[2]) + string("/dynamic_data/") + string(argv[3]);
  ofstream outputStream(output_filename.c_str(), ios_base::out);
  outputCommDepEdges(outputStream);
}