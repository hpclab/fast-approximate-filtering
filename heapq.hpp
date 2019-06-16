#ifndef HEAPQ_HPP
#define HEAPQ_HPP

#include <algorithm>
#include <vector>


struct heapq {
private:
    /**
     * Gets the parent position within the heap
     * @param pos The element position
     * @return The position of the parent of i within the heap
     */
    static inline std::size_t
    parent(std::size_t pos) {
        return (pos - 1) / 2;
    }


    /**
     * Gets the left children position within the heap
     * @param pos The element position
     * @return The position of the left children of i within the heap
     */
    static inline std::size_t
    left(std::size_t pos) {
        return (2 * pos + 1);
    }


    /**
     * Percolates up the given element within the heap to satisfy the heap property
     * @param heap The heap vector
     * @param pos The element position
     */
    template<typename T, class _Compare=std::less<T>>
    static inline void
    percolate_up(std::vector<T> &heap, std::size_t pos, _Compare comp) {
        std::size_t p;

        while (pos > 0) {
            p = parent(pos);

            // check if pos is smaller than its parent
            if (comp(heap[pos], heap[p])) {
                std::swap(heap[pos], heap[p]);
                pos = p;
            } else {
                break;
            }
        }
    }


    /**
     * Percolates down the given element within the heap to satisfy the heap property
     * @param heap The heap vector
     * @param pos The element position
     */
    template<typename T, class _Compare=std::less<T>>
    static inline void
    percolate_down(std::vector<T> &heap, std::size_t pos, _Compare comp) {
        std::size_t n = heap.size();
        std::size_t smallest;
        std::size_t l, r;

        while (true) {
            // find the maximum_ among pos and its children
            l = left(pos);
            if (l >= n) {
                break;
            }
            r = l + 1;
            if (comp(heap[l], heap[pos])) {
                smallest = l;
            } else {
                smallest = pos;
            }
            if (r < n && comp(heap[r], heap[smallest])) {
                smallest = r;
            }

            // pos is already the smallest element
            if (smallest == pos) {
                break;
            }
            // swap pos with its smallest children
            std::swap(heap[pos], heap[smallest]);
            pos = smallest;
        }
    }

public:
    /**
     * Build a min heap in linear time
     * @tparam T Type of elements
     * @param vec The vector to heapify
     */
    template<typename T>
    static inline void
    heapify(std::vector<T> &vec) {
        heapify(vec, std::less<T>());
    }


    /**
     * Build a min heap in linear time
     * @tparam T Type of elements
     * @tparam _Compare Compare class
     * @param vec The vector to heapify
     * @param comp The comparison operator to use to compare the heap elements
     */
    template<typename T, class _Compare>
    static void
    heapify(std::vector<T> &vec, _Compare comp) {
        std::size_t n = vec.size();
        if (n <= 1) {
            return;
        }
        for (std::size_t i = parent(n - 1); i > 0; --i) {
            percolate_down(vec, i, comp);
        }
        percolate_down(vec, 0, comp);
    }


    /**
     * Push an element inside the heap
     * @tparam T Type of elements
     * @param heap The heap vector
     * @param element The element to push in
     */
    template<typename T>
    static inline void
    push(std::vector<T> &heap, const T &element) {
        push(heap, element, std::less<T>());
    }


    /**
     * Push an element inside the heap
     * @tparam T Type of elements
     * @tparam _Compare Compare class
     * @param heap The heap vector
     * @param element The element to push in
     * @param comp The comparison operator to use to compare the heap elements
     */
    template<typename T, class _Compare>
    static void
    push(std::vector<T> &heap, const T &element, _Compare comp) {
        heap.push_back(element);
        percolate_up(heap, heap.size() - 1, comp);
    }


    /**
     * Push an element inside the heap
     * @tparam T Type of elements
     * @param heap The heap vector
     * @param element The element to push in
     */
    template<typename T>
    static inline void
    push(std::vector<T> &heap, T &&element) {
        push(heap, static_cast<T &&>(element), std::less<T>());
    }


    /**
     * Push an element inside the heap
     * @tparam T Type of elements
     * @tparam _Compare Compare class
     * @param heap The heap vector
     * @param element The element to push in
     * @param comp The comparison operator to use to compare the heap elements
     */
    template<typename T, class _Compare>
    static void
    push(std::vector<T> &heap, T &&element, _Compare comp) {
        heap.emplace_back(element);
        percolate_up(heap, heap.size() - 1, comp);
    }


    /**
     * Pop an element from the heap
     * @tparam T Type of elements
     * @param heap The heap vector
     */
    template<typename T>
    static inline void
    pop(std::vector<T> &heap) {
        pop(heap, std::less<T>());
    }


    /**
     * Pop an element from the heap
     * @tparam T Type of elements
     * @tparam _Compare Compare class
     * @param heap The heap vector
     * @param comp The comparison operator to use to compare the heap elements
     */
    template<typename T, class _Compare>
    static void
    pop(std::vector<T> &heap, _Compare comp) {
        heap[0] = heap.back();
        heap.pop_back();
        percolate_down(heap, 0, comp);
    }


    /**
     * Replace the minimum element of the heap with the given one. This operation costs less than performing pop and then push.
     * @tparam T Type of elements
     * @param heap The heap vector
     * @param element The element to push in
     */
    template<typename T>
    static inline void
    replace(std::vector<T> &heap, const T &element) {
        replace(heap, element, std::less<T>());
    }


    /**
     * Replace the minimum element of the heap with the given one. This operation costs less than performing pop and then push.
     * @tparam T Type of elements
     * @tparam _Compare Compare class
     * @param heap The heap vector
     * @param element The element to push in
     * @param comp The comparison operator to use to compare the heap elements
     */
    template<typename T, class _Compare>
    static void
    replace(std::vector<T> &heap, const T &element, _Compare comp) {
        heap[0] = element;
        percolate_down(heap, 0, comp);
    }


    /**
     * Replace the minimum element of the heap with the given one. This operation costs less than performing pop and then push.
     * @tparam T Type of elements
     * @param heap The heap vector
     * @param element The element to push in
     */
    template<typename T>
    static inline void
    replace(std::vector<T> &heap, T &&element) {
        replace(heap, static_cast<T &&>(element), std::less<T>());
    }


    /**
     * Replace the minimum element of the heap with the given one. This operation costs less than performing pop and then push.
     * @tparam T Type of elements
     * @tparam _Compare Compare class
     * @param heap The heap vector
     * @param element The element to push in
     * @param comp The comparison operator to use to compare the heap elements
     */
    template<typename T, class _Compare>
    static void
    replace(std::vector<T> &heap, T &&element, _Compare comp) {
        heap[0] = std::move(element);
        percolate_down(heap, 0, comp);
    }
};

#endif //HEAPQ_HPP
