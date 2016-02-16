/*
 *  Cypress -- C++ Spiking Neural Network Simulation Framework
 *  Copyright (C) 2016  Andreas Stöckel
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#ifndef CYPRESS_CORE_NETWORK_HPP
#define CYPRESS_CORE_NETWORK_HPP

#include <iostream>

#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <sstream>

#include <cypress/core/connector.hpp>
#include <cypress/core/neurons.hpp>
#include <cypress/util/clone_ptr.hpp>

namespace cypress {

/*
 * Forward declarations
 */
class Backend;
class Network;
class NeuronBase;
class PopulationBase;
class PopulationImpl;
class NetworkImpl;
template <typename T>
class PopulationView;
template <typename T>
class Population;
template <typename T>
class Neuron;

/**
 * Class defining the basic static method used to connect neurons.
 */
template <typename Impl>
class PopulationConnector {
private:
	Impl &impl() { return reinterpret_cast<Impl &>(*this); }

	const Impl &impl() const { return reinterpret_cast<const Impl &>(*this); }

	uint32_t i0() const { return impl().begin()->idx(); }
	uint32_t i1() const { return impl().end()->idx(); }

public:
	/**
	 * Connects all neurons in this population with the range of neurons
	 * pointed at by the target iterators.
	 *
	 * @param tar_begin is an iterator pointing at the first neuron in the
	 * target population that should be connected.
	 * @param tar_end is an iterator pointing at the first neuron in the
	 * target population that should be connected.
	 * @param connector is the object establishing the actual connections.
	 */
	template <typename TargetIterator>
	Impl &connect(TargetIterator tar_begin, TargetIterator tar_end,
	              std::unique_ptr<Connector> connector,
	              typename std::enable_if<true>::type * = nullptr)
	{
		impl().network().connect(impl(), i0(), i1(), tar_begin->population(),
		                         tar_begin->idx(), tar_end->idx(),
		                         std::move(connector));
		return impl();
	}

	/**
	 * Connects all neurons in this population with the single neuron
	 * pointed at by the target iterator.
	 *
	 * @param tar is an interator pointing at the target neuron.
	 * @param connector is the object establishing the actual connections.
	 */
	template <typename TargetIterator>
	Impl &connect(TargetIterator tar, std::unique_ptr<Connector> connector,
	              typename std::enable_if<
	                  std::is_base_of<std::random_access_iterator_tag,
	                                  TargetIterator>::value>::type * = nullptr)
	{
		impl().network().connect(impl(), i0(), i1(), tar->population(),
		                         tar->idx(), tar->idx() + 1,
		                         std::move(connector));
		return impl();
	}

	/**
	 * Connects all neurons in this population with the given single neuron.
	 *
	 * @param tar is the target neuron.
	 * @param connector is the object.
	 */
	template <typename TargetNeuron>
	Impl &connect(
	    TargetNeuron tar, std::unique_ptr<Connector> connector,
	    typename std::enable_if<
	        std::is_base_of<NeuronBase, TargetNeuron>::value>::type * = nullptr)
	{
		impl().network().connect(impl(), i0(), i1(), tar.population(),
		                         tar.idx(), tar.idx() + 1,
		                         std::move(connector));
		return impl();
	}

	/**
	 * Connects all neurons in this population with all neurons in the given
	 * target population.
	 *
	 * @param nid_src0 is the index of the first neuron in this population
	 * that is going to be connected.
	 * @param nid_src1 is the index of the last-plus-one neuron in this
	 * population being connected.
	 * @param tar is an iterator pointing at the target neuron.
	 */
	template <typename TargetPopulation>
	Impl &connect(
	    TargetPopulation tar, std::unique_ptr<Connector> connector,
	    typename std::enable_if<
	        std::is_base_of<PopulationBase, TargetPopulation>::value>::type * =
	        nullptr)
	{
		impl().network().connect(impl(), i0(), i1(), tar, tar.begin()->idx(),
		                         tar.end()->idx(), std::move(connector));
		return impl();
	}
};

