/*
 * Copyright (C) 2014-2015 Olzhas Rakhimov
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

/// @file fault_tree_analysis.h
/// Fault Tree Analysis.

#ifndef SCRAM_SRC_FAULT_TREE_ANALYSIS_H_
#define SCRAM_SRC_FAULT_TREE_ANALYSIS_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "analysis.h"
#include "boolean_graph.h"
#include "event.h"
#include "logger.h"
#include "mocus.h"
#include "preprocessor.h"
#include "settings.h"

namespace scram {

/// @class FaultTreeDescriptor
/// Fault tree description gatherer.
/// General information about a fault tree
/// described by a gate as its root.
class FaultTreeDescriptor {
 public:
  using GatePtr = std::shared_ptr<Gate>;
  using BasicEventPtr = std::shared_ptr<BasicEvent>;
  using HouseEventPtr = std::shared_ptr<HouseEvent>;

  /// Gathers all information about a fault tree with a root gate.
  ///
  /// @param[in] root  The root gate of a fault tree.
  ///
  /// @pre Gate marks must be clear.
  ///
  /// @warning If the fault tree structure is changed,
  ///          this description does not incorporate the changed structure.
  ///          Moreover, the data may get corrupted.
  explicit FaultTreeDescriptor(const GatePtr& root);

  virtual ~FaultTreeDescriptor() = default;

  /// @returns The top gate that is passed to the analysis.
  const GatePtr& top_event() const { return top_event_; }

  /// @returns The container of intermediate events.
  ///
  /// @warning If the fault tree has changed,
  ///          this is only a snapshot of the past
  const std::unordered_map<std::string, GatePtr>& inter_events() const {
    return inter_events_;
  }

  /// @returns The container of all basic events of this tree.
  ///
  /// @warning If the fault tree has changed,
  ///          this is only a snapshot of the past
  const std::unordered_map<std::string, BasicEventPtr>& basic_events() const {
    return basic_events_;
  }

  /// @returns Basic events that are in some CCF groups.
  ///
  /// @warning If the fault tree has changed,
  ///          this is only a snapshot of the past
  const std::unordered_map<std::string, BasicEventPtr>& ccf_events() const {
    return ccf_events_;
  }

  /// @returns The container of house events of the fault tree.
  ///
  /// @warning If the fault tree has changed,
  ///          this is only a snapshot of the past
  const std::unordered_map<std::string, HouseEventPtr>& house_events() const {
    return house_events_;
  }

 private:
  using EventPtr = std::shared_ptr<Event>;
  using FormulaPtr = std::unique_ptr<Formula>;

  /// Gathers information about the correctly initialized fault tree.
  /// Databases for events are manipulated
  /// to best reflect the state and structure of the fault tree.
  /// This function must be called after validation.
  /// This function must be called before any analysis is performed
  /// because there would not be necessary information
  /// available for analysis like primary events of this fault tree.
  /// Moreover, all the nodes of this fault tree
  /// are expected to be defined fully and correctly.
  /// Gates are marked upon visit.
  /// The mark is checked to prevent revisiting.
  ///
  /// @param[in] gate  The gate to start traversal from.
  void GatherEvents(const GatePtr& gate) noexcept;

  /// Traverses formulas recursively to find all events.
  ///
  /// @param[in] formula  The formula to get events from.
  void GatherEvents(const FormulaPtr& formula) noexcept;

  /// Clears marks from gates that were traversed.
  /// Marks are set to empty strings.
  /// This is important
  /// because other code may assume that marks are empty.
  void ClearMarks() noexcept;

  GatePtr top_event_;  ///< Top event of this fault tree.

  /// Container for intermediate events.
  std::unordered_map<std::string, GatePtr> inter_events_;

  /// Container for basic events.
  std::unordered_map<std::string, BasicEventPtr> basic_events_;

  /// Container for house events of the tree.
  std::unordered_map<std::string, HouseEventPtr> house_events_;

  /// Container for basic events that are identified to be in some CCF group.
  /// These basic events are not necessarily in the same CCF group.
  std::unordered_map<std::string, BasicEventPtr> ccf_events_;
};

/// @class FaultTreeAnalysis
/// Fault tree analysis functionality.
/// The analysis must be done on
/// a validated and fully initialized fault trees.
/// After initialization of the analysis,
/// the fault tree under analysis should not change;
/// otherwise, the success of the analysis is not guaranteed,
/// and the results may be invalid.
/// After the requested analysis is done,
/// the fault tree can be changed without restrictions.
/// To conduct a new analysis on the changed fault tree,
/// a new FaultTreeAnalysis object must be created.
/// In general, rerunning the same analysis twice
/// will mess up the analysis and corrupt the previous results.
///
/// @warning Run analysis only once.
///          One analysis per FaultTreeAnalysis object.
class FaultTreeAnalysis : public Analysis, public FaultTreeDescriptor {
 public:
  using GatePtr = std::shared_ptr<Gate>;
  using CutSet = std::set<std::string>;  ///< Cut set with basic event IDs.

  /// Traverses a valid fault tree from the root gate
  /// to collect databases of events, gates,
  /// and other members of the fault tree.
  /// The passed fault tree must be pre-validated without cycles,
  /// and its events must be fully initialized.
  ///
  /// @param[in] root  The top event of the fault tree to analyze.
  /// @param[in] settings  Analysis settings for all calculations.
  ///
  /// @note It is assumed that analysis is done only once.
  ///
  /// @warning If the fault tree structure is changed,
  ///          this analysis does not incorporate the changed structure.
  ///          Moreover, the analysis results may get corrupted.
  FaultTreeAnalysis(const GatePtr& root, const Settings& settings);

  virtual ~FaultTreeAnalysis() = default;

  /// Analyzes the fault tree and performs computations.
  /// This function must be called
  /// only after initializing the fault tree
  /// with or without its probabilities.
  ///
  /// @note This function is expected to be called only once.
  ///
  /// @warning If the original fault tree is invalid,
  ///          this function will not throw or indicate any errors.
  ///          The behavior is undefined for invalid fault trees.
  /// @warning If the fault tree structure has changed
  ///          since the construction of the analysis,
  ///          the analysis will be invalid or fail.
  /// @warning The gates' visit marks must be clean.
  virtual void Analyze() noexcept = 0;

  /// @returns Set with minimal cut sets.
  ///
  /// @note The user should make sure that the analysis is actually done.
  const std::set<CutSet>& min_cut_sets() const { return min_cut_sets_; }

  /// @returns Collection of basic events that are in the minimal cut sets.
  const std::unordered_map<std::string, BasicEventPtr>&
      mcs_basic_events() const {
    return mcs_basic_events_;
  }

  /// @returns Map with minimal cut sets and their probabilities.
  ///
  /// @note The user should make sure that the analysis is actually done.
  const std::map<CutSet, double>& mcs_probability() const {
    assert(kSettings_.probability_analysis());
    return mcs_probability_;
  }

  /// @returns The sum of minimal cut set probabilities.
  ///
  /// @note This value is the same as the rare-event approximation.
  double sum_mcs_probability() const {
    assert(kSettings_.probability_analysis());
    return sum_mcs_probability_;
  }

  /// @returns The maximum order of the found minimal cut sets.
  int max_order() const { return max_order_; }

 protected:
  /// Converts minimal cut sets from indices to strings
  /// for future reporting.
  /// This function also collects basic events in minimal cut sets
  /// and calculates their probabilities.
  ///
  /// @param[in] imcs  Min cut sets with indices of events.
  /// @param[in] ft  Indexed fault tree with basic event indices and pointers.
  ///
  /// @todo Probability calculation feels more like a hack than design.
  void SetsToString(const std::vector<Set>& imcs,
                    const BooleanGraph* ft) noexcept;

  /// Container for minimal cut sets.
  std::set<CutSet> min_cut_sets_;

  /// Container for basic events in minimal cut sets.
  std::unordered_map<std::string, BasicEventPtr> mcs_basic_events_;

  /// Container for minimal cut sets and their respective probabilities.
  std::map<CutSet, double> mcs_probability_;

  double sum_mcs_probability_;  ///< The sum of minimal cut set probabilities.
  int max_order_;  ///< Maximum order of minimal cut sets.
};

/// @class FaultTreeAnalyzer
///
/// @tparam Algorithm Fault tree analysis algorithm.
///
/// Fault tree analysis facility with specific algorithms.
/// This class is meant to be specialized by fault tree analysis algorithms.
template<typename Algorithm>
class FaultTreeAnalyzer : public FaultTreeAnalysis {
 public:
  /// Constructor with a fault tree and analysis settings.
  using FaultTreeAnalysis::FaultTreeAnalysis;

  /// Runs fault tree analysis with the given algorithm.
  void Analyze() noexcept;

  /// @returns Pointer to the analysis algorithm.
  const Algorithm* algorithm() const { return algorithm_.get(); }

  /// @returns Pointer to the Boolean graph representing the fault tree.
  const BooleanGraph* graph() const { return graph_.get(); }

 protected:
  std::unique_ptr<Algorithm> algorithm_;  ///< Analysis algorithm.
  std::unique_ptr<BooleanGraph> graph_;  ///< Boolean graph of the fault tree.
};

template<typename Algorithm>
void FaultTreeAnalyzer<Algorithm>::Analyze() noexcept {
  CLOCK(analysis_time);

  CLOCK(ft_creation);
  graph_ = std::unique_ptr<BooleanGraph>(new BooleanGraph(
          FaultTreeDescriptor::top_event(), kSettings_.ccf_analysis()));
  LOG(DEBUG2) << "Boolean graph is created in " << DUR(ft_creation);

  CLOCK(prep_time);  // Overall preprocessing time.
  LOG(DEBUG2) << "Preprocessing...";
  Preprocessor* preprocessor = new CustomPreprocessor<Algorithm>(graph_.get());
  preprocessor->Run();
  delete preprocessor;  // No exceptions are expected.
  LOG(DEBUG2) << "Finished preprocessing in " << DUR(prep_time);

  algorithm_ =
      std::unique_ptr<Algorithm>(new Algorithm(graph_.get(), kSettings_));
  algorithm_->FindMcs();
  analysis_time_ = DUR(analysis_time);  // Duration of MCS generation.
  FaultTreeAnalysis::SetsToString(algorithm_->GetGeneratedMcs(), graph_.get());
}

}  // namespace scram

#endif  // SCRAM_SRC_FAULT_TREE_ANALYSIS_H_
