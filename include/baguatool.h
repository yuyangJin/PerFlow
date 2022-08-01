#ifndef BAGUATOOL_H_
#define BAGUATOOL_H_
#include <fstream>
#include <map>
#include <memory>
#include "nlohmann/json.hpp"
#include <stack>
#include <string>
#include <tuple>
#include <unordered_set>
#include <vector>
#include "common/common.h"
#include "common/tprintf.h"

using namespace nlohmann;

// All public APIs are defined here.

namespace baguatool {

namespace type {
class graph_t;                         /**<igragh-graph wrapper */
class vertex_set_t;                    /**<igraph_vs_t wrapper */
typedef int vertex_t;                  /**<vertex id type (int)*/
typedef int edge_t;                    /**<edge id type (int)*/
typedef unsigned long long int addr_t; /**<address type (unsigned long long int)*/
typedef double num_t;
typedef double perf_data_t;                   /**<performance data type (double) */
typedef std::stack<type::addr_t> call_path_t; /**<call path type (need to modify)*/
typedef int thread_t;
typedef int procs_t;

enum vertex_type_t {
  BB_NODE = -12,
  INST_NODE = -11,
  ADDR_NODE = -10,
  // COMPUTE_NODE = -9,
  // COMBINE = -8,
  CALL_IND_NODE = -7,
  CALL_REC_NODE = -6,
  CALL_NODE = -5,
  FUNC_NODE = -4,
  // COMPOUND = -3,
  BRANCH_NODE = -2,
  LOOP_NODE = -1,
  MPI_NODE = 1
};

enum edge_type_t { STA_CALL_EDGE = -102, DYN_CALL_EDGE = -101, NONE_EDGE_TYPE = -100 };

struct addr_debug_info_t {
  type::addr_t addr;
  std::string file_name;
  std::string func_name;
  int line_num;
  bool is_executable;

  type::addr_t GetAddress() { return this->addr; }
  std::string& GetFileName() { return this->file_name; }
  std::string& GetFuncName() { return this->func_name; }
  int GetLineNum() { return this->line_num; }

  void SetAddress(type::addr_t addr) { this->addr = addr; }
  void SetFileName(std::string& file_name) { this->file_name = std::string(file_name); }
  void SetFuncName(std::string& func_name) { this->func_name = std::string(func_name); }
  void SetLineNum(int line_num) { this->line_num = line_num; }
  void SetIsExecutableFlag(bool flag) {this->is_executable = flag;}

  bool IsExecutable(){return this->is_executable;}
};

/** Determine whether an address is in .text segment.
 * @param addr - input address.
 * @return true stands for the input address is in .text segment while false means it is not.
*/
bool IsTextAddr(type::addr_t addr);

/** Determine whether an address is in .dynamic segment.
 * @param addr - input address.
 * @return true stands for the input address is in .dynamic segment while false means it is not.
*/
bool IsDynAddr(type::addr_t addr);

/** Determine whether an address is a valid address.
 * @param addr - input address.
 * @return true stands for the input address is a valid address while false means it is not.
*/
bool IsValidAddr(type::addr_t addr);
}

namespace core {

// class type::graph_t;
// class baguatool::type::vertex_set_t;
// typedef int type::vertex_t;
// typedef int type::edge_t;

class GraphPerfData {
 private:
  json j_perf_data; /**<[json format] Record performance data in a graph*/

 public:
  /** Default Constructor
   */
  GraphPerfData();
  /** Default Destructor
   */
  ~GraphPerfData();

  /** Read json file (Input)
   * @param file_name - name of input file
   */
  void Read(std::string& file_name);

  /** Dump as json file (Output)
   * @param file_name - name of output file
   */
  void Dump(std::string& file_name);

  /** Record a piece of data of a specific vertex, including metric, process id, thread id, and value.
   * @param vertex_id - id of the specific vertex
   * @param metric - metric name of this value
   * @param procs_id - process id
   * @param thread_id - thread id
   * @param value - value
   */
  void SetPerfData(type::vertex_t vertex_id, std::string& metric, int procs_id, int thread_id,
                   baguatool::type::perf_data_t value);

  /** Query the value of a piece of data of a specific vertex through metric, process id, thread id.
   * @param vertex_id - id of the specific vertex
   * @param metric - metric name of this value
   * @param procs_id - process id
   * @param thread_id - thread id
   * @return value
   */
  baguatool::type::perf_data_t GetPerfData(type::vertex_t vertex_id, std::string& metric, int procs_id, int thread_id);

  /** Query if a specific vertex has the input metric
   * @param vertex_id - id of the specific vertex
   * @param metric - metric name
   * @return true for existance, false for
   */
  bool HasMetric(type::vertex_t vertex_id, std::string& metric);

  /** Query metric list of
   */
  void GetVertexPerfDataMetrics(type::vertex_t vertex_id, std::vector<std::string>&);
  int GetMetricsPerfDataProcsNum(type::vertex_t vertex_id, std::string& metric, std::vector<type::procs_t>& procs_list);
  int GetProcsPerfDataThreadNum(type::vertex_t vertex_id, std::string& metric, int procs_id,
                                std::vector<type::thread_t>& thread_vec);
  void GetProcsPerfData(type::vertex_t vertex_id, std::string& metric, int procs_id,
                        std::map<type::thread_t, type::perf_data_t>& proc_perf_data);
  void SetProcsPerfData(type::vertex_t vertex_id, std::string& metric, int procs_id,
                        std::map<type::thread_t, type::perf_data_t>& proc_perf_data);