/**
 * Mixin class providing all iterator-related facilities to a Population class.
 */
template <typename Value, typename Begin, typename End, typename Impl>
class PopulationIterator {
public:
	/**
	 * Generic class implementing the various iterator types exposed by the
	 * Population class. Note that comparison operators only compare the neuron
	 * index and do not check for equivalence of the population object.
	 * Comparing
	 * iterators from different population objects is nonsense.
	 *
	 * @tparam Dir is the direction into which the iterator advances.
	 * @tparam Const if true, the iterator allows no write access to the
	 * underlying population. If false, the corresponding access operators are
	 * enabled.
	 */
	template <int Dir, bool Const>
	class iterator_ : public std::iterator<std::random_access_iterator_tag,
	                                       Value, ssize_t, Value, Value> {
	private:
		/**
		 * Shorthand for the own type.
		 */
		using Self = iterator_<Dir, Const>;

		/**
		 * Pointer at the backing population object. May either be a const
		 * pointer (if the Const flag is true) or a normal pointer (if the Const
		 * flag is false).
		 */
		using PopulationPtr =
		    typename std::conditional<Const, PopulationBase const *,
		                              PopulationBase *>::type;

		/**
		 * Reference to the graph instance this iterator is refering to. The
		 * graph instance is used to dereference
		 */
		PopulationPtr m_population;

		/**
		 * Current neuron index the iterator points at.
		 */
		ssize_t m_idx;

	public:
		/**
		 * Constructor, creates an iterator pointing at the neuron with the
		 * index idx in the given population.
		 *
		 * @param population is a pointer at the underlying population.
		 * @param idx is the index of the neuron.
		 */
		iterator_(PopulationPtr population, ssize_t idx)
		    : m_population(population), m_idx(idx)
		{
		}

		/**
		 * Allow implicit conversion to const-iterators.
		 */
		operator iterator_<Dir, true>() const
		{
			return iterator_<Dir, true>(m_population, m_idx);
		}

		bool operator==(const Self &o) const { return m_idx == o.m_idx; }

		bool operator!=(const Self &o) const { return m_idx != o.m_idx; }

		bool operator<(const Self &o) const
		{
			return m_idx * Dir < o.m_idx * Dir;
		}

		bool operator>(const Self &o) const
		{
			return m_idx * Dir > o.m_idx * Dir;
		}

		bool operator<=(const Self &o) const
		{
			return m_idx * Dir <= o.m_idx * Dir;
		}

		bool operator>=(const Self &o) const
		{
			return m_idx * Dir >= o.m_idx * Dir;
		}

		Self operator+(ssize_t b)
		{
			return Self(m_population, m_idx + b * Dir);
		}

		Self operator-(ssize_t b)
		{
			return Self(m_population, m_idx - b * Dir);
		}

		ssize_t operator-(const Self &o) const
		{
			return (m_idx - o.m_idx) * Dir;
		}

		Self &operator++()
		{
			m_idx += Dir;
			return *this;
		}
		Self operator++(int)
		{
			auto cpy = Self(m_population, m_idx);
			m_idx += Dir;
			return cpy;
		}

		Self &operator--()
		{
			m_idx -= Dir;
			return *this;
		}

		Self operator--(int)
		{
			auto cpy = Self(m_population, m_idx);
			m_idx -= Dir;
			return cpy;
		}

		Self &operator+=(ssize_t n)
		{
			m_idx += n * Dir;
			return *this;
		}

		Self &operator-=(ssize_t n)
		{
			m_idx -= n * Dir;
			return *this;
		}

		template <typename U = Value,
		          typename = typename std::enable_if<!Const, U>::type>
		Value operator*()
		{
			return Value(*m_population, m_idx);
		}

		template <typename U = Value,
		          typename = typename std::enable_if<!Const, U>::type>
		Value operator->()
		{
			return Value(*m_population, m_idx);
		}

		template <typename U = Value,
		          typename = typename std::enable_if<!Const, U>::type>
		Value operator[](ssize_t n)
		{
			return Value(*m_population, m_idx + n * Dir);
		}

		Value operator*() const { return Value(*m_population, m_idx); }

		Value operator->() const { return Value(*m_population, m_idx); }

		Value operator[](ssize_t n) const
		{
			return Value(*m_population, m_idx + n * Dir);
		}
	};

private:
	Impl *impl() { return reinterpret_cast<Impl *>(this); }
	const Impl *impl() const { return reinterpret_cast<const Impl *>(this); }

protected:
	ssize_t i0() const { return Begin()(impl()); }
	ssize_t i1() const { return End()(impl()); }
	ssize_t ri0() const { return End()(impl()) - 1; }
	ssize_t ri1() const { return Begin()(impl()) - 1; }

#ifndef NDEBUG
	/**
	 * Makes sure the given begin and end indices are inside the valid range.
	 */
	void check_range(ssize_t begin_idx, ssize_t end_idx) const
	{
		if (begin_idx < i0() || end_idx > i1()) {
			throw std::out_of_range(
			    "Range must be a subset of the source population range.");
		}
	}
#else
	void check_range(ssize_t, ssize_t) const {}
#endif

public:
	using iterator = iterator_<1, false>;
	using const_iterator = iterator_<1, true>;
	using reverse_iterator = iterator_<-1, false>;
	using const_reverse_iterator = iterator_<-1, true>;

