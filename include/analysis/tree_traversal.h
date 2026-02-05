// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

#ifndef PERFLOW_ANALYSIS_TREE_TRAVERSAL_H_
#define PERFLOW_ANALYSIS_TREE_TRAVERSAL_H_

#include <functional>
#include <memory>
#include <queue>
#include <stack>
#include <vector>

#include "analysis/performance_tree.h"

namespace perflow {
namespace analysis {

/// Visitor interface for tree traversal
/// Implement this interface to process nodes during traversal
class TreeVisitor {
 public:
  virtual ~TreeVisitor() = default;
  
  /// Called when visiting a node
  /// @param node The node being visited
  /// @param depth Current depth in the tree (root = 0)
  /// @return true to continue traversal, false to stop
  virtual bool visit(const std::shared_ptr<TreeNode>& node, size_t depth) = 0;
};

/// Predicate function type for filtering nodes
using NodePredicate = std::function<bool(const std::shared_ptr<TreeNode>&)>;

/// TreeTraversal provides various tree traversal algorithms
class TreeTraversal {
 public:
  /// Traverse tree in depth-first order (pre-order)
  /// @param root Starting node
  /// @param visitor Visitor to process each node
  /// @param max_depth Maximum depth to traverse (0 = unlimited)
  static void depth_first_preorder(const std::shared_ptr<TreeNode>& root,
                                   TreeVisitor& visitor,
                                   size_t max_depth = 0) {
    if (!root) return;
    depth_first_preorder_impl(root, visitor, 0, max_depth);
  }
  
  /// Traverse tree in depth-first order (post-order)
  /// @param root Starting node
  /// @param visitor Visitor to process each node
  /// @param max_depth Maximum depth to traverse (0 = unlimited)
  static void depth_first_postorder(const std::shared_ptr<TreeNode>& root,
                                    TreeVisitor& visitor,
                                    size_t max_depth = 0) {
    if (!root) return;
    depth_first_postorder_impl(root, visitor, 0, max_depth);
  }
  
  /// Traverse tree in breadth-first order (level-order)
  /// @param root Starting node
  /// @param visitor Visitor to process each node
  /// @param max_depth Maximum depth to traverse (0 = unlimited)
  static void breadth_first(const std::shared_ptr<TreeNode>& root,
                           TreeVisitor& visitor,
                           size_t max_depth = 0) {
    if (!root) return;
    
    std::queue<std::pair<std::shared_ptr<TreeNode>, size_t>> queue;
    queue.push({root, 0});
    
    while (!queue.empty()) {
      auto [node, depth] = queue.front();
      queue.pop();
      
      if (max_depth > 0 && depth >= max_depth) {
        continue;
      }
      
      if (!visitor.visit(node, depth)) {
        return;  // Stop traversal
      }
      
      for (const auto& child : node->children()) {
        queue.push({child, depth + 1});
      }
    }
  }
  
  /// Collect all nodes matching a predicate
  /// @param root Starting node
  /// @param predicate Filter function
  /// @return Vector of matching nodes
  static std::vector<std::shared_ptr<TreeNode>> find_nodes(
      const std::shared_ptr<TreeNode>& root,
      NodePredicate predicate) {
    std::vector<std::shared_ptr<TreeNode>> result;
    
    class CollectorVisitor : public TreeVisitor {
     public:
      CollectorVisitor(NodePredicate pred, 
                      std::vector<std::shared_ptr<TreeNode>>& res)
          : predicate_(pred), result_(res) {}
      
      bool visit(const std::shared_ptr<TreeNode>& node, size_t) override {
        if (predicate_(node)) {
          result_.push_back(node);
        }
        return true;  // Continue traversal
      }
      
     private:
      NodePredicate predicate_;
      std::vector<std::shared_ptr<TreeNode>>& result_;
    };
    
    CollectorVisitor visitor(predicate, result);
    depth_first_preorder(root, visitor);
    return result;
  }
  
