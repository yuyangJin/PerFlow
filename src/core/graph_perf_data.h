#ifndef GRAPH_PERF_DATA_H_
#define GRAPH_PERF_DATA_H_

#include "baguatool.h"
#include <fstream>
#include <nlohmann/json.hpp>

namespace baguatool::core {

/******************************************************************
 * format of graph_perf_data {
 *      vertex_id : vertex_perf_data {
 *          metric (TOT_INS ...) : metric_perf_data {
 *              process_id : process_perf_data {
 *                  thread_id : thread_perf_data (double or float)
 *                                        // perf_data of a thread
 *                  ...
 *              }  // perf_data of a process
 *              ...
 *          }  // perf_data of a metric
 *          ...
 *      }  // perf_data of a vertex
 *      ...
 * }; // perf_data of a graph
 *******************************************************************/
// typedef double perf_data_t;

// class GraphPerfData {
//  private:
//   json j_perf_data;
//  public:
//   GraphPerfData();
//   ~GraphPerfData();

//   void Read(std::string&);
//   void Dump(std::string&);

//   void SetPerfData(vertex_t vertex_id, std::string& metric, int process_id,
//   int thread_id, perf_data_t value); perf_data_t GetPerfData(vertex_t
//   vertex_id, std::string& metric, int process_id, int thread_id);

//   bool HasMetric(vertex_t vertex_id, std::string& metric);
//   void GetVertexPerfDataMetrics(vertex_t, std::vector<std::string>&);
//   void GetProcessPerfData(vertex_t vertex_id, std::string& metric, int
//   process_id, perf_data_t* proc_perf_data);

// };  // class GraphPerfData

} // namespace baguatool::core

#endif // GRAPH_PERF_DATA_H_