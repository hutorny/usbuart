/** @brief vector with a lock
 *  @file  vector_lock.hpp
 *  @addtogroup core
 */
/* This file is part of USBUART Library. http://usbuart.info/
 *
 * Copyright (C) 2016 Eugene Hutorny <eugene@hutorny.in.ua>
 *
 * The USBUART Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License v2
 * as published by the Free Software Foundation;
 *
 * The USBUART Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with the USBUART Library; if not, see
 * <http://www.gnu.org/licenses/gpl-2.0.html>.
 */

#ifndef VECTOR_LOCK_HPP_
#define VECTOR_LOCK_HPP_
#include <vector>
#include <mutex>
namespace usbuart {

/**
 * A simple rwlock
 */
class rwlock : public std::mutex {
public:
	void shared_lock() {
		std::lock_guard<mutex> _lock(m);
		if( ++n == 1 ) lock();
	}
	void shared_unlock() {
		std::lock_guard<mutex> _lock(m);
		if( --n == 0 ) unlock();
	}
	void upgrade() {
		std::lock_guard<mutex> _lock(m);
		if( --n ) lock();
	}
private:
	std::mutex m;
	unsigned n = 0;
};

/**
 * A shared guard to operate on rwlock as a reader
 */
template<typename M>
class shared_guard {
public:
	typedef M mutex_type;
	explicit shared_guard(mutex_type& _m) : m(_m) { m.shared_lock(); }
	~shared_guard() { if(excl) m.unlock(); else m.shared_unlock(); }
	inline void upgrade() {	m.upgrade(); excl = true; }
	shared_guard(const shared_guard&) = delete;
	shared_guard& operator=(const shared_guard&) = delete;
private:
	mutex_type&  m;
	bool excl = false;
};

/**
 * Just a vector with a shared lock, nothing more than that
 */
template<typename T, typename A = std::allocator<T> >
class vector_lock : public std::vector<T,A> {
public:
	inline void lock() { _lock.lock(); }
	inline void unlock() { _lock.unlock(); }
	inline void upgrade() { _lock.upgrade(); }
	inline void shared_lock() { _lock.shared_lock(); }
	inline void shared_unlock() { _lock.shared_unlock(); }
private:
	rwlock _lock;
};

}


#endif /* SRC_RWLOCK_HPP_ */