  /// Find first node matching a predicate
  /// @param root Starting node
  /// @param predicate Filter function
  /// @return First matching node or nullptr
  static std::shared_ptr<TreeNode> find_first(
      const std::shared_ptr<TreeNode>& root,
      NodePredicate predicate) {
    std::shared_ptr<TreeNode> result;
    
    class FinderVisitor : public TreeVisitor {
     public:
      FinderVisitor(NodePredicate pred, std::shared_ptr<TreeNode>& res)
          : predicate_(pred), result_(res) {}
      
      bool visit(const std::shared_ptr<TreeNode>& node, size_t) override {
        if (predicate_(node)) {
          result_ = node;
          return false;  // Stop traversal
        }
        return true;  // Continue traversal
      }
      
     private:
      NodePredicate predicate_;
      std::shared_ptr<TreeNode>& result_;
    };
    
    FinderVisitor visitor(predicate, result);
    depth_first_preorder(root, visitor);
    return result;
  }
  
  /// Get path from root to a specific node
  /// @param root Starting node (tree root)
  /// @param target Target node
  /// @return Path from root to target (empty if not found)
  static std::vector<std::shared_ptr<TreeNode>> get_path_to_node(
      const std::shared_ptr<TreeNode>& root,
      const std::shared_ptr<TreeNode>& target) {
    std::vector<std::shared_ptr<TreeNode>> path;
    if (get_path_impl(root, target, path)) {
      return path;
    }
    return {};
  }
  
  /// Get all leaf nodes (nodes with no children)
  /// @param root Starting node
  /// @return Vector of leaf nodes
  static std::vector<std::shared_ptr<TreeNode>> get_leaf_nodes(
      const std::shared_ptr<TreeNode>& root) {
    return find_nodes(root, [](const std::shared_ptr<TreeNode>& node) {
      return node->children().empty();
    });
  }
  
  /// Get nodes at a specific depth
  /// @param root Starting node
  /// @param target_depth Desired depth (root = 0)
  /// @return Vector of nodes at the specified depth
  static std::vector<std::shared_ptr<TreeNode>> get_nodes_at_depth(
      const std::shared_ptr<TreeNode>& root,
      size_t target_depth) {
    std::vector<std::shared_ptr<TreeNode>> result;
    
    class DepthVisitor : public TreeVisitor {
     public:
      DepthVisitor(size_t target, std::vector<std::shared_ptr<TreeNode>>& res)
          : target_depth_(target), result_(res) {}
      
      bool visit(const std::shared_ptr<TreeNode>& node, size_t depth) override {
        if (depth == target_depth_) {
          result_.push_back(node);
        }
        return true;  // Continue traversal
      }
      
     private:
      size_t target_depth_;
      std::vector<std::shared_ptr<TreeNode>>& result_;
    };
    
    DepthVisitor visitor(target_depth, result);
    breadth_first(root, visitor);
    return result;
  }
  
  /// Get neighbors (siblings) of a node
  /// @param node Target node
  /// @return Vector of sibling nodes
  static std::vector<std::shared_ptr<TreeNode>> get_siblings(
      const std::shared_ptr<TreeNode>& node) {
    std::vector<std::shared_ptr<TreeNode>> siblings;
    
    if (!node || !node->parent()) {
      return siblings;
    }
    
    const auto& parent_children = node->parent()->children();
    for (const auto& child : parent_children) {
      if (child != node) {
        siblings.push_back(child);
      }
    }
    
    return siblings;
  }
  
  /// Calculate tree depth (maximum distance from root to any leaf)
  /// @param root Starting node
  /// @return Maximum depth
  static size_t get_tree_depth(const std::shared_ptr<TreeNode>& root) {
    if (!root) return 0;
    
    size_t max_depth = 0;
    
    class DepthCalculator : public TreeVisitor {
     public:
      explicit DepthCalculator(size_t& max) : max_depth_(max) {}
      
      bool visit(const std::shared_ptr<TreeNode>&, size_t depth) override {
        if (depth > max_depth_) {
          max_depth_ = depth;
        }
        return true;
      }
      
     private:
      size_t& max_depth_;
    };
    
    DepthCalculator visitor(max_depth);
    depth_first_preorder(root, visitor);
    return max_depth;
  }
  
