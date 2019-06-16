#ifndef FILTERING_PRUNING_TOPK_HPP
#define FILTERING_PRUNING_TOPK_HPP

#include <vector>
#include "heapq.hpp"
#include "pruner.hpp"


/**
 * Topk pruning
 * @tparam ScoreFun Score function type
 *
 * @note This pruning guarantees only the 0.5-optimality
 */
template <typename ScoreFun>
class PrunerTopk: public Pruner<ScoreFun> {
public:
    /**
     * Constructor
     * @param score_fun Score function used to score the solutions
     * @param k Maximum number of elements to keep
     */
    PrunerTopk(const ScoreFun * score_fun, k_type k) :
            Pruner<ScoreFun>(score_fun),
            k(k) {
    }

    /**
     * Prunes the given list of relevances and returns a pruning solution containing the k greatest elements of
     * rel_list, in the same order they appear in rel_list.
     * @param rel_list List containing the relevance scores, ordered according to some attribute
     * @param n Number of elements of rel_list
     * @param minmax_element The pair containing the min and maximum elements of the list
     * @return The pruning solution built on top of the given list of relevances containing only the k greatest elements
     */
    PrunerSolution
    operator()(const relevance_type * rel_list, const index_type n, const minmax_type &minmax_element) const {
        (void)(minmax_element); // to suppress the unused parameter warning

        PrunerSolution solution;
        if (n <= this->k) {
            solution.indices.resize(n);
            for (index_type i=0; i < n; ++i) {
                solution.indices[i] = i;
            }
            return solution;
        }

        // fill the heap with the top-k
        std::vector<relevance_type> heap(this->k);
        for (std::size_t i = 0, i_end = this->k; i < i_end; ++i) {
            heap[i] = rel_list[i];
        }
        heapq::heapify(heap);
        for (std::size_t i = this->k; i < n; ++i) {
            if (rel_list[i] < heap[0]) {
                continue;
            }
            heapq::replace(heap, rel_list[i]);
        }

        // fill the solution according to the heap elements and preserving the sort by attribute
        solution.indices.reserve(heap.size());
        for (std::size_t i = 0; i < n; ++i) {
            if (rel_list[i] < heap[0]) {
                continue;
            }

            solution.indices.push_back(i);
            if (rel_list[i] == heap[0]) {
                heapq::pop(heap);
                if (heap.empty()) {
                    break;
                }
            }
        }

        return solution;
    }

public:
    /**
     * Maximum number of elements to keep
     */
    const k_type k;
};

#endif //FILTERING_PRUNING_TOPK_HPP