	/**
	 * Returns a reference at the i-th neuron in the Population or
	 * PopulationView.
	 */
	Value operator[](size_t i) { return Value(*impl(), i0() + i); }

	/**
	 * Returns a const-reference at the i-th neuron.
	 */
	const Value operator[](size_t i) const { return Value(*impl(), i0() + i); }

	size_t size() { return i1() - i0(); }

	iterator begin() { return iterator(impl(), i0()); }

	iterator end() { return iterator(impl(), i1()); }

	reverse_iterator rbegin() { return reverse_iterator(impl(), ri0()); }

	reverse_iterator rend() { return reverse_iterator(impl(), ri1()); }

	const_iterator begin() const { return cbegin(); }

	const_iterator end() const { return cend(); }

	const_reverse_iterator rbegin() const { return crbegin(); }

	const_reverse_iterator rend() const { return crend(); }

	const_iterator cbegin() const { return const_iterator(impl(), i0()); }

	const_iterator cend() const { return const_iterator(impl(), i1()); }

	const_reverse_iterator crbegin() const
	{
		return const_reverse_iterator(impl(), ri0());
	}

	const_reverse_iterator crend() const
	{
		return const_reverse_iterator(impl(), ri1());
	}
};

/**
 * Internally used mixin class which is used to store the first and the last
 * index of the neurons represented by a PopulationView.
 */
class PopulationViewRange {
private:
	/**
	 * Index of the first neuron.
	 */
	size_t m_begin_idx;

	/**
	 * Index of the last plus one neuron.
	 */
	size_t m_end_idx;

public:
	/**
	 * Constructor of the PopulationViewData mixin. Simply copies the given
	 * arguments to its internal storage.
	 *
	 * @param begin_idx is the index of the first neuron in the population view.
	 * @param end_idx is the index of the last plus one neuron in the population
	 * view.
	 */
	PopulationViewRange(size_t begin_idx, size_t end_idx)
	    : m_begin_idx(begin_idx), m_end_idx(end_idx)
	{
	}

	/**
	 * Functor which allows to access the index of the first element from a
	 * pointer to PopulationViewRange.
	 */
	struct Begin {
		ssize_t operator()(const PopulationViewRange *range)
		{
			return static_cast<const PopulationViewRange *>(range)->m_begin_idx;
		}
	};

