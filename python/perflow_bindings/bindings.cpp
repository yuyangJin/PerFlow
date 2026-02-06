// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>

#include "analysis/performance_tree.h"
#include "analysis/tree_builder.h"
#include "analysis/tree_traversal.h"
#include "analysis/analysis_tasks.h"
#include "analysis/online_analysis.h"
#include "analysis/tree_visualizer.h"
#include "sampling/call_stack.h"

namespace py = pybind11;
using namespace perflow::analysis;
using namespace perflow::sampling;

// ============================================================================
// Python Visitor Wrapper
// ============================================================================

class PyTreeVisitor : public TreeVisitor {
 public:
  using TreeVisitor::TreeVisitor;
  
  bool visit(const std::shared_ptr<TreeNode>& node, size_t depth) override {
    PYBIND11_OVERRIDE_PURE(bool, TreeVisitor, visit, node, depth);
  }
};

// ============================================================================
// Module Definition
// ============================================================================

PYBIND11_MODULE(_perflow_bindings, m) {
  m.doc() = "PerFlow Python bindings for performance analysis";
  
  // ========================================================================
  // Enums
  // ========================================================================
  
  py::enum_<TreeBuildMode>(m, "TreeBuildMode",
      "Defines how call stacks are aggregated into a tree")
    .value("CONTEXT_FREE", TreeBuildMode::kContextFree,
           "Nodes with same function are merged regardless of calling context")
    .value("CONTEXT_AWARE", TreeBuildMode::kContextAware,
           "Nodes are distinguished by their full calling context")
    .export_values();
  
  py::enum_<SampleCountMode>(m, "SampleCountMode",
      "Defines how samples are counted during tree building")
    .value("EXCLUSIVE", SampleCountMode::kExclusive,
           "Only track self samples (samples at leaf nodes)")
    .value("INCLUSIVE", SampleCountMode::kInclusive,
           "Only track total samples (all samples including children)")
    .value("BOTH", SampleCountMode::kBoth,
           "Track both inclusive and exclusive samples")
    .export_values();
  
  py::enum_<ColorScheme>(m, "ColorScheme",
      "Defines the coloring strategy for visualization")
    .value("GRAYSCALE", ColorScheme::kGrayscale,
           "Grayscale based on sample count")
    .value("HEATMAP", ColorScheme::kHeatmap,
           "Heat map (blue -> red based on hotness)")
    .value("RAINBOW", ColorScheme::kRainbow,
           "Rainbow colors")
    .export_values();
  
  py::enum_<ParallelBuildStrategy>(m, "ParallelBuildStrategy",
      "Strategy for parallel tree construction")
    .value("COARSE_LOCK", ParallelBuildStrategy::kCoarseLock,
           "Single mutex for entire tree (simplest, highest contention)")
    .value("FINE_GRAINED_LOCK", ParallelBuildStrategy::kFineGrainedLock,
           "Per-node mutexes (medium contention)")
    .value("THREAD_LOCAL_MERGE", ParallelBuildStrategy::kThreadLocalMerge,
           "Thread-local trees with merge (lowest contention, best for high parallelism)")
    .value("LOCK_FREE", ParallelBuildStrategy::kLockFree,
           "Atomic counters with locks only for structure changes")
    .export_values();
  
  // ========================================================================
  // ResolvedFrame
  // ========================================================================
  
  py::class_<ResolvedFrame>(m, "ResolvedFrame",
      "A single resolved stack frame with symbol information")
    .def(py::init<>())
    .def_readwrite("raw_address", &ResolvedFrame::raw_address,
                   "Original raw address")
    .def_readwrite("offset", &ResolvedFrame::offset,
                   "Offset within the library/binary")
    .def_readwrite("library_name", &ResolvedFrame::library_name,
                   "Library or binary name")
    .def_readwrite("function_name", &ResolvedFrame::function_name,
                   "Function name (if available)")
    .def_readwrite("filename", &ResolvedFrame::filename,
                   "Source file name (if available)")
    .def_readwrite("line_number", &ResolvedFrame::line_number,
                   "Line number (if available)")
    .def("__repr__", [](const ResolvedFrame& f) {
      return "<ResolvedFrame function='" + f.function_name + 
             "' library='" + f.library_name + "'>";
    });
  
  // ========================================================================
  // TreeNode
  // ========================================================================
  
  py::class_<TreeNode, std::shared_ptr<TreeNode>>(m, "TreeNode",
      "Represents a vertex in the performance tree")
    .def("frame", &TreeNode::frame, py::return_value_policy::reference,
         "Get the resolved frame")
    .def("sampling_counts", &TreeNode::sampling_counts,
         "Get sampling counts for all processes")
    .def("execution_times", &TreeNode::execution_times,
         "Get execution times for all processes (in microseconds)")
    .def("total_samples", &TreeNode::total_samples,
         "Get total samples across all processes")
    .def("self_samples", &TreeNode::self_samples,
         "Get self samples (samples at this leaf)")
    .def("children", &TreeNode::children,
         "Get children nodes")
    .def("parent", &TreeNode::parent, py::return_value_policy::reference,
         "Get parent node")
    .def("find_child", &TreeNode::find_child,
         "Find a child by frame information (context-free)",
         py::arg("frame"))
    .def("find_child_context_aware", &TreeNode::find_child_context_aware,
         "Find a child by full frame information (context-aware)",
         py::arg("frame"))
    .def("get_call_count", &TreeNode::get_call_count,
         "Get call count to a specific child",
         py::arg("child"))
    .def("__repr__", [](const TreeNode& n) {
      return "<TreeNode function='" + n.frame().function_name + 
             "' samples=" + std::to_string(n.total_samples()) + ">";
    });
  
  // ========================================================================
  // PerformanceTree
  // ========================================================================
  
  py::class_<PerformanceTree>(m, "PerformanceTree",
      "Aggregates call stack samples into a tree structure")
    .def(py::init<TreeBuildMode, SampleCountMode>(),
         py::arg("mode") = TreeBuildMode::kContextFree,
         py::arg("count_mode") = SampleCountMode::kExclusive,
         "Create a new performance tree")
    .def("root", &PerformanceTree::root,
         "Get the root node")
    .def("process_count", &PerformanceTree::process_count,
         "Get the number of processes")
    .def("set_process_count", &PerformanceTree::set_process_count,
         "Set the number of processes",
         py::arg("count"))
    .def("build_mode", &PerformanceTree::build_mode,
         "Get the tree build mode")
    .def("set_build_mode", &PerformanceTree::set_build_mode,
         "Set the tree build mode",
         py::arg("mode"))
    .def("sample_count_mode", &PerformanceTree::sample_count_mode,
         "Get the sample count mode")
    .def("set_sample_count_mode", &PerformanceTree::set_sample_count_mode,
         "Set the sample count mode",
         py::arg("mode"))
    .def("insert_call_stack", &PerformanceTree::insert_call_stack,
         "Insert a call stack into the tree",
         py::arg("frames"),
         py::arg("process_id"),
         py::arg("count") = 1,
         py::arg("time_us") = 0.0)
    .def("total_samples", &PerformanceTree::total_samples,
         "Get total number of samples in the tree")
    .def("clear", &PerformanceTree::clear,
         "Clear the tree");
  
  // ========================================================================
  // TreeBuilder
  // ========================================================================
  
  py::class_<TreeBuilder>(m, "TreeBuilder",
      "Constructs performance trees from sample data")
    .def(py::init<TreeBuildMode, SampleCountMode>(),
         py::arg("mode") = TreeBuildMode::kContextFree,
         py::arg("count_mode") = SampleCountMode::kExclusive,
         "Create a new tree builder")
    .def("tree", py::overload_cast<>(&TreeBuilder::tree),
         py::return_value_policy::reference,
         "Get the performance tree")
    .def("build_mode", &TreeBuilder::build_mode,
         "Get the tree build mode")
    .def("set_build_mode", &TreeBuilder::set_build_mode,
         "Set the tree build mode",
         py::arg("mode"))
    .def("sample_count_mode", &TreeBuilder::sample_count_mode,
         "Get the sample count mode")
    .def("set_sample_count_mode", &TreeBuilder::set_sample_count_mode,
         "Set the sample count mode",
         py::arg("mode"))
    .def("parallel_strategy", &TreeBuilder::parallel_strategy,
         "Get the parallel build strategy")
    .def("set_parallel_strategy", &TreeBuilder::set_parallel_strategy,
         "Set the parallel build strategy for multi-threaded construction",
         py::arg("strategy"))
    .def("build_from_files", 
         [](TreeBuilder& self, const std::vector<std::pair<std::string, int>>& sample_files, double time_per_sample) {
           // Convert signed int to uint32_t
           std::vector<std::pair<std::string, uint32_t>> converted;
           converted.reserve(sample_files.size());
           for (const auto& pair : sample_files) {
             if (pair.second < 0) {
               throw std::invalid_argument("Process/rank ID must be non-negative, got: " + std::to_string(pair.second));
             }
             converted.emplace_back(pair.first, static_cast<uint32_t>(pair.second));
           }
           return self.build_from_files<>(converted, time_per_sample);
         },
         "Build tree from multiple sample files",
         py::arg("sample_files"),
         py::arg("time_per_sample") = 1000.0)
    .def("build_from_files_parallel",
         [](TreeBuilder& self, const std::vector<std::pair<std::string, int>>& sample_files, double time_per_sample, unsigned int num_threads) {
           // Convert signed int to uint32_t
           std::vector<std::pair<std::string, uint32_t>> converted;
           converted.reserve(sample_files.size());
           for (const auto& pair : sample_files) {
             if (pair.second < 0) {
               throw std::invalid_argument("Process/rank ID must be non-negative, got: " + std::to_string(pair.second));
             }
             converted.emplace_back(pair.first, static_cast<uint32_t>(pair.second));
           }
           return self.build_from_files_parallel(converted, time_per_sample, num_threads);
         },
         "Build tree from multiple sample files in parallel",
         py::arg("sample_files"),
         py::arg("time_per_sample") = 1000.0,
         py::arg("num_threads") = 0)
    .def("load_library_maps", 
         [](TreeBuilder& self, const std::vector<std::pair<std::string, int>>& libmap_files) {
           // Convert signed int to uint32_t
           std::vector<std::pair<std::string, uint32_t>> converted;
           converted.reserve(libmap_files.size());
           for (const auto& pair : libmap_files) {
             // Convert negative rank IDs (like -1) to special value or skip
             uint32_t rank_id = (pair.second < 0) ? 0xFFFFFFFF : static_cast<uint32_t>(pair.second);
             converted.emplace_back(pair.first, rank_id);
           }
           return self.load_library_maps(converted);
         },
         "Load library maps for address resolution",
         py::arg("libmap_files"))
    .def("clear", &TreeBuilder::clear,
         "Clear all data");
  
  // ========================================================================
  // TreeVisitor
  // ========================================================================
  
  py::class_<TreeVisitor, PyTreeVisitor, std::shared_ptr<TreeVisitor>>(
      m, "TreeVisitor",
      "Visitor interface for tree traversal")
    .def(py::init<>())
    .def("visit", &TreeVisitor::visit,
         "Called when visiting a node. Return True to continue, False to stop.",
         py::arg("node"),
         py::arg("depth"));
  
  // ========================================================================
  // TreeTraversal
  // ========================================================================
  
  py::class_<TreeTraversal>(m, "TreeTraversal",
      "Provides various tree traversal algorithms")
    .def_static("depth_first_preorder", &TreeTraversal::depth_first_preorder,
                "Traverse tree in depth-first order (pre-order)",
                py::arg("root"),
                py::arg("visitor"),
                py::arg("max_depth") = 0)
    .def_static("depth_first_postorder", &TreeTraversal::depth_first_postorder,
                "Traverse tree in depth-first order (post-order)",
                py::arg("root"),
                py::arg("visitor"),
                py::arg("max_depth") = 0)
    .def_static("breadth_first", &TreeTraversal::breadth_first,
                "Traverse tree in breadth-first order (level-order)",
                py::arg("root"),
                py::arg("visitor"),
                py::arg("max_depth") = 0)
    .def_static("find_nodes", &TreeTraversal::find_nodes,
                "Collect all nodes matching a predicate",
                py::arg("root"),
                py::arg("predicate"))
    .def_static("find_first", &TreeTraversal::find_first,
                "Find first node matching a predicate",
                py::arg("root"),
                py::arg("predicate"))
    .def_static("get_path_to_node", &TreeTraversal::get_path_to_node,
                "Get path from root to a specific node",
                py::arg("root"),
                py::arg("target"))
    .def_static("get_leaf_nodes", &TreeTraversal::get_leaf_nodes,
                "Get all leaf nodes (nodes with no children)",
                py::arg("root"))
    .def_static("get_nodes_at_depth", &TreeTraversal::get_nodes_at_depth,
                "Get nodes at a specific depth",
                py::arg("root"),
                py::arg("target_depth"))
    .def_static("get_siblings", &TreeTraversal::get_siblings,
                "Get neighbors (siblings) of a node",
                py::arg("node"))
    .def_static("get_tree_depth", &TreeTraversal::get_tree_depth,
                "Calculate tree depth (maximum distance from root to any leaf)",
                py::arg("root"))
    .def_static("count_nodes", &TreeTraversal::count_nodes,
                "Count total nodes in tree",
                py::arg("root"));
  
  // ========================================================================
  // Analysis Results
  // ========================================================================
  
  py::class_<BalanceAnalysisResult>(m, "BalanceAnalysisResult",
      "Contains workload balance information")
    .def(py::init<>())
    .def_readwrite("mean_samples", &BalanceAnalysisResult::mean_samples)
    .def_readwrite("std_dev_samples", &BalanceAnalysisResult::std_dev_samples)
    .def_readwrite("min_samples", &BalanceAnalysisResult::min_samples)
    .def_readwrite("max_samples", &BalanceAnalysisResult::max_samples)
    .def_readwrite("imbalance_factor", &BalanceAnalysisResult::imbalance_factor)
    .def_readwrite("most_loaded_process", &BalanceAnalysisResult::most_loaded_process)
    .def_readwrite("least_loaded_process", &BalanceAnalysisResult::least_loaded_process)
    .def_readwrite("process_samples", &BalanceAnalysisResult::process_samples)
    .def("__repr__", [](const BalanceAnalysisResult& r) {
      return "<BalanceAnalysisResult mean=" + std::to_string(r.mean_samples) +
             " imbalance=" + std::to_string(r.imbalance_factor) + ">";
    });
  
  py::class_<HotspotInfo>(m, "HotspotInfo",
      "Describes a performance hotspot")
    .def(py::init<>())
    .def_readwrite("function_name", &HotspotInfo::function_name)
    .def_readwrite("library_name", &HotspotInfo::library_name)
    .def_readwrite("source_location", &HotspotInfo::source_location)
    .def_readwrite("total_samples", &HotspotInfo::total_samples)
    .def_readwrite("percentage", &HotspotInfo::percentage)
    .def_readwrite("self_samples", &HotspotInfo::self_samples)
    .def_readwrite("self_percentage", &HotspotInfo::self_percentage)
    .def("__repr__", [](const HotspotInfo& h) {
      return "<HotspotInfo function='" + h.function_name +
             "' percentage=" + std::to_string(h.percentage) + "%>";
    });
  
  // ========================================================================
  // Analyzers
  // ========================================================================
  
  py::class_<BalanceAnalyzer>(m, "BalanceAnalyzer",
      "Analyzes workload distribution across processes")
    .def_static("analyze", &BalanceAnalyzer::analyze,
                "Analyze workload balance in the performance tree",
                py::arg("tree"));
  
  py::class_<HotspotAnalyzer>(m, "HotspotAnalyzer",
      "Identifies performance bottlenecks")
    .def_static("find_hotspots", &HotspotAnalyzer::find_hotspots,
                "Find top hotspots by exclusive/self time (default)",
                py::arg("tree"),
                py::arg("top_n") = 10)
    .def_static("find_self_hotspots", &HotspotAnalyzer::find_self_hotspots,
                "Find hotspots by self samples (exclusive time)",
                py::arg("tree"),
                py::arg("top_n") = 10)
    .def_static("find_total_hotspots", &HotspotAnalyzer::find_total_hotspots,
                "Find hotspots by total samples (inclusive time)",
                py::arg("tree"),
                py::arg("top_n") = 10);
  
  // ========================================================================
  // OnlineAnalysis
  // ========================================================================
  
  py::class_<OnlineAnalysis>(m, "OnlineAnalysis",
      "Provides a complete online analysis framework")
    .def(py::init<>())
    .def("set_monitor_directory", &OnlineAnalysis::set_monitor_directory,
         "Set the directory to monitor",
         py::arg("directory"),
         py::arg("poll_interval_ms") = 1000)
    .def("start_monitoring", &OnlineAnalysis::start_monitoring,
         "Start monitoring")
    .def("stop_monitoring", &OnlineAnalysis::stop_monitoring,
         "Stop monitoring")
    .def("monitor_directory", &OnlineAnalysis::monitor_directory,
         "Get the monitored directory")
    .def("builder", py::overload_cast<>(&OnlineAnalysis::builder),
         py::return_value_policy::reference,
         "Get the tree builder")
    .def("tree", py::overload_cast<>(&OnlineAnalysis::tree),
         py::return_value_policy::reference,
         "Get the performance tree")
    .def("set_sample_count_mode", &OnlineAnalysis::set_sample_count_mode,
         "Set the sample count mode",
         py::arg("mode"))
    .def("sample_count_mode", &OnlineAnalysis::sample_count_mode,
         "Get the sample count mode")
    .def("analyze_balance", &OnlineAnalysis::analyze_balance,
         "Perform balance analysis")
    .def("find_hotspots", &OnlineAnalysis::find_hotspots,
         "Find performance hotspots by exclusive/self time (default)",
         py::arg("top_n") = 10)
    .def("find_self_hotspots", &OnlineAnalysis::find_self_hotspots,
         "Find self-time hotspots",
         py::arg("top_n") = 10)
    .def("find_total_hotspots", &OnlineAnalysis::find_total_hotspots,
         "Find total/inclusive time hotspots",
         py::arg("top_n") = 10)
    .def("export_visualization", &OnlineAnalysis::export_visualization,
         "Export tree visualization",
         py::arg("output_pdf"),
         py::arg("scheme") = ColorScheme::kHeatmap,
         py::arg("max_depth") = 0)
    .def("export_tree", &OnlineAnalysis::export_tree,
         "Export tree data",
         py::arg("directory"),
         py::arg("filename"),
         py::arg("compressed") = false)
    .def("export_tree_text", &OnlineAnalysis::export_tree_text,
         "Export tree as text",
         py::arg("directory"),
         py::arg("filename"));
  
  // ========================================================================
  // Common Predicates
  // ========================================================================
  
  m.def("samples_above", &predicates::samples_above,
        "Predicate for nodes with sample count above threshold",
        py::arg("threshold"));
  m.def("self_samples_above", &predicates::self_samples_above,
        "Predicate for nodes with self samples above threshold",
        py::arg("threshold"));
  m.def("function_name_equals", &predicates::function_name_equals,
        "Predicate for nodes matching function name",
        py::arg("name"));
  m.def("function_name_contains", &predicates::function_name_contains,
        "Predicate for nodes containing function name substring",
        py::arg("substring"));
  m.def("library_name_equals", &predicates::library_name_equals,
        "Predicate for nodes in a specific library",
        py::arg("name"));
  m.def("is_leaf", &predicates::is_leaf,
        "Predicate for leaf nodes");
}