  /// Count total nodes in tree
  /// @param root Starting node
  /// @return Total number of nodes
  static size_t count_nodes(const std::shared_ptr<TreeNode>& root) {
    if (!root) return 0;
    
    size_t count = 0;
    
    class Counter : public TreeVisitor {
     public:
      explicit Counter(size_t& cnt) : count_(cnt) {}
      
      bool visit(const std::shared_ptr<TreeNode>&, size_t) override {
        ++count_;
        return true;
      }
      
     private:
      size_t& count_;
    };
    
    Counter visitor(count);
    depth_first_preorder(root, visitor);
    return count;
  }

 private:
  static bool depth_first_preorder_impl(
      const std::shared_ptr<TreeNode>& node,
      TreeVisitor& visitor,
      size_t depth,
      size_t max_depth) {
    if (!node) return true;
    
    if (max_depth > 0 && depth >= max_depth) {
      return true;
    }
    
    if (!visitor.visit(node, depth)) {
      return false;  // Stop traversal
    }
    
    for (const auto& child : node->children()) {
      if (!depth_first_preorder_impl(child, visitor, depth + 1, max_depth)) {
        return false;
      }
    }
    
    return true;
  }
  
  static bool depth_first_postorder_impl(
      const std::shared_ptr<TreeNode>& node,
      TreeVisitor& visitor,
      size_t depth,
      size_t max_depth) {
    if (!node) return true;
    
    if (max_depth > 0 && depth >= max_depth) {
      return true;
    }
    
    for (const auto& child : node->children()) {
      if (!depth_first_postorder_impl(child, visitor, depth + 1, max_depth)) {
        return false;
      }
    }
    
    if (!visitor.visit(node, depth)) {
      return false;  // Stop traversal
    }
    
    return true;
  }
  
  static bool get_path_impl(
      const std::shared_ptr<TreeNode>& node,
      const std::shared_ptr<TreeNode>& target,
      std::vector<std::shared_ptr<TreeNode>>& path) {
    if (!node) return false;
    
    path.push_back(node);
    
    if (node == target) {
      return true;
    }
    
    for (const auto& child : node->children()) {
      if (get_path_impl(child, target, path)) {
        return true;
      }
    }
    
    path.pop_back();
    return false;
  }
};

/// Common node predicates for filtering
namespace predicates {

/// Predicate for nodes with sample count above threshold
inline NodePredicate samples_above(uint64_t threshold) {
  return [threshold](const std::shared_ptr<TreeNode>& node) {
    return node->total_samples() >= threshold;
  };
}

/// Predicate for nodes with self samples above threshold
inline NodePredicate self_samples_above(uint64_t threshold) {
  return [threshold](const std::shared_ptr<TreeNode>& node) {
    return node->self_samples() >= threshold;
  };
}

/// Predicate for nodes matching function name
inline NodePredicate function_name_equals(const std::string& name) {
  return [name](const std::shared_ptr<TreeNode>& node) {
    return node->frame().function_name == name;
  };
}

/// Predicate for nodes containing function name substring
inline NodePredicate function_name_contains(const std::string& substring) {
  return [substring](const std::shared_ptr<TreeNode>& node) {
    return node->frame().function_name.find(substring) != std::string::npos;
  };
}

/// Predicate for nodes in a specific library
inline NodePredicate library_name_equals(const std::string& name) {
  return [name](const std::shared_ptr<TreeNode>& node) {
    return node->frame().library_name == name;
  };
}

/// Predicate for leaf nodes
inline NodePredicate is_leaf() {
  return [](const std::shared_ptr<TreeNode>& node) {
    return node->children().empty();
  };
}

}  // namespace predicates

}  // namespace analysis
}  // namespace perflow

#endif  // PERFLOW_ANALYSIS_TREE_TRAVERSAL_H_