	/**
	 * Functor which allows to access the index of the first element from a
	 * pointer to PopulationViewRange.
	 */
	struct End {
		ssize_t operator()(const PopulationViewRange *range)
		{
			return static_cast<const PopulationViewRange *>(range)->m_end_idx;
		}
	};

	/**
	 * Internally used functor which returns the index of the first index in a
	 * population (which always is zero).
	 */
	struct StaticBegin {
		ssize_t operator()(const PopulationBase *) { return 0; }
	};

	/**
	 * Internally used functor which returns the index of the last plus one
	 * index in a population (which always corresponds to its size).
	 */
	struct StaticEnd {
		ssize_t operator()(const PopulationBase *pop);
	};
};

/**
 * Class representing a neuron population of unspecified type. Note that this
 * instance is merely a handle at the actual population object. It can be
 * cheaply copied around. Do not directly use this class, but the more advanced,
 * templated Population<T> class.
 */
class PopulationBase
    : public PopulationConnector<PopulationBase>,
      public PopulationIterator<NeuronBase, PopulationViewRange::StaticBegin,
                                PopulationViewRange::StaticEnd,
                                PopulationBase> {
private:
	friend class Network;
	friend void connect(PopulationBase &, size_t, size_t, PopulationBase &tar,
	                    size_t, size_t, std::unique_ptr<Connector>);

	/**
	 * Reference at the network instance from which this population handle was
	 * retrieved.
	 */
	Network &m_network;

	/**
	 * Reference at the actual implementation of the population object.
	 */
	PopulationImpl &m_impl;

protected:
	PopulationImpl &impl() { return m_impl; }
	const PopulationImpl &impl() const { return m_impl; }

	/**
	 * Constructor of the PopulationBase class. Populations can only be created
	 * by the Network class.
	 */
	explicit PopulationBase(Network &network, PopulationImpl &impl)
	    : m_network(network), m_impl(impl){};

public:
	PopulationBase(const PopulationBase &) = default;
	PopulationBase(PopulationBase &&) noexcept = default;
	PopulationBase &operator=(const PopulationBase &) = default;
	PopulationBase &operator=(PopulationBase &&) = default;

	/**
	 * Returns a reference at the underlying network instance.
	 */
	Network &network() { return m_network; }

	/**
	 * Returns a const reference at the underlying network instance.
	 */
	const Network &network() const { return m_network; }

	/**
	 * Returns a reference at the neuron type descriptor.
	 */
	const NeuronType &type() const;

	/**
	 * Returns the size of the population.
	 */
	size_t size() const;

	/**
	 * Returns the name of the population.
	 */
	const std::string &name() const;

	/**
	 * Returns true if all neurons in the population share the same parameters.
	 */
	bool homogeneous() const;
};

/**
 * Class representing a neuron of unspecified type -- consists of a reference
 * at the underlying population instance and the index of the neuron within
 * that population. Note that this class merely represents a reference at the
 * underlying neuron. Copying this class will not create a new neuron.
 */
class NeuronBase {
private:
	/**
	 * Reference at the internal, non-public population object this neuron
	 * belongs to.
	 */
	PopulationBase m_pop;

	/**
	 * Index of the neuron within the neuron population.
	 */
	size_t m_idx;

public:
	/**
	 * Creates a new NeuronBase instance pointing at idx-th neuron in the given
	 * population.
	 *
	 * @param population is the parent population.
	 * @param idx is the neuron within the parent population.
	 */
	NeuronBase(const PopulationBase &population, size_t idx)
	    : m_pop(population), m_idx(idx)
	{
	}

	/**
	 * Returns a handle at the population this neuron is part of.
	 */
	PopulationBase population() { return m_pop; }

	/**
	 * Returns a const-handle at the population this neuron is part of.
	 */
	const PopulationBase population() const { return m_pop; }

