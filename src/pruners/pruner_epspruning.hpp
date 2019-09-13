#ifndef PRUNERS_PRUNER_EPSPRUNING_HPP
#define PRUNERS_PRUNER_EPSPRUNING_HPP

#include <algorithm>
#include <cassert>
#include <cmath>
#include <vector>
#include "../data_structures/heapq.hpp"
#include "../filtering/pruner.hpp"


/**
 * Epsilon pruning.
 * @tparam ScoreFun Score function type
 *
 * @note This pruning guarantees the (1-epsilon)-optimality. It implements the pruning described in the paper by Nardini et al.
 * "Fast Approximate Filtering of Search Results Sorted by Attribute"
 */
template <typename ScoreFun>
class PrunerEpsPruning: public Pruner<ScoreFun> {
public:
    /**
     * Constructor
     * @param score_fun Score function used to score the solutions
     * @param k Maximum number of elements to keep
     * @param epsilon Maximum approximation error
     */
    PrunerEpsPruning(const std::shared_ptr<ScoreFun> score_fun, k_type k, score_type epsilon) :
            Pruner<ScoreFun>(score_fun),
            k(k),
            epsilon(epsilon) {
    }

    /**
     * Prunes the given list of relevances and returns a pruning solution containing the elements that can compose
     * a (1-epsilon)-optimal filtering solution.
     * @param rel_list List containing the relevance scores, ordered according to some attribute
     * @param n Number of elements of rel_list
     * @param minmax_element The pair containing the min and maximum elements of the list
     * @return The pruning solution built on top of the given list of relevances
     */
    PrunerSolution
    operator()(const relevance_type * rel_list, const index_type n, const minmax_type &minmax_element) const {
        const score_type delta = (1 - this->epsilon);
        const ScoreFun & score_fun = *(this->score_fun.get());

        const score_type max_gain = score_fun.gain_factor(minmax_element.max);
        const score_type min_gain = std::max(
                // min element
                score_fun.gain_factor(minmax_element.min),
                // the contribution of all elements after M must not be over epsilon times M
                (this->epsilon * max_gain * score_fun.discount_factor(1)) / (delta * score_fun.discount_factor_sum(2, this->k))
        ) * (1.0 - 1e-16);  // workaround to fix numerical instability
        relevance_type min_threshold = score_fun.gain_factor_inverse(min_gain);
        for (std::size_t i = 16;
             i > 0 && score_fun.gain_factor(min_threshold) > min_gain; --i) {  // workaround to fix numerical instability
            min_threshold = score_fun.gain_factor_inverse(min_gain - std::pow(0.1, i));
        }
//    while (score_fun.gain_factor(min_threshold) > min_gain) {  // workaround to fix numerical instability
//        min_threshold *= 1.0 - 1e-16;
//    }

        // compute the number of intervals
        std::vector<relevance_type> interval_boundaries(
                1 + static_cast<std::size_t>(1 + std::ceil(std::log2(min_gain / max_gain) / std::log2(delta)))
        );
        // and fill the boundaries vector with all the boundaries
        double v = max_gain;
        for (std::size_t i = interval_boundaries.size(); i > 0; --i) {
            interval_boundaries[i - 1] = score_fun.gain_factor_inverse(v);
            v *= delta;
        }
        interval_boundaries.back() = minmax_element.max; // fix the error of the last interval due to the inverse operation
        assert(interval_boundaries[0] <= min_threshold);

        // output pruned list
        PrunerSolution solution;
        solution.indices.reserve(std::min(interval_boundaries.size() * this->k, static_cast<std::size_t>(n)));

        // heap used to prune the rel_list
        // fill it with the last k elements on the right that pass the min_threshold
        std::vector<relevance_type> heap;
        heap.reserve(this->k);
        std::size_t i = n;
        while (i > 0) {
            --i;
            if (rel_list[i] >= min_threshold) {
                solution.indices.push_back(i);
                heap.push_back(rel_list[i]);

                if (heap.size() == this->k) {
                    break;
                }
            }
        }

        // heapify
        heapq::heapify(heap);

        // min interval id
        std::size_t min_interval_id = 0;
        while (interval_boundaries[min_interval_id] < heap[0]) {
            ++min_interval_id;
        }
        min_threshold = interval_boundaries[min_interval_id];

        while (i > 0) {
            --i;
            if (rel_list[i] <= min_threshold) {
                continue;
            }
            solution.indices.push_back(i);
            heapq::replace(heap, rel_list[i]);

            // update min_interval_id and threshold
            if (interval_boundaries[min_interval_id] < heap[0]) {
                ++min_interval_id;
                while (interval_boundaries[min_interval_id] < heap[0]) {
                    ++min_interval_id;
                }
                if (min_interval_id == (interval_boundaries.size() - 1)) {
                    break;
                }
                min_threshold = interval_boundaries[min_interval_id];
            }
        }

        std::reverse(solution.indices.begin(), solution.indices.end());

        return solution;
    }

public:
    /**
     * Maximum number of elements to keep
     */
    const k_type k;

    /**
     * Maximum approximation error
     */
    const score_type epsilon;
};

#endif //PRUNERS_PRUNER_EPSPRUNING_HPP
