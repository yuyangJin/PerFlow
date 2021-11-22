#include "core/hybrid_analysis.h"
#include <stdlib.h>
#include "common/utils.h"
#include "core/graph_perf_data.h"
#include "core/vertex_type.h"

namespace baguatool::core {

HybridAnalysis::HybridAnalysis() { this->graph_perf_data = new GraphPerfData(); }

HybridAnalysis::~HybridAnalysis() { delete this->graph_perf_data; }

void HybridAnalysis::ReadStaticControlFlowGraphs(const char *dir_name) {
  // Get name of files in this directory
  std::vector<std::string> file_names;
  getFiles(std::string(dir_name), file_names);

  // Traverse the files
  for (const auto &fn : file_names) {
    dbg(fn);

    // Read a ControlFlowGraph from each file
    ControlFlowGraph *func_cfg = new ControlFlowGraph();
    func_cfg->ReadGraphGML(fn.c_str());
    // new_pag->DumpGraph((file_name + std::string(".bak")).c_str());
    this->func_cfg_map[std::string(func_cfg->GetGraphAttributeString("name"))] = func_cfg;
  }
}

void HybridAnalysis::GenerateControlFlowGraphs(const char *dir_name) { this->ReadStaticControlFlowGraphs(dir_name); }

ControlFlowGraph *HybridAnalysis::GetControlFlowGraph(std::string func_name) { return this->func_cfg_map[func_name]; }

std::map<std::string, ControlFlowGraph *> &HybridAnalysis::GetControlFlowGraphs() { return this->func_cfg_map; }

void HybridAnalysis::ReadStaticProgramCallGraph(const char *binary_name) {
  // Get name of static program call graph's file
  std::string static_pcg_file_name = std::string(binary_name) + std::string(".pcg");

  // Read a ProgramCallGraph from each file
  this->pcg = new ProgramCallGraph();
  this->pcg->ReadGraphGML(static_pcg_file_name.c_str());
}

void HybridAnalysis::ReadDynamicProgramCallGraph(std::string perf_data_file) {
  PerfData *perf_data = new PerfData();
  perf_data->Read(perf_data_file.c_str());
  auto data_size = perf_data->GetVertexDataSize();

  /** Optimization: First scan all call path and store <call_addr, callee_addr> pairs,
   * then AddEdgeWithAddr. It can reduce redundant graph query **/

  // AddEdgeWithAddr for each <call_addr, callee_addr> pair of each call path
  for (unsigned long int i = 0; i < data_size; i++) {
    std::stack<unsigned long long> call_path;
    perf_data->GetVertexDataCallPath(i, call_path);
    auto value = perf_data->GetVertexDataValue(i);
    // auto process_id = perf_data->GetVertexDataProcsId(i);
    // auto thread_id = perf_data->GetVertexDataThreadId(i, process_id);

    while (!call_path.empty()) {
      auto call_addr = call_path.top();
      call_path.pop();
      auto callee_addr = call_path.top();
      this->pcg->AddEdgeWithAddr(call_addr, callee_addr);
    }
  }
}

void HybridAnalysis::GenerateProgramCallGraph(const char *binary_name) {
  this->ReadStaticProgramCallGraph(binary_name);
}

ProgramCallGraph *HybridAnalysis::GetProgramCallGraph() { return this->pcg; }

void HybridAnalysis::ReadFunctionAbstractionGraphs(const char *dir_name) {
  // Get name of files in this directory
  std::vector<std::string> file_names;
  getFiles(std::string(dir_name), file_names);

  // Traverse the files
  for (const auto &fn : file_names) {
    dbg(fn);

    // Read a ProgramAbstractionGraph from each file
    ProgramAbstractionGraph *new_pag = new ProgramAbstractionGraph();
    new_pag->ReadGraphGML(fn.c_str());
    // new_pag->DumpGraph((file_name + std::string(".bak")).c_str());
    func_pag_map[std::string(new_pag->GetGraphAttributeString("name"))] = new_pag;
  }
}

/** Intra-procedural Analysis **/

ProgramAbstractionGraph *HybridAnalysis::GetFunctionAbstractionGraph(std::string func_name) {
  return this->func_pag_map[func_name];
}

std::map<std::string, ProgramAbstractionGraph *> &HybridAnalysis::GetFunctionAbstractionGraphs() {
  return this->func_pag_map;
}

void HybridAnalysis::IntraProceduralAnalysis() {}

/** Inter-procedural Analysis **/

typedef struct InterProceduralAnalysisArg {
  std::map<std::string, ProgramAbstractionGraph *> *func_pag_map;
  ProgramCallGraph *pcg;
} InterPAArg;

// FIXME: `void *` should not appear in cpp
void ConnectCallerCallee(ProgramAbstractionGraph *pag, int vertex_id, void *extra) {
  InterPAArg *arg = (InterPAArg *)extra;
  std::map<std::string, ProgramAbstractionGraph *> *func_name_2_pag = arg->func_pag_map;
  ProgramCallGraph *pcg = arg->pcg;

  // dbg(pag->GetGraphAttributeString("name"));
  int type = pag->GetVertexType(vertex_id);
  if (type == CALL_NODE || type == CALL_IND_NODE || type == CALL_REC_NODE) {
    int addr = pag->GetVertexAttributeNum("saddr", vertex_id);

    // dbg(vertex_id, addr);
    type::vertex_t call_vertex_id = pcg->GetCallVertexWithAddr(addr);
    // dbg(call_vertex_id);

    // ProgramAbstractionGraph *callee_pag =
    //     (*func_name_2_pag)[std::string(pag->GetVertexAttributeString("name", vertex_id))];
    auto callee_func_name = pcg->GetCallee(call_vertex_id);
    // free(callee_func_name);

    string callee_func_name_str = std::string(callee_func_name);

    // dbg(callee_func_name_str);

    if (callee_func_name) {
      ProgramAbstractionGraph *callee_pag = (*func_name_2_pag)[callee_func_name_str];

      if (!callee_pag->GetGraphAttributeFlag("scanned")) {
        void (*ConnectCallerCalleePointer)(ProgramAbstractionGraph *, int, void *) = &(ConnectCallerCallee);
        // dbg(callee_pag->GetGraphAttributeString("name"));
        callee_pag->VertexTraversal(ConnectCallerCalleePointer, extra);
        callee_pag->SetGraphAttributeFlag("scanned", true);
      }

      // Add Vertex to
      int vertex_count = pag->GetCurVertexNum();

      pag->AddGraph(callee_pag);

      pag->AddEdge(vertex_id, vertex_count);
    }
  }
}

void HybridAnalysis::InterProceduralAnalysis() {
  // Search root node , "name" is "main"
  // std::map<std::string, ProgramAbstractionGraph *> func_name_2_pag;

  for (auto &kv : this->func_pag_map) {
    // func_name_2_pag[std::string(pag->GetGraphAttributeString("name"))] = pag;
    auto pag = kv.second;
    pag->SetGraphAttributeFlag("scanned", false);
    if (strcmp(kv.first.c_str(), "main") == 0) {
      this->root_pag = pag;
      //std::cout << "Find 'main'" << std::endl;
      // break;
    }
  }

  // DFS From root node
  InterPAArg *arg = new InterPAArg();
  arg->pcg = this->pcg;
  arg->func_pag_map = &(this->func_pag_map);

  // void (*ConnectCallerCalleePointer)(Graph *, int, void *) = &ConnectCallerCallee;
  // dbg(this->root_pag->GetGraphAttributeString("name"));
  this->root_pag->VertexTraversal(&ConnectCallerCallee, arg);

  for (auto &kv : this->func_pag_map) {
    // func_name_2_pag[std::string(pag->GetGraphAttributeString("name"))] = pag;
    auto pag = kv.second;
    pag->RemoveGraphAttribute("scanned");
  }
  delete arg;

  this->root_pag->VertexSortChild();

  return;
}  // function InterProceduralAnalysis

void HybridAnalysis::GenerateProgramAbstractionGraph() { this->InterProceduralAnalysis(); }

void HybridAnalysis::SetProgramAbstractionGraph(ProgramAbstractionGraph *pag) { this->root_pag = pag; }

ProgramAbstractionGraph *HybridAnalysis::GetProgramAbstractionGraph() { return this->root_pag; }

void HybridAnalysis::DataEmbedding(PerfData *perf_data) {
  // Query for each call path
  auto data_size = perf_data->GetVertexDataSize();
  for (unsigned long int i = 0; i < data_size; i++) {
    std::stack<unsigned long long> call_path;
    perf_data->GetVertexDataCallPath(i, call_path);
    if (!call_path.empty()) {
      call_path.pop();
    }
    auto value = perf_data->GetVertexDataValue(i);
    auto process_id = perf_data->GetVertexDataProcsId(i);
    auto thread_id = perf_data->GetVertexDataThreadId(i);

    type::vertex_t queried_vertex_id = this->root_pag->GetVertexWithCallPath(0, call_path);
    // dbg(queried_vertex_id);
    perf_data_t data =
        this->graph_perf_data->GetPerfData(queried_vertex_id, perf_data->GetMetricName(), process_id, thread_id);
    data += value;
    this->graph_perf_data->SetPerfData(queried_vertex_id, perf_data->GetMetricName(), process_id, thread_id, data);
  }

}  // function Dataembedding

GraphPerfData *HybridAnalysis::GetGraphPerfData() { return this->graph_perf_data; }

perf_data_t ReduceOperation(std::vector<perf_data_t> &perf_data, int num, string &op) {
  if (num == 0) {
    return 0.0;
  }
  if (!strcmp(op.c_str(), "AVG")) {
    perf_data_t avg = 0.0;
    for (int i = 0; i < num; i++) {
      avg += perf_data[i];
    }
    avg /= (perf_data_t)num;
    return avg;
  } else {
    return perf_data[0];
  }
}

typedef struct ReducePerfDataArg {
  // input
  GraphPerfData *graph_perf_data = nullptr;
  std::string metric;
  std::string op;
  // output
  perf_data_t total_reduced_data = 0.0;
} RPDArg;

void ReducePerfData(ProgramAbstractionGraph *pag, int vertex_id, void *extra) {
  RPDArg *arg = (RPDArg *)extra;
  GraphPerfData *graph_perf_data = arg->graph_perf_data;
  std::string metric(arg->metric);
  std::string op(arg->op);

  int num_procs = graph_perf_data->GetMetricsPerfDataProcsNum(vertex_id, metric);

  std::vector<perf_data_t> im_reduced_data;

  for (int i = 0; i < num_procs; i++) {
    // perf_data_t* procs_perf_data = nullptr;
    std::vector<perf_data_t> procs_perf_data;
    graph_perf_data->GetProcsPerfData(vertex_id, metric, i, procs_perf_data);
    // int num_thread = graph_perf_data->GetProcsPerfDataThreadNum(vertex_id, metric, i);
    int num_thread = procs_perf_data.size();

    if (num_thread > 0) {
      im_reduced_data.push_back(ReduceOperation(procs_perf_data, num_thread, op));
      // dbg(im_reduced_data[i]);
    } else {
      im_reduced_data[i] = 0.0;
    }

    FREE_CONTAINER(procs_perf_data);
  }

  perf_data_t reduced_data = ReduceOperation(im_reduced_data, num_procs, op);
  // dbg(reduced_data);

  FREE_CONTAINER(im_reduced_data);

  pag->SetVertexAttributeString(std::string(metric + std::string("_") + op).c_str(), (type::vertex_t)vertex_id,
                                std::to_string(reduced_data).c_str());

  arg->total_reduced_data += reduced_data;
}

// Reduce each vertex's perf data
perf_data_t HybridAnalysis::ReduceVertexPerfData(std::string &metric, std::string &op) {
  RPDArg *arg = new RPDArg();
  arg->graph_perf_data = this->graph_perf_data;
  arg->metric = std::string(metric);
  arg->op = std::string(op);
  arg->total_reduced_data = 0.0;

  this->root_pag->VertexTraversal(&ReducePerfData, arg);

  perf_data_t total = arg->total_reduced_data;
  delete arg;
  return total;
}

typedef struct PerfDataToPercentArg {
  // input
  std::string new_metric;
  std::string metric;
  perf_data_t total;
  // output
  //...
} PDTPArg;

void PerfDataToPercent(ProgramAbstractionGraph *pag, int vertex_id, void *extra) {
  PDTPArg *arg = (PDTPArg *)extra;
  std::string new_metric(arg->new_metric);
  std::string metric(arg->metric);
  perf_data_t total = arg->total;

  perf_data_t data = strtod(pag->GetVertexAttributeString(metric.c_str(), (type::vertex_t)vertex_id), NULL);
  perf_data_t percent = data / total;
  pag->SetVertexAttributeString(new_metric.c_str(), (type::vertex_t)vertex_id, std::to_string(percent).c_str());
}

// convert vertex's reduced data to percent
void HybridAnalysis::ConvertVertexReducedDataToPercent(std::string &metric, perf_data_t total,
                                                       std::string &new_metric) {
  PDTPArg *arg = new PDTPArg();
  arg->new_metric = std::string(new_metric);
  arg->metric = std::string(metric);
  arg->total = total;

  this->root_pag->VertexTraversal(&PerfDataToPercent, arg);

  delete arg;
}

void HybridAnalysis::GenerateMultiProgramAbstractionGraph() {
  root_mpag = new ProgramAbstractionGraph();
  root_mpag->GraphInit("Multi-process Program Abstraction Graph");

  std::vector<type::vertex_t> pre_order_vertex_seq;
  root_pag->PreOrderTraversal(0, pre_order_vertex_seq);

  type::vertex_t last_new_vertex_id = -1;
  for (auto &vertex_id : pre_order_vertex_seq) {
    type::vertex_t new_vertex_id = root_mpag->AddVertex();
    root_mpag->CopyVertex(new_vertex_id, root_pag, vertex_id);
    if (last_new_vertex_id != -1) {
      root_mpag->AddEdge(last_new_vertex_id, new_vertex_id);
    }
    last_new_vertex_id = new_vertex_id;
  }
}

ProgramAbstractionGraph *HybridAnalysis::GetMultiProgramAbstractionGraph() { return root_mpag; }

void HybridAnalysis::PthreadAnalysis(PerfData *pthread_data) {
  // Query for each call path
  auto data_size = pthread_data->GetVertexDataSize();
  for (unsigned long int i = 0; i < data_size; i++) {
    std::stack<unsigned long long> call_path;
    pthread_data->GetVertexDataCallPath(i, call_path);
    if (!call_path.empty()) {
      call_path.pop();
    }
    auto time = pthread_data->GetVertexDataValue(i);
    auto create_thread_id = pthread_data->GetVertexDataProcsId(i);
    auto thread_id = pthread_data->GetVertexDataThreadId(i);

    // TODO: Need to judge if it is in current thread
    // if thread_id == cur_thread

    if (time == (baguatool::core::perf_data_t)(-1)) {
      type::vertex_t queried_vertex_id = this->root_pag->GetVertexWithCallPath(0, call_path);
      type::addr_t addr = call_path.top();
      dbg(addr);
      type::vertex_t pthread_vertex_id = root_pag->AddVertex();
      root_pag->SetVertexBasicInfo(pthread_vertex_id, core::CALL_NODE, "pthread_create");
      root_pag->SetVertexDebugInfo(pthread_vertex_id, addr, addr);
      root_pag->SetVertexAttributeNum("id", pthread_vertex_id, pthread_vertex_id);
      root_pag->AddEdge(queried_vertex_id, pthread_vertex_id);
      for (unsigned long int j = 0; j < data_size; j++) {
        if (j != i) {
          if (create_thread_id == pthread_data->GetVertexDataProcsId(j)) {
            dbg(create_thread_id, pthread_data->GetVertexDataProcsId(j));
            std::stack<unsigned long long> call_path_j;
            pthread_data->GetVertexDataCallPath(j, call_path_j);
            if (!call_path_j.empty()) {
              call_path_j.pop();
            }
            auto time_j = pthread_data->GetVertexDataValue(j);
            auto create_thread_id_j = pthread_data->GetVertexDataProcsId(j);
            auto thread_id_j = pthread_data->GetVertexDataThreadId(j);

            type::vertex_t queried_vertex_id_j = this->root_pag->GetVertexWithCallPath(0, call_path_j);
            type::addr_t addr_j = call_path_j.top();
            dbg(addr_j);
            type::vertex_t pthread_join_vertex_id = root_pag->AddVertex();
            dbg(pthread_join_vertex_id);
            root_pag->SetVertexBasicInfo(pthread_join_vertex_id, core::CALL_NODE, "pthread_join");
            root_pag->SetVertexDebugInfo(pthread_join_vertex_id, addr_j, addr_j);
            root_pag->SetVertexAttributeNum("id", pthread_join_vertex_id, pthread_join_vertex_id);
            root_pag->AddEdge(queried_vertex_id_j, pthread_join_vertex_id);
            FREE_CONTAINER(call_path_j);
            break;
          }
        }
      }
    }
    FREE_CONTAINER(call_path);
  }

  root_pag->VertexSortChild();
}

}  // namespace baguatool::core