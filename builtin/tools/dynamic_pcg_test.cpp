#include <cstring>
#include "baguatool.h"
#include "graph_perf.h"

int main(int argc, char** argv) {
  const char* bin_name = argv[1];
  char pag_dir_name[20] = {0};
  char pcg_name[20] = {0};
  strcpy(pag_dir_name, bin_name);
  strcat(pag_dir_name, ".pag/");
  strcpy(pcg_name, bin_name);
  strcat(pcg_name, ".pcg");

  const char* perf_data_file_name = argv[2];
  baguatool::core::PerfData* perf_data = new baguatool::core::PerfData();
  perf_data->Read(perf_data_file_name);

  auto gperf = std::make_unique<graph_perf::GPerf>();

  std::string shared_obj_map_file_name = std::string(argv[3]);

  gperf->ReadFunctionAbstractionGraphs(pag_dir_name);
  gperf->GenerateDynAddrDebugInfo(perf_data, shared_obj_map_file_name);
  gperf->GenerateProgramCallGraph(bin_name, perf_data);
  gperf->GetProgramCallGraph()->DumpGraphGML("hy_pcg.gml");
}