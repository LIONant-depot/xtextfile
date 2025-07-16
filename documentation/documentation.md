# Basic Usage

## Opening and Closing Files
- Use `Open(bool isRead, std::wstring_view path, file_type type, flags flags = {})`.
  - `isRead`: True for reading, false for writing.
  - `type`: `file_type::TEXT` or `::BINARY`.
  - `flags`: Control float writing (decimal vs. hex) and endian swap (binary only).
- Always check the returned `err2`.
- Call `close()` when done.

#### file_type (Enum Class)

| Value  | Description             |
|--------|-------------------------|
| TEXT   | Human-readable text file. |
| BINARY | Compact binary file.    |

#### flags (Union)

Controls writing behavior (bitfields):
- `bool m_isWriteFloats`: If true, writes floats as decimal numbers (default: false, writes as hex for precision).
- `bool m_isWriteEndianSwap`: If true, swaps endianness when writing binary files.

Example:
```cpp
xtextfile::stream s;
auto err = s.Open(false, L"data.txt", xtextfile::file_type::TEXT);
if (err) { /* Handle error */ }
s.close();
```

## Errors using err and err2

Basic error handler. Convertible to `bool` (true if error present). Methods:
- `operator bool() const noexcept`: Checks if an error exists.
- `state getState() const noexcept`: Gets the error state.
- `void clear() noexcept`: Clears the error.
- `bool isError(err& Error) const noexcept`: Checks and copies error if present.
- Static creators: `create()`, `create_f()` for compile-time errors.

#### err2
Extended error with dynamic messages using as a data container (```std::string```)
otherwise it is identical to err.

## Records
Records are named groups of entries (like tables). Use `Record` overloads:
- With count: Provide a callback for count (set or assert) and another for per-entry processing.
- Without count: Simpler for fixed or single-entry records.

The symmetry shines here: The per-entry callback calls `Field` similarly for read/write.

Example:
```cpp
struct data
{
    std::string     str;
    double          d; 
    float           f;
};

std::vector<data> list;
xtextfile::stream file;

file.Open(false, L"data.bin", xtextfile::file_type::BINARY);

// This code will read and write to/from the file...
file.Record(err, "MyList"
    , [&](std::size_t& count, xtextfile::err&) 
    {
        if (s.isReading()) list.resize(count);
        else               count = list.size();
    }
    , [&](std::size_t index, xtextfile::err& e) 
    {
        s.Field("String", list[index].str);
        s.Field("Floats", list[index].d, list[index].f);
    });
```

## Fields
Fields are individual data elements in a record.
- `Field(const char(&name)[N], T_ARGS&... args)`: Name and values/refs matching supported types (int, float, string, etc.).
- Multiple args per field create sub-columns.

This minimizes code: The same `Field` call works for both modes.

The fileds can be one of these supported data types

| Type | Description                  |
|------|------------------------------|
| f    | 32-bit float                 |
| F    | 64-bit double                |
| d    | 32-bit signed integer        |
| g    | 32-bit unsigned integer      |
| D    | 64-bit signed integer        |
| G    | 64-bit unsigned integer      |
| c    | 8-bit signed integer         |
| h    | 8-bit unsigned integer       |
| C    | 16-bit signed integer        |
| H    | 16-bit unsigned integer      |
| s    | String (assumed `std::string`) |
| S    | wide string (assumed `std::wstring`) |

## Comments
- `WriteComment(std::string_view comment)`: Adds comments in text mode (ignored on read).

# Advanced Usage

### User-Defined Types
Define composite types to reuse structures.
- Create `user_defined_types` with name and type string (e.g., "fff" for three floats).
- Add via `AddUserType` or `AddUserTypes(span)`.
- Use in fields: `Field(crc32 type_crc, "name", args...)`.

Example (from unit test):
```cpp
constexpr xtextfile::user_defined_types v3     {"V3", "fff"};
constexpr xtextfile::user_defined_types quat   {"QUATERNION", "ffff"};
constexpr xtextfile::user_defined_types random {"RANDOM", "fsd"};

constexpr static std::array Types = { v3, quat, random};
file.AddUserTypes(Types);

float pos[3] = {0.1f, 0.5f, 0.6f};
float quaternion[4] = {0.1f, 0.2f, 0.3f, 0.4f };
float a = 22;
std::string str("Hello");
int   d = 33;

file.Record(err, "TestUserTypes",
    [&](xtextfile::err& e) 
    {
        0
        || s.Field(v3.m_CRC,     "Position", pos[0], pos[1], pos[2]).isError(e)
        || s.Field(quat.m_CRC,   "Rotation", quaternion[0], quaternion[1], quaternion[2],quaternion[3] ).isError(e)
        || s.Field(random.m_CRC, "Random",   a, str, d ).isError(e);
    });
```

- On read: Use `ReadFieldUserType(crc32& type, "field")` to get type, then switch and call `Field(type, "field", args...)`.
- Minimizes code: Types are declared once, used symmetrically.

## Variable Types Per Line
Sometimes you need the types to be changing per line such forinstance saving a list of properties. We support this via
dynamic types (e.g., "Value:?").
- Append "?" to name for variable typing.
- On write: Choose type per entry.
- On read: Read type first, then data.

Example (properties-like, from unit test):
```cpp
std::vector<float> ListOfFloats;
std::vector<int>   ListOfInts;

file.Record(err, "Properties",
    [&](std::size_t& count, xtextfile::err&) 
    { 
        if (file.isReading()) 
        {
            ListOfFloats.resize(count/2);
            ListOfInts.resize(count/2);
        }
        else
        {
            count += ListOfFloats.size() + ListOfInts.size();
        }
    }
    , [&](std::size_t i, xtextfile::err& e) 
    {
        if (i&1) s.Field(type, "Value:?", ListOfFloats[i/2] );
        else     s.Field(type, "Value:?", ListOfInts[i/2] );
    });
```

