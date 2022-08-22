#include "baguatool.h"
#include "graph_perf.h"
#include <cstring>
#include <string>

int main(int argc, char **argv) {
  const char *bin_name = argv[1];
  char pag_dir_name[20] = {0};
  char pcg_name[20] = {0};
  strcpy(pag_dir_name, bin_name);
  strcat(pag_dir_name, ".pag/");
  strcpy(pcg_name, bin_name);
  strcat(pcg_name, ".pcg");
  const char *perf_data_file_name = argv[2];
  // std::string shared_obj_map_file_name = std::string(argv[3]);

  auto gperf = std::make_unique<graph_perf::GPerf>();

  gperf->ReadFunctionAbstractionGraphs(pag_dir_name);

  /** Read perf data */
  baguatool::core::PerfData *perf_data = new baguatool::core::PerfData();
  perf_data->Read(perf_data_file_name);

  // gperf->GenerateDynAddrDebugInfo(perf_data, shared_obj_map_file_name);
  gperf->GenerateProgramCallGraph(bin_name, perf_data);
  gperf->GetProgramCallGraph()->DumpGraphGML("hy_pcg.gml");

  gperf->GenerateProgramAbstractionGraph(perf_data);

  baguatool::core::ProgramAbstractionGraph *pag =
      gperf->GetProgramAbstractionGraph();

  gperf->DataEmbedding(perf_data);
  std::string metric("TOT_CYC");
  std::string op("SUM");
  baguatool::type::perf_data_t total = pag->ReduceVertexPerfData(metric, op);
  std::string avg_metric("TOT_CYC_SUM");
  std::string new_metric("CYCAVGPERCENT");
  pag->ConvertVertexReducedDataToPercent(avg_metric, total, new_metric);

  auto graph_perf_data =
      gperf->GetProgramAbstractionGraph()->GetGraphPerfData();
  std::string output_file_name_str("output.json");
  graph_perf_data->Dump(output_file_name_str);

  // gperf->GetProgramAbstractionGraph()->PreserveHotVertices("CYCAVGPERCENT");

  gperf->GetProgramAbstractionGraph()->DumpGraphGML("pag.gml");
}