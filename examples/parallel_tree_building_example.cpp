// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

/// @file parallel_tree_building_example.cpp
/// @brief Example demonstrating parallel tree building with different concurrency models
///
/// This example shows how to:
/// 1. Use different concurrency models for tree building
/// 2. Build performance trees from sample files in parallel
/// 3. Compare performance of different concurrency models
/// 4. Configure thread count for parallel operations
///
/// Three concurrency models are available:
/// - Serial: Single global lock (default, safest)
/// - FineGrainedLock: Per-node locks for parallel subtree insertion
/// - ThreadLocalMerge: Thread-local trees merged after construction
/// - LockFree: Atomic operations for counters, locks for structural changes

#include <chrono>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

#include "analysis/performance_tree.h"
#include "analysis/tree_builder.h"
#include "sampling/call_stack.h"

using namespace perflow::analysis;
using namespace perflow::sampling;

/// Helper function to create sample call stack frames
std::vector<ResolvedFrame> create_sample_frames(const std::string& leaf_name,
                                                 size_t depth = 3) {
    std::vector<ResolvedFrame> frames;
    
    // Create a call chain: main -> intermediate -> leaf
    ResolvedFrame main_frame;
    main_frame.function_name = "main";
    main_frame.library_name = "app";
    main_frame.offset = 0x1000;
    frames.push_back(main_frame);
    
    for (size_t i = 1; i < depth - 1; ++i) {
        ResolvedFrame intermediate;
        intermediate.function_name = "intermediate_" + std::to_string(i);
        intermediate.library_name = "app";
        intermediate.offset = 0x2000 + i * 0x100;
        frames.push_back(intermediate);
    }
    
    ResolvedFrame leaf_frame;
    leaf_frame.function_name = leaf_name;
    leaf_frame.library_name = "app";
    leaf_frame.offset = 0x3000;
    frames.push_back(leaf_frame);
    
    return frames;
}

