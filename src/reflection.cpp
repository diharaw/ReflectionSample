
struct TypeDescriptor
{
    const char* m_name;
    size_t m_size;

    TypeDescriptor(const char* name, size_t size) : m_name(name), m_size(size) {}
};

template <typename T>
struct TypeDescriptor_Primitive : public TypeDescriptor
{
    TypeDescriptor_Primitive(const char* name, size_t size) : TypeDescriptor(name, size) {}
};

struct TypeDescriptor_Struct : public TypeDescriptor
{
    struct Member
    {
        const char*     name;
        size_t          offset;
        TypeDescriptor* type;
    };

    int m_num_members;
    Member* m_members;

    TypeDescriptor_Struct(const char* name, size_t size, void(*initialize)()) : TypeDescriptor(name, size) 
    {
        *initialize();
    }

    void init(Member* members, int num_members)
    {
        m_members = members;
        m_num_members = num_members;
    }
};

#define REFLECT() static TypeDescriptor_Struct Reflection; \   
                  static init_reflection()                  

#define BEGIN_DECLARE_REFLECT(TYPE) void TYPE::init_reflection()     \
                                   {                                \
                                       using T = TYPE;          \
                                       static Member members[] = {  

#define END_DECLARE_REFLECT()   };                                                                               \                 
                                Reflection.init(members, sizeof(members)/sizeof(TypeDescriptor_Struct::Member)); \
                            }

#define REFLECT_MEMBER(MEMBER) { #MEMBER, offsetof(T, MEMBER), TypeResolver<decltype(MEMBER)>::get() },

struct Test
{
    int a;
    float b;

    REFLECT()
};

void Test::init_reflection()
{
    static Member members[] = {
        { "a", offsetof(Test, a), TypeResolver<decltype(a)>::get() },
        { "b", offsetof(Test, b), TypeResolver<decltype(b)>::get() }
    };

    Reflection.init(&members[0], sizeof(members)/sizeof(TypeDescriptor_Struct::Member));
}

BEGIN_DECLARE_REFLEC(Test)
    REFLECT_MEMBER(a)
    REFLECT_MEMBER(b)
END_DECLARE_REFLECT()

template <typename T>
struct TypeResolver
{
    template <typename T> static char func(decltype(&T::Reflection));
    template <typename T> static int func(...);
    template <typename T>
    struct IsReflected {
        enum { value = (sizeof(func<T>(nullptr)) == sizeof(char)) };
    };

    template <typename T, typename std::enable_if<IsReflected<T>::value, int>::type = 0>
    static TypeDescriptor* get() 
    {
        return &T::Reflection;
    }

    template <typename std::enable_if<!IsReflected<T>::value, int>::type = 0>
    static TypeDescriptor* get()
    {
        static auto desc = TypeDescriptor_Primitive<T>();
        return &desc;
    }
};

