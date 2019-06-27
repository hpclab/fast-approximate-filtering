#ifndef FILTERING_ASSESSMENT_HPP
#define FILTERING_ASSESSMENT_HPP

#include <vector>
#include "filter.hpp"
#include "pruner.hpp"
#include "types.hpp"
#include "utils.hpp"


/**
 * Representation of a test on a single list.
 */
typedef struct {
    /**
     * Score of the solution
     */
    score_type score = 0;
    /**
     * Indices of the elements composing the solution
     */
    std::vector<index_type> indices;
    /**
     * Num elements pruned in the first stage
     */
    index_type num_elements_pruned = 0;
    /**
     * Num elements not pruned in the first stage
     */
    index_type num_elements_not_pruned = 0;
    /**
     * Time spent in the first stage (pruning)
     */
    double first_stage_time = 0;
    /**
     * Time spent in the second stage (filtering)
     */
    double second_stage_time = 0;
    /**
     * Time spent in filtering the list (pruning + filtering)
     */
    double total_time = 0;
} SingleTestOutcome;


/**
 * Representation of a test on multiple lists.
 */
typedef struct {
    /**
     * Average score
     */
    double avg_score = 0;
    /**
     * Maximum approximation error
     */
    double max_approximation_error = 0;
    /**
     * Average approximation error
     */
    double avg_approximation_error = 0;
    /**
     * Average number of elements pruned in the first stage
     */
    double avg_num_elements_pruned = 0;
    /**
     * Average number of elements not pruned in the first stage
     */
    double avg_num_elements_not_pruned = 0;
    /**
     * Average time spent in the first stage (pruning)
     */
    double avg_first_stage_time = 0;
    /**
     * Average time spent in the second stage (filtering)
     */
    double avg_second_stage_time = 0;
    /**
     * Average time spent in filtering the lists (pruning + filtering)
     */
    double avg_total_time = 0;
} MultipleTestOutcome;


/**
 * Abstraction used to arbitrarily compose pruning and filtering strategies.
 * @tparam ScoreFun Score function type
 */
template <typename ScoreFun>
class PrunerFilterCompositionTest {
public:
    /**
     * Constructor of an arbitrary composition
     * @param pruner The pruner to use in the first stage
     * @param filter The filter to use in the second stage
     * @param num_runs The number of runs each test must be repeated (in order to have more accurate timings)
     */
    PrunerFilterCompositionTest(const Pruner<ScoreFun> * pruner, const Filter<ScoreFun> * filter, const int num_runs) :
            pruner(pruner),
            filter(filter),
            num_runs(num_runs) {
        if (this->filter == nullptr) {
            throw std::runtime_error("The parameter filter must be not null");
        }
        if (this->num_runs <= 0) {
            throw std::runtime_error("The parameter num_runs must be a strictly positive number");
        }
    }

    /**
     * Filters the given list of relevances and returns a the outcome of the filtering@k.
     * @param rel_list List containing the relevance scores, ordered according to some attribute
     * @param n Number of elements of rel_list
     * @return The filtering solution built on top of the given list of relevances
     */
    SingleTestOutcome
    operator()(const relevance_type * rel_list, const index_type n, const minmax_type &minmax_element) {
        SingleTestOutcome solution;
        FilterSolution filteringSolution;

        if (this->pruner != nullptr) {
            // First stage
            solution.first_stage_time = get_time_milliseconds();

            PrunerSolution pruningSolution = this->pruner->operator()(rel_list, n, minmax_element);
            for (int run = 0; run < this->num_runs; ++run) {
                doNotOptimizeAway(this->pruner->operator()(rel_list, n, minmax_element).size());
            }

            solution.first_stage_time = (get_time_milliseconds() - solution.first_stage_time) / this->num_runs;

            index_type n2 = pruningSolution.size();
            solution.num_elements_pruned = n - n2;
            solution.num_elements_not_pruned = n2;

            // create the list for the second stage
            relevance_type *new_rel_list = new relevance_type[n2];
            for (index_type i = 0; i < n2; ++i) {
                new_rel_list[i] = rel_list[pruningSolution.indices[i]];
            }

            // Second stage
            solution.second_stage_time = get_time_milliseconds();

            filteringSolution = this->filter->operator()(new_rel_list, n2);
            for (int run=0; run < this->num_runs; ++run) {
                doNotOptimizeAway(this->filter->operator()(new_rel_list, n2).size());
            }

            solution.second_stage_time = (get_time_milliseconds() - solution.second_stage_time) / this->num_runs;
            delete[](new_rel_list);

            // update the indices according to the results of the first stage
            for (index_type i=0, i_end=filteringSolution.size(); i < i_end; ++i) {
                filteringSolution.indices[i] = pruningSolution.indices[filteringSolution.indices[i]];
            }
        } else {
            // Second stage
            solution.second_stage_time = get_time_milliseconds();

            filteringSolution = this->filter->operator()(rel_list, n);
            for (int run=0; run < this->num_runs; ++run) {
                doNotOptimizeAway(this->filter->operator()(rel_list, n).size());
            }

            solution.second_stage_time = (get_time_milliseconds() - solution.second_stage_time) / this->num_runs;
        }

        // fill the remaining properties
        solution.score = filteringSolution.score;
        solution.indices = std::move(filteringSolution.indices);
        solution.total_time = solution.first_stage_time + solution.second_stage_time;

        return solution;
    }

public:
    /**
     * The pruner used in the first stage
     */
    const Pruner<ScoreFun> * pruner;
    /**
     * The filter used in the second stage
     */
    const Filter<ScoreFun> * filter;
    /**
     * The number of times each test must be repeated
     */
    int num_runs;
};


#endif //FILTERING_ASSESSMENT_HPP
