# babyjson-demo

Education purpose C++17 JSON parser, teach you how to parsing string and numeric literials, as well as using std::variant and std::optional elegently.

Bilibili Video: https://www.bilibili.com/video/BV1pa4y1g7v6

## How to Run

```
cmake -B build
cmake --build build
build/main
```

## Homework

1. Implement the parsing of three special literials: null, false, true and test them in recursive cases.
2. Support string literials in single-quotes as well, e.g. 'string' and "string" should both work.
3. Support for hex character escape sequence like '\x0D' in string (may need two more phase enums: Hex1, Hex2).

## Challenge (accept if you can)

1. Support for UCS2 character escape sequence like '\u000D' in string, and encode it as UTF-8 into std::string.
2. Support for UCS4 character escape sequence like '\U0000000D' in string, and encode it as UTF-8 into std::string.
3. Support any JSONObject to be key for JSONDict (requires hash and equal_to traits for JSONObject).
4. Implement a JSON dumper (as an opposite to the parser) as well.
5. Add an optional argument 'isPretty' in the dump() function, when true, dump JSON with indent and spaces.
6. Support JSONDict keys to be optionally not quoted, e.g. {"hello": "world"} and {hello: "world"} are equivalent.
7. Extend this JSON parser / dumper into a YAML parser / dumper (YAML is a superset of JSON).
