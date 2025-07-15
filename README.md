# xtextfile

The simplest most compact and redable file format with version sensitive serialization which supports both text and binary

The `xtextfile::stream` class provides a streamlined way to read and write structured data to text or binary files. 
It emphasizes minimal code duplication, especially between reading and writing operations. The API is designed such 
that the same callback functions can often handle both modes with little to no modification—typically just a check 
for whether you're reading or writing (e.g., via `isReading()` or a passed boolean flag).

## Key Features
- **Symmetry Between Read and Write**: Unlike many serialization libraries that require separate functions for 
     serialization and deserialization, `xtextfile::stream` uses unified methods like `Record` and `Field`. 
     This minimizes code: you define data layout once in a callback, and it works for both modes. For example,
- in writing mode, you pass values; in reading mode, you pass references/pointers to populate them.
- **Human-Readable Text Format**: Text files are formatted for easy inspection/debugging, with headers,           
     types, and aligned columns. Binary is compact and precise.
- **NonLossy Floats in Text**: As noted, text mode may lose floating-point precision unless using hex representation    
     (default when `flags.m_isWriteFloats` is false).
- **Error Handling**: Operations return `err` objects; check with `isError()` or implicit bool conversion.
- **User-Defined Types**: Compose custom structures from primitives to avoid repeating field definitions.
- **Flexibility**: Supports fixed columns, variable types per line, and dynamic counts in records.
- **Always well typed**: All the data is well typed no guessing not miss interpretation which leads to correct versions
- **Binary serialization** With no code changes
- **No dependencies** Zero dependencies to make it easy to include in your projects
- **MIT License**: As free as it gets
- **documentation**: You can find the [documentation here](https://github.com/LIONant-depot/xtextfile/blob/main/documentation/documentation.md)
- **unit-test**: That servers as examples as well as to test the code

## Example
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

---