  void EraseProcsPerfData(type::vertex_t vertex_id, std::string& metric, int procs_id);
};  // class GraphPerfData

/** @brief Wrapper class of igraph. Provide basic graph operations.
    @author Yuyang Jin, PACMAN, Tsinghua University
    @date March 2021
    */
class Graph {
 protected:
  std::unique_ptr<type::graph_t> ipag_; /**<igraph_t wrapper struct */
  int cur_vertex_num;                   /**<initial the number of vertices in this graph */
  GraphPerfData* graph_perf_data;       /**<performance data in a graph*/

 public:
  /** Constructor. Create an graph and enable graph attributes.
   */
  Graph();

  /** Destructor. Destroy graph.
   */
  ~Graph();

  /** Initialize the graph. Build a directed graph with zero vertices and zero edges. Set name of the graph with the
   * input parameter.
   * @param graph_name - name of the graph
   */
  void GraphInit(const char* graph_name);

  /** Create a vertex in the graph.
   * @return id of the new vertex
   */
  type::vertex_t AddVertex();

  /** Create an edge in the graph.
   * @param src_vertex_id - id of the source vertex of the edge
   * @param dest_vertex_id - id of the destination vertex of the edge
   * @return id of the new edge
   */
  type::edge_t AddEdge(const type::vertex_t src_vertex_id, const type::vertex_t dest_vertex_id);

  /** Append a graph to the graph. Copy all the vertices and edges (and all their attributes) of a graph to this graph.
   * @param g - the graph to be appended
   * @return id of entry vertex of the appended new graph (not id in old graph)
   */
  type::vertex_t AddGraph(Graph* g);

  /** Delete a vertex.
   * @param vertex_id - id of vertex to be removed
   */
  void DeleteVertex(type::vertex_t vertex_id);

  /** Delete a set of vertex.
   * @param vs - set of vertices to be deleted
   */
  void DeleteVertices(baguatool::type::vertex_set_t* vs);

  /** Delete extra vertices at the end of vertices. (No need to expose to developers)
   */
  void DeleteExtraTailVertices();

  /** Delete an edge.
   * @param src_vertex_id - id of the source vertex of the edge to be removed
   * @param dest_vertex_id - id of the destination vertex of the edge to be removed
   */
  void DeleteEdge(type::vertex_t src_vertex_id, type::vertex_t dest_vertex_id);

  /** Swap two vertices. Swap all attributes except for "id"
   * @param vertex_id_1 - id of the first vertex
   * @param vertex_id_2 - id of the second vertex
   */
  void SwapVertex(type::vertex_t vertex_id_1, type::vertex_t vertex_id_2);

  /** Query a vertex. (Not implement yet.)
   */
  void QueryVertex();

  /** Query an edge with source and destination vertex ids.
   * @param src_vertex_id - id of the source vertex of the edge
   * @param dest_vertex_id - id of the destination vertex of the edge
   * @return id of the queried edge
   */
  type::edge_t QueryEdge(type::vertex_t src_vertex_id, type::vertex_t dest_vertex_id);

  /** Get the source vertex of the input edge.
   * @param edge_id - input edge id
   * @return id of the source vertex
   */
  type::vertex_t GetEdgeSrc(type::edge_t edge_id);

  /** Get the destination vertex of the input edge.
   * @param edge_id - input edge id
   * @return id of the destination vertex
   */
  type::vertex_t GetEdgeDest(type::edge_t edge_id);

  /** Get the other side vertex of the input edge. (Not implement yet.)
   * @param edge_id - input edge id
   * @param vertex_id - input vertex id
   * @return id of the vertex in the other side of the input edge
   */
  void GetEdgeOtherSide();

  /** Checks whether a graph attribute exists.
   *  Time complexity: O(A), the number of graph attributes, assuming attribute names are not too long.
   * @param attr_name - name of the graph attribute
   * @return Logical value, TRUE if the attribute exists, FALSE otherwise.
  */
  bool HasGraphAttribute(const char* attr_name);

  /** Checks whether a vertex attribute exists.
   *  Time complexity: O(A), the number of vertex attributes, assuming attribute names are not too long.
   * @param attr_name - name of the graph attribute
   * @return Logical value, TRUE if the attribute exists, FALSE otherwise.
  */
  bool HasVertexAttribute(const char* attr_name);

  /** Checks whether a edge attribute exists.
   *  Time complexity: O(A), the number of edge attributes, assuming attribute names are not too long.
   * @param attr_name - name of the graph attribute
   * @return Logical value, TRUE if the attribute exists, FALSE otherwise.
  */
  bool HasEdgeAttribute(const char* attr_name);

  /** Set a string graph attribute
   * @param attr_name - name of the graph attribute
   * @param value - the (new) value of the graph attribute
   */
  void SetGraphAttributeString(const char* attr_name, const char* value);

  /** Set a numeric graph attribute
   * @param attr_name - name of the graph attribute
   * @param value - the (new) value of the graph attribute
   */
  void SetGraphAttributeNum(const char* attr_name, const type::num_t value);

  /** Set a boolean graph attribute as flag
   * @param attr_name - name of the graph attribute
   * @param value - the (new) value of the graph attribute
   */
  void SetGraphAttributeFlag(const char* attr_name, const bool value);

