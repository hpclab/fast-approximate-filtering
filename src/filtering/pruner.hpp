#ifndef FILTERING_PRUNER_HPP
#define FILTERING_PRUNER_HPP

#include <memory>
#include "types.hpp"


/**
 * Pruning solution representation.
 */
typedef struct _pruning_solution {
public:
    /**
     * Equal operator among two pruning solutions.
     * The two solutions are equal iff they are composed by the same indices.
     * @param l First solution
     * @param r Second solution
     * @return A boolean stating if the two given solutions are equal
     */
    friend bool
    operator==(const _pruning_solution &l, const _pruning_solution &r) {
        if (l.size() != r.size()) {
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
     * Indices of the elements composing the solution
     */
    std::vector<index_type> indices;
} PrunerSolution;


/**
 * Abstract class implementing a generic pruner.
 * @tparam ScoreFun Score function type
 */
template <typename ScoreFun>
class Pruner {
public:
    /**
     * Constructor of a generic pruner.
     * @param k Maximum number of elements to keep
     * @param score_fun Score function used to score the solutions
     */
    Pruner(const std::shared_ptr<ScoreFun> score_fun) :
            score_fun(score_fun) {
    }

    /**
     * Default destructor
     */
    virtual
    ~Pruner() {}

    /**
     * Prunes the given list of relevances and returns a pruning solution representing the outcome of the pruning.
     * @param rel_list List containing the relevance scores, ordered according to some attribute
     * @param n Number of elements of rel_list
     * @param minmax_element The min and maximum elements of the list
     * @return The pruning solution built on top of the given list of relevances
     */
    virtual PrunerSolution
    operator()(const relevance_type * rel_list, const index_type n, const minmax_type &minmax_element) const = 0;

public:
    /**
     * Score function used to score the solutions
     */
    const std::shared_ptr<ScoreFun> score_fun;
};


#endif //FILTERING_PRUNER_HPP
