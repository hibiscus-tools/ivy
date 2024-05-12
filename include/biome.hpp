#pragma once

#include <filesystem>
#include <list>
#include <optional>

#include <microlog/microlog.h>

#include "components.hpp"
#include "core/mesh.hpp"
#include "core/material.hpp"
#include "core/transform.hpp"

namespace ivy {

// Forward declarations
struct Biome;
struct Inhabitant;

// Each reference is really an index to the component in the vectorized list
template <typename T>
struct ComponentRef {
	// The originating table
	std::reference_wrapper <std::vector <T>> table;

	// Index, if refering to concrete value
	std::optional <uint32_t> index;

	// Table reference initialization (no value)
	ComponentRef(const std::reference_wrapper <std::vector <T>> &table_)
			: table(table_) {}

	// Table and index
	ComponentRef(const std::reference_wrapper <std::vector <T>> &table_, uint32_t index_)
			: table(table_), index(index_) {}

	// Checking validity
	bool has_value() const {
		return index.has_value();
	}

	// Getting the index
	uint32_t hash() const {
		return *index;
	}

	// Retrieving the value
	T *operator->() {
		return &table.get()[*index];
	}

	const T *operator->() const {
		return &table.get()[*index];
	}

	T &operator*() {
		return table.get()[*index];
	}

	const T &operator*() const {
		return table.get()[*index];
	}
};

// Type table for component dependencies
template <typename T>
struct dependency_translation {
	static void check(const Inhabitant &) {}
};

struct Inhabitant {
	std::reference_wrapper <Biome> biome;

	Inhabitant(Biome &, const std::string &);

	std::string identifier;
	ComponentRef <Inhabitant> parent;
	std::vector <ComponentRef <Inhabitant>> children;

	// Component references
	ComponentRef <Transform> transform;
	ComponentRef <Geometry> geometry;
	ComponentRef <Collider> collider;

	// Checking for components
	template <typename T, typename ... Args>
	bool has() const {
		if constexpr (sizeof...(Args) == 0) {
			if constexpr (std::is_same <T, Transform> ::value) {
				return transform.has_value();
			} else if constexpr (std::is_same <T, Geometry> ::value) {
				return geometry.has_value();
			} else {
				static_assert(false, "Unknown component type");
			}

			return false;
		} else {
			return has <T> () && has <Args...> ();
		}
	}

	// Grabbing components (even multiple) at a time
	template <typename T, typename ... Args>
	std::optional <std::tuple <ComponentRef <T>, ComponentRef <Args>...>> grab() const {
		if constexpr (sizeof...(Args) == 0) {
			if constexpr (std::is_same <T, Transform> ::value) {
				if (transform.has_value())
					return std::optional(std::make_tuple(transform));
			} else if constexpr (std::is_same <T, Geometry> ::value) {
				if (geometry.has_value())
					return std::optional(std::make_tuple(geometry));
			} else {
				static_assert(false, "Unknown component type");
			}

			return std::nullopt;
		} else {
			auto prev = grab <Args...> ();
			if (!prev)
				return std::nullopt;

			auto current = grab <T> ();
			if (!current)
				return std::nullopt;

			return std::tuple_cat(*current, *prev);
		}
	}

	template <typename T, typename ... Args>
	// requires std::is_constructible_v <T, Args...>
	void add_component(Args ...args);
};

// Dependency specializations
template <>
struct dependency_translation <Geometry> {
	static void check(const Inhabitant &i) {
		if (!i.has <Transform> ())
			ulog_warning("geometry component", "Inhabitant (%s) is missing a Transform!\n", i.identifier.c_str());
	}
};

// A biome is a collection of geometry and materials
struct Biome {
	std::vector <Inhabitant> inhabitants;
	std::vector <Transform> transforms;
	std::vector <Geometry> geometries;
	std::vector <Collider> colliders;
	// TODO: recycling vector

	// Default is OK
	Biome() = default;

	// No copies
	Biome(const Biome &) = delete;
	Biome &operator=(const Biome &) = delete;

	ComponentRef <Inhabitant> new_inhabitant();

	// Gathering components
	template <typename ... Args>
	auto grab_all() {
		std::vector <std::tuple <ComponentRef <Args>...>> tuples;
		for (auto &inh : inhabitants) {
			// TODO: has instead? faster?
			auto tuple = inh.grab <Args...> ();
			if (tuple)
				tuples.push_back(*tuple);
		}

		return tuples;
	}

	template <typename ... Args>
	auto grab_all() const {
		std::vector <std::tuple <ComponentRef <Args>...>> tuples;
		for (auto &inh : inhabitants) {
			// TODO: has instead? faster?
			auto tuple = inh.grab <Args...> ();
			if (tuple)
				tuples.push_back(*tuple);
		}

		return tuples;
	}

	// Loading from a file
	// TODO: standard scene description vs in house format
	static Biome &blank();
	static Biome &load(const std::filesystem::path &);

	// Fixed memory location of all actively loaded biomes
	static std::list <Biome> active;
};

template <typename T, typename ... Args>
// requires std::is_constructible_v <T, Args...>
void Inhabitant::add_component(Args ...args)
{
	dependency_translation <T> ::check(*this);

	// TODO: defer to structure specializations
	Biome &b = biome;
	if constexpr (std::is_same_v <T, Geometry>) {
		uint32_t size = b.geometries.size();
		b.geometries.emplace_back(args...);
		geometry = { std::ref(b.geometries), size };
	} else if constexpr (std::is_same_v <T, Collider>) {
		uint32_t size = b.colliders.size();
		b.colliders.emplace_back(args...);
		collider = { std::ref(b.colliders), size };
	} else if constexpr (std::is_same_v <T, Transform>) {
		uint32_t size = b.transforms.size();
		b.transforms.emplace_back(args...);
		transform = { std::ref(b.transforms), size };
	} else {
		static_assert(false, "Unsupported component type");
	}
}

}