	/**
	 * Returns a reference at the neuron type descriptor.
	 */
	const NeuronType &type() const { return m_pop.type(); }

	/**
	 * Returns the index of this neuron within the population. These indices
	 * are always relative to the top-level population and not to the population
	 * view the neuron might have been accessed from.
	 *
	 * @return the index of the neuron in the top-level population.
	 */
	size_t idx() const { return m_idx; }

	/*
	 * Deference operator. Needed for the population iterator ->() operator to
	 * be standards compliant.
	 */
	NeuronBase *operator->() { return this; }
	const NeuronBase *operator->() const { return this; }

	/*
	 * As we already have the -> operator, we should also implement the *
	 * operator.
	 */
	NeuronBase &operator*() { return *this; }
	const NeuronBase &operator*() const { return *this; }
};

template <typename T>
class Neuron : public NeuronBase {
public:
	using NeuronBase::NeuronBase;

	using NeuronType = T;
	using Parameters = typename T::Parameters;

	const NeuronType &type() { return NeuronType::inst(); }

	Parameters &parameters() { return population().parameters(idx()); }

	/**
	 * Returns a handle at the population this neuron is part of.
	 */
	Population<T> population()
	{
		return Population<T>(NeuronBase::population());
	}

	/**
	 * Returns a const-handle at the population this neuron is part of.
	 */
	const Population<T> population() const
	{
		return Population<T>(NeuronBase::population());
	}

	/*
	 * Deference operator. Needed for the population iterator ->() operator to
	 * be standards compliant.
	 */
	Neuron<T> *operator->() { return this; }
	const Neuron<T> *operator->() const { return this; }

	/*
	 * As we already have the -> operator, we should also implement the *
	 * operator.
	 */
	Neuron<T> &operator*() { return *this; }
	const Neuron<T> &operator*() const { return *this; }
};

/**
 * The PopulationViewBase class facilitates access at a portion of a neuron
 * population. It allows to set neuron parameters for the currently viewed
 * portion of the population, to iterate over the neurons in the range and to
 * establish connections to other population views.
 *
 * The perculiar construction with the "Begin" and "End" template parameters
 * has the following rationale: The Population<T> class is a specialisation of
 * PopulationViewBase, as it is less general (it only allows to iterate from
 * the first neuron of a population to the last), yet apart from this provides
 * exactly the same interface as PopulationViewBase. However, Population<T>
 * does not need to store the indices of the first and last neuron, since these
 * indices are always "0" and "size()". Begin and End are functors which allow
 * to access first and last index and may either return some constant or the
 * acutal indices.
 *
 * @tparam T is the neuron type.
 * @tparam Begin is a functor to which a "this" pointer is passed and which
 * should return the index of the first neuron in the population view.
 * @tparam End is a function to which a "this" pointer is passed and which
 * should return the index of the last plus one neuron in the population view.
 * @tparam Impl is the actually implementing type.
 */