/// Example 1: Serial tree building (default)
void example_serial_tree_building() {
    std::cout << "=== Example 1: Serial Tree Building ===\n\n";
    
    // Create tree with Serial concurrency model (default)
    PerformanceTree tree(TreeBuildMode::kContextFree, SampleCountMode::kBoth,
                         ConcurrencyModel::kSerial);
    tree.set_process_count(4);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Insert call stacks serially
    for (size_t pid = 0; pid < 4; ++pid) {
        for (int i = 0; i < 1000; ++i) {
            auto frames = create_sample_frames("worker_" + std::to_string(pid));
            tree.insert_call_stack(frames, pid, 1, 10.0);
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "  Concurrency Model: Serial\n";
    std::cout << "  Total Samples: " << tree.total_samples() << "\n";
    std::cout << "  Tree Nodes: " << tree.node_count() << "\n";
    std::cout << "  Time: " << duration.count() << " ms\n\n";
}

/// Example 2: Fine-grained lock tree building
void example_fine_grained_lock_tree_building() {
    std::cout << "=== Example 2: Fine-Grained Lock Tree Building ===\n\n";
    
    // Create tree with FineGrainedLock concurrency model
    PerformanceTree tree(TreeBuildMode::kContextFree, SampleCountMode::kBoth,
                         ConcurrencyModel::kFineGrainedLock);
    tree.set_process_count(4);
    tree.set_num_threads(4);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Insert call stacks from multiple threads
    std::vector<std::thread> threads;
    for (size_t pid = 0; pid < 4; ++pid) {
        threads.emplace_back([&tree, pid]() {
            for (int i = 0; i < 1000; ++i) {
                auto frames = create_sample_frames("worker_" + std::to_string(pid));
                tree.insert_call_stack(frames, pid, 1, 10.0);
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "  Concurrency Model: FineGrainedLock\n";
    std::cout << "  Threads Used: " << tree.num_threads() << "\n";
    std::cout << "  Total Samples: " << tree.total_samples() << "\n";
    std::cout << "  Tree Nodes: " << tree.node_count() << "\n";
    std::cout << "  Time: " << duration.count() << " ms\n\n";
}

/// Example 3: Thread-local merge tree building
void example_thread_local_merge_tree_building() {
    std::cout << "=== Example 3: Thread-Local Merge Tree Building ===\n\n";
    
    // Create tree with ThreadLocalMerge concurrency model
    PerformanceTree tree(TreeBuildMode::kContextFree, SampleCountMode::kBoth,
                         ConcurrencyModel::kThreadLocalMerge);
    tree.set_process_count(4);
    tree.set_num_threads(4);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Create thread-local trees and populate them
    std::vector<std::unique_ptr<PerformanceTree>> local_trees;
    std::vector<std::thread> threads;
    
    for (size_t pid = 0; pid < 4; ++pid) {
        local_trees.push_back(tree.create_thread_local_tree());
    }
    
    // Insert into thread-local trees (no contention)
    for (size_t pid = 0; pid < 4; ++pid) {
        threads.emplace_back([&local_trees, pid]() {
            for (int i = 0; i < 1000; ++i) {
                auto frames = create_sample_frames("worker_" + std::to_string(pid));
                local_trees[pid]->insert_call_stack(frames, pid, 1, 10.0);
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // Merge thread-local trees into main tree
    for (auto& local_tree : local_trees) {
        tree.merge_thread_local_tree(*local_tree);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "  Concurrency Model: ThreadLocalMerge\n";
    std::cout << "  Threads Used: " << tree.num_threads() << "\n";
    std::cout << "  Total Samples: " << tree.total_samples() << "\n";
    std::cout << "  Tree Nodes: " << tree.node_count() << "\n";
    std::cout << "  Time: " << duration.count() << " ms\n\n";
}

/// Example 4: Lock-free tree building
void example_lock_free_tree_building() {
    std::cout << "=== Example 4: Lock-Free Tree Building ===\n\n";
    
    // Create tree with LockFree concurrency model
    PerformanceTree tree(TreeBuildMode::kContextFree, SampleCountMode::kBoth,
                         ConcurrencyModel::kLockFree);
    tree.set_process_count(4);
    tree.set_num_threads(4);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Insert call stacks from multiple threads
    std::vector<std::thread> threads;
    for (size_t pid = 0; pid < 4; ++pid) {
        threads.emplace_back([&tree, pid]() {
            for (int i = 0; i < 1000; ++i) {
                auto frames = create_sample_frames("worker_" + std::to_string(pid));
                tree.insert_call_stack(frames, pid, 1, 10.0);
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // Important: Consolidate atomic counters after parallel insertion
    tree.consolidate_atomic_counters();
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "  Concurrency Model: LockFree\n";
    std::cout << "  Threads Used: " << tree.num_threads() << "\n";
    std::cout << "  Total Samples: " << tree.total_samples() << "\n";
    std::cout << "  Tree Nodes: " << tree.node_count() << "\n";
    std::cout << "  Time: " << duration.count() << " ms\n\n";
}

/// Example 5: Using TreeBuilder with concurrency models
void example_tree_builder_parallel() {
    std::cout << "=== Example 5: TreeBuilder with Concurrency Models ===\n\n";
    
    // Create TreeBuilder with FineGrainedLock model
    TreeBuilder builder(TreeBuildMode::kContextFree, SampleCountMode::kBoth,
                        ConcurrencyModel::kFineGrainedLock);
    builder.set_num_threads(4);
    builder.tree().set_process_count(4);
    
    std::cout << "  TreeBuilder Configuration:\n";
    std::cout << "    Build Mode: ContextFree\n";
    std::cout << "    Sample Count Mode: Both\n";
    std::cout << "    Concurrency Model: FineGrainedLock\n";
    std::cout << "    Threads: " << builder.num_threads() << "\n\n";
    
    // Simulate building from multiple sources
    std::vector<std::thread> threads;
    for (size_t pid = 0; pid < 4; ++pid) {
        threads.emplace_back([&builder, pid]() {
            for (int i = 0; i < 500; ++i) {
                auto frames = create_sample_frames("worker_" + std::to_string(pid));
                builder.tree().insert_call_stack(frames, pid, 1, 10.0);
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    std::cout << "  Results:\n";
    std::cout << "    Total Samples: " << builder.tree().total_samples() << "\n";
    std::cout << "    Tree Nodes: " << builder.tree().node_count() << "\n";
    std::cout << "    Max Depth: " << builder.tree().max_depth() << "\n\n";
}

/// Example 6: Benchmarking different concurrency models
void example_benchmark_comparison() {
    std::cout << "=== Example 6: Concurrency Model Benchmark ===\n\n";
    
    const size_t num_processes = 8;
    const int samples_per_process = 5000;
    
    std::cout << "Benchmark Configuration:\n";
    std::cout << "  Processes: " << num_processes << "\n";
    std::cout << "  Samples per process: " << samples_per_process << "\n";
    std::cout << "  Total samples: " << num_processes * samples_per_process << "\n\n";
    
    std::cout << std::setw(20) << "Model" 
              << std::setw(15) << "Time (ms)"
              << std::setw(15) << "Samples"
              << std::setw(15) << "Nodes" << "\n";
    std::cout << std::string(65, '-') << "\n";
    
    // Benchmark each model
    ConcurrencyModel models[] = {
        ConcurrencyModel::kSerial,
        ConcurrencyModel::kFineGrainedLock,
        ConcurrencyModel::kThreadLocalMerge,
        ConcurrencyModel::kLockFree
    };
    
    const char* model_names[] = {
        "Serial",
        "FineGrainedLock",
        "ThreadLocalMerge",
        "LockFree"
    };
    
    for (size_t m = 0; m < 4; ++m) {
        PerformanceTree tree(TreeBuildMode::kContextFree, SampleCountMode::kBoth,
                             models[m]);
        tree.set_process_count(num_processes);
        tree.set_num_threads(num_processes);
        
        auto start = std::chrono::high_resolution_clock::now();
        
        if (models[m] == ConcurrencyModel::kThreadLocalMerge) {
            // Thread-local merge workflow
            std::vector<std::unique_ptr<PerformanceTree>> local_trees;
            for (size_t i = 0; i < num_processes; ++i) {
                local_trees.push_back(tree.create_thread_local_tree());
            }
            
            std::vector<std::thread> threads;
            for (size_t pid = 0; pid < num_processes; ++pid) {
                threads.emplace_back([&local_trees, pid, samples_per_process]() {
                    for (int i = 0; i < samples_per_process; ++i) {
                        auto frames = create_sample_frames("worker_" + std::to_string(pid % 4));
                        local_trees[pid]->insert_call_stack(frames, pid, 1, 10.0);
                    }
                });
            }
            
            for (auto& t : threads) {
                t.join();
            }
            
            for (auto& local_tree : local_trees) {
                tree.merge_thread_local_tree(*local_tree);
            }
        } else if (models[m] == ConcurrencyModel::kSerial) {
            // Serial - no parallelism
            for (size_t pid = 0; pid < num_processes; ++pid) {
                for (int i = 0; i < samples_per_process; ++i) {
                    auto frames = create_sample_frames("worker_" + std::to_string(pid % 4));
                    tree.insert_call_stack(frames, pid, 1, 10.0);
                }
            }
        } else {
            // FineGrainedLock or LockFree
            std::vector<std::thread> threads;
            for (size_t pid = 0; pid < num_processes; ++pid) {
                threads.emplace_back([&tree, pid, samples_per_process]() {
                    for (int i = 0; i < samples_per_process; ++i) {
                        auto frames = create_sample_frames("worker_" + std::to_string(pid % 4));
                        tree.insert_call_stack(frames, pid, 1, 10.0);
                    }
                });
            }
            
            for (auto& t : threads) {
                t.join();
            }
            
            if (models[m] == ConcurrencyModel::kLockFree) {
                tree.consolidate_atomic_counters();
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << std::setw(20) << model_names[m]
                  << std::setw(15) << duration.count()
                  << std::setw(15) << tree.total_samples()
                  << std::setw(15) << tree.node_count() << "\n";
    }
    
    std::cout << "\n";
}

int main() {
    std::cout << "PerFlow Parallel Tree Building Example\n";
    std::cout << "======================================\n\n";
    
    std::cout << "Hardware concurrency: " << std::thread::hardware_concurrency() << " threads\n\n";
    
    example_serial_tree_building();
    example_fine_grained_lock_tree_building();
    example_thread_local_merge_tree_building();
    example_lock_free_tree_building();
    example_tree_builder_parallel();
    example_benchmark_comparison();
    
    std::cout << "=== Summary ===\n\n";
    std::cout << "Concurrency Model Selection Guide:\n";
    std::cout << "  - Serial: Best for small datasets or debugging\n";
    std::cout << "  - FineGrainedLock: Good for deep trees with many distinct paths\n";
    std::cout << "  - ThreadLocalMerge: Best for processing many independent files\n";
    std::cout << "  - LockFree: Good when many threads update the same nodes\n\n";
    
    return 0;
}
