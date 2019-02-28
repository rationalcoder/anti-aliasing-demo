
static b32 operator == (buffer32 lhs, buffer32 rhs);
static b32 operator == (buffer32 buffer, char c);
static b32 operator == (buffer32 buffer, const char* str);
static b32 operator != (buffer32 lhs, buffer32 rhs);
static b32 operator != (buffer32 buffer, char c);
static b32 operator != (buffer32 buffer, const char* str);

// stb_printf callback
static char* push_stbsp_temp_storage(char* /*str*/, void* /*userData*/, int len);

static char*    fmt_cstr(const char* fmt, ...);
static buffer32 fmt(const char* fmt, ...);
static string32 to_str(const char* s);
static string32 to_str(buffer32 b);
static char*    to_cstr(string32 b);

static string32 cat(string32 a, string32 b);
static string32 cat(const char* a, string32 b);
static string32 cat(string32 a, const char* b);
static string32 cat(const char* a, const char* b);

static string32 dup(string32 b);
static string32 dup(const char* s);

static char* cstr_line(buffer32 b);

static buffer32 read_file_buffer(const char* file);
static buffer32 read_file_buffer(string32 file);

static buffer32 next_line(buffer32 buffer);

// does not include the newline.
static u32 line_length(buffer32 buffer);

static b32 starts_with(buffer32 buffer, const char* prefix);
static b32 starts_with(buffer32 buffer, const char* prefix, u32 n);
static b32 starts_with(buffer32 buffer, const char** prefixes, u32 n);
template <u32 Size_> 
static b32 starts_with(buffer32 buffer, char (&prefixes)[Size_]);
template <u32 Size_> 
static b32 starts_with(buffer32 buffer, const char* (&prefixes)[Size_]);

// TODO: eat_whitespace() ?
static buffer32 eat_spaces(buffer32 buffer);
static buffer32 eat_spaces_and_tabs(buffer32 buffer);

static string32 first_word(buffer32 buffer);
static string32 next_word(string32 word, buffer32 buffer);