  /** Set a string vertex attribute
   * @param attr_name - name of the vertex attribute
   * @param vertex_id - the vertex id
   * @param value - the (new) value of the vertex attribute
   */
  void SetVertexAttributeString(const char* attr_name, type::vertex_t vertex_id, const char* value);

  /** Set a numeric vertex attribute
   * @param attr_name - name of the vertex attribute
   * @param vertex_id - the vertex id
   * @param value - the (new) value of the vertex attribute
   */
  void SetVertexAttributeNum(const char* attr_name, type::vertex_t vertex_id, const type::num_t value);

  /** Set a boolean vertex attribute as flag
   * @param attr_name - name of the vertex attribute
   * @param vertex_id - the vertex id
   * @param value - the (new) value of the vertex attribute
   */
  void SetVertexAttributeFlag(const char* attr_name, type::vertex_t vertex_id, const bool value);

  /** Set a string edge attribute
   * @param attr_name - name of the edge attribute
   * @param type::edge_t - the edge id
   * @param value - the (new) value of the edge attribute
   */
  void SetEdgeAttributeString(const char* attr_name, type::edge_t edge_id, const char* value);

  /** Set a numeric edge attribute
   * @param attr_name - name of the edge attribute
   * @param type::edge_t - the edge id
   * @param value - the (new) value of the edge attribute
   */
  void SetEdgeAttributeNum(const char* attr_name, type::edge_t edge_id, const type::num_t value);

  /** Set a boolean edge attribute as flag
   * @param attr_name - name of the edge attribute
   * @param type::edge_t - the edge id
   * @param value - the (new) value of the edge attribute
   */
  void SetEdgeAttributeFlag(const char* attr_name, type::edge_t edge_id, const bool value);

  /** Get a string graph attribute
   * @param attr_name - name of the graph attribute
   * @return the (new) value of the graph attribute
   */
  const char* GetGraphAttributeString(const char* attr_name);

  /** Get a numeric graph attribute
   * @param attr_name - name of the graph attribute
   * @return the (new) value of the graph attribute
   */
  const type::num_t GetGraphAttributeNum(const char* attr_name);

  /** Get a flag graph attribute
   * @param attr_name - name of the graph attribute
   * @return the (new) value of the graph attribute
   */
  const bool GetGraphAttributeFlag(const char* attr_name);

  /** Get a string vertex attribute
   * @param attr_name - name of the vertex attribute
   * @param vertex_id - the vertex id
   * @return value - the (new) value of the vertex attribute
   */
  const char* GetVertexAttributeString(const char* attr_name, type::vertex_t vertex_id);

  /** Get a numeric vertex attribute
   * @param attr_name - name of the vertex attribute
   * @param vertex_id - the vertex id
   * @return value - the (new) value of the vertex attribute
   */
  const type::num_t GetVertexAttributeNum(const char* attr_name, type::vertex_t vertex_id);

  /** Get a flag vertex attribute
   * @param attr_name - name of the vertex attribute
   * @param vertex_id - the vertex id
   * @return value - the (new) value of the vertex attribute
   */
  const bool GetVertexAttributeFlag(const char* attr_name, type::vertex_t vertex_id);

  /** Get a string edge attribute
   * @param attr_name - name of the edge attribute
   * @param type::edge_t - the edge id
   * @return value - the (new) value of the edge attribute
   */
  const char* GetEdgeAttributeString(const char* attr_name, type::edge_t edge_id);

  /** Get a numeric edge attribute
   * @param attr_name - name of the edge attribute
   * @param type::edge_t - the edge id
   * @return value - the (new) value of the edge attribute
   */
  const type::num_t GetEdgeAttributeNum(const char* attr_name, type::edge_t edge_id);

  /** Get a flag edge attribute
   * @param attr_name - name of the edge attribute
   * @param type::edge_t - the edge id
   * @return value - the (new) value of the edge attribute
   */
  const bool GetEdgeAttributeFlag(const char* attr_name, type::edge_t edge_id);

  /** Remove a graph attribute
   * @param attr_name - name of the graph attribute
   */
  void RemoveGraphAttribute(const char* attr_name);

  /** Remove a vertex attribute
   * @param attr_name - name of the vertex attribute
   */
  void RemoveVertexAttribute(const char* attr_name);

  /** Remove an edge attribute
   * @param attr_name - name of the edge attribute
   */
  void RemoveEdgeAttribute(const char* attr_name);

  void MergeVertices();
  void SplitVertex();

  /** Copy a vertex to the designated vertex. All attributes (include "id") are copied.
   * @param new_vertex_id - id of the designated vertex
   * @param g - graph that contains the vertex to be copied
   * @param vertex_id - id of the vertex to be copied
   */
  void DeepCopyVertex(type::vertex_t new_vertex_id, Graph* g, type::vertex_t vertex_id);

  /** Copy a vertex to the designated vertex. All attributes, except "id", are copied.
   * @param new_vertex_id - id of the designated vertex
   * @param g - graph that contains the vertex to be copied
   * @param vertex_id - id of the vertex to be copied
   */
  void CopyVertex(type::vertex_t new_vertex_id, Graph* g, type::vertex_t vertex_id);
  // TODO: do not expose inner igraph

  /** Depth-First Search, not implement yet.
   */
  void Dfs();

  /** Read a graph from a GML format file.
   * @param file_name - name of input file
   */
  void ReadGraphGML(const char* file_name);

