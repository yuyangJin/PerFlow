// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

// Python bindings for PerFlow Analysis Module
// This file provides Python access to the C++ performance tree and analysis APIs

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>

#include "analysis/analysis_tasks.h"
#include "analysis/parallel_file_reader.h"
#include "analysis/performance_tree.h"
#include "analysis/tree_builder.h"
#include "sampling/call_stack.h"

namespace py = pybind11;

using namespace perflow::analysis;
using namespace perflow::sampling;

PYBIND11_MODULE(_perflow, m) {
    m.doc() = "PerFlow Performance Analysis Python Bindings";

    // ========================================================================
    // Enumerations
    // ========================================================================

    py::enum_<TreeBuildMode>(m, "TreeBuildMode", 
        "Defines how call stacks are aggregated into a tree")
        .value("ContextFree", TreeBuildMode::kContextFree,
            "Nodes with same function are merged regardless of calling context")
        .value("ContextAware", TreeBuildMode::kContextAware,
            "Nodes are distinguished by their full calling context")
        .export_values();

    py::enum_<SampleCountMode>(m, "SampleCountMode",
        "Defines how samples are counted during tree building")
        .value("Exclusive", SampleCountMode::kExclusive,
            "Only track self samples (samples at leaf nodes)")
        .value("Inclusive", SampleCountMode::kInclusive,
            "Only track total samples (all samples including children)")
        .value("Both", SampleCountMode::kBoth,
            "Track both inclusive and exclusive samples")
        .export_values();

    py::enum_<ConcurrencyModel>(m, "ConcurrencyModel",
        "Defines how concurrent tree building is handled")
        .value("Serial", ConcurrencyModel::kSerial,
            "Single global lock for all operations (simple, no parallelism)")
        .value("FineGrainedLock", ConcurrencyModel::kFineGrainedLock,
            "Per-node locks allowing parallel subtree insertion (O(depth) locks)")
        .value("ThreadLocalMerge", ConcurrencyModel::kThreadLocalMerge,
            "Thread-local trees merged after construction (no contention during build)")
        .value("LockFree", ConcurrencyModel::kLockFree,
            "Atomic operations for counters, locks only for structural changes")
        .export_values();

    // ========================================================================
    // ResolvedFrame - Stack frame information
    // ========================================================================

    py::class_<ResolvedFrame>(m, "ResolvedFrame",
        "Represents a resolved stack frame with symbol information")
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
    // TreeNode - Performance tree vertex
    // ========================================================================

    py::class_<TreeNode, std::shared_ptr<TreeNode>>(m, "TreeNode",
        "Represents a vertex in the performance tree")
        // Frame information
        .def_property_readonly("frame", &TreeNode::frame,
            "Get the resolved frame information")
        .def_property_readonly("function_name", 
            [](const TreeNode& n) { return n.frame().function_name; },
            "Get the function name")
        .def_property_readonly("library_name",
            [](const TreeNode& n) { return n.frame().library_name; },
            "Get the library name")
        
        // Sample counts
        .def_property_readonly("total_samples", &TreeNode::total_samples,
            "Get total samples across all processes")
        .def_property_readonly("self_samples", &TreeNode::self_samples,
            "Get self samples (samples at this leaf)")
        .def("sampling_count", &TreeNode::sampling_count,
            py::arg("process_id"),
            "Get sampling count for a specific process")
        .def_property_readonly("sampling_counts", &TreeNode::sampling_counts,
            "Get all per-process sampling counts")
        
        // Execution times
        .def("execution_time", &TreeNode::execution_time,
            py::arg("process_id"),
            "Get execution time for a specific process (microseconds)")
        .def_property_readonly("total_execution_time", &TreeNode::total_execution_time,
            "Get total execution time across all processes (microseconds)")
        .def_property_readonly("execution_times", &TreeNode::execution_times,
            "Get all per-process execution times")
        
        // Tree structure
        .def_property_readonly("parent", 
            [](const TreeNode& n) -> TreeNode* { return n.parent(); },
            py::return_value_policy::reference,
            "Get parent node (None if root)")
        .def_property_readonly("children", &TreeNode::children,
            "Get list of child nodes")
        .def_property_readonly("child_count", &TreeNode::child_count,
            "Get number of children")
        .def_property_readonly("is_leaf", &TreeNode::is_leaf,
            "Check if this is a leaf node")
        .def_property_readonly("is_root", &TreeNode::is_root,
            "Check if this is the root node")
        .def_property_readonly("depth", &TreeNode::depth,
            "Get depth in the tree (root = 0)")
        .def("siblings", &TreeNode::siblings,
            "Get all sibling nodes")
        .def("get_path", &TreeNode::get_path,
            "Get path from root to this node as function names")
        
        // Child lookup
        .def("find_child_by_name", &TreeNode::find_child_by_name,
            py::arg("func_name"),
            "Find child by function name")
        .def("get_call_count", 
            [](const TreeNode& n, const std::shared_ptr<TreeNode>& child) {
                return n.get_call_count(child);
            },
            py::arg("child"),
            "Get call count to a specific child")
        
        .def("__repr__", [](const TreeNode& n) {
            return "<TreeNode function='" + n.frame().function_name + 
                   "' samples=" + std::to_string(n.total_samples()) + ">";
        })
        .def("__str__", [](const TreeNode& n) {
            return n.frame().function_name + " (" + 
                   std::to_string(n.total_samples()) + " samples)";
        });

    // ========================================================================
    // PerformanceTree - Main tree structure
    // ========================================================================

    py::class_<PerformanceTree>(m, "PerformanceTree",
        "Aggregates call stack samples into a tree structure")
        .def(py::init<TreeBuildMode, SampleCountMode, ConcurrencyModel>(),
            py::arg("mode") = TreeBuildMode::kContextFree,
            py::arg("count_mode") = SampleCountMode::kExclusive,
            py::arg("concurrency_model") = ConcurrencyModel::kSerial,
            "Create a new performance tree")
        
        // Basic properties
        .def_property_readonly("root", &PerformanceTree::root,
            "Get the root node")
        .def_property("process_count",
            &PerformanceTree::process_count,
            &PerformanceTree::set_process_count,
            "Number of processes")
        .def_property_readonly("total_samples", &PerformanceTree::total_samples,
            "Total number of samples in the tree")
        .def_property_readonly("build_mode", &PerformanceTree::build_mode,
            "Tree build mode (ContextFree or ContextAware)")
        .def_property_readonly("sample_count_mode", &PerformanceTree::sample_count_mode,
            "Sample count mode")
        
        // Concurrency configuration
        .def_property_readonly("concurrency_model", &PerformanceTree::concurrency_model,
            "Concurrency model for parallel tree building")
        .def("set_concurrency_model", &PerformanceTree::set_concurrency_model,
            py::arg("model"),
            "Set the concurrency model (must be called before inserting)")
        .def_property("num_threads",
            &PerformanceTree::num_threads,
            &PerformanceTree::set_num_threads,
            "Number of threads for parallel operations")
        .def("consolidate_atomic_counters", &PerformanceTree::consolidate_atomic_counters,
            "Consolidate atomic counters after parallel insertion (for lock-free model)")
        
        // Tree operations
        .def("clear", &PerformanceTree::clear,
            "Clear all data from the tree")
        .def("insert_call_stack", &PerformanceTree::insert_call_stack,
            py::arg("frames"),
            py::arg("process_id"),
            py::arg("count") = 1,
            py::arg("time_us") = 0.0,
            "Insert a call stack into the tree")
        
        // Tree statistics
        .def_property_readonly("node_count", &PerformanceTree::node_count,
            "Total number of nodes in the tree")
        .def_property_readonly("max_depth", &PerformanceTree::max_depth,
            "Maximum depth of the tree")
        
        // Node access
        .def_property_readonly("all_nodes", &PerformanceTree::all_nodes,
            "Get all nodes in the tree")
        .def_property_readonly("leaf_nodes", &PerformanceTree::leaf_nodes,
            "Get all leaf nodes")
        .def("nodes_at_depth", &PerformanceTree::nodes_at_depth,
            py::arg("depth"),
            "Get all nodes at a specific depth")
        
        // Search and filter
        .def("find_nodes_by_name", &PerformanceTree::find_nodes_by_name,
            py::arg("func_name"),
            "Find all nodes matching a function name")
        .def("find_nodes_by_library", &PerformanceTree::find_nodes_by_library,
            py::arg("lib_name"),
            "Find all nodes in a specific library")
        .def("filter_by_samples", &PerformanceTree::filter_by_samples,
            py::arg("min_samples"),
            "Get nodes with samples above a threshold")
        .def("filter_by_self_samples", &PerformanceTree::filter_by_self_samples,
            py::arg("min_self_samples"),
            "Get nodes with self samples above a threshold")
        
        // Traversal
        .def("traverse_preorder", 
            [](const PerformanceTree& tree, 
               std::function<bool(std::shared_ptr<TreeNode>)> visitor) {
                tree.traverse_preorder(visitor);
            },
            py::arg("visitor"),
            "Traverse tree in pre-order (depth-first, parent before children)")
        .def("traverse_postorder",
            [](const PerformanceTree& tree,
               std::function<bool(std::shared_ptr<TreeNode>)> visitor) {
                tree.traverse_postorder(visitor);
            },
            py::arg("visitor"),
            "Traverse tree in post-order (depth-first, children before parent)")
        .def("traverse_levelorder",
            [](const PerformanceTree& tree,
               std::function<bool(std::shared_ptr<TreeNode>)> visitor) {
                tree.traverse_levelorder(visitor);
            },
            py::arg("visitor"),
            "Traverse tree level-by-level (breadth-first)")
        
        .def("__repr__", [](const PerformanceTree& t) {
            return "<PerformanceTree nodes=" + std::to_string(t.node_count()) +
                   " samples=" + std::to_string(t.total_samples()) + ">";
        });

    // ========================================================================
    // TreeBuilder - Build trees from sample files
    // ========================================================================

    py::class_<TreeBuilder>(m, "TreeBuilder",
        "Constructs performance trees from sample data files")
        .def(py::init<TreeBuildMode, SampleCountMode, ConcurrencyModel>(),
            py::arg("mode") = TreeBuildMode::kContextFree,
            py::arg("count_mode") = SampleCountMode::kExclusive,
            py::arg("concurrency_model") = ConcurrencyModel::kSerial,
            "Create a new tree builder")
        
        .def_property_readonly("tree", 
            py::overload_cast<>(&TreeBuilder::tree),
            py::return_value_policy::reference,
            "Get the performance tree")
        .def_property_readonly("build_mode", &TreeBuilder::build_mode,
            "Get the build mode")
        .def_property_readonly("sample_count_mode", &TreeBuilder::sample_count_mode,
            "Get the sample count mode")
        .def_property_readonly("concurrency_model", &TreeBuilder::concurrency_model,
            "Get the concurrency model")
        
        .def("set_build_mode", &TreeBuilder::set_build_mode,
            py::arg("mode"),
            "Set the tree build mode (must be called before building)")
        .def("set_sample_count_mode", &TreeBuilder::set_sample_count_mode,
            py::arg("mode"),
            "Set the sample count mode (must be called before building)")
        .def("set_concurrency_model", &TreeBuilder::set_concurrency_model,
            py::arg("model"),
            "Set the concurrency model (must be called before building)")
        .def("set_num_threads", &TreeBuilder::set_num_threads,
            py::arg("num_threads"),
            "Set the number of threads for parallel operations")
        .def_property_readonly("num_threads", &TreeBuilder::num_threads,
            "Get the number of threads for parallel operations")
        
        .def("build_from_file", 
            &TreeBuilder::build_from_file<>,
            py::arg("sample_file"),
            py::arg("process_id"),
            py::arg("time_per_sample") = 1000.0,
            "Build tree from a single sample data file")
        
        .def("build_from_files",
            &TreeBuilder::build_from_files<>,
            py::arg("sample_files"),
            py::arg("time_per_sample") = 1000.0,
            "Build tree from multiple sample files (list of (path, process_id) tuples)")
        
        .def("build_from_files_parallel",
            &TreeBuilder::build_from_files_parallel<>,
            py::arg("sample_files"),
            py::arg("time_per_sample") = 1000.0,
            "Build tree from multiple sample files in parallel using the configured concurrency model")
        
        .def("load_library_maps", &TreeBuilder::load_library_maps,
            py::arg("libmap_files"),
            "Load library maps for address resolution (list of (path, process_id) tuples)")
        
        .def("clear", &TreeBuilder::clear,
            "Clear all data")
        
        .def("__repr__", [](const TreeBuilder& b) {
            return "<TreeBuilder samples=" + 
                   std::to_string(b.tree().total_samples()) + ">";
        });

    // ========================================================================
    // Analysis Results
    // ========================================================================

    py::class_<BalanceAnalysisResult>(m, "BalanceAnalysisResult",
        "Contains workload balance information")
        .def_readonly("mean_samples", &BalanceAnalysisResult::mean_samples,
            "Mean samples per process")
        .def_readonly("std_dev_samples", &BalanceAnalysisResult::std_dev_samples,
            "Standard deviation of samples")
        .def_readonly("min_samples", &BalanceAnalysisResult::min_samples,
            "Minimum samples across processes")
        .def_readonly("max_samples", &BalanceAnalysisResult::max_samples,
            "Maximum samples across processes")
        .def_readonly("imbalance_factor", &BalanceAnalysisResult::imbalance_factor,
            "Imbalance factor (max - min) / mean")
        .def_readonly("most_loaded_process", &BalanceAnalysisResult::most_loaded_process,
            "Process with most samples")
        .def_readonly("least_loaded_process", &BalanceAnalysisResult::least_loaded_process,
            "Process with least samples")
        .def_readonly("process_samples", &BalanceAnalysisResult::process_samples,
            "Per-process sample counts")
        .def("__repr__", [](const BalanceAnalysisResult& r) {
            return "<BalanceAnalysisResult imbalance=" + 
                   std::to_string(r.imbalance_factor) + ">";
        });

    py::class_<HotspotInfo>(m, "HotspotInfo",
        "Describes a performance hotspot")
        .def_readonly("function_name", &HotspotInfo::function_name,
            "Function name")
        .def_readonly("library_name", &HotspotInfo::library_name,
            "Library name")
        .def_readonly("source_location", &HotspotInfo::source_location,
            "Source file location")
        .def_readonly("total_samples", &HotspotInfo::total_samples,
            "Total/inclusive samples")
        .def_readonly("percentage", &HotspotInfo::percentage,
            "Percentage of total samples")
        .def_readonly("self_samples", &HotspotInfo::self_samples,
            "Self/exclusive samples")
        .def_readonly("self_percentage", &HotspotInfo::self_percentage,
            "Percentage of self samples")
        .def("__repr__", [](const HotspotInfo& h) {
            return "<HotspotInfo function='" + h.function_name + 
                   "' self=" + std::to_string(h.self_percentage) + "%>";
        });

    // ========================================================================
    // Analysis Functions
    // ========================================================================

    py::class_<BalanceAnalyzer>(m, "BalanceAnalyzer",
        "Analyzes workload distribution across processes")
        .def_static("analyze", &BalanceAnalyzer::analyze,
            py::arg("tree"),
            "Analyze workload balance in a performance tree");

    py::class_<HotspotAnalyzer>(m, "HotspotAnalyzer",
        "Identifies performance bottlenecks")
        .def_static("find_hotspots", &HotspotAnalyzer::find_hotspots,
            py::arg("tree"),
            py::arg("top_n") = 10,
            "Find top hotspots by self time (exclusive)")
        .def_static("find_self_hotspots", &HotspotAnalyzer::find_self_hotspots,
            py::arg("tree"),
            py::arg("top_n") = 10,
            "Find hotspots by self samples (alias for find_hotspots)")
        .def_static("find_total_hotspots", &HotspotAnalyzer::find_total_hotspots,
            py::arg("tree"),
            py::arg("top_n") = 10,
            "Find hotspots by total samples (inclusive)");

    // ========================================================================
    // Parallel File Reader
    // ========================================================================

    py::class_<ParallelFileReader::FileReadResult>(m, "FileReadResult",
        "Status of a file read operation")
        .def_readonly("filepath", &ParallelFileReader::FileReadResult::filepath)
        .def_readonly("process_id", &ParallelFileReader::FileReadResult::process_id)
        .def_readonly("success", &ParallelFileReader::FileReadResult::success)
        .def_readonly("samples_read", &ParallelFileReader::FileReadResult::samples_read)
        .def_readonly("error_message", &ParallelFileReader::FileReadResult::error_message)
        .def("__repr__", [](const ParallelFileReader::FileReadResult& r) {
            return "<FileReadResult path='" + r.filepath + 
                   "' success=" + (r.success ? "true" : "false") + ">";
        });

    py::class_<ParallelFileReader>(m, "ParallelFileReader",
        "Provides parallel file reading for sample data")
        .def(py::init<size_t>(),
            py::arg("num_threads") = 0,
            "Create parallel file reader (0 = auto-detect thread count)")
        .def_property_readonly("thread_count", &ParallelFileReader::thread_count,
            "Number of threads used")
        .def("set_progress_callback", &ParallelFileReader::set_progress_callback,
            py::arg("callback"),
            "Set progress callback function(completed, total)")
        .def("read_files_parallel",
            &ParallelFileReader::read_files_parallel<>,
            py::arg("sample_files"),
            py::arg("tree"),
            py::arg("converter"),
            py::arg("time_per_sample") = 1000.0,
            "Read multiple sample files in parallel");

    // ========================================================================
    // Offset Converter (basic binding for parallel reader)
    // ========================================================================

    py::class_<OffsetConverter>(m, "OffsetConverter",
        "Converts raw call stack addresses to resolved frames")
        .def(py::init<>())
        .def("has_snapshot", &OffsetConverter::has_snapshot,
            py::arg("id"),
            "Check if a snapshot exists for a given process ID")
        .def_property_readonly("snapshot_count", &OffsetConverter::snapshot_count,
            "Number of map snapshots stored")
        .def("clear", &OffsetConverter::clear,
            "Clear all map snapshots");

    // ========================================================================
    // Module-level utility functions
    // ========================================================================

    m.def("version", []() {
        return "0.1.0";
    }, "Get PerFlow version");
}
