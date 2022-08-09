#ifndef PAG_H_
#define PAG_H_

#include <stdint.h>

#include <string>

#include "common/common.h"
#include "core/pg.h"
#include "vertex_type.h"
#include <igraph.h>

namespace baguatool::core {

struct PAG_graph_t {
  igraph_t graph;
};

struct PAG_vertex_set_t {
  igraph_vs_t vertices;
};

// struct PAG_vertex_t {
//   igraph_integer_t vertex_id;
// };

// struct PAG_edge_t {
//   igraph_integer_t edge_id;
// };

} // namespace baguatool::core
#endif // PAG_H