  /** Dump the graph as a GML format file.
   * @param file_name - name of output file
   */
  void DumpGraphGML(const char* file_name);

  /** Dump the graph as a dot format file.
   * @param file_name - name of output file
   */
  void DumpGraphDot(const char* file_name);

  /** Get the number of vertices.
   * @return the number of vertices
   */
  int GetCurVertexNum();

  /** Get a set of child vertices.
   * @param vertex_id - id of a vertex
   * @param child_vec - a vector that stores the id of child vertices
   */
  void GetChildVertexSet(type::vertex_t vertex_id, std::vector<type::vertex_t>& child_vec);

  /** Get parent vertex.
   * @param vertex_id - id of a vertex
   * @return parent vertex of input vertex
   *
  */
  type::vertex_t GetParentVertex(type::vertex_t vertex_id);

  /** [Graph Algorithm] Traverse all vertices and execute CALL_BACK_FUNC when accessing each vertex.
   * @param CALL_BACK_FUNC - callback function when a vertex is accessed. The input parameters of this function contain
   * a pointer to the graph being traversed, id of the accessed vertex, and an extra pointer for developers to pass more
   * parameters.
   * @param extra - a pointer for developers to pass more parameters as the last parameter of CALL_BACK_FUNC
   */
  void VertexTraversal(void (*CALL_BACK_FUNC)(Graph*, type::vertex_t, void*), void* extra);

  /** [Graph Algorithm] Perform Pre-order traversal on the graph.
   * @param root_vertex_id - id of the starting vertex
   * @param pre_order_vertex_vec - a vector that stores the accessing sequence (id) of vertex
   */
  void PreOrderTraversal(type::vertex_t root_vertex_id, std::vector<type::vertex_t>& pre_order_vertex_vec);

  /** [Graph Algorithm] Perform Depth-First Search on the graph.
   * @param root - The id of the root vertex.
   * @param IN_CALL_BACK_FUNC - callback function when a new vertex is discovered / accessed. The input parameters of
   * this function contain
   * a pointer to the graph being traversed, id of the accessed vertex, and an extra pointer for developers to pass more
   * parameters.
   * @param OUT_CALL_BACK_FUNC - callback function when the subtree of a vertex is completed. The input parameters of
   * this function contain
   * a pointer to the graph being traversed, id of the accessed vertex, and an extra pointer for developers to pass more
   * parameters.
   * @param extra - a pointer for developers to pass more parameters as the last parameter of IN_CALL_BACK_FUNC and
   * OUT_CALL_BACK_FUNC
  */
  void DFS(type::vertex_t root, void (*IN_CALL_BACK_FUNC)(Graph*, int, void*),
           void (*OUT_CALL_BACK_FUNC)(Graph*, int, void*), void* extra);

  /** [Graph Algorithm] Perform Breadth-First Search on the graph.
   * @param root - The id of the root vertex.
   * @param CALL_BACK_FUNC - callback function when a new vertex is discovered / accessed. The input parameters of
   * this function contain a pointer to the graph being traversed, id of the accessed vertex, and an extra pointer for
   * developers to pass more
   * parameters.
   * @param extra - a pointer for developers to pass more parameters as the last parameter of CALL_BACK_FUNC
  */
  void BFS(type::vertex_t root, void (*CALL_BACK_FUNC)(Graph*, int, void*), void* extra);

  GraphPerfData* GetGraphPerfData();

  /** Sort child vertices from a starting vertex recursively
   * @param starting_vertex - a starting vertex.
   * @param attr_name - a key attribute to sort by.
  */
  void SortBy(type::vertex_t starting_vertex, const char* attr_name);
};

class ProgramGraph : public Graph {
 private:
 public:
  /** Default Constructor
   */
  ProgramGraph();

  /** Default Destructor
   */
  ~ProgramGraph();

  /** Set basic information of a vertex, including type and name.
   * @param vertex_id - id of the target vertex
   * @param vertex_type - type of the target vertex
   * @param vertex_name - name of the target vertex
   * @return 0 is success
   */
  int SetVertexBasicInfo(const type::vertex_t vertex_id, const int vertex_type, const char* vertex_name);

  /** Set debug information of a vertex, including address or line number (need to extend).
   * @param vertex_id - id of the target vertex
   * @param entry_addr - entry address of the target vertex
   * @param exit_addr - exit address of the target vertex
   * @return 0 is success
   */
  int SetVertexDebugInfo(const type::vertex_t vertex_id, const type::addr_t entry_addr, const type::addr_t exit_addr);

  /** Get the type of a vertex.
   * @param vertex_id - id of the target vertex
   * @return type of the vertex
   */
  int GetVertexType(type::vertex_t vertex_id);

  /** Set type of an edge.
   * @param edge_id - id of the target edge
   * @param edge_type - type of the target edge
   * @return 0 is success
   */
  int SetEdgeType(const type::edge_t edge_id, const int edge_type);

  /** Get the type of an edge.
   * @param edge_id - id of the edge
   * @return type of the edge
   */
  int GetEdgeType(type::edge_t edge_id);

  /** Get the type of an edge (src vertex, dest vertex).
   * @param src_vertex_id - id of the source vertex
   * @param dest_vertex_id - id of the destination vertex
   * @return type of the edge
   */
  int GetEdgeType(type::vertex_t src_vertex_id, type::vertex_t dest_vertex_id);

