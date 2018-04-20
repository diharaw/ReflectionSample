#include <iostream>

#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>

#include <application.h>
#include <camera.h>
#include <render_device.h>
#include <utility.h>
#include <scene.h>
#include <material.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include <stb_image.h>

#include <vec3.h>
#include <vector>

#include <Macros.h>
#include <stdio.h>

#define CAMERA_SPEED 0.05f
#define CAMERA_SENSITIVITY 0.02f
#define CAMERA_ROLL 0.0

struct TypeDescriptor
{
    const char* m_name;
    size_t m_size;
    
    TypeDescriptor(const char* name, size_t size) : m_name(name), m_size(size) {}
    virtual void gui(void* obj, const char* name) = 0;
};

struct TypeDescriptor_Struct : public TypeDescriptor
{
    struct Member
    {
        const char*     m_name;
        size_t          m_offset;
        TypeDescriptor* m_type;
        
        Member(const char* name, size_t offset, TypeDescriptor* type) : m_name(name), m_offset(offset), m_type(type)
        {
            
        }
    };
    
    int m_num_members;
    Member* m_members;
    
    TypeDescriptor_Struct(const char* name, size_t size, void(*initialize)()) : TypeDescriptor(name, size)
    {
        initialize();
    }
    
    void init(Member* members, int num_members)
    {
        m_members = members;
        m_num_members = num_members;
    }
    
    virtual void gui(void* obj, const char* name) override
    {
        char* char_obj = (char*)obj;
        
        ImGui::Text("%s", name);
        ImGui::Spacing();
        
        for (int i = 0; i < m_num_members; i++)
            m_members[i].m_type->gui(char_obj + m_members[i].m_offset, m_members[i].m_name);
    }
};

struct TypeDescriptor_Enum : public TypeDescriptor
{
    struct Constant
    {
        const char* m_name;
        int         m_value;
        
        Constant(const char* name, int value) : m_name(name), m_value(value)
        {
            
        }
    };
    
    TypeDescriptor_Enum(const char* name, size_t size) : TypeDescriptor(name, size)
    {

    }
    
    int current_value_index(int value)
    {
        for (int i = 0; i < m_num_constants; i++)
        {
            if (m_constants[i].m_value == value)
                return i;
        }
        
        return 0;
    }
    
    virtual void gui(void* obj, const char* name) override
    {
        int& value = *((int*)obj);
        int index = current_value_index(value);
        
        if (ImGui::BeginCombo(name, m_constants[index].m_name))
        {
            for (int i = 0; i < m_num_constants; i++)
            {
                if (ImGui::Selectable(m_constants[i].m_name, m_constants[i].m_value == value))
                    value = m_constants[i].m_value;
            }
            ImGui::EndCombo();
        }
    }
    
    int       m_num_constants;
    Constant* m_constants;
};

template <typename TYPE>
const char* type_name()
{
    //    static_assert(false, "Type not implemented");
    return nullptr;
}

