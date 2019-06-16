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
class PrunerTopkOptimized: public Pruner<ScoreFun> {
public:
    /**
     * Constructor
     * @param score_fun Score function used to score the solutions
     * @param k Maximum number of elements to keep
     */
    PrunerTopkOptimized(const ScoreFun * score_fun, k_type k) :
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
        std::vector<relpos_pair_type> heap;
        heap.reserve(this->k);
        index_type i = n;
        while (i > n - this->k) {
            --i;
            heap.emplace_back(relpos_pair_type(rel_list[i], i));
        }
        heapq::heapify(heap, this->relpos_comparator_relevance);
        while (i > 0) {
            --i;
            if (rel_list[i] < heap[0].relevance) {
                continue;
            }
            heapq::replace(heap, relpos_pair_type(rel_list[i], i), this->relpos_comparator_relevance);
        }

        // fill the solution according to the heap elements
        std::sort(heap.begin(), heap.end(), this->relpos_comparator_position);
        solution.indices.resize(heap.size());
        for (i=0; i < heap.size(); ++i) {
            solution.indices[i] = heap[i].position;
        }

        return solution;
    }

public:
    /**
     * Maximum number of elements to keep
     */
    const k_type k;

private:
    typedef struct _relpos_pair {
        _relpos_pair(relevance_type relevance, index_type position) :
                relevance(relevance),
                position(position) {
        }

        relevance_type relevance;
        index_type position;
    } relpos_pair_type;

    /**
     * Support function used to compare two relpos_pair_type elements based on their relevance.
     * @param l The first element
     * @param r The second element
     * @return True iff the relevance of the first element is strictly smaller than the relevance of the second element
     */
    static bool
    relpos_comparator_relevance(const relpos_pair_type &l, const relpos_pair_type &r) {
        return l.relevance < r.relevance;
    }

    /**
     * Support function used to compare two relpos_pair_type elements based on their positions.
     * @param l The first element
     * @param r The second element
     * @return True iff the position of the first element is strictly smaller than the position of the second element
     */
    static bool
    relpos_comparator_position(const relpos_pair_type &l, const relpos_pair_type &r) {
        return l.position < r.position;
    }
};

#endif //FILTERING_PRUNING_TOPK_HPP