template <typename T, typename Begin, typename End, typename Impl>
class PopulationViewBase
    : public PopulationBase,
      public PopulationConnector<Impl>,
      public PopulationIterator<Neuron<T>, Begin, End, Impl> {
private:
	template <typename T2>
	friend class Population;

	template <typename T2>
	friend class PopulationView;

protected:
	using PopulationBase::PopulationBase;
	using PopulationBase::impl;

	using PopulationIterator<Neuron<T>, Begin, End, Impl>::i0;
	using PopulationIterator<Neuron<T>, Begin, End, Impl>::i1;
	using PopulationIterator<Neuron<T>, Begin, End, Impl>::ri0;
	using PopulationIterator<Neuron<T>, Begin, End, Impl>::ri1;
	using PopulationIterator<Neuron<T>, Begin, End, Impl>::check_range;

public:
	using NeuronType = T;
	using Parameters = typename T::Parameters;

	/*
	 * Forward all the "connect" methods provided by the PopulationConnector
	 * base class.
	 */
	using PopulationConnector<Impl>::connect;

	/*
	 * Forward the various container access methods provided by
	 * PopulationIterator.
	 */
	using PopulationIterator<Neuron<T>, Begin, End, Impl>::iterator;
	using PopulationIterator<Neuron<T>, Begin, End, Impl>::reverse_iterator;
	using PopulationIterator<Neuron<T>, Begin, End, Impl>::const_iterator;
	using PopulationIterator<Neuron<T>, Begin, End,
	                         Impl>::const_reverse_iterator;
	using PopulationIterator<Neuron<T>, Begin, End, Impl>::operator[];
	using PopulationIterator<Neuron<T>, Begin, End, Impl>::begin;
	using PopulationIterator<Neuron<T>, Begin, End, Impl>::end;
	using PopulationIterator<Neuron<T>, Begin, End, Impl>::rbegin;
	using PopulationIterator<Neuron<T>, Begin, End, Impl>::rend;
	using PopulationIterator<Neuron<T>, Begin, End, Impl>::cbegin;
	using PopulationIterator<Neuron<T>, Begin, End, Impl>::cend;
	using PopulationIterator<Neuron<T>, Begin, End, Impl>::crbegin;
	using PopulationIterator<Neuron<T>, Begin, End, Impl>::crend;
	using PopulationIterator<Neuron<T>, Begin, End, Impl>::size;

	/**
	 * Returns a reference at the underlying neuron type descriptor.
	 */
	const NeuronType &type() const { return NeuronType::inst(); }

	/**
	 * Returns a new population view which represents a range of neurons within
	 * this Population or PopulationView. If this class already is a population
	 * view the given indices are relative to the first neuron in this view.
	 *
	 * @param begin_idx is the index of the first neuron in the resulting
	 * population view, relative to the index of the first neuron in this
	 * population.
	 * @param end_idx is the index of the last plus one neuron in the resulting
	 * population view, relative to the index of the first neuron in this
	 * population.
	 */
	PopulationView<T> range(size_t begin_idx, size_t end_idx);
};

/**
 * The PopulationView class allows to view a certain range of neurons within a
 * population. This facilitates to construct connections between parts of
 * populations. Note that this class is merely a handle representing a part of
 * a population. Copying this object is cheap and will not create a new
 * population.
 *
 * @tparam T is the neuron type of the neurons stored in this population.
 */
template <typename T>
class PopulationView
    : public PopulationViewBase<T, PopulationViewRange::Begin,
                                PopulationViewRange::End, PopulationView<T>>,
      public PopulationViewRange {
public:
	using PopulationViewBaseType =
	    PopulationViewBase<T, PopulationViewRange::Begin,
	                       PopulationViewRange::End, PopulationView<T>>;

	/**
	 * Creates a new PopulationView instance which corresponds to a subrange of
	 * another population or population view. Checks whether the given begin_idx
	 * and end_idx actually are a subrange of the parent object. This behaviour
	 * is disabled if NDEBUG is defined.
	 *
	 * @param parent is the Population or PopulationView object for which a view
	 * should be constructed.
	 * @param begin_idx is the index of the first neuron in the parent
	 * population which should be represented by the new population view.
	 * @param end_idx is the index of the last plus one neuron in the parent
	 * population which should be represented by the new population view.
	 */
	template <typename PopulationType>
	PopulationView(PopulationType &parent, size_t begin_idx, size_t end_idx)
	    : PopulationViewBaseType(parent.network(), parent.impl()),
	      PopulationViewRange(parent.i0() + begin_idx, parent.i0() + end_idx)
	{
		parent.check_range(PopulationViewBaseType::i0(),
		                   PopulationViewBaseType::i1());
	}
};

