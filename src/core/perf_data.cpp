#include "perf_data.h"
#include "dbg.h"

namespace baguatool::core {

/** TODO: need to rename these two functions */
void preserve_call_path_tail_so_addr(std::stack<type::addr_t>& call_path) {
  std::stack<type::addr_t> tmp;
  while (!call_path.empty()) {
    type::addr_t addr = call_path.top();
    call_path.pop();
    if (type::IsValidAddr(addr)) {
      tmp.push(addr);
    }
  }
  bool exe_addr_exist = false;
  while (!tmp.empty()) {
    type::addr_t addr = tmp.top();
    tmp.pop();
    if (type::IsTextAddr(addr)) {
      call_path.push(addr);
      exe_addr_exist = true;
    } else if (exe_addr_exist == false) {
      call_path.push(addr);
    }
  }
  FREE_CONTAINER(tmp);
}

void delete_all_so_addr(std::stack<type::addr_t>& call_path) {
  std::stack<type::addr_t> tmp;
  while (!call_path.empty()) {
    type::addr_t addr = call_path.top();
    call_path.pop();
    if (type::IsTextAddr(addr)) {
      tmp.push(addr);
    }
  }

  while (!tmp.empty()) {
    type::addr_t addr = tmp.top();
    tmp.pop();
    call_path.push(addr);
  }
  FREE_CONTAINER(tmp);
}

PerfData::PerfData() {
  this->vertex_perf_data_space_size = MAX_TRACE_MEM / sizeof(VDS);
  // dbg(this->vertex_perf_data_space_size);
  this->vertex_perf_data = new VDS[this->vertex_perf_data_space_size];
  this->vertex_perf_data_count = 0;

  this->edge_perf_data_space_size = MAX_TRACE_MEM / sizeof(EDS);
  this->edge_perf_data = new EDS[this->edge_perf_data_space_size];
  this->edge_perf_data_count = 0;

  strcpy(this->file_name, "SAMPLE.TXT");
}
PerfData::~PerfData() {
  delete[] this->vertex_perf_data;
  delete[] this->edge_perf_data;
  if (this->has_open_output_file) {
    fclose(this->perf_data_fp);
  }
}

// Sequential read
void PerfData::Read(const char* infile_name) {
  // char infile_name_str[MAX_CALL_PATH_LEN];
  // strcpy(infile_name_str, std::string(infile_name).c_str());

  //dbg(infile_name);

  this->perf_data_in_file.open(std::string(infile_name), std::ios::in);
  if (!(this->perf_data_in_file.is_open())) {
    LOG_INFO("Failed to open %s\n", infile_name);
    this->vertex_perf_data_count = __sync_and_and_fetch(&this->vertex_perf_data_count, 0);
    return;
  }

  // char line[MAX_LINE_LEN];
  std::string line;
  // Read a line for VDS counts
  // this->perf_data_in_file.getline(line, MAX_CALL_PATH_LEN);
  getline(this->perf_data_in_file, line);
  unsigned long int count = strtoul(line.c_str(), 0, 10);
  //dbg(count);

  // Read lines, each line is a VDS
  while (count-- && getline(this->perf_data_in_file, line)) {
    // Read a line
    // this->perf_data_in_file.getline(line, MAX_CALL_PATH_LEN);

    // Parse the line
    // First '|'
    // std::vector<std::string> line_vec = split(line, "|");
    std::vector<std::string> line_vec;
    split(line, "|", line_vec);
    int cnt = line_vec.size();

    int procs_id = atoi(line_vec[2].c_str());

    // dbg(cnt);

    if (cnt == 4 && procs_id >= 0) {
      unsigned long int x = __sync_fetch_and_add(&this->vertex_perf_data_count, 1);

      //std::cout << count << " " << line_vec[1].c_str() <<std::endl;
      this->vertex_perf_data[x].value = atof(line_vec[1].c_str());
      this->vertex_perf_data[x].procs_id = atoi(line_vec[2].c_str());
      this->vertex_perf_data[x].thread_id = atoi(line_vec[3].c_str());

      // Then parse call path
      std::vector<std::string> addr_vec;
      split(line_vec[0], " ", addr_vec);
      int call_path_len = addr_vec.size();
      this->vertex_perf_data[x].call_path_len = call_path_len;
      for (int i = 0; i < call_path_len; i++) {
        this->vertex_perf_data[x].call_path[i] = strtoul(addr_vec[i].c_str(), 0, 16);
        // dbg(this->vertex_perf_data[x].call_path[i]);
      }

      // LOG_INFO("DATA[%lu]: %s | %lf | %d |%d\n", x, line_vec[0].c_str(), this->vertex_perf_data[x].value,
      //         this->vertex_perf_data[x].procs_id, this->vertex_perf_data[x].thread_id);

      FREE_CONTAINER(addr_vec);
    } else {
      // dbg(cnt, line);
    }

    FREE_CONTAINER(line_vec);
  }

  // Read a line for EDS counts
  // this->perf_data_in_file.getline(line, MAX_CALL_PATH_LEN);
  getline(this->perf_data_in_file, line);
  count = strtoul(line.c_str(), 0, 10);
  //dbg(count);

  while (count-- && getline(this->perf_data_in_file, line)) {
    // Read a line
    // this->perf_data_in_file.getline(line, MAX_CALL_PATH_LEN);

    // Parse the line
    // First '|'
    // char delim[] = "|";
    // line_vec[4][MAX_CALL_PATH_LEN];
    std::vector<std::string> line_vec;
    split(line, "|", line_vec);
    int cnt = line_vec.size();

    // dbg(cnt);

    if (cnt == 7) {
      // First fetch as x, then add 1
      unsigned long int x = __sync_fetch_and_add(&this->edge_perf_data_count, 1);

      this->edge_perf_data[x].value = atof(line_vec[2].c_str());
      this->edge_perf_data[x].procs_id = atoi(line_vec[3].c_str());
      this->edge_perf_data[x].out_procs_id = atoi(line_vec[4].c_str());
      this->edge_perf_data[x].thread_id = atoi(line_vec[5].c_str());
      this->edge_perf_data[x].out_thread_id = atoi(line_vec[6].c_str());

      // Then parse call path
      std::vector<std::string> addr_vec;
      split(line_vec[0], " ", addr_vec);
      int call_path_len = addr_vec.size();
      this->edge_perf_data[x].call_path_len = call_path_len;
      for (int i = 0; i < call_path_len; i++) {
        this->edge_perf_data[x].call_path[i] = strtoul(addr_vec[i].c_str(), 0, 16);
      }
      FREE_CONTAINER(addr_vec);

      split(line_vec[1], " ", addr_vec);
      int out_call_path_len = addr_vec.size();
      this->edge_perf_data[x].out_call_path_len = out_call_path_len;
      for (int i = 0; i < out_call_path_len; i++) {
        this->edge_perf_data[x].out_call_path[i] = strtoul(addr_vec[i].c_str(), 0, 16);
      }
      FREE_CONTAINER(addr_vec);

      // LOG_INFO("DATA[%lu]: %s | %lf | %d |%d\n", x, line_vec[0].c_str(),
      //          this->edge_perf_data[x].value,
      //          this->edge_perf_data[x].procs_id,
      //          this->edge_perf_data[x].thread_id);
    } else {
      dbg(cnt, line);
    }
  }

  this->perf_data_in_file.close();
}

void PerfData::Dump(const char* output_file_name) {
  if (output_file_name != nullptr) {
    strcpy(this->file_name, output_file_name);
  }
  if (!has_open_output_file) {
    this->perf_data_fp = fopen(this->file_name, "w");
    if (!this->perf_data_fp) {
      LOG_INFO("Failed to open %s\n", this->file_name);
      this->vertex_perf_data_count = __sync_and_and_fetch(&this->vertex_perf_data_count, 0);
      return;
    }
  }

  // LOG_INFO("Rank %d : WRITE %d ADDR to %d TXT\n", mpiRank, call_path_addr_log_pointer[i], i);
  fprintf(this->perf_data_fp, "%lu\n", this->vertex_perf_data_count);
  for (unsigned long int i = 0; i < this->vertex_perf_data_count; i++) {
    for (int j = 0; j < this->vertex_perf_data[i].call_path_len; j++) {
      fprintf(this->perf_data_fp, "%llx ", this->vertex_perf_data[i].call_path[j]);
    }
    fprintf(this->perf_data_fp, " | %lf | %d | %d\n", this->vertex_perf_data[i].value,
            this->vertex_perf_data[i].procs_id, this->vertex_perf_data[i].thread_id);
    fflush(this->perf_data_fp);
  }
  this->vertex_perf_data_count = __sync_and_and_fetch(&this->vertex_perf_data_count, 0);

  fprintf(this->perf_data_fp, "%lu\n", this->edge_perf_data_count);
  for (unsigned long int i = 0; i < this->edge_perf_data_count; i++) {
    for (int j = 0; j < this->edge_perf_data[i].call_path_len; j++) {
      fprintf(this->perf_data_fp, "%llx ", this->edge_perf_data[i].call_path[j]);
    }
    fprintf(this->perf_data_fp, " | ");
    for (int j = 0; j < this->edge_perf_data[i].out_call_path_len; j++) {
      fprintf(this->perf_data_fp, "%llx ", this->edge_perf_data[i].out_call_path[j]);
    }
    fprintf(this->perf_data_fp, " | %lf | %d | %d | %d | %d\n", this->edge_perf_data[i].value,
            this->edge_perf_data[i].procs_id, this->edge_perf_data[i].out_procs_id, this->edge_perf_data[i].thread_id,
            this->edge_perf_data[i].out_thread_id);
    fflush(this->perf_data_fp);
  }
  this->edge_perf_data_count = __sync_and_and_fetch(&this->edge_perf_data_count, 0);
}

unsigned long int PerfData::GetVertexDataSize() { return this->vertex_perf_data_count; }

unsigned long int PerfData::GetEdgeDataSize() { return this->edge_perf_data_count; }

std::string& PerfData::GetMetricName() { return this->metric_name; }

void PerfData::SetMetricName(std::string& metric_name) { this->metric_name = std::string(metric_name); }

bool CallPathCmp(type::addr_t* cp_1, int cp_1_len, type::addr_t* cp_2, int cp_2_len) {
  if (cp_1_len != cp_2_len) {
    return false;
  } else {
    for (int i = 0; i < cp_1_len; i++) {
      if (cp_1[i] != cp_2[i]) {
        return false;
      }
    }
  }
  return true;
}

int PerfData::QueryVertexData(type::addr_t* call_path, int call_path_len, int procs_id, int thread_id) {
  for (unsigned long int i = 0; i < this->vertex_perf_data_count; i++) {
    // call_path
    if (CallPathCmp(call_path, call_path_len, this->vertex_perf_data[i].call_path,
                    this->vertex_perf_data[i].call_path_len) == true &&
        this->vertex_perf_data[i].thread_id == thread_id && this->vertex_perf_data[i].procs_id == procs_id) {
      return i;
    }
  }

  return -1;
}

int PerfData::QueryEdgeData(type::addr_t* call_path, int call_path_len, type::addr_t* out_call_path,
                            int out_call_path_len, int procs_id, int out_procs_id, int thread_id, int out_thread_id) {
  for (unsigned long int i = 0; i < this->edge_perf_data_count; i++) {
    // call_path
    if (CallPathCmp(call_path, call_path_len, this->edge_perf_data[i].call_path,
                    this->edge_perf_data[i].call_path_len) == true &&
        CallPathCmp(out_call_path, out_call_path_len, this->edge_perf_data[i].out_call_path,
                    this->edge_perf_data[i].out_call_path_len) == true &&
        this->edge_perf_data[i].thread_id == thread_id && this->edge_perf_data[i].procs_id == procs_id &&
        this->edge_perf_data[i].out_thread_id == out_thread_id &&
        this->edge_perf_data[i].out_procs_id == out_procs_id) {
      return i;
    }
  }

  return -1;
}

// TODO: Modify current aggregation mode to non-aggregation mode, users can do aggregation with our APIs
void PerfData::RecordVertexData(type::addr_t* call_path, int call_path_len, int procs_id, int thread_id,
                                perf_data_t value) {
  int index = this->QueryVertexData(call_path, call_path_len, procs_id, thread_id);

  if (index >= 0) {
    this->vertex_perf_data[index].value += value;
  } else {
    // Thread-safe, first fetch a index, then record data
    unsigned long long int x = __sync_fetch_and_add(&this->vertex_perf_data_count, 1);

    this->vertex_perf_data[x].call_path_len = call_path_len;
    for (int i = 0; i < call_path_len; i++) {
      this->vertex_perf_data[x].call_path[i] = call_path[i];
    }
    this->vertex_perf_data[x].value = value;
    this->vertex_perf_data[x].thread_id = thread_id;
    this->vertex_perf_data[x].procs_id = procs_id;
  }

  if (this->vertex_perf_data_count >= this->vertex_perf_data_space_size - 5) {
    // TODO: asynchronous dump
    this->Dump(this->file_name);
  }
}

void PerfData::RecordEdgeData(type::addr_t* call_path, int call_path_len, type::addr_t* out_call_path,
                              int out_call_path_len, int procs_id, int out_procs_id, int thread_id, int out_thread_id,
                              perf_data_t value) {
  // Thread-safe, first fetch a index, then record data
  unsigned long long int x = __sync_fetch_and_add(&this->edge_perf_data_count, 1);

  this->edge_perf_data[x].call_path_len = call_path_len;
  for (int i = 0; i < call_path_len; i++) {
    this->edge_perf_data[x].call_path[i] = call_path[i];
  }
  this->edge_perf_data[x].out_call_path_len = out_call_path_len;
  for (int i = 0; i < out_call_path_len; i++) {
    this->edge_perf_data[x].out_call_path[i] = out_call_path[i];
  }

  this->edge_perf_data[x].value = value;
  this->edge_perf_data[x].thread_id = thread_id;
  this->edge_perf_data[x].procs_id = procs_id;
  this->edge_perf_data[x].out_thread_id = out_thread_id;
  this->edge_perf_data[x].out_procs_id = out_procs_id;

  if (this->edge_perf_data_count >= this->edge_perf_data_space_size - 5) {
    // TODO: asynchronous dump
    this->Dump(this->file_name);
  }
}

void PerfData::GetVertexDataCallPath(unsigned long int data_index, std::stack<type::addr_t>& call_path_stack) {
  VDS* data = &(this->vertex_perf_data[data_index]);
  for (int i = 0; i < data->call_path_len; i++) {
    call_path_stack.push(data->call_path[i]);
  }
  preserve_call_path_tail_so_addr(call_path_stack);
  return;
}

// void PerfData::SetVertexDataCallPath(unsigned long int data_index, std::stack<type::addr_t>& call_path_stack) {
//   VDS* data = &(this->vertex_perf_data[data_index]);
//   int call_path_len = call_path_stack.size();
//   for (int i = 0; i < call_path_len; i++) {
//     data->call_path[call_path_len - i - 1] = call_path_stack.top();
//     call_path_stack.pop();
//   }
//   return;
// }

void PerfData::GetEdgeDataSrcCallPath(unsigned long int data_index, std::stack<type::addr_t>& call_path_stack) {
  EDS* data = &(this->edge_perf_data[data_index]);
  for (int i = 0; i < data->call_path_len; i++) {
    call_path_stack.push(data->call_path[i]);
  }
  delete_all_so_addr(call_path_stack);
  return;
}

void PerfData::GetEdgeDataDestCallPath(unsigned long int data_index, std::stack<type::addr_t>& call_path_stack) {
  EDS* data = &(this->edge_perf_data[data_index]);
  for (int i = 0; i < data->out_call_path_len; i++) {
    call_path_stack.push(data->out_call_path[i]);
  }
  delete_all_so_addr(call_path_stack);
  return;
}

perf_data_t PerfData::GetVertexDataValue(unsigned long int data_index) {
  VDS* data = &(this->vertex_perf_data[data_index]);
  return data->value;
}

perf_data_t PerfData::GetEdgeDataValue(unsigned long int data_index) {
  EDS* data = &(this->edge_perf_data[data_index]);
  return data->value;
}

int PerfData::GetVertexDataProcsId(unsigned long int data_index) {
  VDS* data = &(this->vertex_perf_data[data_index]);
  return data->procs_id;
}

int PerfData::GetEdgeDataSrcProcsId(unsigned long int data_index) {
  EDS* data = &(this->edge_perf_data[data_index]);
  return data->procs_id;
}

int PerfData::GetEdgeDataDestProcsId(unsigned long int data_index) {
  EDS* data = &(this->edge_perf_data[data_index]);
  return data->out_procs_id;
}

int PerfData::GetVertexDataThreadId(unsigned long int data_index) {
  VDS* data = &(this->vertex_perf_data[data_index]);
  return data->thread_id;
}

int PerfData::GetEdgeDataSrcThreadId(unsigned long int data_index) {
  EDS* data = &(this->edge_perf_data[data_index]);
  return data->thread_id;
}

int PerfData::GetEdgeDataDestThreadId(unsigned long int data_index) {
  EDS* data = &(this->edge_perf_data[data_index]);
  return data->out_thread_id;
}

}  // namespace baguatool::core