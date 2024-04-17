#pragma once

#include <filesystem>
#include <list>
#include <optional>
#include <variant>

#include <oak/mesh.hpp>
#include <oak/material.hpp>
#include <oak/transform.hpp>

#include "components.hpp"

// Forward declarations
struct Biome;
struct Inhabitant;

// TODO: components header
// Defines the client interface for arbitrary ivy objects; by default exposes nothing
template <typename T>
struct concrete_ref_base {};

template <typename T>
struct concrete_ref {
	concrete_ref_base <T> data;

	concrete_ref(T &data_) : data(data_) {}

	concrete_ref_base <T> *operator->() {
		return &data;
	}
};

// TODO: finish the mask type
struct mask {
	uint8_t &value;
	uint8_t mask;
};

template <typename T>
struct assign_ref {
	// TODO: pointer to dirty flag
	T &dst;

	assign_ref &operator=(const T &value) {
		dst = value;
		return *this;
	}
};

// Each reference is really an index to the component in the vectorized list
template <typename T>
class ComponentRef {
	// The originating table
	// TODO: a better reference type or raw pointer
	std::reference_wrapper <std::vector <T>> table;

	// TODO: dirty flag (mask, int ref value)

	// Index, if refering to concrete value
	std::optional <uint32_t> index;
public:
	// Table reference initialization (no value)
	ComponentRef(const std::reference_wrapper <std::vector <T>> &table_)
			: table(table_) {}

	// Table and index
	ComponentRef(const std::reference_wrapper <std::vector <T>> &table_, uint32_t index_)
			: table(table_), index(index_) {}

	// Checking validity
	operator bool() const {
		return index.has_value();
	}

	// Getting the index
	uint32_t hash() {
		return *index;
	}

	// Accessing an immutable value
	const T &operator*() const {
		return table.get()[*index];
	}

	const T &value() const {
		return table.get()[*index];
	}

	// Direct assignment
	// TODO: enable only for some
	assign_ref <T> operator=(const T &value) {
		T &ref = table.get()[*index];
		return (assign_ref <T> (ref) = value);
	}

	// Accessing properties
	concrete_ref <T> operator->() {
		return table.get()[*index];
	}
};

using InhabitantRef = ComponentRef <Inhabitant>;

// TODO: ref_tracker type
// TODO: concrete ref method e.g. world()
template <>
class concrete_ref_base <Inhabitant> {
	const Inhabitant &source;

	Biome &biome;
	InhabitantRef &parent;
	std::vector <InhabitantRef> &children;
public:
	// TODO: reference to dirty bit
	std::string &identifier;
	ComponentRef <Transform> &transform;
	ComponentRef <Geometry> &geometry;

	concrete_ref_base(Inhabitant &);

	// Adding components
	template <typename T, typename ... Args>
	requires std::is_constructible_v <T, Args...>
	void add_component(const Args & ...);

	// Creating parent-child links is exclusive to this function
	friend void link(InhabitantRef &, InhabitantRef &);
};

template <>
class concrete_ref_base <Transform> {
	const Transform &source;
	// TODO: dirty flag
public:
	// TODO: index ref to the same dirty flag
	assign_ref <glm::vec3> position;
	assign_ref <glm::vec3> rotation;
	assign_ref <glm::vec3> scale;

	concrete_ref_base(Transform &t)
			: source(t), position(t.position),
			rotation(t.rotation), scale(t.scale) {}

	glm::mat4 matrix() {
		return source.matrix();
	}
};

template <>
struct concrete_ref_base <Geometry> {
	Mesh &mesh;
	Material &material;

	concrete_ref_base(Geometry &);
};

// Type table for component dependencies
template <typename T>
struct dependency_translation {
	static void check(const Inhabitant &) {}
};

struct Inhabitant {
	std::reference_wrapper <Biome> biome;

	Inhabitant(Biome &, const std::string &);

	InhabitantRef parent;

	std::string identifier;
	std::vector <InhabitantRef> children;

	// Component references
	ComponentRef <Transform> transform;
	ComponentRef <Geometry> geometry;

	// Checking for components
	template <typename T, typename ... Args>
	bool has() const {
		if constexpr (sizeof...(Args) == 0) {
			if constexpr (std::is_same <T, Transform> ::value) {
				return transform;
			} else if constexpr (std::is_same <T, Geometry> ::value) {
				return geometry;
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
	std::optional <std::tuple <ComponentRef <T>, ComponentRef <Args>...>> grab() {
		if constexpr (sizeof...(Args) == 0) {
			if constexpr (std::is_same <T, Transform> ::value) {
				if (transform)
					return std::optional(std::make_tuple(transform));
			} else if constexpr (std::is_same <T, Geometry> ::value) {
				if (geometry)
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
	std::optional <std::tuple <ComponentRef <T>, ComponentRef <Args>...>> grab() const {
		if constexpr (sizeof...(Args) == 0) {
			if constexpr (std::is_same <T, Transform> ::value) {
				if (transform)
					return std::optional(std::make_tuple(transform));
			} else if constexpr (std::is_same <T, Geometry> ::value) {
				if (geometry)
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
};

// Dependency specializations
template <>
struct dependency_translation <Geometry> {
	static void check(const Inhabitant &i) {
		if (!i.has <Transform> ())
			ulog_warning("add_geometry", "Inhabitant is missing a Transform!\n");
	}
};

// A biome is a collection of geometry and materials
struct Biome {
	std::vector <Inhabitant> inhabitants;
	std::vector <Transform> transforms;
	std::vector <Geometry> geometries;
	// TODO: recycling vector

	// Default is OK
	Biome() = default;

	// No copies
	Biome(const Biome &) = delete;
	Biome &operator=(const Biome &) = delete;

	InhabitantRef new_inhabitant();

	// TODO: should be add_component
	// TODO: make this private...
	template <typename T, typename ... Args>
	requires std::is_constructible_v <T, Args...>
	ComponentRef <T> new_component(const Args & ... args) {
		// TODO: check for dependencies that the component needs
		if constexpr (std::is_same_v <T, Geometry>) {
			uint32_t size = geometries.size();
			geometries.emplace_back(args...);
			return { std::ref(geometries), size };
		} else if constexpr (std::is_same_v <T, Transform>) {
			uint32_t size = transforms.size();
			transforms.emplace_back(args...);
			return { std::ref(transforms), size };
		} else {
			static_assert(false, "Unsupported component type");
		}
	}

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
requires std::is_constructible_v <T, Args...>
void concrete_ref_base <Inhabitant> ::add_component(const Args & ... args)
{
	dependency_translation <T> ::check(source);

	Biome &b = biome;
	if constexpr (std::is_same_v <T, Geometry>) {
		uint32_t size = b.geometries.size();
		b.geometries.emplace_back(args...);
		geometry = { std::ref(b.geometries), size };
	} else if constexpr (std::is_same_v <T, Transform>) {
		uint32_t size = b.transforms.size();
		b.transforms.emplace_back(args...);
		transform = { std::ref(b.transforms), size };
	} else {
		static_assert(false, "Unsupported component type");
	}
}
