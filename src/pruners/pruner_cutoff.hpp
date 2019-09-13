#ifndef PRUNERS_PRUNER_CUTOFF_HPP
#define PRUNERS_PRUNER_CUTOFF_HPP

#include "../filtering/pruner.hpp"


/**
 * Cutoff pruning
 * @tparam ScoreFun Score function type
 *
 * @note This pruning does not provide performance guarantees
 */
template <typename ScoreFun>
class PrunerCutoff: public Pruner<ScoreFun> {
public:
    /**
     * Constructor
     * @param score_fun Score function used to score the solutions
     */
    PrunerCutoff(const std::shared_ptr<ScoreFun> score_fun) :
            Pruner<ScoreFun>(score_fun) {
    }

    /**
     * Prunes the given list of relevances and returns a pruning solution containing the elements above the threshold (max+min)/2.
     * @param rel_list List containing the relevance scores, ordered according to some attribute
     * @param n Number of elements of rel_list
     * @param minmax_element The pair containing the min and maximum elements of the list
     * @return The pruning solution built on top of the given list of relevances containing only the elements above the threshold.
     */
    PrunerSolution
    operator()(const relevance_type * rel_list, const index_type n, const minmax_type &minmax_element) const {
        const relevance_type cutoff = 0.5 * minmax_element.min + 0.5 * minmax_element.max;
        PrunerSolution solution;
        solution.indices.reserve(n);
        for (index_type i = 0; i < n; ++i) {
            if (rel_list[i] >= cutoff) {
                solution.indices.push_back(i);
            }
        }

        return solution;
    }
};

#endif //PRUNERS_PRUNER_CUTOFF_HPP