  /** Get the entry address of a specific vertex
   * @param vertex_id - id of the specific vertex
   * @return entry address of the specific vertex
   */
  type::addr_t GetVertexEntryAddr(type::vertex_t vertex_id);

  /** Get the exit address of a specific vertex
   * @param vertex_id - id of the specific vertex
   * @return exit address of the specific vertex
   */
  type::addr_t GetVertexExitAddr(type::vertex_t vertex_id);

  /** Identify the vertex corresponding to the address from a specific starting vertex.
   * @param root_vertex_id - id of the starting vertex
   * @param addr - address
   * @return id of the identified vertex
   */
  type::vertex_t GetChildVertexWithAddr(type::vertex_t root_vertex_id, type::addr_t addr);

  /** Identify the vertex corresponding to the call path from a specific starting vertex.
   * @param root_vertex_id - id of the starting vertex
   * @param call_path - call path
   * @return id of the identified vertex
   */
  type::vertex_t GetVertexWithCallPath(type::vertex_t root_vertex_id, std::stack<unsigned long long>& call_path);

  /** Identify the call vertex corresponding to the address from all vertices.
   * @param addr - address
   * @return id of the identified vertex
   */
  type::vertex_t GetCallVertexWithAddr(type::addr_t addr);

  /** Identify the function vertex corresponding to the address from all vertices.
   * @param addr - address
   * @return id of the identified vertex
   */
  type::vertex_t GetFuncVertexWithAddr(type::addr_t addr);

  /** Add a new edge between call vertex and callee function vertex through their addresses.
   * @param call_addr - address of the call instruction
   * @param callee_addr - address of the callee function
   * @return
   */
  int AddEdgeWithAddr(type::addr_t call_addr, type::addr_t callee_addr);

  /** Get name of a vertex's callee vertex
   * @param vertex_id - id of vertex
   * @return name of the callee vertex
   */
  const char* GetCallee(type::vertex_t vertex_id);
  
  /** Get name of a vertex's callee vertex, call edge type must be the input type
   * @param vertex_id - id of vertex
   * @param input_type - the input type
   * @return name of the callee vertex
   */
  const char* GetCallee(type::vertex_t vertex_id, int input_type);

  /** Get entry address of a vertex's callee vertex
   * @param vertex_id - id of vertex
   * @return entry address of the callee vertex
   */
  type::addr_t GetCalleeEntryAddr(type::vertex_t vertex_id);

  /** Get entry address of a vertex's callee vertex, and edge type must be the input type
   * @param vertex_id - id of vertex
   * @param input_type - the input type
   * @return entry address of the callee vertex
   */
  type::addr_t GetCalleeEntryAddr(type::vertex_t vertex_id, int input_type);

  /** Get entry address of a vertex's callee vertex
   * @param vertex_id - id of vertex
   * @param entry_addrs - (output) entry addresses of the callee vertex
   */
  void GetCalleeEntryAddrs(type::vertex_t vertex_id, std::vector<type::addr_t>& entry_addrs);

  /** Get entry address of a vertex's callee vertex, and edge type must be the input type
   * @param vertex_id - id of vertex
   * @param entry_addrs - (output) entry addresses of the callee vertex
   * @param input_type - the input type
   */
  void GetCalleeEntryAddrs(type::vertex_t vertex_id, std::vector<type::addr_t> &entry_addrs, int input_type);

  /** Sort vertices by entry addresses
   */
  void VertexSortChild();

  /** [Graph Algorithm] Traverse all vertices and execute CALL_BACK_FUNC when accessing each vertex.
   * @param CALL_BACK_FUNC - callback function when a vertex is accessed. The input parameters of this function contain
   * a pointer to the graph being traversed, id of the accessed vertex, and an extra pointer for developers to pass more
   * parameters.
   * @param extra - a pointer for developers to pass more parameters as the last parameter of CALL_BACK_FUNC
   */
  void VertexTraversal(void (*CALL_BACK_FUNC)(ProgramGraph*, int, void*), void* extra);

  /** [Graph Algorithm] Traverse all edges and execute CALL_BACK_FUNC when accessing each edge.
   * @param CALL_BACK_FUNC - callback function when an edge is accessed. The input parameters of this function contain
   * a pointer to the graph being traversed, id of the accessed edge, and an extra pointer for developers to pass more
   * parameters.
   * @param extra - a pointer for developers to pass more parameters as the last parameter of CALL_BACK_FUNC
   */
  void EdgeTraversal(void (*CALL_BACK_FUNC)(ProgramGraph*, int, void*), void* extra);

  /** Sort by address
  */
  void SortByAddr(type::vertex_t starting_vertex);
};

class ProgramAbstractionGraph : public ProgramGraph {
 private:
 public:
  /** Default Constructor
   */
  ProgramAbstractionGraph();
  /** Default Destructor
   */
  ~ProgramAbstractionGraph();
  /** [Graph Algorithm] Traverse all vertices and execute CALL_BACK_FUNC when accessing each vertex.
   * @param CALL_BACK_FUNC - callback function when a vertex is accessed. The input parameters of this function contain
   * a pointer to the graph being traversed, id of the accessed vertex, and an extra pointer for developers to pass more
   * parameters.
   * @param extra - a pointer for developers to pass more parameters as the last parameter of CALL_BACK_FUNC
   */
  void VertexTraversal(void (*CALL_BACK_FUNC)(ProgramAbstractionGraph*, int, void*), void* extra);

