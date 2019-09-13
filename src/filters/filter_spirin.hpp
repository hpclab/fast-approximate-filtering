#ifndef FILTERS_FILTER_SPIRIN_HPP
#define FILTERS_FILTER_SPIRIN_HPP

#include <algorithm>
#include <cassert>
#include "../filtering/filter.hpp"


/**
 * Lossless Filter@k algorithm of Spirin et al.
 * @tparam ScoreFun Score function type
 *
 * @note This is my implementation of the filtering method described in the paper by Spirin et al.
 * "Relevance-aware Filtering of Tuples Sorted by an Attribute Value via Direct Optimization of Search Quality Metrics"
 */
template <typename ScoreFun>
class FilterSpirin: public Filter<ScoreFun> {
public:
    /**
     * Constructor
     * @param k Maximum number of elements to keep
     * @param score_fun Score function used to score the solutions
     */
    FilterSpirin(k_type k, const std::shared_ptr<ScoreFun> score_fun) :
            Filter<ScoreFun>(k, score_fun) {
    }

    /**
     * Filters the given list of relevances and returns a filtering solution representing the outcome of the filtering@k.
     * @param rel_list List containing the relevance scores, ordered according to some attribute
     * @param n Number of elements of rel_list
     * @return The filtering solution built on top of the given list of relevances
     */
    FilterSolution
    operator()(const relevance_type * rel_list, const index_type n) const {
        return this->filter_impl(rel_list, n);
    }

private:
    template <bool debug_print=false>
    inline FilterSolution
    filter_impl(const relevance_type * rel_list, const index_type n) const {
        FilterSolution solution;
        if (n == 0 || this->k == 0) {
            return solution;
        }
        // check the value of k
        const ScoreFun & score_fun = *(this->score_fun.get());
        const k_type k = (this->k > n) ? n : this->k;

        // matrix used by the dynamic algorithm
        // I use a malloc here to avoid the cost of initializing all elements
        score_type *M = new score_type[((k - 1) * (k - 1 + 1) / 2) + k * (n - (k - 1))];
        score_type *buffer = new score_type[n + k];
        score_type *gains = buffer, *discounts = buffer + n;
        for (std::size_t i = 0; i < k; ++i) {
            gains[i] = score_fun.gain_factor(rel_list[i]);
            discounts[i] = score_fun.discount_factor(i + 1);
        }
        for (std::size_t i = k; i < n; ++i) {
            gains[i] = score_fun.gain_factor(rel_list[i]);
        }

        // support variables used to shift within the one-dimension vector as if it were a matrix
        std::size_t prev_row_shift = 0;
        std::size_t curr_row_shift = 0;

        // filling the table
        M[0] = gains[0] * discounts[0];
        if (debug_print) {
            std::cout << 0 << "\t" << M[0] << std::endl;
        }
        for (std::size_t row = 1; row < k; ++row) {  // the triangular block ends in position k-1
            curr_row_shift = prev_row_shift + row;

            M[curr_row_shift + 0] = std::max(M[prev_row_shift + 0], gains[row] * discounts[0]);
            for (std::size_t col = 1; col < row; ++col) {
                M[curr_row_shift + col] = std::max(M[prev_row_shift + col],
                                                   M[prev_row_shift + col - 1] + gains[row] * discounts[col]);
            }
            M[curr_row_shift + row] = M[prev_row_shift + row - 1] + gains[row] * discounts[row];

            if (debug_print) {
                std::cout << row << "\t" << M[curr_row_shift + 0];
                for (std::size_t col=1; col <= row; ++col) {
                    std::cout << "\t" << M[curr_row_shift + col];
                }
                std::cout << std::endl;
            }

            prev_row_shift = curr_row_shift;
        }
        for (std::size_t row = k; row < n; ++row) {  // after position k-1 the block is rectangular
            curr_row_shift = prev_row_shift + k;

            M[curr_row_shift + 0] = std::max(M[prev_row_shift + 0], gains[row] * discounts[0]);
            for (std::size_t col = 1; col < k; ++col) {
                M[curr_row_shift + col] = std::max(M[prev_row_shift + col],
                                                   M[prev_row_shift + col - 1] + gains[row] * discounts[col]);
            }

            if (debug_print) {
                std::cout << row << "\t" << M[curr_row_shift + 0];
                for (std::size_t col=1; col < k; ++col) {
                    std::cout << "\t" << M[curr_row_shift + col];
                }
                std::cout << std::endl;
            }

            prev_row_shift = curr_row_shift;
        }

        solution.indices.reserve(n);
        // identifying the best score within the last row
        index_type best_column = 0;
        // curr_row_shift is already the shift of the last row
        for (std::size_t col = 0; col < k; ++col) {
            if (M[curr_row_shift + col] > solution.score) {
                solution.score = M[curr_row_shift + col];
                best_column = col;
            }
        }

        // going back to identify the elements participating to the solution
        for (std::size_t row = n - 1; row > 0; --row) {
            assert(curr_row_shift >= row);
            prev_row_shift = curr_row_shift - ((row < k) ? row : k);
            if (M[curr_row_shift + best_column] > M[prev_row_shift + best_column]) {
                solution.indices.push_back(row);
                if (best_column-- == 0) {
                    break;
                }
            }
            curr_row_shift = prev_row_shift;
        }
        if (curr_row_shift == 0) {
            solution.indices.push_back(0);
        }
        assert(best_column == static_cast<index_type>(-1) || curr_row_shift == 0);

        // reverse the vector containing the indices, because I filled it from right to left
        std::reverse(solution.indices.begin(), solution.indices.end());
        delete[](buffer);
        delete[](M);

        return solution;
    }
};


#endif //FILTERS_FILTER_SPIRIN_HPP