## Floating-Point Precision
- Set `flags.m_isWriteFloats = true` for decimal output (lossy).
- Default (false): Hex for exact binary round-trip.
- Use `AlmostEqualRelative` for comparisons when lossy.

# FileFormat

## Text File Format

The text format is human-readable, with sections for records, user types, and data in aligned columns. It's lossy for floats unless hex is used.

## Structure
- **Comments**: Lines starting with "//" (ignored on read).
- **User Types**: "< TypeName:types >" (e.g., "< V3:fff BOOL:c STRING:s >"). Added automatically on write if used.
- **Record Header**: "[ RecordName : Count ]" or "[ RecordName ]" for no count.
- **Column Headers**: "{ FieldName:type/subtypes ... }" with types (e.g., "s" for string, "Ff" for double+float).
  - Variable types: "Field:?".
  - User types: "Field;UserType".
- **Data Rows**: Aligned values, strings in quotes, hex for precise floats (e.g., "#321D2298C").
  - Variable type rows: Start with ";UserType" then values.

#### Example:
```

//----------------------------------------------------
// Example that shows how to use all system types
//----------------------------------------------------

[ TestTypes : 4 ]
{ String:s        WString:S                                       Floats:Ff                    Ints:DdCc              UInts:GgHh                    }
//--------------  ----------------------------------------------  ---------------------------  ---------------------  -----------------------------
  "String Test"   "\u4F60\u597D\uFF0C(Better)\u4E16\u754C\uFF01"  #C040295F056385E2 #C2480000  13452     0 -2312  16  #321D2298C #1FC52AC #7A78 #21
  "Another test"  "\u5B66\u4E60\u5FEB\u4E50"                      #4001D9905512D013 #4007E426   -321 21231   211  21        #141   #36471  #8AD #16
  "Yet another"   "\u548C\u5E73\u4E07\u5C81"                      #3FF4FF4687F11D06 #3FA7FA34   1434  4344   344 -44        #59A   #1B34A #2CAA #2C
  "String Test"   "\u4F60\u597D\uFF0C(Better)\u4E16\u754C\uFF01"  #C040295F056385E2 #C2480000  13452     0 -2312  16  #321D2298C #1FC52AC #7A78 #21

//----------------------------------------------------
// Example that shows how to use per line (rather than per column) types
//----------------------------------------------------

[ VariableTypes : 3 ]
{ Ints:d  value:?                                            }
//------  --------------------------------------------------
      0   :s  "String Test"                                 
  21231   :S  "\u4F60\u597D\uFF0C(Better)\u4E16\u754C\uFF01"
   4344   :Ff #C040295F056385E2 #C2480000                   

//----------------------------------------------------
// Example that shows how to use user types
//----------------------------------------------------

// New Types
< V3:fff BOOL:c STRING:s >

[ TestUserTypes ]
{ Position;V3                    IsValid;BOOL  Name;STRING }
//-----------------------------  ------------  -----------
  #3DCCCCCD #3F000000 #3F19999A       1          "Hello"  

[ VariableUserTypes : 3 ]
{ Value:?                                IsValid;BOOL }
//-------------------------------------  ------------
  ;V3     #3DCCCCCD #3F000000 #3F19999A       1      
  ;BOOL   1                                   1      
  ;STRING "Hello"                             1      

[ Properties : 3 ]
{ Name:s          Value:?                               }
//--------------  -------------------------------------
  "String Test"   ;V3     #3DCCCCCD #3F000000 #3F19999A
  "Another test"  ;BOOL   1                            
  "Yet another"   ;STRING "Hello"                      

```

## Example Breakdown (from provided output)
1. **All System Types Record**:
   - Header: "[ TestTypes : 5 ]"
   - Columns: String (s), Floats (F double, f float), Ints (D int64, d int32, C int16, c int8), UInts (G uint64, g uint32, H uint16, h uint8).
   - Data: Repeated rows with values; floats as decimals (since m_isWriteFloats=true in this sample), uints as hex.

2. **Variable Types Record**:
   - Header: "[ VariableTypes : 5 ]"
   - Columns: Fixed "Ints:d", variable "value:?".
   - Data: Each row has int, then type-specific data (e.g., ":s \"String Test\"", ":Ff -32.32... -50").

3. **User Types and Properties**:
   - Types declared first.
   - Records use ";" for user types in columns or data.

Binary format is not shown but is compact, with similar structure but raw bytes (endian-aware).

This format aids debugging: Edit text files manually if needed.

# Unitest Examples

You can find the unitest header file under the source folder

## Example 1: All System Types
Demonstrates basic fields with primitives.
- Record "TestTypes" with 384 entries (repeated 128 times).
- Fields: String, Floats (double+float), Ints (various sizes), UInts (various sizes).
- Symmetry: Same callback for read/write; assert on read, set on write.
- Precision check: Use almost-equal for floats if written as decimals.

Code snippet (see unit test for full).

## Example 2: Variable Types Per Line
- Record "VariableTypes" with dynamic "value:?".
- Per entry: Fixed int, then switch on index for type (string, floats, ints, uints).
- On read: Would need to read type first (not shown, but similar to properties).

## Example 3: User-Defined Types
- Add types "V3", "BOOL", "STRING".
- Record "TestUserTypes": Fixed fields using user types.
- Record "VariableUserTypes": Dynamic "Value:?" with user types.

## Example 4: Properties-Like
- Record "Properties": Name + dynamic value based on type.
- On read: Read type, switch, read data.
- Minimizes code for variant data.

## Running Tests
- The `Test()` function writes/reads text and binary files, asserting equality.
- Files like "TextFileTest.la1.txt" generated for inspection.

---