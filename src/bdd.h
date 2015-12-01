/*
 * Copyright (C) 2015 Olzhas Rakhimov
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/// @file bdd.h
/// Fault tree analysis with the Binary Decision Diagram algorithms.

#ifndef SCRAM_SRC_BDD_H_
#define SCRAM_SRC_BDD_H_

#include <array>
#include <functional>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include <boost/functional/hash.hpp>
#include <boost/unordered_map.hpp>

#include "boolean_graph.h"
#include "settings.h"

namespace scram {

/// @class Vertex
/// Representation of a vertex in BDD graphs.
class Vertex {
 public:
  /// @param[in] terminal  Flag for terminal nodes.
  /// @param[in] id  Identificator of the BDD graph.
  explicit Vertex(bool terminal = false, int id = 0);

  Vertex(const Vertex&) = delete;
  Vertex& operator=(const Vertex&) = delete;

  virtual ~Vertex() = 0;  ///< Abstract class.

  /// @returns true if this vertex is terminal.
  bool terminal() const { return terminal_; }

  /// @returns Identificator of the BDD graph rooted by this vertex.
  int id() const { return id_; }

 protected:
  int id_;  ///< Unique identifier of the BDD graph with this vertex.

 private:
  bool terminal_;  ///< Flag for terminal vertices. RTTI hack.
};

/// @class Terminal
/// Representation of terminal vertices in BDD graphs.
/// It is expected
/// that in a reduced BDD graphs,
/// there are at most two terminal vertices of value 1 or 0.
/// If the BDD graph has attributed edges,
/// only single terminal vertex is expected with value 1.
class Terminal : public Vertex {
 public:
  /// @param[in] value  True or False (1 or 0) terminal.
  explicit Terminal(bool value);

  /// @returns The value of the terminal vertex.
  ///
  /// @note The value serves as an id for this terminal vertex.
  ///       Non-terminal if-then-else vertices should never have
  ///       identifications of value 0 or 1.
  bool value() const { return value_; }

  /// Recovers a shared pointer to Terminal from a pointer to Vertex.
  ///
  /// @param[in] vertex  Pointer to a Vertex known to be a Terminal.
  ///
  /// @return Casted pointer to Terminal.
  static std::shared_ptr<Terminal> Ptr(const std::shared_ptr<Vertex>& vertex) {
    return std::static_pointer_cast<Terminal>(vertex);
  }

 private:
  bool value_;  ///< The meaning of the terminal.
};

using VertexPtr = std::shared_ptr<Vertex>;  ///< Shared BDD vertices.
using TerminalPtr = std::shared_ptr<Terminal>;  ///< Shared terminal vertices.

/// @class NonTerminal
/// Representation of non-terminal vertices in BDD graphs.
class NonTerminal : public Vertex {
 public:
  /// @param[in] index  Index of this non-terminal vertex.
  /// @param[in] order  Specific ordering number for BDD graphs.
  NonTerminal(int index, int order);

  virtual ~NonTerminal() = 0;  ///< Abstract base class.

  /// @returns The index of this vertex.
  int index() const { return index_; }

  /// @returns The order of the vertex.
  int order() const {
    assert(order_ > 0);
    return order_;
  }

  /// @returns true if this vertex represents a module gate.
  bool module() const { return module_; }

  /// Sets this vertex for representation of a module.
  void module(bool flag) { module_ = flag; }

  using Vertex::id;  ///< Conflicting with the overload.

  /// Sets the unique identifier of the ROBDD graph.
  ///
  /// @param[in] id  Unique identifier of the ROBDD graph.
  ///                The identifier should not collide
  ///                with the identifiers of terminal nodes.
  void id(int id) {
    assert(id > 1);  // Must not have an ID of terminal nodes.
    Vertex::id_ = id;
  }

  /// @returns (1/True/then/left) branch if-then-else vertex.
  const VertexPtr& high() const { return high_; }

  /// Sets the (1/True/then/left) branch vertex.
  ///
  /// @param[in] high  The if-then-else vertex.
  void high(const VertexPtr& high) { high_ = high; }

  /// @returns (0/False/else/right) branch vertex.
  ///
  /// @note This edge may have complement interpretation.
  ///       Check complement_edge() upon using this edge.
  const VertexPtr& low() const { return low_; }

  /// Sets the (0/False/else/right) branch vertex.
  ///
  /// @param[in] low  The vertex.
  ///
  /// @note This may have complement interpretation.
  ///       Keep the complement_edge() flag up-to-date
  ///       after setting this edge.
  void low(const VertexPtr& low) { low_ = low; }

  /// @returns The mark of this vertex.
  bool mark() const { return mark_; }

  /// Marks this vertex.
  ///
  /// @param[in] flag  A flag with the meaning for the user of marks.
  void mark(bool flag) { mark_ = flag; }

 protected:
  int index_;  ///< Index of the variable.
  int order_;  ///< Order of the variable.
  bool module_;  ///< Mark for module variables.
  VertexPtr high_;  ///< 1 (True/then) branch in the Shannon decomposition.
  VertexPtr low_;  ///< O (False/else) branch in the Shannon decomposition.
  bool mark_;  ///< Traversal mark.
};

/// @class ComplementEdge
/// Mixin for vertices
/// that may need to indicate
/// that one of edges must be interpreted as complement.
///
/// @note This is not about the vertex,
///       but it is about the chosen edge.
class ComplementEdge {
 public:
  /// Sets the complement edge to false by default.
  ComplementEdge();

  virtual ~ComplementEdge() = 0;  ///< Abstract class.

  /// @returns true if the chosen edge is complement.
  bool complement_edge() const { return complement_edge_; }

  /// Sets the complement flag for the chosen edge.
  ///
  /// @param[in] flag  Indicator to treat the chosen edge as a complement.
  void complement_edge(bool flag) { complement_edge_ = flag; }

 private:
  bool complement_edge_;  ///< Flag for complement edge.
};

/// @class Ite
/// Representation of non-terminal if-then-else vertices in BDD graphs.
/// This class is designed to help construct and manipulate BDD graphs.
///
/// @note This class provides one attributed complement edge.
///       The BDD data structure must choose
///       one of two (high/low) edges to assign the attribute.
///       Consistency is not the responsibility of this class
///       but of BDD algorithms and users.
class Ite : public NonTerminal, public ComplementEdge {
 public:
  using NonTerminal::NonTerminal;  ///< Constructor with index and order.

  /// @returns The probability of the function graph.
  double p() const { return p_; }

  /// Sets the probability of the function graph.
  ///
  /// @param[in] value  Calculated value for the probability.
  void p(double value) { p_ = value; }

  /// @returns Saved results of importance factor calculations.
  double factor() const { return factor_; }

  /// Memorized results of importance factor calculations.
  ///
  /// @param[in] value  Calculation results for importance factor.
  void factor(double value) { factor_ = value; }

  /// Recovers a shared pointer to Ite from a pointer to Vertex.
  ///
  /// @param[in] vertex  Pointer to a Vertex known to be an Ite.
  ///
  /// @return Casted pointer to Ite.
  static std::shared_ptr<Ite> Ptr(const std::shared_ptr<Vertex>& vertex) {
    return std::static_pointer_cast<Ite>(vertex);
  }

 private:
  double p_ = 0;  ///< Probability of the function graph.
  double factor_ = 0;  ///< Importance factor calculation results.
};

using ItePtr = std::shared_ptr<Ite>;  ///< Shared if-then-else vertices.
using IteWeakPtr = std::weak_ptr<Ite>;  ///< Pointer for storage outside of BDD.

using Triplet = std::array<int, 3>;  ///< (v, G, H) triplet for functions.

/// @struct TripletHash
/// Functor for hashing triplets of ordered numbers.
struct TripletHash : public std::unary_function<const Triplet, std::size_t> {
  /// Operator overload for hashing three ordered ID numbers.
  ///
  /// @param[in] triplet (v, G, H) nodes.
  ///
  /// @returns Hash value of the triplet.
  std::size_t operator()(const Triplet& triplet) const noexcept {
    return boost::hash_range(triplet.begin(), triplet.end());
  }
};

/// Hash table for triplets of numbers as keys.
///
/// @tparam Value  Type of values to be stored in the table.
template<typename Value>
using TripletTable = std::unordered_map<Triplet, Value, TripletHash>;

class Zbdd;  // For analysis purposes.

/// @class Bdd
/// Analysis of Boolean graphs with Binary Decision Diagrams.
/// This binary decision diagram data structure
/// represents Reduced Ordered BDD with attributed edges.
///
/// @note The low/else edge is chosen to have the attribute for an ITE vertex.
///       There is only one terminal vertex of value 1/True.
class Bdd {
 public:
  /// Constructor with the analysis target.
  /// Reduced Ordered BDD is produced from a Boolean graph.
  ///
  /// @param[in] fault_tree  Preprocessed, partially normalized,
  ///                        and indexed fault tree.
  /// @param[in] settings  The analysis settings.
  ///
  /// @note The passed Boolean graph must already have variable ordering.
  /// @note BDD construction may take considerable time.
  Bdd(const BooleanGraph* fault_tree, const Settings& settings);

  /// To handle incomplete ZBDD type with unique pointers.
  ~Bdd() noexcept;

  /// @struct Function
  /// Holder of computation resultant functions and gate representations.
  struct Function {
    bool complement;  ///< The interpretation of the function.
    VertexPtr vertex;  ///< The root vertex of the BDD function graph.
  };

  /// @returns The root function of the ROBDD.
  const Function& root() const { return root_; }

  /// @returns Mapping of Boolean graph modules and BDD graph vertices.
  const std::unordered_map<int, Function>& modules() const { return modules_; }

  /// @returns Mapping of variable indices to their orders.
  const std::unordered_map<int, int>& index_to_order() const {
    return index_to_order_;
  }

  /// Helper function to clear and set vertex marks.
  ///
  /// @param[in] mark  Desired mark for BDD vertices.
  ///
  /// @note Marks will propagate to modules as well.
  ///
  /// @warning If the graph is discontinuously and partially marked,
  ///          this function will not help with the mess.
  void ClearMarks(bool mark) { Bdd::ClearMarks(root_.vertex, mark); }

  /// Runs the Qualitative analysis
  /// with the representation of a Boolean graph
  /// as ROBDD.
  ///
  /// @pre Only coherent analysis is requested.
  void Analyze() noexcept;

  /// @returns Cut sets generated by the analysis.
  ///
  /// @pre Analysis is done.
  const std::vector<std::vector<int>>& cut_sets() const;

 private:
  using UniqueTable = TripletTable<IteWeakPtr>;  ///< To keep BDD reduced.
  /// To store computed results with an ordered pair of arguments.
  /// This table introduces circular reference
  /// if one of the arguments is the computation result.
  using ComputeTable = boost::unordered_map<std::pair<int, int>, Function>;

  /// @class GarbageCollector
  /// This garbage collector manages tables of a BDD.
  /// The garbage collection is triggered
  /// when the reference count of a BDD vertex reaches 0.
  class GarbageCollector {
   public:
    /// @param[in,out] bdd  BDD to manage.
    explicit GarbageCollector(Bdd* bdd) noexcept : bdd_(bdd) {}

    /// Frees the memory
    /// and triggers the garbage collection ONLY if requested.
    ///
    /// @param[in] ptr  Pointer to an ITE vertex with reference count 0.
    void operator()(Ite* ptr) noexcept {
      if (bdd_->garbage_collection_) {
        bdd_->unique_table_.erase(
            {ptr->index(),
             ptr->high()->id(),
             (ptr->complement_edge() ? -1 : 1) * ptr->low()->id()});
      }
      delete ptr;
    }

   private:
    Bdd* bdd_;  ///< Pointer to the managed BDD.
  };

  /// Fetches a unique if-then-else vertex from a hash table.
  /// If the vertex doesn't exist,
  /// a new vertex is created.
  ///
  /// @param[in] index  Positive index of the variable.
  /// @param[in] high  The high vertex.
  /// @param[in] low  The low vertex.
  /// @param[in] complement_edge  Interpretation of the low vertex.
  /// @param[in] order The order for the vertex variable.
  /// @param[in] module  A flag for the modular ZBDD proxy.
  ///
  /// @returns If-then-else node with the given parameters.
  ///
  /// @pre Expired pointers in the unique table are garbage collected.
  /// @pre Only pointers in the unique table are
  ///      either in the BDD or in the computation table.
  ItePtr FetchUniqueTable(int index, const VertexPtr& high,
                          const VertexPtr& low, bool complement_edge,
                          int order, bool module) noexcept;

  /// Converts all gates in the Boolean graph
  /// into if-then-else BDD graphs.
  /// Registers processed gates.
  ///
  /// @param[in] gate  The root or current parent gate of the graph.
  /// @param[in,out] gates  Processed gates.
  ///
  /// @returns The BDD function representing the gate.
  const Function& IfThenElse(const IGatePtr& gate,
                             std::unordered_map<int, Function>* gates) noexcept;

  /// Fetches computation tables for results.
  ///
  /// @param[in] type  The operator or type of the gate.
  /// @param[in] arg_one  First argument function graph.
  /// @param[in] arg_two  Second argument function graph.
  /// @param[in] complement_one  Interpretation of arg_one as complement.
  /// @param[in] complement_two  Interpretation of arg_two as complement.
  ///
  /// @returns New function with nullptr vertex pointer
  ///          for uploading the computation results
  ///          if it doesn't exists.
  /// @returns If computation is already performed,
  ///          the non-null result vertex with the return function.
  ///
  /// @note The arguments should not be the same functions.
  ///       It is assumed
  ///       that equal ID functions are handled by the reduction.
  /// @note Even though the arguments are not ItePtr,
  ///       it is expected that they are if-then-else vertices.
  Function& FetchComputeTable(Operator type, const VertexPtr& arg_one,
                              const VertexPtr& arg_two, bool complement_one,
                              bool complement_two) noexcept;

  /// Applies Boolean operation to BDD graphs.
  ///
  /// @param[in] type  The operator or type of the gate.
  /// @param[in] arg_one  First argument function graph.
  /// @param[in] arg_two  Second argument function graph.
  /// @param[in] complement_one  Interpretation of arg_one as complement.
  /// @param[in] complement_two  Interpretation of arg_two as complement.
  ///
  /// @returns The BDD function as a result of operation.
  ///
  /// @note The order of arguments does not matter for two variable operators.
  Function Apply(Operator type,
                 const VertexPtr& arg_one, const VertexPtr& arg_two,
                 bool complement_one, bool complement_two) noexcept;

  /// Applies the logic of a Boolean operator
  /// to terminal vertices.
  ///
  /// @param[in] type  The operator to apply.
  /// @param[in] term_one  First argument terminal vertex.
  /// @param[in] term_two  Second argument terminal vertex.
  /// @param[in] complement_one  Interpretation of term_one as complement.
  /// @param[in] complement_two  Interpretation of term_two as complement.
  ///
  /// @returns The resulting BDD function.
  Function Apply(Operator type,
                 const TerminalPtr& term_one, const TerminalPtr& term_two,
                 bool complement_one, bool complement_two) noexcept;

  /// Applies the logic of a Boolean operator
  /// to non-terminal and terminal vertices.
  ///
  /// @param[in] type  The operator or type of the gate.
  /// @param[in] ite_one  Non-terminal vertex.
  /// @param[in] term_two  Terminal vertex.
  /// @param[in] complement_one  Interpretation of ite_one as complement.
  /// @param[in] complement_two  Interpretation of term_two as complement.
  ///
  /// @returns The resulting BDD function.
  Function Apply(Operator type,
                 const ItePtr& ite_one, const TerminalPtr& term_two,
                 bool complement_one, bool complement_two) noexcept;

  /// Applies Boolean operation for a special case of the same arguments.
  ///
  /// @param[in] type  The operator or type of the gate.
  /// @param[in] single_arg  One of two identical arguments.
  /// @param[in] complement_one  Interpretation of the first vertex argument.
  /// @param[in] complement_two  Interpretation of the second vertex argument.
  ///
  /// @returns The BDD function as a result of operation.
  Function Apply(Operator type, const VertexPtr& single_arg,
                 bool complement_one, bool complement_two) noexcept;

  /// Applies Boolean operation to BDD graph non-terminal vertices.
  ///
  /// @param[in] type  The operator or type of the gate.
  /// @param[in] arg_one  First argument if-then-else vertex.
  /// @param[in] arg_two  Second argument if-then-else vertex.
  /// @param[in] complement_one  Interpretation of arg_one as complement.
  /// @param[in] complement_two  Interpretation of arg_two as complement.
  ///
  /// @returns High and Low BDD functions as a result of operation.
  ///
  /// @pre Argument if-then-else vertices must be ordered.
  std::pair<Function, Function> Apply(Operator type,
                                      const ItePtr& arg_one,
                                      const ItePtr& arg_two,
                                      bool complement_one,
                                      bool complement_two) noexcept;

  /// Counts the number of if-then-else nodes.
  ///
  /// @param[in] vertex  The starting root vertex of BDD.
  ///
  /// @returns The number of ITE nodes in the BDD.
  ///
  /// @pre Non-terminal node marks are clear (false).
  int CountIteNodes(const VertexPtr& vertex) noexcept;

  /// Clears marks of vertices in BDD graph.
  ///
  /// @param[in] vertex  The starting root vertex of the graph.
  /// @param[in] mark  The desired mark for the vertices.
  ///
  /// @note Marks will propagate to modules as well.
  void ClearMarks(const VertexPtr& vertex, bool mark) noexcept;

  /// Checks BDD graphs for errors in the structure.
  /// Errors are assertions that fail at runtime.
  ///
  /// @param[in] vertex  The root vertex of BDD.
  ///
  /// @pre Non-terminal node marks are clear (false).
  void TestStructure(const VertexPtr& vertex) noexcept;

  const Settings kSettings_;  ///< Analysis settings.
  Function root_;  ///< The root function of this BDD.

  /// Table of unique if-then-else nodes denoting function graphs.
  /// The key consists of ite(index, id_high, id_low),
  /// where IDs are unique (id_high != id_low) identifications of
  /// unique reduced-ordered function graphs.
  UniqueTable unique_table_;

  /// Table of processed computations over functions.
  /// The argument functions are recorded with their IDs (not vertex indices).
  /// In order to keep only unique computations,
  /// the argument IDs must be ordered.
  /// The key is {min_id, max_id}.
  ComputeTable and_table_;  ///< Table of processed AND computations.
  ComputeTable or_table_;  ///< Table of processed OR computations.

  std::unordered_map<int, Function> modules_;  ///< Module graphs.
  std::unordered_map<int, int> index_to_order_;  ///< Indices and orders.
  const TerminalPtr kOne_;  ///< Terminal True.
  int function_id_;  ///< Identification assignment for new function graphs.
  bool garbage_collection_;  ///< Switch for garbage collection.
  std::unique_ptr<Zbdd> zbdd_;  ///< ZBDD as a result of analysis.
};

}  // namespace scram

#endif  // SCRAM_SRC_BDD_H_
