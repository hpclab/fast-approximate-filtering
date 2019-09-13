#ifndef FILTERING_UTILS_ASSESSMENT_HPP
#define FILTERING_UTILS_ASSESSMENT_HPP

#include <memory>
#include <string>
#include <vector>
#include "../filtering/filter.hpp"
#include "../filtering/pruner.hpp"
#include "../filtering/types.hpp"
#include "../utils/utils.hpp"


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
} TestOutcome;


/**
 * Representation of a test on multiple lists.
 */
typedef struct tests_aggregation_outcome {
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

    void
    update_aggregation(
            const TestOutcome &test_outcome,
            const std::size_t num_lists_previously_assessed,
            const score_type optimal_score=-1
    ) {
        double new_multiplier = 1.0 / (num_lists_previously_assessed + 1.0);
        double old_multiplier = num_lists_previously_assessed * new_multiplier;

        double approximation_error = 0;
        if (optimal_score >= 0) {
            approximation_error = 1.0 - (test_outcome.score / optimal_score);
        }
        if (approximation_error > this->max_approximation_error) {
            this->max_approximation_error = approximation_error;
        }
        // This kind of averaging introduces some error in the computation, but guarantees to not overflow with a huge number of updates
        this->avg_score = new_multiplier * test_outcome.score + old_multiplier * this->avg_score;
        this->avg_approximation_error = new_multiplier * approximation_error + old_multiplier * this->avg_approximation_error;

        this->avg_num_elements_pruned = new_multiplier * test_outcome.num_elements_pruned + old_multiplier * this->avg_num_elements_pruned;
        this->avg_num_elements_not_pruned = new_multiplier * test_outcome.num_elements_not_pruned + old_multiplier * this->avg_num_elements_not_pruned;
        this->avg_first_stage_time = new_multiplier * test_outcome.first_stage_time + old_multiplier * this->avg_first_stage_time;
        this->avg_second_stage_time = new_multiplier * test_outcome.second_stage_time + old_multiplier * this->avg_second_stage_time;
        this->avg_total_time = new_multiplier * test_outcome.total_time + old_multiplier * this->avg_total_time;
    }

    /**
     * Writes on the output stream a json representation of the aggregation of all tests
     * @param os the output stream where to write
     * @param test the test to write
     * @return the output stream
     */
    friend std::ostream & operator<<(std::ostream &os, const struct tests_aggregation_outcome &outcome) {
        os << "{";

        os << "\"avg_score\": " << outcome.avg_score;
        os << ", \"max_approximation_error\": " << outcome.max_approximation_error;
        os << ", \"avg_approximation_error\": " << outcome.avg_approximation_error;
        os << ", \"avg_num_elements_pruned\": " << outcome.avg_num_elements_pruned;
        os << ", \"avg_num_elements_not_pruned\": " << outcome.avg_num_elements_not_pruned;
        os << ", \"avg_first_stage_time\": " << outcome.avg_first_stage_time;
        os << ", \"avg_second_stage_time\": " << outcome.avg_second_stage_time;
        os << ", \"avg_total_time\": " << outcome.avg_total_time;

        os << "}";
        return os;
    }
} TestsAggregationOutcome;


/**
 * Abstraction used to arbitrarily compose pruning and filtering strategies.
 * @tparam ScoreFun Score function type
 */
template <typename ScoreFun>
class PrunerFilterCompositionTest {
public:
    /**
     * The name of the test
     */
    const std::string name;

    /**
     * The pruner used in the first stage
     */
    const std::shared_ptr<Pruner<ScoreFun>> pruner;
    /**
     * The filter used in the second stage
     */
    const std::shared_ptr<Filter<ScoreFun>> filter;

    /**
     * The number of times each test must be repeated
     */
    const int num_runs;

    /**
     * Maximum approximation error (below the optimal score) guaranteed
     */
    const double epsilon_below;

    /**
     * Maximum approximation error (above the optimal score) guaranteed
     */
    const double epsilon_above;

public:
    /**
     * Constructor of an arbitrary composition
     * @param test_name The name of this test
     * @param pruner The pruner to use in the first stage
     * @param filter The filter to use in the second stage
     * @param num_runs The number of runs each test must be repeated (in order to have more accurate timings)
     * @param epsilon_below maximum approximation error (below the optimal score)
     * @param epsilon_above maximum approximation error (above the optimal score)
     */
    PrunerFilterCompositionTest(std::string name, std::shared_ptr<Pruner<ScoreFun>> pruner, std::shared_ptr<Filter<ScoreFun>> filter, const int num_runs=1, double epsilon_below=0.0, double epsilon_above=0.0) :
            name(std::move(name)),
            pruner(pruner),
            filter(filter),
            num_runs(num_runs),
            epsilon_below(epsilon_below),
            epsilon_above(epsilon_above)
    {
        if (this->filter == nullptr) {
            throw std::invalid_argument("The parameter filters must be not null");
        }
        if (this->num_runs <= 0) {
            throw std::invalid_argument("The parameter num_runs must be a strictly positive number");
        }
        if (epsilon_below < 0) {
            throw std::invalid_argument("The parameter epsilon_below must be a positive floating number");
        }
        if (epsilon_above < 0) {
            throw std::invalid_argument("The parameter epsilon_above must be a positive floating number");
        }
    }

    /**
     * Destructor
     */
    virtual
    ~PrunerFilterCompositionTest() {}

    /**
     * Filters the given list of relevances and returns a the outcome of the filtering@k.
     * @param rel_list List containing the relevance scores, ordered according to some attribute
     * @param n Number of elements of rel_list
     * @return The filtering solution built on top of the given list of relevances
     */
    virtual TestOutcome
    operator()(const relevance_type * rel_list, const index_type n, const minmax_type &minmax_element) {
        TestOutcome solution;
        FilterSolution filteringSolution;

        if (this->pruner.get() != nullptr) {
            // First stage
            solution.first_stage_time = get_time_milliseconds();

            PrunerSolution pruningSolution = this->pruner->operator()(rel_list, n, minmax_element);
            for (int run = 1; run < this->num_runs; ++run) {
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
            for (int run=1; run < this->num_runs; ++run) {
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
            for (int run=1; run < this->num_runs; ++run) {
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
};


#endif //FILTERING_UTILS_ASSESSMENT_HPP