#define DECLARE_PRIMITIVE_TYPENAME(TYPE) template <> \
                                         const char* type_name<TYPE>() { return #TYPE; }

DECLARE_PRIMITIVE_TYPENAME(float)
DECLARE_PRIMITIVE_TYPENAME(char)
DECLARE_PRIMITIVE_TYPENAME(bool)
DECLARE_PRIMITIVE_TYPENAME(double)
DECLARE_PRIMITIVE_TYPENAME(int32_t)
DECLARE_PRIMITIVE_TYPENAME(uint32_t)
DECLARE_PRIMITIVE_TYPENAME(int16_t)
DECLARE_PRIMITIVE_TYPENAME(uint16_t)
DECLARE_PRIMITIVE_TYPENAME(int64_t)
DECLARE_PRIMITIVE_TYPENAME(uint64_t)

template <typename T>
TypeDescriptor* get_primitive_descriptor();

// This is the primary class template for finding all TypeDescriptors:
struct TypeResolver
{
    template <typename T> static char func(decltype(&T::Reflection));
    template <typename T> static int func(...);
    template <typename T>
    struct is_reflected
    {
        enum { value = (sizeof(func<T>(nullptr)) == sizeof(char)) };
    };
    
    // This version is called if T has a static member named "Reflection":
    template <typename T, typename std::enable_if<is_reflected<T>::value, int>::type = 0>
    static TypeDescriptor* get()
    {
        return &T::Reflection;
    }
    
    // This version is called otherwise:
    template <typename T, typename std::enable_if<!is_reflected<T>::value, int>::type = 0>
    static TypeDescriptor* get()
    {
        static auto desc = get_primitive_descriptor<T>();
        return desc;
    }
};

struct TypeDescriptor_Int : TypeDescriptor
{
    TypeDescriptor_Int() : TypeDescriptor{"int32_t", sizeof(int32_t)}
    {
        
    }
    
    virtual void gui(void* obj, const char* name) override
    {
        ImGui::InputInt(name, (int*)obj);
    }
};

template <>
TypeDescriptor* get_primitive_descriptor<int>()
{
    static TypeDescriptor_Int typeDesc;
    return &typeDesc;
}

struct TypeDescriptor_Bool : TypeDescriptor
{
    TypeDescriptor_Bool() : TypeDescriptor{"bool", sizeof(bool)}
    {
        
    }
    
    virtual void gui(void* obj, const char* name) override
    {
        ImGui::Checkbox(name, (bool*)obj);
    }
};

template <>
TypeDescriptor* get_primitive_descriptor<bool>()
{
    static TypeDescriptor_Bool typeDesc;
    return &typeDesc;
}

struct TypeDescriptor_Float : TypeDescriptor
{
    TypeDescriptor_Float() : TypeDescriptor{"float", sizeof(float)}
    {
        
    }
    
    virtual void gui(void* obj, const char* name) override
    {
        ImGui::InputFloat(name, (float*)obj);
    }
};

template <>
TypeDescriptor* get_primitive_descriptor<float>()
{
    static TypeDescriptor_Float typeDesc;
    return &typeDesc;
}

#define REFLECT() static TypeDescriptor_Struct Reflection; \
                  static void init_reflection();

#define BEGIN_DECLARE_REFLECT(TYPE) TypeDescriptor_Struct TYPE::Reflection{ #TYPE, sizeof(TYPE), TYPE::init_reflection }; \
                                    void TYPE::init_reflection()                                                          \
                                    {                                                                                     \
                                        using T = TYPE;                                                                   \
                                        static TypeDescriptor_Struct::Member members[] = {

#define END_DECLARE_REFLECT()   };                                                                               \
                                Reflection.init(members, sizeof(members)/sizeof(TypeDescriptor_Struct::Member)); \
                            }

#define REFLECT_MEMBER(MEMBER) { #MEMBER, offsetof(T, MEMBER), TypeResolver::get<decltype(MEMBER)>() },

#define DECLARE_ENUM_TYPE_DESC(TYPE) struct TypeDescriptor_##TYPE : TypeDescriptor_Enum   \
{                                                    \
TypeDescriptor_##TYPE();                         \
};                                                   \
template <>                                          \
TypeDescriptor* get_primitive_descriptor<TYPE>()     \
{                                                    \
static TypeDescriptor_##TYPE typeDesc;           \
return &typeDesc;                                \
}

#define BEGIN_ENUM_TYPE_DESC(TYPE) TypeDescriptor_##TYPE::TypeDescriptor_##TYPE() : TypeDescriptor_Enum(#TYPE, sizeof(TYPE)) { \
static Constant constants[] = {                                                         \

#define REFLECT_ENUM_CONST(VALUE) { #VALUE, VALUE },
#define END_ENUM_TYPE_DESC()    };                                                    \
m_constants = &constants[0];                          \
m_num_constants = sizeof(constants)/sizeof(Constant); \
}

namespace dd
{
    struct DW_ALIGNED(16) CameraUniforms
    {
        glm::mat4 view_proj;
    };
    
    struct VertexWorld
    {
        glm::vec3 position;
        glm::vec2 uv;
        glm::vec3 color;
    };
    
    struct DrawCommand
    {
        int type;
        int vertices;
    };
    
#define MAX_VERTICES 100000
    
    const glm::vec4 kFrustumCorners[] = {
        glm::vec4(-1.0f, -1.0f, 1.0f, 1.0f),
        glm::vec4(-1.0f, 1.0f, 1.0f, 1.0f),
        glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
        glm::vec4(1.0f, -1.0f, 1.0f, 1.0f),
        glm::vec4(-1.0f, -1.0f, -1.0f, 1.0f),
        glm::vec4(-1.0f, 1.0f, -1.0f, 1.0f),
        glm::vec4(1.0f, 1.0f, -1.0f, 1.0f),
        glm::vec4(1.0f, -1.0f, -1.0f, 1.0f)
    };
    
    class Renderer
    {
    private:
        CameraUniforms m_uniforms;
        VertexArray* m_line_vao;
        VertexBuffer* m_line_vbo;
        InputLayout* m_line_il;
        Shader* m_line_vs;
        Shader* m_line_fs;
        ShaderProgram* m_line_program;
        UniformBuffer* m_ubo;
        std::vector<VertexWorld> m_world_vertices;
        std::vector<DrawCommand> m_draw_commands;
        RenderDevice* m_device;
        RasterizerState* m_rs;
        DepthStencilState* m_ds;
        
    public:
        Renderer()
        {
            m_world_vertices.resize(MAX_VERTICES);
            m_world_vertices.clear();
            
            m_draw_commands.resize(MAX_VERTICES);
            m_draw_commands.clear();
        }
        
        bool init(RenderDevice* _device)
        {
            m_device = _device;
            
            std::string vs_str;
            Utility::ReadText("shader/debug_draw_vs.glsl", vs_str);
            
            std::string fs_str;
            Utility::ReadText("shader/debug_draw_fs.glsl", fs_str);
            
            m_line_vs = m_device->create_shader(vs_str.c_str(), ShaderType::VERTEX);
            m_line_fs = m_device->create_shader(fs_str.c_str(), ShaderType::FRAGMENT);
            
            if (!m_line_vs || !m_line_fs)
            {
                LOG_FATAL("Failed to create Shaders");
                return false;
            }
            
            Shader* shaders[] = { m_line_vs, m_line_fs };
            m_line_program = m_device->create_shader_program(shaders, 2);
            
            BufferCreateDesc bc;
            InputLayoutCreateDesc ilcd;
            VertexArrayCreateDesc vcd;
            
            DW_ZERO_MEMORY(bc);
            bc.data = nullptr;
            bc.data_type = DataType::FLOAT;
            bc.size = sizeof(VertexWorld) * MAX_VERTICES;
            bc.usage_type = BufferUsageType::DYNAMIC;
            
            m_line_vbo = m_device->create_vertex_buffer(bc);
            
            InputElement elements[] =
            {
                { 3, DataType::FLOAT, false, 0, "POSITION" },
                { 2, DataType::FLOAT, false, sizeof(float) * 3, "TEXCOORD" },
                { 3, DataType::FLOAT, false, sizeof(float) * 5, "COLOR" }
            };
            
            DW_ZERO_MEMORY(ilcd);
            ilcd.elements = elements;
            ilcd.num_elements = 3;
            ilcd.vertex_size = sizeof(float) * 8;
            
            m_line_il = m_device->create_input_layout(ilcd);
            
            DW_ZERO_MEMORY(vcd);
            vcd.index_buffer = nullptr;
            vcd.vertex_buffer = m_line_vbo;
            vcd.layout = m_line_il;
            
            m_line_vao = m_device->create_vertex_array(vcd);
            
            if (!m_line_vao || !m_line_vbo)
            {
                LOG_FATAL("Failed to create Vertex Buffers/Arrays");
                return false;
            }
            
            RasterizerStateCreateDesc rs_desc;
            DW_ZERO_MEMORY(rs_desc);
            rs_desc.cull_mode = CullMode::NONE;
            rs_desc.fill_mode = FillMode::SOLID;
            rs_desc.front_winding_ccw = true;
            rs_desc.multisample = true;
            rs_desc.scissor = false;
            
            m_rs = m_device->create_rasterizer_state(rs_desc);
            
            DepthStencilStateCreateDesc ds_desc;
            DW_ZERO_MEMORY(ds_desc);
            ds_desc.depth_mask = true;
            ds_desc.enable_depth_test = true;
            ds_desc.enable_stencil_test = false;
            ds_desc.depth_cmp_func = ComparisonFunction::LESS_EQUAL;
            
            m_ds = m_device->create_depth_stencil_state(ds_desc);
            
            BufferCreateDesc uboDesc;
            DW_ZERO_MEMORY(uboDesc);
            uboDesc.data = nullptr;
            uboDesc.data_type = DataType::FLOAT;
            uboDesc.size = sizeof(CameraUniforms);
            uboDesc.usage_type = BufferUsageType::DYNAMIC;
            
            m_ubo = m_device->create_uniform_buffer(uboDesc);
            
            return true;
        }
        
        void shutdown()
        {
            m_device->destroy(m_ubo);
            m_device->destroy(m_line_program);
            m_device->destroy(m_line_vs);
            m_device->destroy(m_line_fs);
            m_device->destroy(m_line_vbo);
            m_device->destroy(m_line_vao);
            m_device->destroy(m_ds);
            m_device->destroy(m_rs);
        }
        
        void capsule(const float& _height, const float& _radius, const glm::vec3& _pos, const glm::vec3& _c)
        {
            // Draw four lines
            line(glm::vec3(_pos.x, _pos.y + _radius, _pos.z-_radius), glm::vec3(_pos.x, _height - _radius, _pos.z-_radius), _c);
            line(glm::vec3(_pos.x, _pos.y + _radius, _pos.z+_radius), glm::vec3(_pos.x, _height - _radius, _pos.z+_radius), _c);
            line(glm::vec3(_pos.x-_radius, _pos.y + _radius, _pos.z), glm::vec3(_pos.x-_radius, _height - _radius, _pos.z), _c);
            line(glm::vec3(_pos.x+_radius, _pos.y + _radius, _pos.z), glm::vec3(_pos.x+_radius, _height - _radius, _pos.z), _c);
            
            glm::vec3 verts[10];
            
            int idx = 0;
            
            for (int i = 0; i <= 180; i += 20)
            {
                float degInRad = glm::radians((float)i);
                verts[idx++] = glm::vec3(_pos.x + cos(degInRad)*_radius, _height - _radius + sin(degInRad)*_radius, _pos.z);
            }
            
            line_strip(&verts[0], 10, _c);
            
            idx = 0;
            
            for (int i = 0; i <= 180; i += 20)
            {
                float degInRad = glm::radians((float)i);
                verts[idx++] = glm::vec3(_pos.x, _height - _radius + sin(degInRad)*_radius, _pos.z + cos(degInRad)*_radius);
            }
            
            line_strip(&verts[0], 10, _c);
            
            idx = 0;
            
            for (int i = 180; i <= 360; i += 20)
            {
                float degInRad = glm::radians((float)i);
                verts[idx++] = glm::vec3(_pos.x + cos(degInRad)*_radius, _radius + sin(degInRad)*_radius, _pos.z);
            }
            
            line_strip(&verts[0], 10, _c);
            
            idx = 0;
            
            for (int i = 180; i <= 360; i += 20)
            {
                float degInRad = glm::radians((float)i);
                verts[idx++] = glm::vec3(_pos.x, _radius + sin(degInRad)*_radius, _pos.z + cos(degInRad)*_radius);
            }
            
            line_strip(&verts[0], 10, _c);
            
            circle_xz(_radius, glm::vec3(_pos.x, _height - _radius, _pos.z), _c);
            circle_xz(_radius, glm::vec3(_pos.x, _radius, _pos.z), _c);
        }
        
        void aabb(const glm::vec3& _min, const glm::vec3& _max, const glm::vec3& _pos, const glm::vec3& _c)
        {
            glm::vec3 min = _pos + _min;
            glm::vec3 max = _pos + _max;
            
            line(min,                            glm::vec3(max.x, min.y, min.z), _c);
            line(glm::vec3(max.x, min.y, min.z), glm::vec3(max.x, min.y, max.z), _c);
            line(glm::vec3(max.x, min.y, max.z), glm::vec3(min.x, min.y, max.z), _c);
            line(glm::vec3(min.x, min.y, max.z), min, _c);
            
            line(glm::vec3(min.x, max.y, min.z), glm::vec3(max.x, max.y, min.z), _c);
            line(glm::vec3(max.x, max.y, min.z), max, _c);
            line(max,                            glm::vec3(min.x, max.y, max.z), _c);
            line(glm::vec3(min.x, max.y, max.z), glm::vec3(min.x, max.y, min.z), _c);
            
            line(min,                            glm::vec3(min.x, max.y, min.z), _c);
            line(glm::vec3(max.x, min.y, min.z), glm::vec3(max.x, max.y, min.z), _c);
            line(glm::vec3(max.x, min.y, max.z), max, _c);
            line(glm::vec3(min.x, min.y, max.z), glm::vec3(min.x, max.y, max.z), _c);
        }
        
        void obb(const glm::vec3& _min, const glm::vec3& _max, const glm::mat4& _model, const glm::vec3& _c)
        {
            glm::vec3 verts[8];
            glm::vec3 size = _max - _min;
            int idx = 0;
            
            for (float x = _min.x; x <= _max.x; x += size.x)
            {
                for (float y = _min.y; y <= _max.y; y += size.y)
                {
                    for (float z = _min.z; z <= _max.z; z += size.z)
                    {
                        glm::vec4 v = _model * glm::vec4(x, y, z, 1.0f);
                        verts[idx++] = glm::vec3(v.x, v.y, v.z);
                    }
                }
            }
            
            line(verts[0], verts[1], _c);
            line(verts[1], verts[5], _c);
            line(verts[5], verts[4], _c);
            line(verts[4], verts[0], _c);
            
            line(verts[2], verts[3], _c);
            line(verts[3], verts[7], _c);
            line(verts[7], verts[6], _c);
            line(verts[6], verts[2], _c);
            
            line(verts[2], verts[0], _c);
            line(verts[6], verts[4], _c);
            line(verts[3], verts[1], _c);
            line(verts[7], verts[5], _c);
        }
        
        void grid(const float& _x, const float& _z, const float& _y_level, const float& spacing, const glm::vec3& _c)
        {
            int offset_x = floor((_x * spacing)/2.0f);
            int offset_z = floor((_z * spacing )/2.0f);
            
            for (int x = -offset_x; x <= offset_x; x += spacing)
            {
                line(glm::vec3(x, _y_level, -offset_z), glm::vec3(x, _y_level, offset_z), _c);
            }
            
            for (int z = -offset_z; z <= offset_z; z += spacing)
            {
                line(glm::vec3(-offset_x, _y_level, z), glm::vec3(offset_x, _y_level, z), _c);
            }
        }
        
        void line(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& c)
        {
            VertexWorld vw0, vw1;
            vw0.position = v0;
            vw0.color = c;
            
            vw1.position = v1;
            vw1.color = c;
            
            m_world_vertices.push_back(vw0);
            m_world_vertices.push_back(vw1);
            
            DrawCommand cmd;
            cmd.type = PrimitiveType::LINES;
            cmd.vertices = 2;
            
            m_draw_commands.push_back(cmd);
        }
        
        void line_strip(glm::vec3* v, const int& count, const glm::vec3& c)
        {
            for (int i = 0; i < count; i++)
            {
                VertexWorld vert;
                vert.position = v[i];
                vert.color = c;
                
                m_world_vertices.push_back(vert);
            }
            
            DrawCommand cmd;
            cmd.type = PrimitiveType::LINE_STRIP;
            cmd.vertices = count;
            
            m_draw_commands.push_back(cmd);
        }
        
        void circle_xy(float radius, const glm::vec3& pos, const glm::vec3& c)
        {
            glm::vec3 verts[19];
            
            int idx = 0;
            
            for (int i=0; i <= 360; i+= 20)
            {
                float degInRad = glm::radians((float)i);
                verts[idx++] = pos + glm::vec3(cos(degInRad)*radius,sin(degInRad)*radius, 0.0f);
            }
            
            line_strip(&verts[0], 19, c);
        }
        
        void circle_xz(float radius, const glm::vec3& pos, const glm::vec3& c)
        {
            glm::vec3 verts[19];
            
            int idx = 0;
            
            for (int i=0; i <= 360; i+= 20)
            {
                float degInRad = glm::radians((float)i);
                verts[idx++] = pos + glm::vec3(cos(degInRad)*radius, 0.0f, sin(degInRad)*radius);
            }
            
            line_strip(&verts[0], 19, c);
        }
        
        void circle_yz(float radius, const glm::vec3& pos, const glm::vec3& c)
        {
            glm::vec3 verts[19];
            
            int idx = 0;
            
            for (int i=0; i <= 360; i+= 20)
            {
                float degInRad = glm::radians((float)i);
                verts[idx++] = pos + glm::vec3(0.0f, cos(degInRad)*radius, sin(degInRad)*radius);
            }
            
            line_strip(&verts[0], 19, c);
        }
        
        void sphere(const float& radius, const glm::vec3& pos, const glm::vec3& c)
        {
            circle_xy(radius, pos, c);
            circle_xz(radius, pos, c);
            circle_yz(radius, pos, c);
        }
        
        void frustum(const glm::mat4& proj, const glm::mat4& view, const glm::vec3& c)
        {
            glm::mat4 inverse = glm::inverse(proj * view);
            glm::vec3 corners[8];
            
            for (int i = 0; i < 8; i++)
            {
                glm::vec4 v = inverse * kFrustumCorners[i];
                v = v/v.w;
                corners[i] = glm::vec3(v.x, v.y, v.z);
            }
            
            glm::vec3 far[5] = {
                corners[0],
                corners[1],
                corners[2],
                corners[3],
                corners[0]
            };
            
            line_strip(&far[0], 5, c);
            
            glm::vec3 near[5] = {
                corners[4],
                corners[5],
                corners[6],
                corners[7],
                corners[4]
            };
            
            line_strip(&near[0], 5, c);
            
            line(corners[0], corners[4], c);
            line(corners[1], corners[5], c);
            line(corners[2], corners[6], c);
            line(corners[3], corners[7], c);
        }
        
        void render(Framebuffer* fbo, int width, int height, const glm::mat4& view_proj)
        {
            m_uniforms.view_proj = view_proj;
            
            void* ptr = m_device->map_buffer(m_line_vbo, BufferMapType::WRITE);
            
            if (m_world_vertices.size() > MAX_VERTICES)
                std::cout << "Vertices are above limit" << std::endl;
            else
                memcpy(ptr, &m_world_vertices[0], sizeof(VertexWorld) * m_world_vertices.size());
            
            m_device->unmap_buffer(m_line_vbo);
            
            ptr = m_device->map_buffer(m_ubo, BufferMapType::WRITE);
            memcpy(ptr, &m_uniforms, sizeof(CameraUniforms));
            m_device->unmap_buffer(m_ubo);
            
            m_device->bind_rasterizer_state(m_rs);
            m_device->bind_depth_stencil_state(m_ds);
            m_device->bind_framebuffer(fbo);
            m_device->set_viewport(width, height, 0, 0);
            m_device->bind_shader_program(m_line_program);
            m_device->bind_uniform_buffer(m_ubo, ShaderType::VERTEX, 0);
            m_device->bind_vertex_array(m_line_vao);
            
            int v = 0;
            
            for (int i = 0; i < m_draw_commands.size(); i++)
            {
                DrawCommand& cmd = m_draw_commands[i];
                m_device->set_primitive_type(cmd.type);
                m_device->draw(v, cmd.vertices);
                v += cmd.vertices;
            }
            
            m_draw_commands.clear();
            m_world_vertices.clear();
        }
    };
}

struct TerrainVertex
{
    glm::vec2 pos;
};

struct DW_ALIGNED(16) TerrainUniforms
{
    glm::mat4 view_proj;
    glm::vec4 rect;
    glm::vec4 scale;
};

struct Terrain
{
    VertexArray* m_vao;
    IndexBuffer* m_ibo;
    VertexBuffer* m_vbo;
    InputLayout* m_il;
    Shader* m_vs;
    Shader* m_fs;
    ShaderProgram* m_program;
    UniformBuffer* m_ubo;
    RenderDevice* m_device;
    RasterizerState* m_rs;
    DepthStencilState* m_ds;
    TerrainVertex* m_vertices;
    TerrainUniforms m_uniforms;
    std::vector<uint32_t> m_indices;
    Texture2D* m_height_map;
	SamplerState* m_sampler;

    Terrain(float x, float z, float distance, RenderDevice* device)
    {
        m_device = device;
        
        float x_half = x/2.0f * distance;
        float z_half = z/2.0f * distance;
        
        int idx = 0;
        m_vertices = new TerrainVertex[(x + 1) * (z + 1)];
        
        for (int z = -z_half; z <= z_half; z += distance)
        {
            for (int x = -x_half; x <= x_half; x += distance)
            {
                m_vertices[idx].pos = glm::vec2(x, z);
                idx++;
            }
        }
        
        // 0 1 2
        // 3 4 5
        // 6 7 8
        
        for (int i = 0; i < z; i++)
        {
            for (int j = 0; j < x; j++)
            {
                m_indices.push_back((x + 1) * i + j);
                m_indices.push_back((x + 1) * (i + 1) + j);
                m_indices.push_back((x + 1) * i + (j + 1));
                
                m_indices.push_back((x + 1) * i + j + 1);
                m_indices.push_back((x + 1) * (i + 1) + j);
                m_indices.push_back((x + 1) * (i + 1) + (j + 1));
            }
        }
        
        std::string vs_str;
        Utility::ReadText("shader/terrain_vs.glsl", vs_str);
        
        std::string fs_str;
        Utility::ReadText("shader/terrain_fs.glsl", fs_str);
        
        m_vs = m_device->create_shader(vs_str.c_str(), ShaderType::VERTEX);
        m_fs = m_device->create_shader(fs_str.c_str(), ShaderType::FRAGMENT);
        
        if (!m_vs || !m_fs)
        {
            LOG_FATAL("Failed to create Shaders");
            return;
        }
        
        Shader* shaders[] = { m_vs, m_fs };
        m_program = m_device->create_shader_program(shaders, 2);
        
        BufferCreateDesc bc;
        InputLayoutCreateDesc ilcd;
        VertexArrayCreateDesc vcd;
        
        DW_ZERO_MEMORY(bc);
        bc.data = &m_vertices[0];
        bc.data_type = DataType::FLOAT;
        bc.size = sizeof(TerrainVertex) * (x + 1) * (z + 1);
        bc.usage_type = BufferUsageType::STATIC;
        
        m_vbo = m_device->create_vertex_buffer(bc);
        
        DW_ZERO_MEMORY(bc);
        bc.data = &m_indices[0];
        bc.data_type = DataType::UINT32;
        bc.size = sizeof(uint32_t) * m_indices.size();
        bc.usage_type = BufferUsageType::STATIC;
        
        m_ibo = m_device->create_index_buffer(bc);
        
        InputElement elements[] =
        {
            { 2, DataType::FLOAT, false, 0, "POSITION" }
        };
        
        DW_ZERO_MEMORY(ilcd);
        ilcd.elements = elements;
        ilcd.num_elements = 1;
        ilcd.vertex_size = sizeof(float) * 2;
        
        m_il = m_device->create_input_layout(ilcd);
        
        DW_ZERO_MEMORY(vcd);
        vcd.index_buffer = m_ibo;
        vcd.vertex_buffer = m_vbo;
        vcd.layout = m_il;
        
        m_vao = m_device->create_vertex_array(vcd);
        
        if (!m_vao || !m_ibo || !m_vbo)
        {
            LOG_FATAL("Failed to create Vertex Buffers/Arrays");
            return;
        }

		SamplerStateCreateDesc ssDesc;
		DW_ZERO_MEMORY(ssDesc);

		ssDesc.max_anisotropy = 0;
		ssDesc.min_filter = TextureFilteringMode::LINEAR;
		ssDesc.mag_filter = TextureFilteringMode::LINEAR;
		ssDesc.wrap_mode_u = TextureWrapMode::CLAMP_TO_EDGE;
		ssDesc.wrap_mode_v = TextureWrapMode::CLAMP_TO_EDGE;
		ssDesc.wrap_mode_w = TextureWrapMode::CLAMP_TO_EDGE;

		m_sampler = m_device->create_sampler_state(ssDesc);
        
        RasterizerStateCreateDesc rs_desc;
        DW_ZERO_MEMORY(rs_desc);
        rs_desc.cull_mode = CullMode::NONE;
        rs_desc.fill_mode = FillMode::WIREFRAME;
        rs_desc.front_winding_ccw = true;
        rs_desc.multisample = true;
        rs_desc.scissor = false;
        
        m_rs = m_device->create_rasterizer_state(rs_desc);
        
        DepthStencilStateCreateDesc ds_desc;
        DW_ZERO_MEMORY(ds_desc);
        ds_desc.depth_mask = true;
        ds_desc.enable_depth_test = true;
        ds_desc.enable_stencil_test = false;
        ds_desc.depth_cmp_func = ComparisonFunction::LESS_EQUAL;
        
        m_ds = m_device->create_depth_stencil_state(ds_desc);
        
        BufferCreateDesc uboDesc;
        DW_ZERO_MEMORY(uboDesc);
        uboDesc.data = nullptr;
        uboDesc.data_type = DataType::FLOAT;
        uboDesc.size = sizeof(TerrainUniforms);
        uboDesc.usage_type = BufferUsageType::DYNAMIC;
        
        m_ubo = m_device->create_uniform_buffer(uboDesc);
        
        m_uniforms.rect.x = x + 1;
        m_uniforms.rect.y = z + 1;
        m_uniforms.rect.z = x_half;
        m_uniforms.rect.w = z_half;
        m_uniforms.scale.x = 1.0f;
        
        load(1024, 1024);
    }
    
    void render(glm::mat4 view_proj, uint32_t width, uint32_t height)
    {
        ImGui::SliderFloat("Terrain Scale", &m_uniforms.scale.x, 1.0f, 300.0f);
		ImGui::Image((ImTextureID)m_height_map->id, ImVec2(1025, 1025));
        
        m_uniforms.view_proj = view_proj;
        
        void* ptr = m_device->map_buffer(m_ubo, BufferMapType::WRITE);
        memcpy(ptr, &m_uniforms, sizeof(TerrainUniforms));
        m_device->unmap_buffer(m_ubo);
        
        m_device->bind_rasterizer_state(m_rs);
        m_device->bind_depth_stencil_state(m_ds);
        m_device->bind_framebuffer(nullptr);
        m_device->set_viewport(width, height, 0, 0);
        m_device->bind_shader_program(m_program);
		m_device->bind_sampler_state(m_sampler, ShaderType::VERTEX, 0);
        m_device->bind_uniform_buffer(m_ubo, ShaderType::VERTEX, 0);
        m_device->bind_texture(m_height_map, ShaderType::VERTEX, 0);
        m_device->bind_vertex_array(m_vao);
        m_device->set_primitive_type(PrimitiveType::TRIANGLES);
        m_device->draw_indexed(m_indices.size());
    }
    
    void shutdown()
    {
		m_device->destroy(m_sampler);
        m_device->destroy(m_ubo);
        m_device->destroy(m_program);
        m_device->destroy(m_vs);
        m_device->destroy(m_fs);
        m_device->destroy(m_ibo);
        m_device->destroy(m_vbo);
        m_device->destroy(m_vao);
        m_device->destroy(m_ds);
        m_device->destroy(m_rs);
    }
    
    bool load(int width, int height)
    {
        int error;
        FILE* filePtr;
        unsigned long long imageSize, count;
        unsigned short* rawImage;
        
        // Open the 16 bit raw height map file for reading in binary.
        filePtr = fopen("heightmap.r16", "rb");
        
        // Calculate the size of the raw image data.
        imageSize = width * height;
        
        // Allocate memory for the raw image data.
        rawImage = new unsigned short[imageSize];
        if(!rawImage)
        {
            return false;
        }
        
        // Read in the raw image data.
        count = fread(rawImage, sizeof(unsigned short), imageSize, filePtr);
        if(count != imageSize)
        {
            return false;
        }

		//for (int i = 0; i < 100; i++)
		//{
		//	uint16_t data = rawImage[i];
		//	float fdata = (float)data;
		//	std::cout << data << std::endl;
		//	std::cout << fdata << std::endl;
		//}
        
        // Close the file.
        error = fclose(filePtr);
        if(error != 0)
        {
            return false;
        }
        
        Texture2DCreateDesc desc;
        
        desc.data = rawImage;
        desc.format = TextureFormat::R16_FLOAT;
        desc.height = height;
        desc.width = width;
        desc.mipmap_levels = 1;
        
        m_height_map = m_device->create_texture_2d(desc);

        // Release the bitmap image data.
        delete [] rawImage;
        rawImage = 0;
        
        return true;
    }
};

struct TypeCounter
{
    static int counter;
    
    template<typename T>
    static int get()
    {
        static int id = counter++;
        return id;
    }
};

int TypeCounter::counter = 0;

enum SomeEnum
{
    VAL_1,
    VAL_2
};

enum SomeOtherEnum
{
    TEST_1,
    TEST_2
};

DECLARE_ENUM_TYPE_DESC(SomeEnum)

BEGIN_ENUM_TYPE_DESC(SomeEnum)
    REFLECT_ENUM_CONST(VAL_1)
    REFLECT_ENUM_CONST(VAL_2)
END_ENUM_TYPE_DESC()

//template <>
//TypeDescriptor* get_primitive_descriptor<SomeEnum>()
//{
//    static TypeDescriptor_SomeEnum typeDesc;
//    return &typeDesc;
//}

struct Test
{
    int a;
    float b;
    bool vsync;
    SomeEnum test_enum;
    
    REFLECT()
};

BEGIN_DECLARE_REFLECT(Test)
    REFLECT_MEMBER(a)
    REFLECT_MEMBER(b)
    REFLECT_MEMBER(vsync)
    REFLECT_MEMBER(test_enum)
END_DECLARE_REFLECT()

struct TransformComponent
{
    struct AoS
    {
        glm::vec3& pos;
        glm::vec3& rot;
        glm::vec3& scale;
        
        AoS(glm::vec3& _pos, glm::vec3& _rot, glm::vec3& _scale) : pos(_pos), rot(_rot), scale(_scale)
        {
            
        }
    };
    
    struct SoA
    {
        int count;
        glm::vec3* positions;
        glm::vec3* rotations;
        glm::vec3* scales;
        
        AoS get_soa(int idx)
        {
            return { positions[idx], rotations[idx], scales[idx] };
        }
    };
};

class DebugDrawDemo : public dw::Application
{
private:
    Camera* m_camera;
    Camera* m_debug_camera;
    float m_heading_speed = 0.0f;
    float m_sideways_speed = 0.0f;
    bool m_mouse_look = false;
    dd::Renderer m_debug_renderer;
    glm::vec3 m_min_extents;
    glm::vec3 m_max_extents;
    glm::vec3 m_pos;
    glm::vec3 m_color;
    float m_rotation;
    float m_grid_spacing;
    float m_grid_y;
    bool m_debug_mode = false;
    glm::mat4 m_model;
    Test test_struct;
    
public:
    bool init() override
    {
        m_camera = new Camera(45.0f,
                              0.1f,
                              10000.0f,
                              ((float)m_width)/((float)m_height),
                              glm::vec3(0.0f, 0.0f, 10.0f),
                              glm::vec3(0.0f, 0.0f, -1.0f));
        
        m_debug_camera = new Camera(45.0f,
                              0.1f,
                              10000.0f,
                              ((float)m_width)/((float)m_height),
                              glm::vec3(0.0f, 0.0f, 10.0f),
                              glm::vec3(0.0f, 0.0f, -1.0f));
        
        m_min_extents = glm::vec3(-10.0f);
        m_max_extents = glm::vec3(10.0f);
        m_pos = glm::vec3(40.0f);
        m_color = glm::vec3(1.0f, 0.0f, 0.0f);
        m_rotation = 60.0f;
        m_grid_spacing = 1.0f;
        m_grid_y = 0.0f;
        
        test_struct.a = 32;
        test_struct.b = 435.5f;
        test_struct.vsync = false;
        test_struct.test_enum = VAL_2;
        
        SomeEnum a;
        SomeOtherEnum b;
        std::cout << TypeCounter::get<decltype(a)>() << std::endl;
        std::cout << TypeCounter::get<decltype(a)>() << std::endl;
        std::cout << TypeCounter::get<decltype(a)>() << std::endl;
        std::cout << TypeCounter::get<decltype(b)>() << std::endl;

        return m_debug_renderer.init(&m_device);
    }
    
    template <typename T>
    void render_properties(T& obj)
    {
        TypeDescriptor* desc = TypeResolver::get<T>();
        ImGui::Begin("Properties");
        desc->gui(&obj, "Test Struct");
        ImGui::End();
    }
    
    void update(double delta) override
    {
        updateCamera();
        
        m_device.bind_framebuffer(nullptr);
        m_device.set_viewport(m_width, m_height, 0, 0);
        
        float clear[] = { 0.3f, 0.3f, 0.3f, 1.0f };
        m_device.clear_framebuffer(ClearTarget::ALL, clear);
        
        ImGui::Begin("Debug Draw");
        
        ImGui::InputFloat3("Min Extents", &m_min_extents[0]);
        ImGui::InputFloat3("Max Extents", &m_max_extents[0]);
        ImGui::InputFloat3("Position", &m_pos[0]);
        ImGui::ColorEdit3("Color", &m_color[0]);
        ImGui::InputFloat("Rotation", &m_rotation);
        ImGui::InputFloat("Grid Spacing", &m_grid_spacing);
        ImGui::InputFloat("Grid Y-Level", &m_grid_y);
        
        if (ImGui::Button("Toggle Debug Camera"))
        {
            m_debug_mode = !m_debug_mode;
        }
        
        ImGui::End();
        ImGui::ShowDemoWindow();
        render_properties(test_struct);
        
        m_debug_renderer.capsule(20.0f, 5.0f, glm::vec3(-20.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0));
//        m_debug_renderer.grid(101.0f, 101.0f, m_grid_y, m_grid_spacing, glm::vec3(1.0f));
        //m_debug_renderer.aabb(m_min_extents, m_max_extents, m_pos, m_color);
        m_debug_renderer.sphere(5.0f, glm::vec3(0.0f, 0.0f, 20.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        m_model = glm::rotate(glm::mat4(1.0f), glm::radians(m_rotation), glm::vec3(0.0f, 1.0f, 0.0f));
        m_debug_renderer.obb(m_min_extents, m_max_extents, m_model, m_color);
        
        if (m_debug_mode)
            m_debug_renderer.frustum(m_camera->m_projection, m_camera->m_view, glm::vec3(0.0f, 1.0f, 0.0f));
        
        m_debug_renderer.render(nullptr, m_width, m_height, m_debug_mode ? m_debug_camera->m_view_projection : m_camera->m_view_projection);
        
        //m_terrain->render(m_debug_mode ? m_debug_camera->m_view_projection : m_camera->m_view_projection, m_width, m_height);
    }
    
    void shutdown() override
    {
       // m_terrain->shutdown();
       // delete m_terrain;
        m_debug_renderer.shutdown();
        delete m_debug_camera;
        delete m_camera;
    }
    
    void key_pressed(int code) override
    {
        if(code == GLFW_KEY_W)
            m_heading_speed = CAMERA_SPEED;
        else if(code == GLFW_KEY_S)
            m_heading_speed = -CAMERA_SPEED;
        
        if(code == GLFW_KEY_A)
            m_sideways_speed = -CAMERA_SPEED;
        else if(code == GLFW_KEY_D)
            m_sideways_speed = CAMERA_SPEED;
        
        if(code == GLFW_KEY_E)
            m_mouse_look = true;
    }
    
    void key_released(int code) override
    {
        if(code == GLFW_KEY_W || code == GLFW_KEY_S)
            m_heading_speed = 0.0f;
        
        if(code == GLFW_KEY_A || code == GLFW_KEY_D)
            m_sideways_speed = 0.0f;
        
        if(code == GLFW_KEY_E)
            m_mouse_look = false;
    }
    
    void updateCamera()
    {
        Camera* current = m_camera;
        
        if (m_debug_mode)
            current = m_debug_camera;
        
        float forward_delta = m_heading_speed * m_delta;
        float right_delta = m_sideways_speed * m_delta;
        
        current->set_translation_delta(current->m_forward, forward_delta);
        current->set_translation_delta(current->m_right, right_delta);
        
        if(m_mouse_look)
        {
            // Activate Mouse Look
            current->set_rotatation_delta(glm::vec3((float)(m_mouse_delta_y * CAMERA_SENSITIVITY * m_delta),
                                                     (float)(m_mouse_delta_x * CAMERA_SENSITIVITY * m_delta),
                                                     (float)(CAMERA_ROLL * CAMERA_SENSITIVITY * m_delta)));
        }
        else
        {
            current->set_rotatation_delta(glm::vec3((float)(0),
                                                     (float)(0),
                                                     (float)(0)));
        }
        
        current->update();
    }
};

DW_DECLARE_MAIN(DebugDrawDemo)
