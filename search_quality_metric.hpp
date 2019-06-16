#ifndef FILTERING_SEARCH_QUALITY_METRIC_HPP
#define FILTERING_SEARCH_QUALITY_METRIC_HPP

#include <cassert>
#include <cmath>
#include "filter.hpp"


struct dcg_metric {
    dcg_metric(std::size_t max_position) :
            discounts(compute_discounts(max_position)),
            discount_sums(compute_discount_sums(max_position, this->discounts)),
            max_position(max_position) {
    }

    ~dcg_metric() {
        delete[](discounts);
        delete[](discount_sums);
    }

    inline score_type
    operator()(relevance_type relevance, index_type position) const {
        return this->gain_factor(relevance) * this->discount_factor(position);
    }

    inline score_type
    gain_factor(relevance_type relevance) const {
        return static_cast<score_type>(std::pow(2, relevance) - 1.0);
    }

    inline relevance_type
    gain_factor_inverse(score_type gain) const {
        return static_cast<relevance_type>(std::log2(gain + 1.0));
    }

    inline score_type
    discount_factor(index_type position) const {
        return discounts[position];
    }

    inline score_type
    discount_factor_sum(index_type left_included, index_type right_included) const {
        return this->discount_sums[right_included] - this->discount_sums[left_included - 1];
    }

private:
    static score_type *
    compute_discounts(std::size_t n) {
        score_type *discounts = new score_type[n + 1];
        discounts[0] = 0;
        for (std::size_t i = 1; i <= n; ++i) {
            discounts[i] = static_cast<score_type>(1.0) / std::log2(i + 1);
        }

        return discounts;
    }

    static score_type *
    compute_discount_sums(std::size_t n, const score_type *discounts) {
        score_type *discount_sums = new score_type[n + 1];
        discount_sums[0] = 0;
        for (std::size_t i = 1; i <= n; ++i) {
            discount_sums[i] = discount_sums[i - 1] + discounts[i];
        }

        return discount_sums;
    }

    const score_type *discounts;
    const score_type *discount_sums;

public:
    const index_type max_position;
};


struct dcglz_metric {
    dcglz_metric(std::size_t max_position) :
            discounts(compute_discounts(max_position)),
            discount_sums(compute_discount_sums(max_position, this->discounts)),
            max_position(max_position) {
    }

    ~dcglz_metric() {
        delete[](discounts);
        delete[](discount_sums);
    }

    inline score_type
    operator()(relevance_type relevance, index_type position) const {
        return this->gain_factor(relevance) * this->discount_factor(position);
    }

    inline score_type
    gain_factor(relevance_type relevance) const {
        return static_cast<score_type>(relevance);
    }

    inline relevance_type
    gain_factor_inverse(score_type gain) const {
        return static_cast<relevance_type>(gain);
    }

    inline score_type
    discount_factor(index_type position) const {
        return discounts[position];
    }

    inline score_type
    discount_factor_sum(index_type left_included, index_type right_included) const {
        return this->discount_sums[right_included] - this->discount_sums[left_included - 1];
    }

private:
    static score_type *
    compute_discounts(std::size_t n) {
        score_type *discounts = new score_type[n + 1];
        discounts[0] = 0;
        for (std::size_t i = 1; i <= n; ++i) {
            discounts[i] = static_cast<score_type>(1.0 / i);
        }

        return discounts;
    }

    static score_type *
    compute_discount_sums(std::size_t n, const score_type *discounts) {
        score_type *discount_sums = new score_type[n + 1];
        discount_sums[0] = 0;
        for (std::size_t i = 1; i <= n; ++i) {
            discount_sums[i] = discount_sums[i - 1] + discounts[i];
        }

        return discount_sums;
    }

    const score_type *discounts;
    const score_type *discount_sums;

public:
    const index_type max_position;
};


#endif //FILTERING_SEARCH_QUALITY_METRIC_HPP
