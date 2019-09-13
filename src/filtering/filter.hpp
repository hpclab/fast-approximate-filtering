#ifndef FILTERING_FILTER_HPP
#define FILTERING_FILTER_HPP

#include <memory>
#include <vector>
#include "types.hpp"


/**
 * Filtering solution representation.
 */
typedef struct _filtering_solution {
public:
    /**
     * Equal operator among two filtering solutions.
     * The two solutions are equal iff they have the same score and are composed by the same indices.
     * @param l First solution
     * @param r Second solution
     * @return A boolean stating if the two given solutions are equal
     */
    friend bool
    operator==(const _filtering_solution &l, const _filtering_solution &r) {
        if (l.score != r.score || l.size() != r.size()) {
            return false;
        }

        for (std::size_t i = 0, i_end = l.size(); i < i_end; ++i) {
            if (l.indices[i] != r.indices[i]) {
                return false;
            }
        }
        return true;
    }

    /**
     * Number of elements composing the solution.
     * @return The number of elements composing the solution
     */
    std::size_t
    size() const {
        return indices.size();
    }

public:
    /**
     * Score of the solution
     */
    score_type score = 0;
    /**
     * Indices of the elements composing the solution
     */
    std::vector<index_type> indices;
} FilterSolution;


/**
 * Abstract class implementing a generic filter@k.
 * @tparam ScoreFun Score function type
 */
template <typename ScoreFun>
class Filter {
public:
    /**
     * Constructor of a generic filter@k.
     * @param k Maximum number of elements to keep
     * @param score_fun Score function used to score the solutions
     */
    Filter(k_type k, const std::shared_ptr<ScoreFun> score_fun) :
            k(k),
            score_fun(score_fun) {
    }

    /**
     * Default destructor
     */
    virtual
    ~Filter() {}

    /**
     * Filters the given list of relevances and returns a filtering solution representing the outcome of the filtering@k.
     * @param rel_list List containing the relevance scores, ordered according to some attribute
     * @param n Number of elements of rel_list
     * @return The filtering solution built on top of the given list of relevances
     */
    virtual FilterSolution
    operator()(const relevance_type * rel_list, const index_type n) const = 0;

public:
    /**
     * Maximum number of elements to keep
     */
    const k_type k;
    /**
     * Score function used to score the solutions
     */
    const std::shared_ptr<ScoreFun> score_fun;
};


/**
 * Abstract class implementing a generic filter@k.
 * Differently from the version 1, this filter uses the information on the left and right heights of each element.
 * @tparam ScoreFun Score function type
 */
template <typename ScoreFun>
class FilterV2: public Filter<ScoreFun> {
public:
    /**
     * Constructor of a generic filter@k.
     * @param k Maximum number of elements to keep
     * @param score_fun Score function used to score the solutions
     */
    FilterV2(k_type k, const std::shared_ptr<ScoreFun> score_fun) :
            Filter<ScoreFun>(k, score_fun) {
    }

    /**
     * Filters the given list of relevances and returns a filtering solution representing the outcome of the filtering@k.
     * @param rel_list List containing the relevance scores, ordered according to some attribute
     * @param n Number of elements of rel_list
     * @return The filtering solution built on top of the given list of relevances
     */
    virtual FilterSolution
    operator()(const relevance_type * rel_list, const index_type n) const {
        return this->operator()(rel_list, n, nullptr, nullptr);
    }

    /**
     * Filters the given list of relevances and returns a filtering solution representing the outcome of the filtering@k.
     * @param rel_list List containing the relevance scores, ordered according to some attribute
     * @param n Number of elements of rel_list
     * @param left_heights An array containing the left height of all elements
     * @param right_heights An array containing the right height of all elements
     * @return The filtering solution built on top of the given list of relevances
     */
    virtual FilterSolution
    operator()(const relevance_type * rel_list, const index_type n, const k_type * left_heights, const k_type * right_heights) const = 0;
};

#endif //FILTERING_FILTER_HPP