  /** [Graph Algorithm] Traverse all edges and execute CALL_BACK_FUNC when accessing each edge.
   * @param CALL_BACK_FUNC - callback function when an edge is accessed. The input parameters of this function contain
   * a pointer to the graph being traversed, id of the accessed edge, and an extra pointer for developers to pass more
   * parameters.
   * @param extra - a pointer for developers to pass more parameters as the last parameter of CALL_BACK_FUNC
   */
  void EdgeTraversal(void (*CALL_BACK_FUNC)(ProgramAbstractionGraph*, int, void*), void* extra);

  /** DFS
   *
  */
  void DFS(type::vertex_t root, void (*IN_CALL_BACK_FUNC)(ProgramAbstractionGraph*, int, void*),
           void (*OUT_CALL_BACK_FUNC)(ProgramAbstractionGraph*, int, void*), void* extra);

  /** Reduce performance data of each process and thread of a speific metric for each vertex (GraphPerfData)
   * @param metric - a specfic metric
   * @param op - reduce operation
   * @return what????
   */
  baguatool::type::perf_data_t ReduceVertexPerfData(std::string& metric, std::string& op);

  /** Convert performance data to percentage format for each vertex (total value as 100 percent).
   * @param metric - a specfic metric
   * @param total - total value
   * @param new_metric - new metric for record percentage format data
   */
  void ConvertVertexReducedDataToPercent(std::string& metric, baguatool::type::perf_data_t total,
                                         std::string& new_metric);

  /** Preserve hotspot vertices
   *
  */
  void PreserveHotVertices(char* metric_name);
};

class ControlFlowGraph : public ProgramGraph {
 private:
 public:
  /** Default Constructor
   */
  ControlFlowGraph();
  /** Default Destructor
   */
  ~ControlFlowGraph();
};

class ProgramCallGraph : public ProgramGraph {
 private:
 public:
  /** Default Constructor
   */
  ProgramCallGraph();
  /** Default Destructor
   */
  ~ProgramCallGraph();

  /** [Graph Algorithm] Traverse all edges and execute CALL_BACK_FUNC when accessing each edge.
   * @param CALL_BACK_FUNC - callback function when an edge is accessed. The input parameters of this function contain
   * a pointer to the graph being traversed, id of the accessed edge, and an extra pointer for developers to pass more
   * parameters.
   * @param extra - a pointer for developers to pass more parameters as the last parameter of CALL_BACK_FUNC
   */
  void EdgeTraversal(void (*CALL_BACK_FUNC)(ProgramCallGraph*, int, void*), void* extra);
};

typedef struct VERTEX_DATA_STRUCT VDS;
typedef struct EDGE_DATA_STRUCT EDS;

#ifndef MAX_LINE_LEN
#define MAX_LINE_LEN 256
#endif

class PerfData {
 private:
  VDS* vertex_perf_data = nullptr;                   /**<pointer to vertex type performance data */
  unsigned long int vertex_perf_data_space_size = 0; /**<pre-allocate space size of vertex type performance data*/
  unsigned long int vertex_perf_data_count = 0;      /**<amount of recorded vertex type performance data*/
  EDS* edge_perf_data = nullptr;                     /**<pointer to edge type performance data */
  unsigned long int edge_perf_data_space_size = 0;   /**<pre-allocate space size of edge type performance data*/
  unsigned long int edge_perf_data_count = 0;        /**<amount of recorded edge type performance data*/
  FILE* perf_data_fp = nullptr;                      /**<file handler for output */
  std::ifstream perf_data_in_file;                   /**<file handler for input */
  bool has_open_output_file = false;                 /**<flag for record whether output file has open or not*/
  char file_name[MAX_LINE_LEN] = {0};                /**<file name for output */
  // TODO: design a method to make metric_name portable
  std::string metric_name = std::string("TOT_CYC"); /**<metric name */

 public:
  /** Default Constructor
   */
  PerfData();
  /** Default Destructor
   */
  ~PerfData();

  /** Read vertex type and edge type performance data (Input)
   * @param file_name - name of input file
   */
  void Read(const char* file_name);

  /** Dump vertex type and edge type performance data (Output)
   * @param file_name - name of output file
   */
  void Dump(const char* file_name);

  /** Get size of recorded vertex type performance data
   * @return size of recorded vertex type performance data
   */
  unsigned long int GetVertexDataSize();

  /** Get size of recorded edge type performance data
   * @return size of recorded edge type performance data
   */
  unsigned long int GetEdgeDataSize();

  /** Set metric name for the performance data
   * @param metric_name - name of the metric
   */
  void SetMetricName(std::string& metric_name);

  /** Get metric name of the performance data
   * @return name of the metric
   */
  std::string& GetMetricName();

  /** Query a piece of vertex type performance data by (call path, call path length, process id, thread id)
   * @param call_path - call path
   * @param call_path_len - depth of the call path
   * @param procs_id - process id
   * @param thread_id - thread id
   * @return index of the queried piece of data
   */
  int QueryVertexData(baguatool::type::addr_t* call_path, int call_path_len, int procs_id, int thread_id);

