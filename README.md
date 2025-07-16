# xtextfile

The simplest most compact and redable file format with version sensitive serialization which supports both text and binary with minimal code duplication.

The `xtextfile::stream` class makes it easy to read and write structured data using a single code path. 
You can use the same callbacks for both reading and writing by checking `isReading()` or using a simple flag. 
This reduces maintenance and keeps your code clean.

## Key Features

* **Unified Read and Write** - The same code handles both reading and writing, reducing duplication.
* **Very Readable Text Format** - Also supports Binary
* **Accurate Floating-Point Handling** - Hexadecimal float encoding to avoid any precision loss.
* **Simple Error Handling** - Light weight string and error code in one
* **Supports Custom Types** - User-defined structures by composing from fields — no repetitive code.
* **Flexible Structure** - Supports fixed columns, dynamic counts, and per-line type flexibility.
* **Strict Typing** - Data is always well-typed, so no ambiguity
* **Binary Serialization with No Code Changes** - Easily switch between text and binary modes.
* **Easy Integration** - Just include the .cpp and .h in your project and you are done.
* **No Dependencies** - Zero external dependencies. Easy to integrate into any project.
* **MIT License** - Open-source, simple, and permissive license.
* **Documentation and Tests Included** - Full [documentation](https://github.com/LIONant-depot/xtextfile/blob/main/documentation/documentation.md) and unit tests that double as examples.

## Code Example

```cpp
struct data
{
    std::string str;
    double      d; 
    float       f;
};

std::vector<data> list;
xtextfile::stream file;

file.Open(false, L"data.bin", xtextfile::file_type::BINARY);

file.Record(err, "MyList",
    [&](std::size_t& count, xtextfile::err&) {
        if (s.isReading()) list.resize(count);
        else               count = list.size();
    },
    [&](std::size_t index, xtextfile::err& e) {
        s.Field("String", list[index].str);
        s.Field("Floats", list[index].d, list[index].f);
    });
```

## Format Example

```
[ SomeTypes : 4 ]
{ String:s        Floats:Ff                          Ints:DdCc              UInts:GgHh                    }
//--------------  ---------------------------------  ---------------------  -----------------------------
  "String Test"   -32.323212312312322  -50           13452     0 -2312  16  #321D2298C #1FC52AC #7A78 #21
  "Another test"    2.2312323233         2.12330008   -321 21231   211  21        #141   #36471  #8AD #16
  "Yet another"     1.3123231230000001   1.31232309   1434  4344   344 -44        #59A   #1B34A #2CAA #2C
                  
// New Types
< V3:fff BOOL:c STRING:s >

[ Properties : 3 ]
{ Name:s          Value:?                               }
//--------------  -------------------------------------
  "String Test"   ;V3     #3DCCCCCD #3F000000 #3F19999A
  "Another test"  ;BOOL   1                            
  "Yet another"   ;STRING "Hello"                      
```

---