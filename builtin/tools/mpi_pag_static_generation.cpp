#include <cstring>
#include "graph_perf.h"

int main(int argc, char** argv) {
  /** Setups */
  const char* bin_name = argv[1];
  char pag_dir_name[20] = {0}, pcg_name[20] = {0};
  strcpy(pag_dir_name, bin_name);
  strcat(pag_dir_name, ".pag/");
  strcpy(pcg_name, bin_name);
  strcat(pcg_name, ".pcg");
  // std::string shared_obj_map_file_name = std::string(argv[3]);
  int num_procs = atoi(argv[2]);
  baguatool::core::PerfData* perf_data = new baguatool::core::PerfData();
  for (int i = 5; i < argc; i++) {
    const char* perf_data_file_name = argv[i];
    perf_data->Read(perf_data_file_name);
  }
  auto graph_perf = std::make_unique<graph_perf::GPerf>();

  /** Read confrol-flow graph and program call graph */
  graph_perf->ReadFunctionAbstractionGraphs(pag_dir_name);
  // graph_perf->GenerateDynAddrDebugInfo(perf_data, shared_obj_map_file_name);
  graph_perf->GenerateProgramCallGraph(bin_name, perf_data);
  // graph_perf->GetProgramCallGraph()->DumpGraphGML("hy_pcg.gml");
  graph_perf->GenerateProgramAbstractionGraph(perf_data);
  baguatool::core::ProgramAbstractionGraph* pag = graph_perf->GetProgramAbstractionGraph();
  pag->DumpGraphGML("static_pag.gml");

  /** Data embedding */
  graph_perf->DataEmbedding(perf_data);
  auto graph_perf_data = pag->GetGraphPerfData();
  std::string output_file_name_str("output.json");
  graph_perf_data->Dump(output_file_name_str);
  std::string metric("TOT_CYC");
  std::string op("SUM");
  baguatool::type::perf_data_t total;
  total = pag->ReduceVertexPerfData(metric, op);
  std::string avg_metric("TOT_CYC_SUM");
  std::string new_metric("CYCAVGPERCENT");
  pag->ConvertVertexReducedDataToPercent(avg_metric, total, new_metric);
  pag->DumpGraphGML("pag.gml");

  /** MPAG generation*/

  int starting_vertex = atoi(argv[3]);
  graph_perf->GenerateMultiProcessProgramAbstractionGraph(starting_vertex, num_procs);
  baguatool::core::PerfData* comm_data = new baguatool::core::PerfData();
  const char* comm_data_file_name = argv[4];
  comm_data->Read(comm_data_file_name);
  graph_perf->AddCommEdgesToMPAG(comm_data);

  auto mpag = graph_perf->GetMultiProgramAbstractionGraph();

  auto mpag_graph_perf_data = mpag->GetGraphPerfData();
  std::string mpag_output_file_name_str("mpag_perf_data.json");
  mpag_graph_perf_data->Dump(mpag_output_file_name_str);

  total = 0;
  total = mpag->ReduceVertexPerfData(metric, op);
  printf("%lf\n", total);
  mpag->ConvertVertexReducedDataToPercent(avg_metric, total / num_procs, new_metric);
  mpag->DumpGraphGML("mpi_mpag.gml");
}