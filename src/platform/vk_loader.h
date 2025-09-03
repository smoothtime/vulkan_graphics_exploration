#if !defined(VK_LOADER_H)

#include <unordered_map>
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

uint32
findAttributeIndex(cgltf_primitive &prim, char *name)
{

    return 0;
}

std::vector<MeshAsset>
loadGltfMeshes(VulkanBackend* backend, const char *path, int32 *success)
{
	cgltf_options options = {};
    cgltf_data* data = NULL;
    cgltf_result result = cgltf_parse_file(&options, path, &data);
    *success = (result == cgltf_result_success);
    cgltf_result buffersLoaded = cgltf_load_buffers(&options, data, path);
    *success = (buffersLoaded == cgltf_result_success);
    std::vector<MeshAsset> assets;
    *success = (result == cgltf_result_success);
    if (*success)
    {

        std::vector<uint32> indices;
        std::vector<Vertex> vertices;
        for (uint32 m = 0; m < data->meshes_count; m++)
        {
            MeshAsset newMesh;
            cgltf_mesh  &currentMesh = data->meshes[m];
            newMesh.name = currentMesh.name;
            indices.clear();
            vertices.clear();
            for(uint32 p = 0; p < currentMesh.primitives_count; p++)
            {
                cgltf_primitive *prim = &currentMesh.primitives[p];
                GeoSurface newSurface;
                newSurface.startIndex = (uint32)indices.size();
                newSurface.count = (uint32)prim->indices->count;

                size_t initialVtx = vertices.size();
                {
                    indices.reserve(indices.size() + prim->indices->count);
                    for(uint32 i = 0; i < prim->indices->count; i++)
                    {
                        cgltf_size index = cgltf_accessor_read_index(prim->indices, i);
                        indices.push_back((uint32)(index + initialVtx));
                    }
                }

                cgltf_accessor *positions = nullptr;
                cgltf_accessor *normals = nullptr;
                cgltf_accessor *uvs = nullptr;
                cgltf_accessor *colors = nullptr;

                // TODO(james): do we need to call this cgltf_buffer_view_data
                for (size_t a = 0; a < prim->attributes_count; ++a)
                {
                    cgltf_attribute* attribute = &prim->attributes[a];
                    cgltf_accessor* accessor = attribute->data;
                    cgltf_size accessorCount = accessor->count;
                    if (attribute->type == cgltf_attribute_type_position)
                    {
                        positions = accessor;
                    } else if (attribute->type == cgltf_attribute_type_normal)
                    {
                        normals = accessor;
                    } else if (attribute->type == cgltf_attribute_type_texcoord)
                    {
                        uvs = accessor;
                    } else if (attribute->type == cgltf_attribute_type_color)
                    {
                        colors = accessor;
                    }
                }
                if (positions == nullptr || positions->count == 0)
                {
                    *success = false;
                    continue;
                } else
                {
                    vertices.reserve(vertices.size() + positions->count);
                    for (uint32 v = 0; v < positions->count; v++)
                    {
                        Vertex vert = {};
                        *success &= cgltf_accessor_read_float(positions, v, (real32*)&vert.position, 3);
                        if (!*success)
                        {
                            break;
                        }
                        vertices.push_back(vert);
                    }
                    for (uint32 n = 0; normals !=nullptr && n < normals->count; n++)
                    {
                        Vertex &vert = vertices[initialVtx + n];
                        *success &= cgltf_accessor_read_float(normals, n, (real32*)&vert.normal, 3);
                        if (!*success)
                        {
                            break;
                        }
                    }
                    for (uint32 u = 0; uvs != nullptr && u < uvs->count; u++)
                    {
                        real32 uvxy[2];
                        *success &= cgltf_accessor_read_float(uvs, u, uvxy, 2);
                        if (!*success)
                        {
                            break;
                        }
                        vertices[initialVtx + u].uvX = uvxy[0];
                        vertices[initialVtx + u].uvY = uvxy[1];
                    }
                    for (uint32 c = 0; colors != nullptr && c < colors->count; c++)
                    {
                        Vertex &vert = vertices[initialVtx + c];
                        *success &= cgltf_accessor_read_float(normals, c, (real32*)&vert.color, 4);
                        if (!*success)
                        {
                            break;
                        }
                    }
                    constexpr bool OverrideColors = true;
                    if (OverrideColors) {
                        for (Vertex& vtx : vertices)
                        {
                            vtx.color = glm::vec4(vtx.normal, 1.f);
                        }
                    }
                }
                newMesh.surfaces.push_back(newSurface);
            }
            if (!*success) continue;
            newMesh.meshBuffers = backend->uploadMesh(indices, vertices);
            assets.emplace_back(newMesh);
            // do mesh stuff

        }

        cgltf_free(data);
    }
    return assets;
}


#define VK_LOADER_H
#endif