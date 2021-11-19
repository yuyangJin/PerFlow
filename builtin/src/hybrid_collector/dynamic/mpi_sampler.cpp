#include <stdio.h>
#include <string.h>
#include <string>
#include "baguatool.h"
#include "mpi_init.h"

#define MODULE_INITED 1

#define NUM_EVENTS 1

#define MAX_CALL_PATH_DEPTH 100

std::unique_ptr<baguatool::core::PerfData> perf_data = nullptr;

std::unique_ptr<baguatool::collector::Sampler> sampler = nullptr;

static int CYC_SAMPLE_COUNT = 0;
static int module_init = 0;

int mpi_rank = -1;

void RecordCallPath(int y) {
  baguatool::type::addr_t call_path[MAX_CALL_PATH_DEPTH] = {0};
  int call_path_len = sampler->GetBacktrace(call_path, MAX_CALL_PATH_DEPTH);
  perf_data->RecordVertexData(call_path, call_path_len, mpi_rank /* process_id */, 0 /* thread_id */, 1);
}

static void init_mock() __attribute__((constructor));
static void fini_mock() __attribute__((destructor));

// User-defined what to do at constructor
static void init_mock() {
  if (module_init == MODULE_INITED) return;
  module_init = MODULE_INITED;

  sampler = std::make_unique<baguatool::collector::Sampler>();
  // TODO one perf_data corresponds to one metric, export it to an array
  perf_data = std::make_unique<baguatool::core::PerfData>();

  sampler->SetSamplingFreq(CYC_SAMPLE_COUNT);
  sampler->Setup();

  void (*RecordCallPathPointer)(int) = &(RecordCallPath);
  sampler->SetOverflow(RecordCallPathPointer);

  sampler->Start();
}

// User-defined what to do at destructor
static void fini_mock() {
  sampler->Stop();
  std::string output_file_name = std::string("SAMPLE") + std::to_string(mpi_rank) + std::string(".TXT");
  perf_data->Dump(output_file_name.c_str());
  // sampler->RecordLdLib();
}