/**
 * The Population class represents a population (a set of) neurons of the same
 * neuron type T. Note that this class is merely a handle representing a
 * population. Copying this object is cheap and will not create a new
 * population.
 *
 * @tparam T is the type of neurons represented by the population.
 */
template <typename T>
class Population
    : public PopulationViewBase<T, PopulationViewRange::StaticBegin,
                                PopulationViewRange::StaticEnd, Population<T>> {
private:
	friend class Network;

	using PopulationViewBaseType =
	    PopulationViewBase<T, PopulationViewRange::StaticBegin,
	                       PopulationViewRange::StaticEnd, Population<T>>;

protected:
	using PopulationViewBaseType::PopulationViewBaseType;

public:
	/**
	 * Constructor of the population class. Creates a new population with the
	 * correct population type, size and the given name.
	 */
	Population(Network &network, size_t size,
	           const typename T::Parameters &params = typename T::Parameters(),
	           const std::string &name = std::string());
};

/**
 * Class representing the entire spiking neural network.
 */
class Network {
private:
	/**
	 * Hidden implementation of the network. The clone_ptr is used instead of a
	 * custom pointer to allow the network instance to be cloned.
	 */
	clone_ptr<NetworkImpl> m_impl;

	template <typename T>
	friend class Population;

	/**
	 * Internally used to add a new population. Use the templated public
	 * create_population method instead.
	 */
	PopulationImpl &create_population(size_t size, const NeuronType &type,
	                                  const NeuronParametersBase &params,
	                                  const std::string &name);

	/**
	 * Internally used to access populations of a given name and optionally
	 * type.
	 *
	 * @param name the name of the population being searched. If empty, all
	 * populations are matched.
	 * @param type is the type of the population being searched. If nullptr,
	 * all populations are matched.
	 * @return a vector of pointers at the PopulationImpl instances backing the
	 * populations.
	 */
	std::vector<PopulationImpl *> populations(const std::string &name,
	                                          const NeuronType *type);

public:
	class NoSuchPopulationException : public std::runtime_error {
	public:
		using std::runtime_error::runtime_error;
	};

	class InvalidConnectionException : public std::runtime_error {
	public:
		using std::runtime_error::runtime_error;
	};

	/**
	 * Constructor of the network class -- returns an empty network.
	 */
	Network();
	Network(const Network &);
	Network(Network &&) noexcept;
	Network &operator=(const Network &);
	Network &operator=(Network &&);

	/**
	 * Destructor of the network class -- destroys the network. All handles
	 * (Population, PopulationView and Neuron) instances still pointing at the
	 * network will become invalid.
	 */
	~Network();

	/**
	 * Returns the time stamp of the last spike stored in a SpikeSourceArray.
	 */
	float duration() const;

	/**
	 * Base connection method. Connects a range of neurons in the source
	 * population
	 * to a range of neurons in the target population. All connection methods
	 * in PopulationConnections are relayed to this method.
	 *
	 * @param src is the source population.
	 * @param nid_src0 is the index of the first neuron in this population that
	 * is going to be connected.
	 * @param nid_src1 is the index of the last-plus-one neuron in this
	 * population being connected.
	 * @param tar is the target population.
	 * @param nid_tar0 is the index of the first neuron in the target population
	 * that is going to be connected.
	 * @param nid_tar1 is the index of the last-plus-one neuron in the target
	 * population that is going to be connected.
	 * @return a reference at this object for simple method chaining.
	 */
	Network &connect(PopulationBase &src, size_t nid_src0, size_t nid_src1,
	                 PopulationBase &tar, size_t nid_tar0, size_t nid_tar1,
	                 std::unique_ptr<Connector> connector);

	/**
	 * Creates a new population instance.
	 *
	 * @param size is the size of the population.
	 * @param params contains the initial parameters for all neurons in this
	 * population.
	 * @param name is the name of the population. The name can be used for
	 * visualisation purposes and to retrieve a population by name.
	 */
	template <typename T>
	Population<T> create_population(
	    size_t size,
	    const typename T::Parameters &params = typename T::Parameters(),
	    const std::string &name = std::string())
	{
		return Population<T>(*this, size, params, name);
	}

