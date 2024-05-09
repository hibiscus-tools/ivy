#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

#include <microlog/microlog.h>

#include "biome.hpp"

namespace ivy {

// Translating paths
static std::string translate_path(const std::filesystem::path &path, const std::string &directory)
{
	std::string normal = std::filesystem::weakly_canonical(path).string();
	if (std::filesystem::exists(normal))
		return normal;

	// Replace \ with /
	std::string unixed = path.string();
	std::replace(unixed.begin(), unixed.end(), '\\', '/');

	unixed = std::filesystem::weakly_canonical(unixed).string();
	if (std::filesystem::exists(unixed))
		return unixed;

	// Redo the checks with the directory
	std::filesystem::path rpath = directory / path;

	normal = std::filesystem::weakly_canonical(path).string();
	if (std::filesystem::exists(normal))
		return normal;

	unixed = rpath.string();
	std::replace(unixed.begin(), unixed.end(), '\\', '/');

	unixed = std::filesystem::weakly_canonical(unixed).string();
	if (std::filesystem::exists(unixed))
		return unixed;

	ulog_error("translate_path", "failed to translated path %s\n", path.string().c_str());
	return "<?>";
}

// Biome loading
struct mesh_result {
	std::string name;
	Mesh mesh;
	Material material;
};

static mesh_result assimp_process_mesh(aiMesh *m, const aiScene *scene, const std::string &dir)
{
	std::vector <glm::vec3> vertices;
	std::vector <glm::vec3> normals;
	std::vector <glm::vec2> uvs;

        std::vector <glm::uvec3> triangles;

	// Process all the mesh's vertices
	for (uint32_t i = 0; i < m->mNumVertices; i++) {
		vertices.emplace_back(m->mVertices[i].x, m->mVertices[i].y, m->mVertices[i].z);

		if (m->HasNormals())
			normals.emplace_back(m->mNormals[i].x, m->mNormals[i].y, m->mNormals[i].z);
		else
			normals.emplace_back(0.0f, 0.0f, 0.0f);

		if (m->HasTextureCoords(0))
			uvs.emplace_back(m->mTextureCoords[0][i].x, m->mTextureCoords[0][i].y);
		else
			uvs.emplace_back(0.0f, 0.0f);
	}

	// Process all the mesh's triangles
	for (uint32_t i = 0; i < m->mNumFaces; i++) {
		aiFace face = m->mFaces[i];
                ulog_assert(face.mNumIndices == 3, "process_mesh",
                            "Only triangles are supported, got %d-sided "
                            "polygon instead\n",
                            face.mNumIndices);

                triangles.emplace_back(face.mIndices[0], face.mIndices[1], face.mIndices[2]);
	}

	// Process the material
	Material material;

	aiMaterial *ai_material = scene->mMaterials[m->mMaterialIndex];

        material.identifier = ai_material->GetName().C_Str();

	// Get diffuse
	aiString path;
	if (ai_material->GetTexture(aiTextureType_DIFFUSE, 0, &path) == AI_SUCCESS) {
		material.textures.diffuse = translate_path(path.C_Str(), dir);
	} else {
		aiColor3D diffuse;
		ai_material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);
		material.diffuse = { diffuse.r, diffuse.g, diffuse.b };
	}

	// Get normal texture
	if (ai_material->GetTexture(aiTextureType_NORMALS, 0, &path) == AI_SUCCESS)
		material.textures.normal = translate_path(path.C_Str(), dir);

	// Get specular
	aiColor3D specular;
	ai_material->Get(AI_MATKEY_COLOR_SPECULAR, specular);
	material.specular = { specular.r, specular.g, specular.b };

	// Get shininess
	float shininess;
	ai_material->Get(AI_MATKEY_SHININESS, shininess);
	material.roughness = 1 - shininess/1000.0f;

	// Finally
	Mesh mesh { vertices, normals, uvs, triangles };
	return { m->mName.C_Str(), mesh, material };
}

static std::vector <mesh_result> assimp_process_node(aiNode *node, const aiScene *scene, const std::string &directory)
{
	std::vector <mesh_result> results;

	// Process all the node's meshes (if any)
	for (uint32_t i = 0; i < node->mNumMeshes; i++) {
		aiMesh *m = scene->mMeshes[node->mMeshes[i]];

		auto mr = assimp_process_mesh(m, scene, directory);
		results.push_back(mr);
	}

	// Recusively process all the node's children
	for (uint32_t i = 0; i < node->mNumChildren; i++) {
		auto ibs = assimp_process_node(node->mChildren[i], scene, directory);

		results.insert(results.end(), ibs.begin(), ibs.end());
	}

	return results;
}

Inhabitant::Inhabitant(Biome &biome_, const std::string &identifier_)
		: biome(biome_),
		identifier(identifier_),
		parent(std::ref(biome_.inhabitants)),
		transform(std::ref(biome_.transforms)),
		geometry(std::ref(biome_.geometries)),
		collider(std::ref(biome_.colliders)) {}

// TODO: move
void link(ComponentRef <Inhabitant> &parent, ComponentRef <Inhabitant> &child)
{
	parent->children.push_back(child);
	child->parent = parent;
}

ComponentRef <Inhabitant> Biome::new_inhabitant()
{
	uint32_t size = inhabitants.size();
	Inhabitant inh(*this, "inbitant" + std::to_string(size));
	inhabitants.push_back(inh);
	return { inhabitants, size };
}

Biome &Biome::blank()
{
	Biome::active.emplace_back();
	return Biome::active.back();
}

Biome &Biome::load(const std::filesystem::path &path)
{
	Assimp::Importer importer;
        ulog_assert(std::filesystem::exists(path),
		__FUNCTION__ , "file \"%s\" does not exist\n", path.c_str());

        // Read scene
	const aiScene *scene;
	scene = importer.ReadFile(path, aiProcess_Triangulate);

	// Check if the scene was loaded
	if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode) {
		ulog_error(__FUNCTION__ , "ASSIMP error: \"%s\"\n", importer.GetErrorString());
		throw "error";
	}

	auto results = assimp_process_node(scene->mRootNode, scene, path.parent_path());

	// Construct the biome from the results, with a top level node
	Biome::active.emplace_back();
	Biome &b = Biome::active.back();

	ComponentRef <Inhabitant> root = b.new_inhabitant();
	if (scene->mName.length)
		root->identifier = scene->mName.C_Str();

	for (const auto &[name, mesh, material] : results) {
		ComponentRef <Inhabitant> added = b.new_inhabitant();
		added->add_component <Transform> ();
		added->add_component <Geometry> (mesh, material, true);
		added->identifier = name;
		link(root, added);
	}

	return b;
}

std::list <Biome> Biome::active;

}
