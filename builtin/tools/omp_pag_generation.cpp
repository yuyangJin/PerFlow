#include "graph_perf.h"
#include <cstring>
#include <string>

#define MAX_NUM_CORE 24

int main(int argc, char **argv) {
  const char *bin_name = argv[1];
  char pag_dir_name[20] = {0};
  char pcg_name[20] = {0};
  strcpy(pag_dir_name, bin_name);
  strcat(pag_dir_name, ".pag/");

  strcpy(pcg_name, bin_name);
  strcat(pcg_name, ".pcg");

  const char *perf_data_file_name = argv[2];
  baguatool::core::PerfData *perf_data = new baguatool::core::PerfData();
  perf_data->Read(perf_data_file_name);

  std::string shared_obj_map_file_name = std::string(argv[3]);

  auto gperf = std::make_unique<graph_perf::GPerf>();

  gperf->ReadFunctionAbstractionGraphs(pag_dir_name);
  gperf->GenerateDynAddrDebugInfo(perf_data, shared_obj_map_file_name,
                                  bin_name);
  gperf->GenerateProgramCallGraph(bin_name, perf_data);
  gperf->GenerateProgramAbstractionGraph(perf_data);

  baguatool::core::ProgramAbstractionGraph *pag =
      gperf->GetProgramAbstractionGraph();

  gperf->DataEmbedding(perf_data);

  std::string metric("TOT_CYC");

  gperf->OpenMPGroupThreadPerfData(metric, MAX_NUM_CORE);

  std::string op("SUM");
  baguatool::type::perf_data_t total = pag->ReduceVertexPerfData(metric, op);
  std::string avg_metric("TOT_CYC_SUM");
  std::string new_metric("CYCAVGPERCENT");
  pag->ConvertVertexReducedDataToPercent(avg_metric, total, new_metric);

  auto graph_perf_data =
      gperf->GetProgramAbstractionGraph()->GetGraphPerfData();
  std::string output_file_name_str("output.json");
  graph_perf_data->Dump(output_file_name_str);

  pag->DeleteExtraTailVertices();
  // gperf->GetProgramAbstractionGraph()->PreserveHotVertices("CYCAVGPERCENT");

  /** MPAG */
  int num_threads = atoi(argv[3]);

  gperf->GenerateOpenMPProgramAbstractionGraph(num_threads);
  auto mpag = gperf->GetMultiProgramAbstractionGraph();

  auto mpag_graph_perf_data = mpag->GetGraphPerfData();
  std::string mpag_output_file_name_str("mpag_gpd.json");
  mpag_graph_perf_data->Dump(mpag_output_file_name_str);

  total = 0;
  total = mpag->ReduceVertexPerfData(metric, op);
  printf("%lf\n", total);
  mpag->ConvertVertexReducedDataToPercent(avg_metric, total / num_threads,
                                          new_metric);

  mpag->DumpGraphGML("multi_thread_pag.gml");
}