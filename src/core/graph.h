#ifndef GRAPH_H_
#define GRAPH_H_

#include <igraph.h>

#define TRUNK_SIZE 1000000

namespace baguatool::type {
struct graph_t {
  igraph_t graph;
};

struct vertex_set_t {
  igraph_vs_t vertices;
};

struct edge_vector_t {
  igraph_vector_t edges;
};

} // namespace baguatool::type

namespace baguatool::core {

// struct PAG_vertex_t {
//   igraph_integer_t vertex_id;
// };

// struct PAG_edge_t {
//   igraph_integer_t edge_id;
// };

// class Graph {
//  protected:
//   std::unique_ptr<type::graph_t> ipag_;
//   int cur_vertex_num;

//  public:
//   Graph();
//   ~Graph();
//   void GraphInit(const char* graph_name);
//   int GetCurVertexNum();
//   PAG_vertex_t AddVertex();
//   PAG_edge_t AddEdge(const PAG_vertex_t src_vertex_id, const PAG_vertex_t
//   dest_vertex_id); void AddGraph(ProgramAbstractionGraph* g); void
//   DeleteVertex(); void DeleteEdge(); void QueryVertex(); void QueryEdge();
//   int GetEdgeSrc(PAG_edge_t edge_id);
//   int GetEdgeDest(PAG_edge_t edge_id);
//   void QueryEdgeOtherSide();
//   void SetVertexAttribute();
//   void SetEdgeAttribute();
//   void GetVertexAttribute();
//   int GetVertexAttributeNum(const char* attr_name, PAG_vertex_t vertex_id);
//   const char* GetVertexAttributeString(const char* attr_name, PAG_vertex_t
//   vertex_id); void GetEdgeAttribute(); const char*
//   GetGraphAttributeString(const char* attr_name); void MergeVertices(); void
//   SplitVertex(); void CopyVertex(PAG_vertex_t new_vertex_id, Graph* g,
//   PAG_vertex_t vertex_id);
//   // TODO: do not expose inner igraph
//   void DeleteVertices(PAG_vertex_set_t* vs);
//   void DeleteExtraTailVertices();
//   void Dfs();
//   void ReadGraphGML(const char* file_name);
//   void DumpGraph(const char* file_name);
//   void DumpGraphDot(const char* file_name);

//   void VertexTraversal(void (*CALL_BACK_FUNC)(Graph*, PAG_vertex_t, void*),
//   void* extra);
// };

} // namespace baguatool::core

#endif // GRAPH_H_