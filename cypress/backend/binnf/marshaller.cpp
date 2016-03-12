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

#include <algorithm>
#include <vector>
#include <string>
#include <unordered_set>

#include <cypress/backend/binnf/binnf.hpp>
#include <cypress/backend/binnf/marshaller.hpp>
#include <cypress/core/network_base.hpp>
#include <cypress/core/network_base_objects.hpp>
#include <cypress/core/neurons.hpp>
#include <cypress/util/matrix.hpp>

namespace cypress {
namespace binnf {

static const auto INT = NumberType::INT;
static const auto FLOAT = NumberType::FLOAT;

/**
 * Constructs and sends the matrix containing the populations to the simulator.
 *
 * @param populations is a vector containing the population descriptors.
 * @param os is the output stream to which the population descriptors should be
 * written.
 */
static void write_populations(const std::vector<PopulationBase> &populations,
                              std::ostream &os)
{
	Header header = {{"count", "type"}, {INT, INT}};

	// Fetch all signals available in the populations
	std::unordered_set<std::string> signals;
	for (auto const &population : populations) {
		for (auto const &signal_name : population.type().signal_names) {
			signals.emplace(signal_name);
		}
	}

	// Add the signals to the header
	for (auto const &signal : signals) {
		header.names.emplace_back(std::string("record_") + signal);
		header.types.emplace_back(INT);
	}

	// Fill the populations matrix
	Matrix<Number> mat(populations.size(), header.size());
	for (size_t i = 0; i < populations.size(); i++) {
		mat(i, 0) = int32_t(populations[i].size());
		mat(i, 1) = int32_t(populations[i].type().type_id);
		size_t j = 2;
		for (auto const &signal : signals) {
			// Lookup the signal index for this population
			auto idx = populations[i].type().signal_index(signal);
			bool record = false;  // If this signal does not exist in this
			                      // population, do not try to record it
			if (idx.valid()) {
				if (populations[i].homogeneous_record()) {
					// All neurons in this population use the same record flag
					record = populations[i].signals().is_recording(idx.value());
				}
				else {
					// Check whether any neuron in this population wants to
					// record this signal
					for (size_t k = 0; k < populations[i].size(); k++) {
						record = record ||
						         populations[i][k].signals().is_recording(
						             idx.value());
					}
				}
			}
			mat(i, j++) = record;
		}
	}

	serialise(os, "populations", header, mat);
}

/**
 * Constructs and sends the connection matrix to the simulator.
 */
static void write_connections(
    const std::vector<ConnectionDescriptor> &descrs, std::ostream &os,
    std::function<size_t(Connection connections[], size_t count)>
        connection_trafo)
{
	static const Header HEADER = {
	    {"pid_src", "pid_tar", "nid_src", "nid_tar", "weight", "delay"},
	    {INT, INT, INT, INT, FLOAT, FLOAT}};

	// Vector containing all connection objects
	std::vector<Connection> connections = instantiate_connections(descrs);

	// Transform the connections
	connections.resize(connection_trafo(&connections[0], connections.size()));

	serialise(os, "connections", HEADER,
	          reinterpret_cast<Number *>(&connections[0]), connections.size());
}

static void write_spike_source_array(const PopulationBase &population,
                                     std::ostream &os)
{
	static const Header TARGET_HEADER = {{"pid", "nid"}, {INT, INT}};
	static const Header SPIKE_TIMES_HEADER = {{"times"}, {FLOAT}};

	for (size_t i = 0; i < population.size(); i++) {
		const auto &params = population[i].parameters();
		if (params.size() > 0) {
			Matrix<Number> mat(1, 2);
			mat(0, 0) = int32_t(population.pid());
			mat(0, 1) = int32_t(i);

			serialise(os, "target", TARGET_HEADER, mat);
			serialise(os, "spike_times", SPIKE_TIMES_HEADER,
			          reinterpret_cast<const Number *>(params.begin()),
			          params.size());
		}
	}
}

static void write_uniform_parameters(const PopulationBase &population,
                                     std::ostream &os)
{
	static const int32_t ALL_NEURONS = std::numeric_limits<int32_t>::max();

	// Assemble the parameter header
	Header header = {{"pid", "nid"}, {INT, INT}};
	for (const auto &name : population.type().parameter_names) {
		header.names.emplace_back(name);
		header.types.emplace_back(FLOAT);
	}

	// In case the population is homogeneous, just send one entry in
	// the parameters matrix -- otherwise send an entry for each neuron in each
	// population
	const bool homogeneous = population.homogeneous_parameters();
	const size_t mat_size = homogeneous ? 1 : population.size();
	Matrix<Number> mat(mat_size, header.size());
	for (size_t i = 0; i < mat_size; i++) {
		mat(0, 0) = int32_t(population.pid());
		mat(0, 1) = int32_t(homogeneous ? ALL_NEURONS : i);

		const auto &params = population[i].parameters();
		std::copy(params.begin(), params.end(), mat.begin(i) + 2);
	}

	serialise(os, "parameters", header, mat);
}

/**
 * Sends the parameters of an individual population to the simulator.
 */
static void write_parameters(const PopulationBase &population, std::ostream &os)
{
	// Do not write anything for zero-sized populations
	if (population.size() == 0) {
		return;
	}

	// Special treatment for spike source arrays
	if (&population.type() == &SpikeSourceArray::inst()) {
		write_spike_source_array(population, os);
	}
	else {
		write_uniform_parameters(population, os);
	}
}

void marshall_network(NetworkBase &net, std::ostream &os,
                      std::function<size_t(Connection connections[],
                                           size_t count)> connection_trafo)
{
	// Write the populations
	const std::vector<PopulationBase> populations = net.populations();
	write_populations(populations, os);

	// Write the connections
	write_connections(net.connections(), os, connection_trafo);

	// Write the population parameters
	for (const auto &population : populations) {
		write_parameters(population, os);
	}
}

bool marshall_response(NetworkBase &net, std::istream &is)
{
	Block block;
	bool had_block = false;
	bool has_target = false;
	PopulationIndex tar_pid = 0;
	NeuronIndex tar_nid = 0;
	while (is.good()) {
		// Deserialise the incomming data, continue until the end of the input
		// stream is reached, skip faulty blocks
		try {
			block = deserialise(is);
			had_block = true;
		}
		catch (BinnfDecodeException ex) {
			// TODO: Log
			continue;
		}

		// Handle the block, depending on its name
		if (block.name == "target") {
			const size_t pid_col = block.colidx("pid");
			const size_t nid_col = block.colidx("nid");
			if (block.matrix.rows() != 1) {
				throw BinnfDecodeException("Invalid target block row count");
			}
			tar_pid = block.matrix(0, pid_col);
			tar_nid = block.matrix(0, nid_col);
			if (tar_pid >= ssize_t(net.population_count()) ||
			    tar_nid >= ssize_t(net.population(tar_pid).size())) {
				throw BinnfDecodeException("Invalid target neuron");
			}
			has_target = true;
		}
		else if (block.name == "spike_times") {
			if (!has_target) {
				throw BinnfDecodeException("No target neuron set");
			}
			if (block.matrix.cols() != 1 || block.colidx("times") != 0) {
				throw BinnfDecodeException("Invalid spike_times column count");
			}
			auto pop = net[tar_pid];
			auto idx = pop.type().signal_index("spikes");
			if (idx.valid() && pop.signals().is_recording(idx.value())) {
				pop[tar_nid].signals().data(
				    idx.value(),
				    std::make_shared<Matrix<float>>(
				        block.matrix.rows(), 1,
				        reinterpret_cast<float *>(block.matrix.begin())));
			}
			has_target = false;
		}
	}
	return had_block;
}
}
}