  /** Query a piece of edge type performance data by (call path, call path length, process id, thread id)
   * @param call_path - call path of source
   * @param call_path_len - depth of the call path of source
   * @param out_call_path - call path of destination
   * @param out_call_path_len - depth of the call path of destination
   * @param procs_id - process id of source
   * @param out_procs_id - process id of destination
   * @param thread_id - thread id of source
   * @param out_thread_id - thread id of destination
   * @return index of the queried piece of data
   */
  int QueryEdgeData(baguatool::type::addr_t* call_path, int call_path_len, baguatool::type::addr_t* out_call_path,
                    int out_call_path_len, int procs_id, int out_procs_id, int thread_id, int out_thread_id);

  /** Record a piece of vertex type performance data
   * @param call_path - call path
   * @param call_path_len - depth of the call path
   * @param procs_id - process id
   * @param thread_id - thread id
   * @param value of this piece of data
   */
  void RecordVertexData(baguatool::type::addr_t* call_path, int call_path_len, int procs_id, int thread_id,
                        baguatool::type::perf_data_t value);

  /** Record a piece of edge type performance data
   * @param call_path - call path of source
   * @param call_path_len - depth of the call path of source
   * @param out_call_path - call path of destination
   * @param out_call_path_len - depth of the call path of destination
   * @param procs_id - process id of source
   * @param out_procs_id - process id of destination
   * @param thread_id - thread id of source
   * @param out_thread_id - thread id of destination
   * @param value of this piece of data
   */
  void RecordEdgeData(baguatool::type::addr_t* call_path, int call_path_len, baguatool::type::addr_t* out_call_path,
                      int out_call_path_len, int procs_id, int out_procs_id, int thread_id, int out_thread_id,
                      baguatool::type::perf_data_t value);

  /** Query call path of a piece of vertex type performance data through index
   * @param data_index - index of the piece of data
   * @param call_path - call path of this piece of data
   */
  void GetVertexDataCallPath(unsigned long int data_index, std::stack<unsigned long long>& call_path);

  // /**
  //  * @brief Set the Vertex Data Call Path object
  //  * 
  //  * @param data_index 
  //  * @param call_path_stack 
  //  */
  // void SetVertexDataCallPath(unsigned long int data_index, std::stack<type::addr_t>& call_path_stack);


  /** Query source call path of a piece of edge type performance data through index
   * @param data_index - index of the piece of data
   * @param call_path - call path of the queried piece of data
   */
  void GetEdgeDataSrcCallPath(unsigned long int data_index, std::stack<unsigned long long>& call_path);

  /** Query destination call path of a piece of edge type performance data through index
   * @param data_index - index of the piece of data
   * @param call_path - call path of the queried piece of data
   */
  void GetEdgeDataDestCallPath(unsigned long int data_index, std::stack<unsigned long long>& call_path);

  /** Query value of a piece of vertex type performance data through index
   * @param data_index - index of the piece of data
   * @return value of the queried piece of data
   */
  baguatool::type::perf_data_t GetVertexDataValue(unsigned long int data_index);

  /** Query value of a piece of edge type performance data through index
   * @param data_index - index of the piece of data
   * @return value of the queried piece of data
   */
  baguatool::type::perf_data_t GetEdgeDataValue(unsigned long int data_index);

  /** Query process id of a piece of vertex type performance data through index
   * @param data_index - index of the piece of data
   * @return process id of the queried piece of data
   */
  int GetVertexDataProcsId(unsigned long int data_index);

  /** Query source process id of a piece of edge type performance data through index
   * @param data_index - index of the piece of data
   * @return process id of the queried piece of data
   */
  int GetEdgeDataSrcProcsId(unsigned long int data_index);

  /** Query destination process id of a piece of edge type performance data through index
   * @param data_index - index of the piece of data
   * @return process id of the queried piece of data
   */
  int GetEdgeDataDestProcsId(unsigned long int data_index);

  /** Query thread id of a piece of vertex type performance data through index
   * @param data_index - index of the piece of data
   * @return thread id of the queried piece of data
   */
  int GetVertexDataThreadId(unsigned long int data_index);

  /** Query source thread id of a piece of edge type performance data through index
   * @param data_index - index of the piece of data
   * @return thread id of the queried piece of data
   */
  int GetEdgeDataSrcThreadId(unsigned long int data_index);

