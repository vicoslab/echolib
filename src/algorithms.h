
/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */

#ifndef ECHO_ALGORITHMS_HPP_
#define ECHO_ALGORITHMS_HPP_

#include <algorithm>
#include <iterator>
#include <functional>
#include <vector>

#if __cplusplus > 199711L
#define register      // Deprecated in C++11.
#endif

namespace echolib {

/*
Copyright (c) 2014, Andrea Sansottera
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice, this
  list of conditions and the following disclaimer in the documentation and/or
  other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

template<class T>
T ilog2(T x);

static const uint8_t ilog2_lookup[256] = {
	0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 
	4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 
	6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 
	6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 
	6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 
	6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7 };

template<>
inline uint8_t ilog2(uint8_t x) {
    return ilog2_lookup[x];
}

template<>
inline uint16_t ilog2(uint16_t x) {
	register uint16_t tmp;
	if ((tmp = x >> 8)) return 8 + ilog2_lookup[tmp];
    return ilog2_lookup[x];
}

template<>
inline uint32_t ilog2(uint32_t x) {
	register uint32_t tmp;
	if ((tmp = x >> 24)) return 24 + ilog2_lookup[tmp];
	if ((tmp = x >> 16)) return 16 + ilog2_lookup[tmp];
	if ((tmp = x >> 8)) return 8 + ilog2_lookup[tmp];
    return ilog2_lookup[x];
}

template<>
inline uint64_t ilog2(uint64_t x) {
	register uint64_t tmp;
	if ((tmp = x >> 56)) return 56 + ilog2_lookup[tmp];
	if ((tmp = x >> 48)) return 48 + ilog2_lookup[tmp];
	if ((tmp = x >> 40)) return 40 + ilog2_lookup[tmp];
	if ((tmp = x >> 32)) return 32 + ilog2_lookup[tmp];
	if ((tmp = x >> 24)) return 24 + ilog2_lookup[tmp];
	if ((tmp = x >> 16)) return 16 + ilog2_lookup[tmp];
	if ((tmp = x >> 8)) return 8 + ilog2_lookup[tmp];
    return ilog2_lookup[x];
}

template<class RAI>
void make_minmaxheap(RAI first, RAI last) {
	typedef typename std::iterator_traits<RAI>::difference_type diff_t;
	if (last - first >= 2) {
		diff_t offset = (last - first) / 2 - 1;
		for (RAI i = first + offset; i > first; --i) {
			trickle_down(first, last, i);
		}
		trickle_down(first, last, first);
	}
}

/*!
Rearranges the values in the range [first,last) as a min-max heap.
*/
template<class RAI, class Compare>
void make_minmaxheap(RAI first, RAI last, Compare comp) {
	typedef typename std::iterator_traits<RAI>::difference_type diff_t;
	if (last - first >= 2) {
		diff_t offset = (last - first) / 2 - 1;
		for (RAI i = first + offset; i > first; --i) {
			trickle_down(first, last, i, comp);
		}
		trickle_down(first, last, first, comp);
	}
}

template<class RAI>
void popmin_minmaxheap(RAI first, RAI last) {
	std::iter_swap(first, last-1);
	trickle_down(first, last-1, first);
}

/*!
Moves the smallest value in the min-max heap to the end of the sequence,
shortening the actual min-max heap range by one position.
*/
template<class RAI, class Compare>
void popmin_minmaxheap(RAI first, RAI last, Compare comp) {
	std::iter_swap(first, last-1);
	trickle_down(first, last-1, first, comp);
}

template<class RAI>
void popmax_minmaxheap(RAI first, RAI last) {
	if (last-first < 2) {
		return;
	}
	if (last-first == 2 || *(first+1) > *(first+2)) {
		std::iter_swap(first+1, last-1);
		trickle_down(first, last-1, first+1);
	} else {
		std::iter_swap(first+2, last-1);
		trickle_down(first, last-1, first+2);
	}
}

/*!
Moves the largest value in the min-max heap to the end of the sequence,
shortening the actual min-max heap range by one position. */
template<class RAI, class Compare>
void popmax_minmaxheap(RAI first, RAI last, Compare comp) {
	if (last-first < 2) {
		return;
	}
	if (last-first == 2 || comp(*(first+2), *(first+1))) {
		std::iter_swap(first+1, last-1);
		trickle_down(first, last-1, first+1, comp);
	} else {
		std::iter_swap(first+2, last-1);
		trickle_down(first, last-1, first+2, comp);
	}
}

template<class RAI>
void push_minmaxheap(RAI first, RAI last) {
	bubble_up(first, last, last-1);
}

/*! Given a min-max heap on the range [first,last), moves the element in the 
last-1) position to its correct position.
*/
template<class RAI, class Compare>
void push_minmaxheap(RAI first, RAI last, Compare comp) {
	bubble_up(first, last, last-1, comp);
}

template<class RAI>
RAI min_minmaxheap(RAI first, RAI last) {
    return first;
}

/*! Returns an iterator pointing to the smallest element. */
template<class RAI, class Compare>
RAI min_minmaxheap(RAI first, RAI last, Compare comp) {
    return first;
}

template<class RAI>
RAI max_minmaxheap(RAI first, RAI last) {
    typedef typename std::iterator_traits<RAI>::difference_type diff_t;
    diff_t count = last - first;
    if (count == 1) {
        return first;
    }
    RAI second = first + 1;
    if (count == 2) {
        return second;
    }
    // largest can be either the second or the third element
    RAI third = second + 1;
    return (*third < *second) ? second : third;
}

/*! Returns an iterator pointing to the largest element. */
template<class RAI, class Compare>
RAI max_minmaxheap(RAI first, RAI last, Compare comp) {
    typedef typename std::iterator_traits<RAI>::difference_type diff_t;
    diff_t count = last - first;
    if (count == 1) {
        return first;
    }
    RAI second = first + 1;
    if (count == 2) {
        return second;
    }
    // largest can be either the second or the third element
    RAI third = second + 1;
    return comp(*third, *second) ? second : third;
}

template<class RAI>
RAI get_left_child(RAI first, RAI last, RAI i) {
	typedef typename std::iterator_traits<RAI>::difference_type diff_t;
	diff_t offset = (i - first) * 2 + 1;
	if (offset >= last - first) {
		return last;
	} else {
		return first + offset;
	}
}

template<class RAI>
RAI get_right_child(RAI first, RAI last, RAI i) {
	typedef typename std::iterator_traits<RAI>::difference_type diff_t;
	diff_t offset = (i - first) * 2 + 2;
	if (offset >= last - first) {
		return last;
	} else {
		return first + offset;
	}
}

template<class RAI>
std::size_t get_level(RAI first, RAI last, RAI i) {
	// the difference cannot be negative, so we can cast to unsigned type
	std::size_t diff = static_cast<std::size_t>(i - first);
	return ilog2(diff+1);
}

template<class RAI>
RAI get_parent(RAI first, RAI last, RAI i) {
	typedef typename std::iterator_traits<RAI>::difference_type diff_t;
	diff_t diff = i - first;
	if (diff < 1) {
		return last;
	}
	return first + (diff-1)/2;
}

template<class RAI>
RAI get_grand_parent(RAI first, RAI last, RAI i) {
	typedef typename std::iterator_traits<RAI>::difference_type diff_t;
	diff_t diff = i - first;
	if (diff < 3) {
		return last;
	}
	return first + (diff-3)/4;
}

template<class RAI>
RAI get_smallest_child_or_grandchild(RAI first, RAI last, RAI i) {
	/* If there are no children, return last */
	RAI smallest = last;
	RAI left = get_left_child(first, last, i);
	if (left < last) {
		smallest = left;
		RAI leftLeft = get_left_child(first, last, left);
		RAI leftRight = get_right_child(first, last, left);
		if (leftLeft < last && *leftLeft < *smallest) {
			smallest = leftLeft;
		}
		if (leftRight < last && *leftRight < *smallest) {
			smallest = leftRight;
		}
	}
	RAI right = get_right_child(first, last, i);
	if (right < last) {
		if (*right < *smallest) {
			smallest = right;
		}
		RAI rightLeft = get_left_child(first, last, right);
		RAI rightRight = get_right_child(first, last, right);
		if (rightLeft < last && *rightLeft < *smallest) {
			smallest = rightLeft;
		}
		if (rightRight < last && *rightRight < *smallest) {
			smallest = rightRight;
		}
	}
	return smallest;
}

template<class RAI, class Compare>
RAI get_smallest_child_or_grandchild(RAI first,
									 RAI last,
									 RAI i,
									 Compare comp) {
	/* If there are no children, return last */
	RAI smallest = last;
	RAI left = get_left_child(first, last, i);
	if (left < last) {
		smallest = left;
		RAI leftLeft = get_left_child(first, last, left);
		RAI leftRight = get_right_child(first, last, left);
		if (leftLeft < last && comp(*leftLeft, *smallest)) {
			smallest = leftLeft;
		}
		if (leftRight < last && comp(*leftRight, *smallest)) {
			smallest = leftRight;
		}
	}
	RAI right = get_right_child(first, last, i);
	if (right < last) {
		if (comp(*right, *smallest)) {
			smallest = right;
		}
		RAI rightLeft = get_left_child(first, last, right);
		RAI rightRight = get_right_child(first, last, right);
		if (rightLeft < last && comp(*rightLeft, *smallest)) {
			smallest = rightLeft;
		}
		if (rightRight < last && comp(*rightRight, *smallest)) {
			smallest = rightRight;
		}
	}
	return smallest;
}

template<class RAI>
RAI get_largest_child_or_grandchild(RAI first, RAI last, RAI i) {
	/* If there are no children return last */
	RAI largest = last;
	RAI left = get_left_child(first, last, i);
	if (left < last) {
		largest = left;
		RAI leftLeft = get_left_child(first, last, left);
		RAI leftRight = get_right_child(first, last, left);
		if (leftLeft < last && *leftLeft > *largest) {
			largest = leftLeft;
		}
		if (leftRight < last && *leftRight > *largest) {
			largest = leftRight;
		}
	}
	RAI right = get_right_child(first, last, i);
	if (right < last) {
		if (*right > *largest) {
			largest = right;
		}
		RAI rightLeft = get_left_child(first, last, right);
		RAI rightRight = get_right_child(first, last, right);
		if (rightLeft < last && *rightLeft > *largest) {
			largest = rightLeft;
		}
		if (rightRight < last && *rightRight > *largest) {
			largest = rightRight;
		}
	}
	return largest;
}

template<class RAI, class Compare>
RAI get_largest_child_or_grandchild(RAI first, RAI last, RAI i, Compare comp) {
	/* If there are no children return last */
	RAI largest = last;
	RAI left = get_left_child(first, last, i);
	if (left < last) {
		largest = left;
		RAI leftLeft = get_left_child(first, last, left);
		RAI leftRight = get_right_child(first, last, left);
		if (leftLeft < last && comp(*largest, *leftLeft)) {
			largest = leftLeft;
		}
		if (leftRight < last && comp(*largest, *leftRight)) {
			largest = leftRight;
		}
	}
	RAI right = get_right_child(first, last, i);
	if (right < last) {
		if (comp(*largest, *right)) {
			largest = right;
		}
		RAI rightLeft = get_left_child(first, last, right);
		RAI rightRight = get_right_child(first, last, right);
		if (rightLeft < last && comp(*largest, *rightLeft)) {
			largest = rightLeft;
		}
		if (rightRight < last && comp(*largest, *rightRight)) {
			largest = rightRight;
		}
	}
	return largest;
}

template<class RAI>
void trickle_down_min(RAI first, RAI last, RAI i) {
	if (get_left_child(first, last, i) < last) {
		RAI m = get_smallest_child_or_grandchild(first, last, i);
		if (get_grand_parent(first, last, m) == i) {
			if (*m < *i) {
				std::iter_swap(m, i);
				RAI parent = get_parent(first, last, m);
				if (*m > *parent) {
					std::iter_swap(m, parent);
				}
				trickle_down_min(first, last, m);
			}
		} else {
			if (*m < *i) {
				std::iter_swap(m, i);
			}
		}
	}
}

template<class RAI, class Compare>
void trickle_down_min(RAI first, RAI last, RAI i, Compare comp) {
	if (get_left_child(first, last, i) < last) {
		RAI m = get_smallest_child_or_grandchild(first, last, i, comp);
		if (get_grand_parent(first, last, m) == i) {
			if (comp(*m, *i)) {
				std::iter_swap(m, i);
				RAI parent = get_parent(first, last, m);
				if (comp(*parent, *m)) {
					std::iter_swap(m, parent);
				}
				trickle_down_min(first, last, m, comp);
			}
		} else {
			if (comp(*m, *i)) {
				std::iter_swap(m, i);
			}
		}
	}
}

template<class RAI>
void trickle_down_max(RAI first, RAI last, RAI i) {
	if (get_left_child(first, last,i) < last) {
		RAI m = get_largest_child_or_grandchild(first, last, i);
		if (get_grand_parent(first, last, m) == i) {
			if (*m > *i) {
				std::iter_swap(m, i);
				RAI parent = get_parent(first, last, m);
				if (*m < *parent) {
					std::iter_swap(m, parent);
				}
				trickle_down_max(first, last, m);
			}
		} else {
			if (*m > *i) {
				std::iter_swap(m, i);
			}
		}
	}
}

template<class RAI, class Compare>
void trickle_down_max(RAI first, RAI last, RAI i, Compare comp) {
	if (get_left_child(first, last,i) < last) {
		RAI m = get_largest_child_or_grandchild(first, last, i, comp);
		if (get_grand_parent(first, last, m) == i) {
			if (comp(*i, *m)) {
				std::iter_swap(m, i);
				RAI parent = get_parent(first, last, m);
				if (comp(*m, *parent)) {
					std::iter_swap(m, parent);
				}
				trickle_down_max(first, last, m, comp);
			}
		} else {
			if (comp(*i, *m)) {
				std::iter_swap(m, i);
			}
		}
	}
}

template<class RAI>
void trickle_down(RAI first, RAI last, RAI i) {
	if (get_level(first, last, i) % 2 == 0) {
		trickle_down_min(first, last, i);
	} else {
		trickle_down_max(first, last, i);
	}
}

template<class RAI, class Compare>
void trickle_down(RAI first, RAI last, RAI i, Compare comp) {
	if (get_level(first, last, i) % 2 == 0) {
		trickle_down_min(first, last, i, comp);
	} else {
		trickle_down_max(first, last, i, comp);
	}
}

template<class RAI>
void bubble_up_min(RAI first, RAI last, RAI i) {
	RAI gp = get_grand_parent(first, last, i);
	if (gp != last && *i < *gp) {
		std::iter_swap(i, gp);
		bubble_up_min(first, last, gp);
	}
}

template<class RAI, class Compare>
void bubble_up_min(RAI first, RAI last, RAI i, Compare comp) {
	RAI gp = get_grand_parent(first, last, i);
	if (gp != last && comp(*i, *gp)) {
		std::iter_swap(i, gp);
		bubble_up_min(first, last, gp, comp);
	}
}

template<class RAI>
void bubble_up_max(RAI first, RAI last, RAI i) {
	RAI gp = get_grand_parent(first, last, i);
	if (gp != last && *i > *gp) {
		std::iter_swap(i, gp);
		bubble_up_max(first, last, gp);
	}
}

template<class RAI, class Compare>
void bubble_up_max(RAI first, RAI last, RAI i, Compare comp) {
	RAI gp = get_grand_parent(first, last, i);
	if (gp != last && comp(*gp, *i)) {
		std::iter_swap(i, gp);
		bubble_up_max(first, last, gp, comp);
	}
}


template<class RAI>
void bubble_up(RAI first, RAI last, RAI i) {
	RAI parent = get_parent(first, last, i);
	if (get_level(first, last, i) % 2 == 0) {
		if (parent != last && *i > *parent) {
			std::iter_swap(i, parent);
			bubble_up_max(first, last, parent);
		} else {
			bubble_up_min(first, last, i);
		}
	} else {
		if (parent != last && *i < *parent) {
			std::iter_swap(i, parent);
			bubble_up_min(first, last, parent);
		} else {
			bubble_up_max(first, last, i);
		}
	}
}

template<class RAI, class Compare>
void bubble_up(RAI first, RAI last, RAI i, Compare comp) {
	RAI parent = get_parent(first, last, i);
	if (get_level(first, last, i) % 2 == 0) {
		if (parent != last && comp(*parent, *i)) {
			std::iter_swap(i, parent);
			bubble_up_max(first, last, parent, comp);
		} else {
			bubble_up_min(first, last, i, comp);
		}
	} else {
		if (parent != last && comp(*i, *parent)) {
			std::iter_swap(i, parent);
			bubble_up_min(first, last, parent, comp);
		} else {
			bubble_up_max(first, last, i, comp);
		}
	}
}

/*!
This template class is a container adapter, implementing a priority queue with 
a bounded the number of items.
Once the queue is full, the elements with the smallest priority are dropped.
The implementation is based on the min-max heap implicit data structure.
If no container template parameter is specified, a vector is used.
If no comparer template parameter is specified, the < operator is used.
*/
template<class T,
		 class Container = std::vector<T>,
		 class Compare = std::less<T> >
class bounded_priority_queue {
private:
	std::size_t m_count;
	std::vector<T> m_heap;
	Compare m_comp;
public:
	/*!
	Constructs an empty bounded priority queue of the given size.
	*/
	bounded_priority_queue(std::size_t size, const Compare & comp = Compare())
		: m_count(0), m_heap(size), m_comp(comp) {
	}
	/*!
	Constructs a bounded priority queue containing the items of the
	container. The maximum size of the priority queue is equal to the size
	of the container.
	*/
	bounded_priority_queue(const Container & container,
						 const Compare & comp = Compare())
		: m_count(0), m_heap(container.size()), m_comp(comp) {
		typename Container::const_iterator itr;
		for (itr = container.begin(); itr != container.end(); ++itr) {
			push(*itr);
		}
	}
	/*!
	Constructs a bounded priority queue containing some of the items of the
	container. The maximum size of the priority queue is provided as a
	paramater and, in general, might be smaller than the size of the container.
	In this case, the lowest priority elements are not inserted.
	*/
	bounded_priority_queue(std::size_t size,
						 const Container & container,
						 const Compare & comp = Compare())
		: m_count(0), m_heap(size), m_comp(comp) {
		typename Container::const_iterator itr;
		for (itr = container.begin(); itr != container.end(); ++itr) {
			push(*itr);
		}
	}
	/*!
	Tries to add a new element to the queue failing if it is full.
	*/
	bool push(const T & obj) {
		if (m_count == m_heap.size()) {
			return false;
		} else {
			m_heap[m_count] = obj;
			++m_count;
			push_minmaxheap(m_heap.begin(), m_heap.begin() + m_count, m_comp);
			return true;
		}

	}

