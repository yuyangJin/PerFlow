# Concurrency Models for Parallel Tree Building

PerFlow supports multiple concurrency models for parallel tree construction, allowing users to choose the most efficient approach for their specific workload characteristics.

## Overview

When building performance trees from multiple sample files in parallel, the concurrent access to shared tree nodes can cause memory access conflicts. PerFlow provides three concurrency models to handle this:

1. **Serial** - Single global lock (default, safest)
2. **Fine-Grained Lock** - Per-node locking
3. **Thread-Local Merge** - Thread-local trees merged after construction
4. **Lock-Free** - Atomic operations with minimal locking

## Concurrency Models

### 1. Serial Mode (`ConcurrencyModel::kSerial`)

**Description:**
- Uses a single global lock for all tree operations
- Simple and safe, but limits scalability
- No parallelism during tree construction

**When to use:**
- Small datasets where parallelization overhead exceeds benefits
- Debugging or when you need deterministic behavior
- When correctness verification is required

**Expected cost:** O(1) lock per operation, but no parallelism

**Example (C++):**
```cpp
PerformanceTree tree(TreeBuildMode::kContextFree, 
                     SampleCountMode::kBoth,
                     ConcurrencyModel::kSerial);
```

**Example (Python):**
```python
from perflow import TreeBuilder, ConcurrencyModel

builder = TreeBuilder(concurrency_model=ConcurrencyModel.Serial)
```

### 2. Fine-Grained Lock Mode (`ConcurrencyModel::kFineGrainedLock`)

**Description:**
- Each tree node has its own mutex
- Nodes are locked individually during updates
- Allows parallel subtree insertion
- Multiple threads can work on different parts of the tree simultaneously

**When to use:**
- Deep trees with many distinct call paths
- Workloads where different threads tend to access different subtrees
- When you need good parallelism with strong consistency

**Expected cost:** O(depth) locks per operation

**Advantages:**
- Good parallelism when threads access different parts of the tree
- Strong consistency guarantees
- Allows fine-grained concurrent access

**Disadvantages:**
- Lock acquisition overhead at each level
- Potential for deadlocks if not careful (PerFlow handles this)
- Memory overhead for per-node mutexes

**Example (C++):**
```cpp
PerformanceTree tree(TreeBuildMode::kContextFree,
                     SampleCountMode::kBoth,
                     ConcurrencyModel::kFineGrainedLock);
tree.set_num_threads(8);

// Concurrent insertions from multiple threads work safely
std::vector<std::thread> threads;
for (int i = 0; i < 8; ++i) {
    threads.emplace_back([&tree, i]() {
        // Insert call stacks concurrently
        tree.insert_call_stack(frames, i, count, time);
    });
}
for (auto& t : threads) t.join();
```

### 3. Thread-Local Merge Mode (`ConcurrencyModel::kThreadLocalMerge`)

**Description:**
- Each thread builds a separate independent tree
- No contention during the build phase
- Trees are merged into the main tree after construction
- Best for workloads where threads process independent files

**When to use:**
- Processing many independent sample files
- When file-level parallelism is sufficient
- When minimizing contention is more important than memory

**Expected cost:** O(1) during build + O(nodes) for single-threaded merge

**Advantages:**
- Zero contention during build phase
- Highest throughput during parallel construction
- Simple to reason about

**Disadvantages:**
- Higher memory usage (multiple trees in memory)
- Merge phase is single-threaded
- Best for file-level parallelism, not within-file parallelism

**Example (C++):**
```cpp
TreeBuilder builder(TreeBuildMode::kContextFree,
                    SampleCountMode::kBoth,
                    ConcurrencyModel::kThreadLocalMerge);
builder.set_num_threads(8);

// Each file processed independently, then merged
builder.build_from_files_parallel(sample_files, time_per_sample);
```

**Example (Python):**
```python
from perflow import TreeBuilder, ConcurrencyModel

builder = TreeBuilder(concurrency_model=ConcurrencyModel.ThreadLocalMerge)
builder.set_num_threads(8)
builder.build_from_files_parallel(sample_files)
```

### 4. Lock-Free Mode (`ConcurrencyModel::kLockFree`)

**Description:**
- Uses atomic operations for counter updates
- Only uses locks for structural changes (adding new nodes)
- Minimizes contention on existing nodes
- Must call `consolidate_atomic_counters()` after parallel phase

**When to use:**
- Many updates to the same nodes from different threads
- Read-heavy workloads during build phase
- When counter updates dominate over structural changes

**Expected cost:** Lock-free counter updates, locks only for new nodes

**Advantages:**
- Minimal contention for counter updates
- Good performance when many threads update same nodes
- Lower latency for individual operations

**Disadvantages:**
- Requires consolidation step after parallel phase
- More complex memory model
- Atomic operations have some overhead

**Example (C++):**
```cpp
PerformanceTree tree(TreeBuildMode::kContextFree,
                     SampleCountMode::kBoth,
                     ConcurrencyModel::kLockFree);

// Parallel insertions
// ... (concurrent inserts) ...

// After all parallel work is done, consolidate counters
tree.consolidate_atomic_counters();
```

## Choosing the Right Model

| Model | Best For | Scalability | Memory | Complexity |
|-------|----------|-------------|--------|------------|
| Serial | Small datasets, debugging | None | Low | Simple |
| Fine-Grained Lock | Deep trees, different paths | Good | Medium | Medium |
| Thread-Local Merge | Many independent files | Excellent | High | Simple |
| Lock-Free | Same-node updates | Good | Medium | Complex |

### Decision Tree

1. **Is your dataset small?** → Use `Serial`
2. **Are you processing many independent files?** → Use `ThreadLocalMerge`
3. **Do threads access mostly different tree regions?** → Use `FineGrainedLock`
4. **Do threads frequently update the same nodes?** → Use `LockFree`

## Configuration

### Setting Thread Count

```cpp
// C++
tree.set_num_threads(8);
builder.set_num_threads(8);
```

```python
# Python
tree.num_threads = 8
builder.set_num_threads(8)
```

### Dataflow Framework Configuration

```python
from perflow.dataflow import WorkflowBuilder

results = (
    WorkflowBuilder("ParallelAnalysis")
    .load_data(
        sample_files=files,
        concurrency_model='FineGrainedLock',
        num_threads=8
    )
    .find_hotspots(top_n=10)
    .execute()
)
```

## Performance Considerations

1. **Thread count**: Setting `num_threads` to 0 uses hardware concurrency (usually best)

2. **NUMA effects**: For large datasets, consider NUMA-aware allocation

3. **Contention monitoring**: Profile to understand where contention occurs

4. **Memory bandwidth**: Thread-local merge uses more memory but avoids shared cache lines

## Thread Safety Notes

- All concurrency models are designed to be thread-safe
- The `Serial` model provides the strongest guarantees
- After using `LockFree` mode, always call `consolidate_atomic_counters()` before reading results
- The `ThreadLocalMerge` mode's merge operation is NOT thread-safe - only one thread should call merge operations

## Backward Compatibility

- Default concurrency model is `Serial`, maintaining backward compatibility
- Existing code continues to work without modification
- New concurrent features are opt-in