  /** Query destination thread id of a piece of edge type performance data through index
   * @param data_index - index of the piece of data
   * @return thread id of the queried piece of data
   */
  int GetEdgeDataDestThreadId(unsigned long int data_index);
};

// class HybridAnalysis {
//  private:
//   std::map<std::string, ControlFlowGraph*> func_cfg_map; /**<control-flow graphs for each function*/
//   ProgramCallGraph* pcg;                                 /**<program call graph*/
//   std::map<std::string, ProgramAbstractionGraph*>
//       func_pag_map; /**<program abstraction graph extracted from control-flow graph (CFG) for each function */
//   ProgramAbstractionGraph* root_pag;  /**<an overall program abstraction graph for a program */
//   ProgramAbstractionGraph* root_mpag; /**<an overall multi-* program abstraction graph for a parallel program*/
//   GraphPerfData* graph_perf_data;     /**<performance data in a graph*/

//  public:
//   HybridAnalysis();
//   ~HybridAnalysis();

//   /** Control Flow Graph of Each Function **/

//   void ReadStaticControlFlowGraphs(const char* dir_name);
//   void GenerateControlFlowGraphs(const char* dir_name);
//   ControlFlowGraph* GetControlFlowGraph(std::string func_name);
//   std::map<std::string, ControlFlowGraph*>& GetControlFlowGraphs();

//   /** Program Call Graph **/

//   void ReadStaticProgramCallGraph(const char* static_pcg_file_name);
//   void ReadDynamicProgramCallGraph(std::string perf_data_file_name);
//   void GenerateProgramCallGraph(const char*);
//   ProgramCallGraph* GetProgramCallGraph();

//   /** Intra-procedural Analysis **/

//   ProgramAbstractionGraph* GetFunctionAbstractionGraph(std::string func_name);
//   std::map<std::string, ProgramAbstractionGraph*>& GetFunctionAbstractionGraphs();
//   void IntraProceduralAnalysis();
//   void ReadFunctionAbstractionGraphs(const char* dir_name);

//   /** Inter-procedural Analysis **/

//   void InterProceduralAnalysis();
//   void GenerateProgramAbstractionGraph();
//   void SetProgramAbstractionGraph(ProgramAbstractionGraph*);
//   ProgramAbstractionGraph* GetProgramAbstractionGraph();

//   /** DataEmbedding **/
//   void DataEmbedding(PerfData*);
//   GraphPerfData* GetGraphPerfData();
//   baguatool::type::perf_data_t ReduceVertexPerfData(std::string& metric, std::string& op);
//   void ConvertVertexReducedDataToPercent(std::string& metric, baguatool::type::perf_data_t total, std::string&
//   new_metric);

//   void GenerateMultiProgramAbstractionGraph();
//   ProgramAbstractionGraph* GetMultiProgramAbstractionGraph();

//   void PthreadAnalysis(PerfData* pthread_data);

// };  // class HybridAnalysis

}  // namespace core

namespace collector {

class StaticAnalysisImpl;

/** Static analysis based on binary. Extract structure and function call relationship and ...
 *
*/
class StaticAnalysis {
 private:
  std::unique_ptr<StaticAnalysisImpl> sa; /**< wrapper for dyninst */

 public:
  /** Constructor. Initialize the class with input binary name.
   *
  */
  StaticAnalysis(char* binary_name);

  /** Destructor.
   *
  */
  ~StaticAnalysis();

  /** Extract control-flow graphs of all functions.
   *
  */
  void IntraProceduralAnalysis();

  /** Capture static program call graph.
   *
  */
  void InterProceduralAnalysis();

  /** Capture static program call graph.
   *
  */
  void CaptureProgramCallGraph();

  /** Dump control-flow graphs of all functions.
   *
  */
  void DumpAllControlFlowGraph();

  /** Dump static program call graph.
   *
  */
  void DumpProgramCallGraph();

  /** Get name of input binary.
   *
  */
  void GetBinaryName();
};  // class StaticAnalysis

// typedef unsigned long long int baguatool::type::addr_t;

class LongLongVec;
class SamplerImpl;

/** Runtime sampler.
 *
*/
class Sampler {
 private:
  std::unique_ptr<SamplerImpl> sa; /**< wrapper of sampler based on papi tool*/

 public:
  /** Constructor.
   *
  */
  Sampler();

  /** Destructor.
   *
  */
  ~Sampler();

  /** Set sampling frequence to input value.
   * @param freq - frequence
  */
  void SetSamplingFreq(int freq);

  /** Setup (Initialize).
   *
  */
  void Setup();

  /** Add a thread at runtime. Sampler could handle new thread.
   *
  */
  void AddThread();

  /** Remove a thread at runtime. Sampler stop sampling this thread.
   *
  */
  void RemoveThread();

  /** Set overflow function. Sampler executes an input function every time the program is interrupted.
   * @param FUNC_AT_OVERFLOW - overflow fucntion.
  */
  void SetOverflow(void (*FUNC_AT_OVERFLOW)(int));

  /** Unset overflow fucntion.
   *
   */
  void UnsetOverflow();

  /** Start counting performance metric events.
   *
  */
  void Start();

  /** Stop counting performance metric events.
   *
  */
  void Stop();

  /** Get overflow event.
   * @param overflow_vector - papi overflow vector to detect which event counter is overflow
   * @return id of overflowed event
  */
  int GetOverflowEvent(LongLongVec* overflow_vector);

  /** backtrace.
   * @param call_path - call path (output)
   * @param max_call_path_depth - max depth of call path
   * @return depth of call path
  */
  int GetBacktrace(baguatool::type::addr_t* call_path, int max_call_path_depth);

  /** backtrace.
   * @param call_path - call path (output)
   * @param max_call_path_depth - max depth of call path
   * @param start_depth - (input) starting depth of recorded call path
   * @return depth of call path
  */
  int GetBacktrace(type::addr_t* call_path, int max_call_path_depth, int start_depth);
};  // class Sampler

// static void* resolve_symbol(const char* symbol_name, int config);
class SharedObjAnalysis {
 private:
  /* data */
  std::vector<std::tuple<type::addr_t, type::addr_t, std::string>> shared_obj_map;

 public:
  SharedObjAnalysis(/* args */);
  ~SharedObjAnalysis();
  void CollectSharedObjMap();

  void ReadSharedObjMap(std::string& file_name);
  void DumpSharedObjMap(std::string& file_name);
  void GetDebugInfo(type::addr_t addr, type::addr_debug_info_t& debug_info);

  void GetDebugInfos(std::unordered_set<type::addr_t>& addrs,
                     std::map<type::addr_t, type::addr_debug_info_t*>& debug_info_map, std::string& binary_name);
};  // class SharedObjAnalysis

}  // namespace collector
}  // namespace baguatool

#endif
