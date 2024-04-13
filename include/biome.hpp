#pragma once

#include <filesystem>
#include <optional>
#include <variant>

#include <oak/mesh.hpp>
#include <oak/material.hpp>
#include <oak/transform.hpp>

// Type traits
template <typename T>
concept biome_component = std::is_same_v <typename T::component, std::true_type>;

template <typename T>
concept transformable_component = biome_component <T> && std::is_same_v <typename T::transformable, std::true_type>;

// Geometry node
struct Geometry {
	using component = std::true_type;
	using transformable = std::true_type;

	Mesh mesh;
	Material material;
	bool visible;
};

// Blank node
struct Anchor {
	using component = std::true_type;
};

// Components with transforms
using transformable_base = std::variant <Geometry>;

struct Transformable : transformable_base {
	using transformable_base::transformable_base;

	Transform transform;

	// Get the concrete component
	template <transformable_component T>
	std::optional <std::reference_wrapper <T>> grab() {
		if (std::holds_alternative <T> (*this)) {
			auto &v = std::get <T> (*this);
			std::reference_wrapper <T> w = std::ref(v);
			return w;
		}

		return std::nullopt;
	}
};

// Hierarchy nodes
using inhabitant_base = std::variant <Transformable, Anchor>;

struct Inhabitant : inhabitant_base {
	using inhabitant_base::inhabitant_base;

	std::string identifier;
	std::vector <Inhabitant> children;

	// Get general component
	template <biome_component T>
	std::optional <std::reference_wrapper <T>> grab() {
		if constexpr (transformable_component <T>) {
			if (!std::holds_alternative <Transformable> (*this))
				return std::nullopt;

			// Transformable
			Transformable &t = std::get <Transformable> (*this);
			return t.grab <T> ();
		} else {
			if (std::holds_alternative <T> (*this)) {
				return std::get <T> (*this);
			}
		}

		return std::nullopt;
	}

	// Get a list of all components
	template <biome_component T>
	std::vector <std::reference_wrapper <T>> grab_all() {
		std::vector <std::reference_wrapper <T>> all;
		if (auto v = grab <T> ())
			all.push_back(v.value());

		for (Inhabitant &i : children) {
			auto vs = i.grab_all <T> ();
			all.insert(all.end(), vs.begin(), vs.end());
		}

		return all;
	}
};

// A biome is a collection of geometry and materials
struct Biome {
	std::vector <Inhabitant> inhabitants;

	// Get a list of all components
	template <biome_component T>
	std::vector <std::reference_wrapper <T>> grab_all() {
		std::vector <std::reference_wrapper <T>> all;
		for (Inhabitant &i : inhabitants) {
			auto vs = i.grab_all <T> ();
			all.insert(all.end(), vs.begin(), vs.end());
		}

		return all;
	}

	static Biome load(const std::filesystem::path &);
};