	/*!
	Tries to add a new element to the queue.
	If the queue is full and the new element has a equal or higher priority
	than the bottom element, the queue is left unmodified.
	If the queue is full and the new element has a lower priority
	than the bottom element, the bottom element is removed and the new element
	is inserted.
	The removed element is returned.
	*/
	T push_over(const T & obj) {
		if (m_count == m_heap.size()) {
			if (m_comp(obj, bottom())) {
				T removed = bottom();
				popmax_minmaxheap(m_heap.begin(), m_heap.end(), m_comp);
				*m_heap.rbegin() = obj;
				push_minmaxheap(m_heap.begin(),
								m_heap.begin() + m_count,
								m_comp);
				return removed;
			} else return obj;
		} return T();
	}

	/*!
	Returns a reference to the highest priority element of the queue.
	*/
	const T & top() const {
		return *(min_minmaxheap(m_heap.begin(), m_heap.end(), m_comp));
	}
	/*!
	Returns a reference to the lowest priority element of the queue.
	*/
	const T & bottom() const {
        return *(max_minmaxheap(m_heap.begin(), m_heap.end(), m_comp));
	}
	/*!
	Removes the highest priority element of the queue.
	*/
	void pop_top() {
		popmin_minmaxheap(m_heap.begin(), m_heap.begin() + m_count, m_comp);
		m_heap[m_count-1] = T(); // Cleanup erased item (removing references)
		--m_count;
	}
	/*!
	Removes the lowest priority element of the queue.
	*/
	void pop_bottom() {
		popmax_minmaxheap(m_heap.begin(), m_heap.begin() + m_count, m_comp);
		--m_count;
	}
	/*!
	Returns the number of elements stored in the queue.
	*/
	std::size_t size() const {
		return m_count;
	}
	/*!
	Returns the maximum number of elements that can be stored in the queue.
	*/
	std::size_t max_size() const {
		return m_heap.size();
	}
	/*!
	Returns true if the queue has no elements, false otherwise.
	*/
	bool empty() const {
		return m_count == 0;
	}
};


}


#endif