	/**
	 * Returns population objects pointing at the population with the given
	 * name.
	 *
	 * @param name is the name of the population that is being searched for.
	 * If an empty string is given, all populations are returned.
	 * @return a vector of population objects.
	 */
	std::vector<PopulationBase> populations(
	    const std::string &name = std::string())
	{
		std::vector<PopulationBase> res;
		for (PopulationImpl *p : populations(name, nullptr)) {
			res.push_back(PopulationBase(*this, *p));
		}
		return res;
	}

	/**
	 * Returns population objects pointing at the population with the given
	 * name.
	 *
	 * @tparam T is the neuron type of the population that should be returned.
	 * @param name is the name of the population that is being searched for. If
	 * empty, all populations are matched.
	 * @return a vector of population objects.
	 */
	template <typename T>
	std::vector<Population<T>> populations(
	    const std::string &name = std::string())
	{
		std::vector<Population<T>> res;
		for (PopulationImpl *p : populations(name, &T::inst())) {
			res.push_back(Population<T>(*this, *p));
		}
		return res;
	}

	/**
	 * Returns a Population object pointing at the last created population with
	 * the given name. If no such population exists, an exception is thrown.
	 *
	 * @param name is the name of the population that should be looked up. If
	 * empty, the last created population is returned.
	 * @return a population handle pointing at the requested population.
	 */
	PopulationBase population(const std::string &name = std::string())
	{
		auto pops = populations(name);
		if (pops.empty()) {
			std::stringstream ss;
			ss << "Population with the given name \"" << name
			   << "\" does not exist.";
			throw NoSuchPopulationException(ss.str());
		}
		return pops.back();
	}

	/**
	 * Returns a Population object pointing at the last created population with
	 * the given name. If no such population exists, an exception is thrown.
	 *
	 * @tparam T is the neuron type of the population that should be returned.
	 * @param name is the name of the population that should be looked up. If
	 * empty, the last created population of the given type is returned.
	 * @return a population handle pointing at the requested population.
	 */
	template <typename T>
	Population<T> population(const std::string &name = std::string())
	{
		auto pops = populations<T>(name);
		if (pops.empty()) {
			std::stringstream ss;
			ss << "Population of type \"" << T::inst().name
			   << "\" with the given name \"" << name << "\" does not exist.";
			throw NoSuchPopulationException(ss.str());
		}
		return pops.back();
	}

	/**
	 * Executes the network on the given backend and stores the results in the
	 * population objects. This function is simply a wrapper for Backend.run().
	 * If there is an error during execution, the run function will throw a
	 * exception.
	 *
	 * @param backend is a reference at the backend instance the network should
	 * be executed on.
	 * @param duration is the simulation-time. If a value smaller or equal to
	 * zero is given, the simulation time is automatically chosen.
	 * @return a reference at this Network instance for simple method chaining.
	 */
	Network &run(const Backend &backend, float duration = 0.0);
};

/*
 * Out-of class member function definitions
 */

inline ssize_t PopulationViewRange::StaticEnd::operator()(
    const PopulationBase *pop)
{
	return pop->size();
}

template <typename T, typename Begin, typename End, typename Impl>
inline PopulationView<T> PopulationViewBase<T, Begin, End, Impl>::range(
    size_t begin_idx, size_t end_idx)
{
	return PopulationView<T>(*this, begin_idx, end_idx);
}

template <typename T>
inline Population<T>::Population(Network &network, size_t size,
                                 const typename T::Parameters &params,
                                 const std::string &name)
    : Population<T>::PopulationViewBaseType(
          network, network.create_population(size, T::inst(), params, name))
{
}
}

#endif /* CYPRESS_CORE_NETWORK_HPP */

